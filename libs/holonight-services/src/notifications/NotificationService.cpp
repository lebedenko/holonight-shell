#include "NotificationService.h"

#include "ActiveWindowService.h"
#include "CompositorService.h"
#include "ConfigService.h"
#include "NotificationFilter.h"
#include "NotificationPolicy.h"
#include "NotificationRuleModel.h"
#include "NotificationStore.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QScreen>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <limits>
#include <utility>

namespace {

// Per-monitor visible filter: accepts a source row only when its monitor matches and its
// lifecycle is Visible. Overriding filterAcceptsRow lets us combine the two predicates that a
// stock single-role QSortFilterProxyModel cannot. No Q_OBJECT needed — only the virtual is
// overridden and no new meta members are introduced.
class NotificationVisibleFilterModel : public QSortFilterProxyModel {
 public:
  NotificationVisibleFilterModel(QString monitor_name, QObject* parent)
      : QSortFilterProxyModel(parent), monitor_name_(std::move(monitor_name)) {}

 protected:
  [[nodiscard]] bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override {
    const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    if (!idx.isValid()) {
      return false;
    }
    const QString monitor = idx.data(NotificationService::MonitorRole).toString();
    const int lifecycle = idx.data(NotificationService::LifecycleRole).toInt();
    return monitor == monitor_name_ && lifecycle == static_cast<int>(NotifLifecycle::Visible);
  }

 private:
  QString monitor_name_;
};

QString accentKindString(NotifAccentKind accent) {
  switch (accent) {
    case NotifAccentKind::Critical:
      return QStringLiteral("critical");
    case NotifAccentKind::Violet:
      return QStringLiteral("violet");
    case NotifAccentKind::Cyan:
      break;
  }
  return QStringLiteral("cyan");
}

QVariantList actionsToVariant(const QList<NotifAction>& actions) {
  QVariantList list;
  list.reserve(actions.size());
  for (const NotifAction& action : actions) {
    QVariantMap entry;
    entry.insert(QStringLiteral("key"), action.key);
    entry.insert(QStringLiteral("label"), action.label);
    list.append(entry);
  }
  return list;
}

}  // namespace

NotificationService::NotificationService(  // NOLINT(readability-function-cognitive-complexity)
    ConfigService* config, ActiveWindowService* active_window, NotificationRuleModel* rule_model, QObject* parent)
    : QAbstractListModel(parent), config_(config), active_window_(active_window), rule_model_(rule_model) {
  if (config_ != nullptr) {
    default_timeout_ms_ = config_->notifications().default_timeout_ms;
    max_visible_ = config_->notifications().max_visible;
    history_config_ = config_->notificationHistory();
    connect(config_, &ConfigService::notificationsChanged, this, &NotificationService::onConfigChanged);
    connect(config_, &ConfigService::notificationHistoryChanged, this, [this] {
      if (config_ != nullptr) {
        history_config_ = config_->notificationHistory();
        ensureHistoryStore();
        if (store_ != nullptr) {
          store_->updateConfig(history_config_);
        }
        if (runEviction()) {
          const int old_count = unread_count_;
          recomputeUnreadCount();
          if (unread_count_ != old_count) {
            emit unreadCountChanged();
          }
          if (store_ != nullptr) {
            store_->persist(history_);
          }
        }
      }
    });
  }

  ensureHistoryStore();
  if (store_ != nullptr) {
    // Age-evict stale entries loaded from disk; all survivors are pre-marked read.
    const bool evicted_on_load = runEviction();
    for (NotificationHistoryItem& item : history_) {
      item.read = true;
    }
    unread_count_ = 0;
    if (evicted_on_load) {
      store_->persist(history_);
    }
  }
}

void NotificationService::ensureHistoryStore() {
  if (!history_config_.enabled || store_ != nullptr) {
    return;
  }
  store_ = new NotificationStore(history_config_, this);
  history_ = store_->load();
  connect(store_, &NotificationStore::writeCompleted, this, [this] {
    if (runEviction()) {
      const int old_count = unread_count_;
      recomputeUnreadCount();
      if (unread_count_ != old_count) {
        emit unreadCountChanged();
      }
      if (store_ != nullptr) {
        store_->persist(history_);
      }
    }
  });
}

int NotificationService::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(row_order_.size());
}

QVariant NotificationService::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= row_order_.size()) {
    return {};
  }
  const uint32_t notif_id = row_order_.at(index.row());
  const auto found = all_notifications_.constFind(notif_id);
  if (found == all_notifications_.constEnd()) {
    return {};
  }
  const NotificationData& notif = *found;

  switch (role) {
    case NotifIdRole:
      return notif.id;
    case SummaryRole:
      return notif.summary;
    case BodyRole:
      return notif.body;
    case AppIconRole:
      return notif.app_icon;
    case ActionsRole:
      return actionsToVariant(notif.actions);
    case HasDefaultActionRole:
      return notif.has_default_action;
    case AccentKindRole:
      return accentKindString(notif.accent);
    case UrgencyRole:
      return static_cast<int>(notif.urgency);
    case IsResidentRole:
      return notif.is_resident;
    case MonitorRole:
      return notif.monitor_name;
    case LifecycleRole:
      return static_cast<int>(notif.lifecycle);
    case CreatedAtMsRole:
      return notif.created_at_ms;
    default:
      return {};
  }
}

QHash<int, QByteArray> NotificationService::roleNames() const {
  static const QHash<int, QByteArray> kRoles{
      {NotifIdRole, "notifId"},       {SummaryRole, "summary"},     {BodyRole, "body"},
      {AppIconRole, "appIcon"},       {ActionsRole, "actions"},     {HasDefaultActionRole, "hasDefaultAction"},
      {AccentKindRole, "accentKind"}, {UrgencyRole, "urgency"},     {IsResidentRole, "isResident"},
      {MonitorRole, "monitorName"},   {LifecycleRole, "lifecycle"}, {CreatedAtMsRole, "createdAtMs"},
  };
  return kRoles;
}

uint32_t NotificationService::addOrReplace(NotificationData data) {
  // Auto-populate rule model before filtering (REQ-F-NH06: even suppressed notifications register the app).
  if (rule_model_ != nullptr) {
    rule_model_->ensureApp(data);
  }

  // Apply DND + per-app filter. Critical urgency always passes (REQ-F-NH16 > REQ-F-NH10).
  if (rule_model_ != nullptr && evaluateFilter(data, dnd_enabled_, rule_model_->rules()) == FilterDecision::Suppress) {
    return 0;
  }

  data.accent = accentForData(data);
  data.effective_timeout_ms = effectiveTimeoutMs(data, default_timeout_ms_);

  // Replace path: data.id carries the caller's replaces_id.
  if (data.id != 0) {
    const auto existing = all_notifications_.constFind(data.id);
    if (existing != all_notifications_.constEnd()) {
      // monitor_name is immutable after arrival; lifecycle/placement is preserved.
      data.monitor_name = existing->monitor_name;
      data.lifecycle = existing->lifecycle;
      data.created_at_ms = existing->created_at_ms;
      all_notifications_.insert(data.id, data);
      // Replace resets the auto-dismiss timer for a still-visible notification (REQ-F-018).
      if (data.lifecycle == NotifLifecycle::Visible) {
        stopTimeout(data.id);
        armTimeout(data.id);
      }
      const int row = rowForId(data.id);
      if (row >= 0) {
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx);
      }
      emit notificationUpdated(data.id);
      return data.id;
    }
  }

  // New notification.
  const uint32_t notif_id = allocateNotificationId();
  if (notif_id == 0) {
    return 0;
  }
  data.id = notif_id;
  data.created_at_ms = QDateTime::currentMSecsSinceEpoch();
  if (data.monitor_name.isEmpty()) {
    data.monitor_name = resolveMonitorName();
  }

  const int new_row = static_cast<int>(row_order_.size());
  beginInsertRows({}, new_row, new_row);
  all_notifications_.insert(notif_id, data);
  row_order_.append(notif_id);
  endInsertRows();

  place(notif_id, data.monitor_name);
  emit notificationAdded(notif_id, data.monitor_name);
  return notif_id;
}

// The per-monitor maps and the id→data hash are addressed with operator[] for its
// get-or-default-insert semantics (a missing monitor key yields a fresh empty list); the ids
// are always known-present, so the bounds-access check does not apply.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
void NotificationService::place(uint32_t notif_id, const QString& monitor_name) {
  QList<uint32_t>& visible = visible_by_monitor_[monitor_name];
  QList<uint32_t>& queue = queue_by_monitor_[monitor_name];
  NotificationData& notif = all_notifications_[notif_id];

  QHash<uint32_t, NotifUrgency> visible_urgencies;
  visible_urgencies.reserve(visible.size());
  for (uint32_t visible_id : std::as_const(visible)) {
    visible_urgencies.insert(visible_id, all_notifications_[visible_id].urgency);
  }

  const NotificationPlacementDecision decision =
      placeNotification(visible, queue, notif_id, notif.urgency, visible_urgencies, max_visible_);
  if (decision.bumped_id != 0) {
    stopTimeout(decision.bumped_id);
    all_notifications_[decision.bumped_id].lifecycle = NotifLifecycle::Queued;
    emitRowChangedForId(decision.bumped_id);
  }

  notif.lifecycle = decision.lifecycle;
  if (decision.lifecycle == NotifLifecycle::Visible) {
    armTimeout(notif_id);
  }
  emitRowChangedForId(notif_id);
  if (decision.queue_changed) {
    emit queueChanged(monitor_name);
  }
}

void NotificationService::promoteFromQueue(const QString& monitor_name) {
  QList<uint32_t>& visible = visible_by_monitor_[monitor_name];
  QList<uint32_t>& queue = queue_by_monitor_[monitor_name];
  const std::optional<uint32_t> promoted_id = promoteQueuedNotification(visible, queue, max_visible_);
  if (!promoted_id) {
    return;
  }
  all_notifications_[*promoted_id].lifecycle = NotifLifecycle::Visible;
  armTimeout(*promoted_id);
  emitRowChangedForId(*promoted_id);
  emit queueChanged(monitor_name);
}

void NotificationService::closeNotification(uint32_t notif_id, NotifCloseReason reason) {
  const auto found = all_notifications_.constFind(notif_id);
  if (found == all_notifications_.constEnd()) {
    return;  // unknown id: no-op, no signal (matches CloseNotification spec)
  }
  const QString monitor_name = found->monitor_name;

  // Build history item before erasing from the map (reference would dangle after remove).
  const bool write_history = history_config_.enabled && store_ != nullptr &&
                             (reason == NotifCloseReason::Expired || reason == NotifCloseReason::Closed);
  NotificationHistoryItem hist_item;
  if (write_history) {
    hist_item.id = found->id;
    hist_item.app_name = found->app_name;
    hist_item.summary = found->summary;
    hist_item.body = found->body;
    hist_item.app_icon = found->app_icon;
    hist_item.app_class = found->hints.value(QStringLiteral("desktop-entry")).toString();
    hist_item.urgency = static_cast<int>(found->urgency);
    hist_item.timestamp_ms = QDateTime::currentMSecsSinceEpoch();
    hist_item.closed_reason = static_cast<int>(reason);
    hist_item.read = false;
    hist_item.actions = found->actions;
  }

  stopTimeout(notif_id);
  visible_by_monitor_[monitor_name].removeOne(notif_id);
  queue_by_monitor_[monitor_name].removeOne(notif_id);

  const int row = rowForId(notif_id);
  if (row >= 0) {
    beginRemoveRows({}, row, row);
    row_order_.removeAt(row);
    all_notifications_.remove(notif_id);
    endRemoveRows();
  } else {
    all_notifications_.remove(notif_id);
  }

  emit notificationClosed(notif_id, static_cast<uint>(reason));
  promoteFromQueue(monitor_name);
  emit queueChanged(monitor_name);

  if (write_history) {
    history_.append(hist_item);
    runEviction();
    recomputeUnreadCount();
    emit unreadCountChanged();
    store_->persist(history_);
  }
}

void NotificationService::armTimeout(uint32_t notif_id) {
  NotificationData& notif = all_notifications_[notif_id];
  notif.remaining_timeout_ms = notif.effective_timeout_ms;
  if (notif.effective_timeout_ms <= 0) {
    return;  // 0 / -1 → never auto-expires (REQ-F-015, REQ-F-016)
  }
  QTimer* timer = timers_.value(notif_id, nullptr);
  if (timer == nullptr) {
    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this,
            [this, notif_id] { closeNotification(notif_id, NotifCloseReason::Expired); });
    timers_.insert(notif_id, timer);
  }
  timer->setInterval(notif.effective_timeout_ms);
  timer->start();
}

void NotificationService::stopTimeout(uint32_t notif_id) {
  QTimer* timer = timers_.take(notif_id);
  if (timer != nullptr) {
    timer->stop();
    timer->deleteLater();
  }
}

void NotificationService::invokeAction(uint notif_id, const QString& action_key) {
  const auto found = all_notifications_.constFind(notif_id);
  if (found == all_notifications_.constEnd()) {
    return;
  }
  // Read residency before closing — closeNotification() erases the entry.
  const bool resident = found->is_resident;
  emit actionInvoked(notif_id, action_key);
  if (!resident) {
    closeNotification(notif_id, NotifCloseReason::Closed);
  }
}

void NotificationService::dismiss(uint notif_id) { closeNotification(notif_id, NotifCloseReason::Dismissed); }

void NotificationService::requestClose(uint32_t notif_id) { closeNotification(notif_id, NotifCloseReason::Closed); }

bool NotificationService::hasNotificationsForMonitor(const QString& monitor_name) const {
  return !visible_by_monitor_.value(monitor_name).isEmpty() || !queue_by_monitor_.value(monitor_name).isEmpty();
}

void NotificationService::hoverEntered(uint notif_id) {
  if (!all_notifications_.contains(notif_id)) {
    return;
  }
  QTimer* timer = timers_.value(notif_id, nullptr);
  if (timer != nullptr && timer->isActive()) {
    all_notifications_[notif_id].remaining_timeout_ms = timer->remainingTime();
    timer->stop();
  }
}

void NotificationService::hoverLeft(uint notif_id) {
  if (!all_notifications_.contains(notif_id)) {
    return;
  }
  QTimer* timer = timers_.value(notif_id, nullptr);
  const int remaining = all_notifications_[notif_id].remaining_timeout_ms;
  if (timer != nullptr && remaining > 0) {
    timer->setInterval(remaining);
    timer->start();
  }
}
// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

void NotificationService::emitRowChangedForId(uint32_t notif_id) {
  const int row = rowForId(notif_id);
  if (row >= 0) {
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
  }
}

QAbstractItemModel* NotificationService::visibleModelForMonitor(const QString& monitor_name) {
  const auto cached = visible_models_.constFind(monitor_name);
  if (cached != visible_models_.constEnd()) {
    return *cached;
  }
  auto* proxy = new NotificationVisibleFilterModel(monitor_name, this);
  proxy->setSourceModel(this);
  visible_models_.insert(monitor_name, proxy);
  return proxy;
}

int NotificationService::rowForId(uint32_t notif_id) const { return static_cast<int>(row_order_.indexOf(notif_id)); }

uint32_t NotificationService::allocateNotificationId() {
  for (uint64_t attempts = 0; attempts <= std::numeric_limits<uint32_t>::max(); ++attempts) {
    const uint32_t candidate = next_id_++;
    if (next_id_ == 0) {
      next_id_ = 1;
    }
    if (candidate != 0 && !all_notifications_.contains(candidate)) {
      return candidate;
    }
  }
  return 0;
}

QString NotificationService::resolveMonitorName() const {
  if (active_window_ != nullptr) {
    const QString focused = active_window_->focusedMonitorName();
    if (!focused.isEmpty()) {
      return focused;
    }
  }
  if (compositor_ != nullptr && compositor_->connected() && compositor_->hasFocusedOutput()) {
    const QString focused = compositor_->focusedOutput();
    if (!focused.isEmpty()) {
      return focused;
    }
  }
  if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr) {
    if (QScreen* primary = QGuiApplication::primaryScreen()) {
      return primary->name();
    }
  }
  return {};
}

QString NotificationService::unreadAppNames() const {
  QStringList names;
  for (const NotificationHistoryItem& item : history_) {
    if (!item.read) {
      QString display = item.app_name;
      if (display.isEmpty()) {
        display = item.app_class;
      }
      if (display.isEmpty()) {
        display = item.summary;
      }
      if (!display.isEmpty() && !names.contains(display)) {
        names.append(display);
      }
    }
  }
  names.sort(Qt::CaseInsensitive);
  return names.join(QStringLiteral(", "));
}

QVariantList NotificationService::recentHistoryGrouped(int maxGroups) const {
  struct Group {
    QString app_name;
    QString app_icon;
    QString latest_summary;
    qint64 latest_timestamp_ms{0};
    int total_count{0};
    int unread_count{0};
  };

  QHash<QString, Group> groups;
  for (const NotificationHistoryItem& item : history_) {
    auto& grp = groups[item.app_name];
    grp.app_name = item.app_name;
    if (grp.app_icon.isEmpty()) {
      grp.app_icon = item.app_icon;
    }
    if (item.timestamp_ms > grp.latest_timestamp_ms) {
      grp.latest_timestamp_ms = item.timestamp_ms;
      grp.latest_summary = item.summary;
    }
    grp.total_count++;
    if (!item.read) {
      grp.unread_count++;
    }
  }

  QList<Group> sorted = groups.values();
  std::ranges::sort(
      sorted, [](const Group& lhs, const Group& rhs) { return lhs.latest_timestamp_ms > rhs.latest_timestamp_ms; });

  if (sorted.size() > maxGroups) {
    sorted.resize(maxGroups);
  }

  QVariantList result;
  result.reserve(sorted.size());
  for (const Group& grp : std::as_const(sorted)) {
    QVariantMap map;
    map[QStringLiteral("appName")] = grp.app_name;
    map[QStringLiteral("appIcon")] = grp.app_icon;
    map[QStringLiteral("unreadCount")] = grp.unread_count;
    map[QStringLiteral("latestSummary")] = grp.latest_summary;
    map[QStringLiteral("latestTimestampMs")] = grp.latest_timestamp_ms;
    map[QStringLiteral("totalCount")] = grp.total_count;
    result.append(map);
  }
  return result;
}

void NotificationService::setDndEnabled(bool enabled) {
  if (dnd_enabled_ == enabled) {
    return;
  }
  dnd_enabled_ = enabled;
  emit dndEnabledChanged();
}

void NotificationService::setDaemonConflict(const QString& owner) {
  daemon_conflict_ = true;
  daemon_conflict_owner_ = owner;
}

void NotificationService::onConfigChanged() {
  if (config_ == nullptr) {
    return;
  }
  // New values apply to future notifications only (REQ-F-040); live timers/sets are untouched.
  default_timeout_ms_ = config_->notifications().default_timeout_ms;
  max_visible_ = config_->notifications().max_visible;
}

void NotificationService::onSidebarOpened(const QString& /*monitor_name*/) {
  if (unread_count_ == 0) {
    return;
  }
  for (NotificationHistoryItem& item : history_) {
    item.read = true;
  }
  unread_count_ = 0;
  emit unreadCountChanged();
  if (store_ != nullptr) {
    store_->persist(history_);
  }
}

void NotificationService::recomputeUnreadCount() {
  unread_count_ =
      static_cast<int>(std::ranges::count_if(history_, [](const NotificationHistoryItem& item) { return !item.read; }));
}

bool NotificationService::runEviction() {
  if (history_.isEmpty()) {
    return false;
  }
  const qsizetype before = history_.size();
  const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
  const qint64 max_age_ms = static_cast<qint64>(history_config_.max_age_days) * 86400LL * 1000LL;
  history_.removeIf(
      [now_ms, max_age_ms](const NotificationHistoryItem& item) { return (now_ms - item.timestamp_ms) > max_age_ms; });
  if (history_.size() > history_config_.max_items) {
    history_.remove(0, history_.size() - history_config_.max_items);
  }
  return history_.size() != before;
}
