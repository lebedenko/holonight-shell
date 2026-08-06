#include "NotificationRuleModel.h"

#include "NotificationRuleStore.h"

#include <QDateTime>
#include <QFileInfo>
#include <QProcess>
#include <QSet>

#include <algorithm>
#include <utility>

namespace {
constexpr QLatin1StringView kShellAppName{"HoloNight Shell"};
constexpr QLatin1StringView kShellLogoUrl{"qrc:/HolonightShell/logo.png"};

QString desktopEntryMatchKey(QString value) {
  value = value.trimmed();
  if (value.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
    value.chop(8);
  }
  return value.toCaseFolded();
}

QString desktopEntryExecutable(const DesktopEntry& entry) {
  const QStringList command = QProcess::splitCommand(stripDesktopExecFieldCodes(entry.exec));
  return command.isEmpty() ? QString{} : QFileInfo(command.first()).fileName();
}

bool desktopEntryMatchesAppName(const DesktopEntry& entry, const QString& app_name) {
  const QString app_key = desktopEntryMatchKey(app_name);
  if (app_key.isEmpty()) {
    return false;
  }
  return app_key == desktopEntryMatchKey(QFileInfo(entry.desktop_file).fileName()) ||
         app_key == desktopEntryMatchKey(entry.startup_wm_class) || app_key == desktopEntryMatchKey(entry.name) ||
         app_key == desktopEntryMatchKey(entry.icon) || app_key == desktopEntryMatchKey(desktopEntryExecutable(entry));
}

bool isShellApp(const QString& app_name) { return app_name.compare(kShellAppName, Qt::CaseInsensitive) == 0; }
}  // namespace

NotificationRuleModel::NotificationRuleModel(QObject* parent) : QAbstractListModel(parent) {}

NotificationRuleModel::NotificationRuleModel(NotificationRuleStore* store, const DesktopEntryScanner& scanner,
                                             QObject* parent)
    : QAbstractListModel(parent) {
  loadPersistedRules(store, scanner);
}

void NotificationRuleModel::loadPersistedRules(NotificationRuleStore* store, const DesktopEntryScanner& scanner) {
  store_ = store;
  if (store_ == nullptr) {
    return;
  }
  connect(store_, &NotificationRuleStore::persistFailed, this, &NotificationRuleModel::rulePersistenceFailed);

  desktop_entries_ = desktopEntriesById(scanner);
  const QList<AppNotificationRule> persisted = store_->load();
  const QList<AppNotificationRule> loaded =
      boundedRules(prunedRules(persisted, QSet<QString>(desktop_entries_.keyBegin(), desktop_entries_.keyEnd())));
  if (rules_.isEmpty()) {
    if (!loaded.isEmpty()) {
      beginInsertRows({}, 0, static_cast<int>(loaded.size() - 1));
      rules_ = loaded;
      endInsertRows();
    }
  } else {
    beginResetModel();
    rules_ = loaded;
    endResetModel();
  }
  if (rules_.size() != persisted.size()) {
    persist(QStringLiteral("pruneRules"));
  }
}

void NotificationRuleModel::ensureApp(const QString& app_name) {
  if (app_name.isEmpty()) {
    return;
  }
  NotificationData data;
  data.app_name = app_name;
  ensureApp(data);
}

void NotificationRuleModel::ensureApp(const NotificationData& data) {
  if (data.app_name.isEmpty()) {
    return;
  }
  const QString desktop_entry = normalizedDesktopEntryId(data.hints.value(QStringLiteral("desktop-entry")).toString());
  const QString app_icon = data.app_icon;
  const qint64 last_seen_ms = QDateTime::currentMSecsSinceEpoch();

  const auto found_iter =
      std::ranges::find_if(rules_, [&](const AppNotificationRule& rule) { return rule.app_name == data.app_name; });
  if (found_iter != rules_.end()) {
    const int row = static_cast<int>(std::distance(rules_.begin(), found_iter));
    QVector<int> changed_roles{LastSeenMsRole};
    found_iter->last_seen_ms = last_seen_ms;
    if (!desktop_entry.isEmpty() && found_iter->desktop_entry != desktop_entry) {
      found_iter->desktop_entry = desktop_entry;
      changed_roles.append(DesktopEntryRole);
    }
    if (!app_icon.isEmpty() && found_iter->app_icon != app_icon) {
      found_iter->app_icon = app_icon;
      changed_roles.append(AppIconRole);
    }
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, changed_roles);
    persist(QStringLiteral("ensureApp"));
    return;
  }

  if (rules_.size() >= kMaxRuleCount) {
    return;
  }

  const int new_row = static_cast<int>(rules_.size());
  beginInsertRows({}, new_row, new_row);
  rules_.append(AppNotificationRule{
      .app_name = data.app_name,
      .desktop_entry = desktop_entry,
      .app_icon = app_icon,
      .last_seen_ms = last_seen_ms,
  });
  endInsertRows();
  persist(QStringLiteral("ensureApp"));
}

QHash<QString, DesktopEntry> NotificationRuleModel::desktopEntriesById(const DesktopEntryScanner& scanner) {
  QHash<QString, DesktopEntry> entries_by_id;
  for (const DesktopEntry& entry : scanner.scanForDefaultApps()) {
    const QString entry_id = normalizedDesktopEntryId(QFileInfo(entry.desktop_file).fileName());
    if (!entry_id.isEmpty()) {
      entries_by_id.insert(entry_id, entry);
    }
    const QString startup_wm_class = normalizedDesktopEntryId(entry.startup_wm_class);
    if (!startup_wm_class.isEmpty()) {
      entries_by_id.insert(startup_wm_class, entry);
    }
  }
  return entries_by_id;
}

DesktopEntry NotificationRuleModel::displayDesktopEntry(const AppNotificationRule& rule) const {
  if (!rule.desktop_entry.isEmpty()) {
    const auto explicit_entry = desktop_entries_.constFind(rule.desktop_entry);
    if (explicit_entry != desktop_entries_.cend()) {
      return *explicit_entry;
    }
  }

  DesktopEntry matched_entry;
  QSet<QString> seen_desktop_files;
  for (const DesktopEntry& entry : desktop_entries_) {
    if (seen_desktop_files.contains(entry.desktop_file) || !desktopEntryMatchesAppName(entry, rule.app_name)) {
      continue;
    }
    seen_desktop_files.insert(entry.desktop_file);
    if (!matched_entry.desktop_file.isEmpty()) {
      return {};
    }
    matched_entry = entry;
  }
  return matched_entry;
}

QList<AppNotificationRule> NotificationRuleModel::prunedRules(const QList<AppNotificationRule>& rules,
                                                              const QSet<QString>& installed_ids) {
  QList<AppNotificationRule> result;
  result.reserve(rules.size());
  for (AppNotificationRule rule : rules) {
    rule.desktop_entry = normalizedDesktopEntryId(rule.desktop_entry);
    if (!rule.desktop_entry.isEmpty() && !installed_ids.contains(rule.desktop_entry)) {
      continue;
    }
    result.append(std::move(rule));
  }
  return result;
}

QList<AppNotificationRule> NotificationRuleModel::boundedRules(QList<AppNotificationRule> rules) {
  while (rules.size() > kMaxRuleCount) {
    const auto oldest = std::ranges::min_element(rules, {}, &AppNotificationRule::last_seen_ms);
    rules.erase(oldest);
  }
  return rules;
}

QString NotificationRuleModel::normalizedDesktopEntryId(QString desktop_entry) {
  desktop_entry = desktop_entry.trimmed();
  if (desktop_entry.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
    desktop_entry.chop(8);
  }
  return desktop_entry;
}

void NotificationRuleModel::persist(const QString& action) const {
  if (store_ != nullptr) {
    store_->persist(rules_, action);
  }
}

void NotificationRuleModel::setEnabled(int row, bool enabled) {
  if (row < 0 || row >= rules_.size()) {
    return;
  }
  if (rules_[row].enabled == enabled) {
    return;
  }
  rules_[row].enabled = enabled;
  const QModelIndex idx = index(row);
  emit dataChanged(idx, idx, {EnabledRole});
  persist(QStringLiteral("setEnabled"));
}

void NotificationRuleModel::setUrgencyFilter(int row, int filter) {
  if (row < 0 || row >= rules_.size()) {
    return;
  }
  const auto clamped = static_cast<uint8_t>(qBound(0, filter, 3));
  if (rules_[row].urgency_filter == static_cast<UrgencyFilter>(clamped)) {
    return;
  }
  rules_[row].urgency_filter = static_cast<UrgencyFilter>(clamped);
  const QModelIndex idx = index(row);
  emit dataChanged(idx, idx, {UrgencyFilterRole});
  persist(QStringLiteral("setUrgencyFilter"));
}

int NotificationRuleModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(rules_.size());
}

QVariant NotificationRuleModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rules_.size()) {
    return {};
  }
  const AppNotificationRule& rule = rules_.at(index.row());
  const DesktopEntry desktop_entry = displayDesktopEntry(rule);
  switch (role) {
    case AppNameRole:
      return rule.app_name;
    case DisplayNameRole:
      return desktop_entry.name.isEmpty() ? rule.app_name : desktop_entry.name;
    case DisplayIconRole:
      if (isShellApp(rule.app_name)) {
        return QString(kShellLogoUrl);
      }
      return desktop_entry.icon.isEmpty() ? rule.app_icon : desktop_entry.icon;
    case EnabledRole:
      return rule.enabled;
    case UrgencyFilterRole:
      return static_cast<int>(rule.urgency_filter);
    case DesktopEntryRole:
      return rule.desktop_entry;
    case AppIconRole:
      return rule.app_icon;
    case LastSeenMsRole:
      return rule.last_seen_ms;
    default:
      return {};
  }
}

QHash<int, QByteArray> NotificationRuleModel::roleNames() const {
  static const QHash<int, QByteArray> kRoles{
      {AppNameRole, "appName"},     {DisplayNameRole, "displayName"},     {DisplayIconRole, "displayIcon"},
      {EnabledRole, "ruleEnabled"}, {UrgencyFilterRole, "urgencyFilter"}, {DesktopEntryRole, "desktopEntry"},
      {AppIconRole, "appIcon"},     {LastSeenMsRole, "lastSeenMs"},
  };
  return kRoles;
}
