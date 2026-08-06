#pragma once

#include "ConfigService.h"
#include "NotificationTypes.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariant>

#include <cstdint>

class ActiveWindowService;
class NotificationRuleModel;
class NotificationStore;
class QTimer;

// QML-facing notification model and business logic. Owns the full notification
// lifecycle: ID allocation, replace semantics, per-monitor visible/queue placement,
// timeout/hover timers, and action/dismissal routing. The pure D-Bus protocol layer
// lives in NotificationServer, which drives this class.
//
// The base QAbstractListModel exposes every live notification (visible + queued) in a
// flat row order; QML consumes per-monitor, visible-only slices via
// visibleModelForMonitor(), which returns a QSortFilterProxyModel.
class NotificationService : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged FINAL)
  Q_PROPERTY(QString unreadAppNames READ unreadAppNames NOTIFY unreadCountChanged FINAL)
  Q_PROPERTY(bool dndEnabled READ dndEnabled WRITE setDndEnabled NOTIFY dndEnabledChanged FINAL)
  Q_PROPERTY(bool daemonConflict READ daemonConflict CONSTANT FINAL)
  Q_PROPERTY(QString daemonConflictOwner READ daemonConflictOwner CONSTANT FINAL)

 public:
  enum Roles : std::uint16_t {  // NOLINT(cppcoreguidelines-use-enum-class): Qt model roles are int-compatible.
    NotifIdRole = Qt::UserRole + 1,
    SummaryRole,
    BodyRole,
    AppIconRole,
    ActionsRole,
    HasDefaultActionRole,
    AccentKindRole,
    UrgencyRole,
    IsResidentRole,
    MonitorRole,
    LifecycleRole,  // int(NotifLifecycle); used by the per-monitor visible filter proxy
    CreatedAtMsRole,
  };
  Q_ENUM(Roles)

  explicit NotificationService(ConfigService* config, ActiveWindowService* active_window,
                               NotificationRuleModel* rule_model = nullptr, QObject* parent = nullptr);
  ~NotificationService() override = default;

  NotificationService(const NotificationService&) = delete;
  NotificationService& operator=(const NotificationService&) = delete;
  NotificationService(NotificationService&&) = delete;
  NotificationService& operator=(NotificationService&&) = delete;

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  // Inserts a new notification or replaces an existing one. When data.id is non-zero and
  // matches a live notification, that notification is updated in place and its id returned;
  // otherwise a fresh monotonic id (>0) is allocated and returned. Drives the D-Bus Notify reply.
  uint32_t addOrReplace(NotificationData data);

  // org.freedesktop.Notifications.CloseNotification entry point: closes the notification with
  // reason Closed. Unknown ids are a no-op (no signal), matching the spec. Driven by NotificationServer.
  void requestClose(uint32_t notif_id);

  // True while the monitor still has any visible or queued notification. NotificationManager
  // uses this on queueChanged to decide whether the monitor's toast surface can be torn down.
  [[nodiscard]] bool hasNotificationsForMonitor(const QString& monitor_name) const;

  // Per-monitor, visible-only slice of the model for ToastStack.qml. Returns a cached
  // QSortFilterProxyModel (one per monitor) filtering MonitorRole == monitor_name and
  // lifecycle == Visible. Ownership stays with this service.
  Q_INVOKABLE QAbstractItemModel* visibleModelForMonitor(const QString& monitor_name);

  // Emits actionInvoked(notif_id, action_key) for the D-Bus relay, then closes the
  // notification with reason Closed — unless it is resident, which suppresses the close (REQ-F-026).
  Q_INVOKABLE void invokeAction(uint notif_id, const QString& action_key);
  // User-initiated close: removes the notification with reason Dismissed (REQ-F-024).
  Q_INVOKABLE void dismiss(uint notif_id);

  // Pauses the auto-dismiss timer while the pointer is over a toast, resuming with the
  // remaining time on leave (REQ-F-017). No-op for notifications that never expire.
  Q_INVOKABLE void hoverEntered(uint notif_id);
  Q_INVOKABLE void hoverLeft(uint notif_id);

  // Returns up to maxGroups notification history groups, each a QVariantMap with keys:
  // appName, appIcon, unreadCount, latestSummary, latestTimestampMs, totalCount.
  // Groups are ordered by latestTimestampMs descending (most recent first).
  [[nodiscard]] Q_INVOKABLE QVariantList recentHistoryGrouped(int maxGroups) const;

  [[nodiscard]] int unreadCount() const { return unread_count_; }
  [[nodiscard]] QString unreadAppNames() const;

  [[nodiscard]] bool dndEnabled() const { return dnd_enabled_; }
  void setDndEnabled(bool enabled);

  [[nodiscard]] bool daemonConflict() const { return daemon_conflict_; }
  [[nodiscard]] QString daemonConflictOwner() const { return daemon_conflict_owner_; }
  void setDaemonConflict(const QString& owner);

 Q_SIGNALS:
  void dndEnabledChanged();
  void notificationAdded(uint notif_id, const QString& monitor_name);
  void notificationUpdated(uint notif_id);
  void notificationClosed(uint notif_id, uint reason);
  void actionInvoked(uint notif_id, const QString& action_key);
  void queueChanged(const QString& monitor_name);
  void unreadCountChanged();

 public Q_SLOTS:
  void onSidebarOpened(const QString& monitor_name);

 private Q_SLOTS:
  void onConfigChanged();

 private:
  // Decides visible-vs-queue placement for a freshly inserted notification: appends to the
  // visible set when under max_visible; on overflow enqueues (FIFO), except a Critical
  // notification bumps the oldest non-critical visible one to the queue FRONT and takes its slot.
  void place(uint32_t notif_id, const QString& monitor_name);
  // Promotes the front of a monitor's queue into its visible set when a slot frees up.
  void promoteFromQueue(const QString& monitor_name);
  // Removes a notification entirely: stops its timer, drops it from the visible/queue sets and
  // the model, emits notificationClosed(reason), then promotes the next queued one.
  void closeNotification(uint32_t notif_id, NotifCloseReason reason);
  // Starts a fresh single-shot auto-dismiss timer (effective_timeout_ms); no-op when <= 0.
  void armTimeout(uint32_t notif_id);
  // Stops and disposes a notification's timer, if any.
  void stopTimeout(uint32_t notif_id);
  [[nodiscard]] uint32_t allocateNotificationId();
  void emitRowChangedForId(uint32_t notif_id);
  [[nodiscard]] int rowForId(uint32_t notif_id) const;
  [[nodiscard]] QString resolveMonitorName() const;

  bool runEviction();
  void recomputeUnreadCount();
  void ensureHistoryStore();

  ConfigService* config_;
  ActiveWindowService* active_window_;
  NotificationRuleModel* rule_model_;

  bool dnd_enabled_{false};
  bool daemon_conflict_{false};
  QString daemon_conflict_owner_;

  QHash<uint32_t, NotificationData> all_notifications_;  // id → data
  QHash<QString, QList<uint32_t>> visible_by_monitor_;   // monitor → ordered visible ids
  QHash<QString, QList<uint32_t>> queue_by_monitor_;     // monitor → FIFO queued ids
  QList<uint32_t> row_order_;                            // flat model order (id per row)
  QHash<QString, QAbstractItemModel*> visible_models_;   // monitor → cached visible proxy
  QHash<uint32_t, QTimer*> timers_;                      // id → active auto-dismiss timer

  QList<NotificationHistoryItem> history_;
  int unread_count_{0};
  NotificationStore* store_{nullptr};
  NotificationHistoryConfig history_config_;

  uint32_t next_id_{1};
  int default_timeout_ms_{5000};
  int max_visible_{3};
};
