#include "NotificationRuleStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QtConcurrent/QtConcurrentRun>

#include <optional>
#include <utility>

Q_LOGGING_CATEGORY(lcNotifRules, "holonight.notifications.rules")

namespace {

constexpr int kRuleStoreVersion = 1;

QString resolveRuleFilePath() {
  QString config_home = qEnvironmentVariable("XDG_CONFIG_HOME");
  if (config_home.isEmpty()) {
    config_home = QDir::homePath() + QLatin1String("/.config");
  }
  return config_home + QLatin1String("/holonight/notification-rules.json");
}

bool requireString(const QJsonObject& obj, QStringView key, QString* out) {
  const QJsonValue val = obj.value(key.toString());
  if (!val.isString()) {
    qCWarning(lcNotifRules) << "rule entry missing or invalid" << key << "; skipping";
    return false;
  }
  *out = val.toString();
  return true;
}

bool requireInt(const QJsonObject& obj, QStringView key, qint64* out) {
  const QJsonValue val = obj.value(key.toString());
  if (!val.isDouble()) {
    qCWarning(lcNotifRules) << "rule entry missing or invalid" << key << "; skipping";
    return false;
  }
  *out = val.toInteger();
  return true;
}

bool requireBool(const QJsonObject& obj, QStringView key, bool* out) {
  const QJsonValue val = obj.value(key.toString());
  if (!val.isBool()) {
    qCWarning(lcNotifRules) << "rule entry missing or invalid" << key << "; skipping";
    return false;
  }
  *out = val.toBool();
  return true;
}

QJsonObject ruleToJson(const AppNotificationRule& rule) {
  QJsonObject obj;
  obj[QStringLiteral("appName")] = rule.app_name;
  obj[QStringLiteral("desktopEntry")] = rule.desktop_entry;
  obj[QStringLiteral("appIcon")] = rule.app_icon;
  obj[QStringLiteral("enabled")] = rule.enabled;
  obj[QStringLiteral("urgencyFilter")] = static_cast<int>(rule.urgency_filter);
  obj[QStringLiteral("lastSeenMs")] = rule.last_seen_ms;
  return obj;
}

std::optional<AppNotificationRule> ruleFromJson(const QJsonObject& obj) {
  AppNotificationRule rule;
  if (!requireString(obj, QStringLiteral("appName"), &rule.app_name) ||
      !requireString(obj, QStringLiteral("desktopEntry"), &rule.desktop_entry) ||
      !requireString(obj, QStringLiteral("appIcon"), &rule.app_icon) ||
      !requireBool(obj, QStringLiteral("enabled"), &rule.enabled)) {
    return std::nullopt;
  }

  qint64 urgency_filter = 0;
  qint64 last_seen_ms = 0;
  if (!requireInt(obj, QStringLiteral("urgencyFilter"), &urgency_filter) ||
      !requireInt(obj, QStringLiteral("lastSeenMs"), &last_seen_ms)) {
    return std::nullopt;
  }
  if (rule.app_name.isEmpty() || urgency_filter < 0 || urgency_filter > 3 || last_seen_ms < 0) {
    qCWarning(lcNotifRules) << "rule entry contains out-of-range values; skipping";
    return std::nullopt;
  }

  rule.urgency_filter = static_cast<UrgencyFilter>(urgency_filter);
  rule.last_seen_ms = last_seen_ms;
  return rule;
}

// Pure function run on a worker thread — writes rules to disk atomically.
RulePersistOutcome writeRulesToDisk(const QString& file_path, const QList<AppNotificationRule>& rules) {
  QJsonArray entries;
  for (const AppNotificationRule& rule : rules) {
    entries.append(ruleToJson(rule));
  }

  QJsonObject root;
  root[QStringLiteral("version")] = kRuleStoreVersion;
  root[QStringLiteral("rules")] = entries;

  QSaveFile file(file_path);
  if (!file.open(QIODevice::WriteOnly)) {
    return RulePersistOutcome{.ok = false, .reason = QStringLiteral("failed to open %1 for writing").arg(file_path)};
  }
  file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  if (!file.commit()) {
    return RulePersistOutcome{.ok = false, .reason = QStringLiteral("failed to commit %1").arg(file_path)};
  }
  return RulePersistOutcome{.ok = true, .reason = {}};
}

std::optional<QJsonArray> parseRules(const QByteArray& data) {
  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(data, &parse_error);
  if (doc.isNull()) {
    qCWarning(lcNotifRules) << "Rule JSON parse error:" << parse_error.errorString();
    return std::nullopt;
  }
  if (!doc.isObject()) {
    qCWarning(lcNotifRules) << "Rule JSON root is not an object; ignoring file";
    return std::nullopt;
  }

  const QJsonObject root = doc.object();
  const QJsonValue version = root.value(QStringLiteral("version"));
  if (!version.isDouble() || version.toInt() != kRuleStoreVersion) {
    qCWarning(lcNotifRules) << "Rule JSON version mismatch; ignoring file";
    return std::nullopt;
  }
  const QJsonValue rules = root.value(QStringLiteral("rules"));
  if (!rules.isArray()) {
    qCWarning(lcNotifRules) << "Rule JSON rules is not an array; ignoring file";
    return std::nullopt;
  }
  return rules.toArray();
}

}  // namespace

NotificationRuleStore::NotificationRuleStore(QObject* parent) : QObject(parent), file_path_(resolveRuleFilePath()) {}

NotificationRuleStore::NotificationRuleStore(QString file_path, QObject* parent)
    : QObject(parent), file_path_(std::move(file_path)) {}

QList<AppNotificationRule> NotificationRuleStore::load() const {
  QList<AppNotificationRule> result;

  QFile file(file_path_);
  if (!file.exists()) {
    return result;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    qCWarning(lcNotifRules) << "Failed to open notification rule file:" << file_path_;
    return result;
  }

  const std::optional<QJsonArray> maybe_rules = parseRules(file.readAll());
  if (!maybe_rules.has_value()) {
    return result;
  }

  result.reserve(maybe_rules->size());
  for (const auto& val : *maybe_rules) {
    if (!val.isObject()) {
      qCWarning(lcNotifRules) << "Rule entry is not a JSON object; skipping";
      continue;
    }
    const std::optional<AppNotificationRule> rule = ruleFromJson(val.toObject());
    if (rule.has_value()) {
      result.append(*rule);
    }
  }

  qCInfo(lcNotifRules) << "Loaded" << result.size() << "notification rules from" << file_path_;
  return result;
}

void NotificationRuleStore::persist(const QList<AppNotificationRule>& rules, const QString& action) {
  if (write_in_flight_) {
    write_dirty_ = true;
    pending_rules_ = rules;
    pending_action_ = action;
    return;
  }
  launchWrite(rules, action);
}

void NotificationRuleStore::launchWrite(QList<AppNotificationRule> rules, QString action) {
  ensureDirectoryExists();

  write_in_flight_ = true;
  write_dirty_ = false;
  in_flight_action_ = std::move(action);

  const QString path = file_path_;

  if (watcher_ == nullptr) {
    watcher_ = new QFutureWatcher<RulePersistOutcome>(this);
    connect(watcher_, &QFutureWatcher<RulePersistOutcome>::finished, this, &NotificationRuleStore::onWriteFinished);
  }

  watcher_->setFuture(QtConcurrent::run([path, items = std::move(rules)] { return writeRulesToDisk(path, items); }));
}

void NotificationRuleStore::onWriteFinished() {
  write_in_flight_ = false;
  const RulePersistOutcome result = watcher_->result();
  emit writeCompleted();
  if (!result.ok) {
    qCWarning(lcNotifRules) << "Failed to persist notification rules (" << in_flight_action_ << "):" << result.reason;
    emit persistFailed(in_flight_action_, result.reason);
  }
  if (write_dirty_) {
    launchWrite(std::move(pending_rules_), std::move(pending_action_));
  }
}

void NotificationRuleStore::ensureDirectoryExists() const {
  const QDir dir = QFileInfo(file_path_).dir();
  if (!dir.exists() && !dir.mkpath(QLatin1String("."))) {
    qCWarning(lcNotifRules) << "Failed to create notification rule directory:" << dir.absolutePath();
  }
}
