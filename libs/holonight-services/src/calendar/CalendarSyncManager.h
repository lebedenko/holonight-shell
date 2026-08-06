#pragma once

#include "CalendarCache.h"
#include "CalendarEventModel.h"
#include "CalendarTypes.h"
#include "IActivityGate.h"

#include <QDateTime>
#include <QHash>
#include <QHashFunctions>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <expected>
#include <memory>
#include <utility>
#include <vector>

// Required to use std::pair<QString,QString> as QSet key.
inline size_t qHash(const std::pair<QString, QString>& pair, size_t seed = 0) noexcept {
  return qHashMulti(seed, pair.first, pair.second);
}

class BatteryService;
class ICalendarProvider;
template <typename T>
class QFutureWatcher;

// Drives periodic CalDAV (15 min) and ICS (60 min) sync, updates CalendarEventModel,
// dispatches desktop notifications, and persists results in CalendarCache.
class CalendarSyncManager : public QObject, public IActivityGate {
  Q_OBJECT

 public:
  explicit CalendarSyncManager(std::vector<std::unique_ptr<ICalendarProvider>> caldav_providers,
                               std::vector<std::unique_ptr<ICalendarProvider>> ics_providers,
                               BatteryService* battery_service, QObject* parent = nullptr);
  ~CalendarSyncManager() override;

  CalendarSyncManager(const CalendarSyncManager&) = delete;
  CalendarSyncManager& operator=(const CalendarSyncManager&) = delete;
  CalendarSyncManager(CalendarSyncManager&&) = delete;
  CalendarSyncManager& operator=(CalendarSyncManager&&) = delete;

  [[nodiscard]] CalendarEventModel* eventModel() { return &model_; }

  // Refreshes the model from cache; triggers sync for any account past its next_sync_time.
  void notifySidebarOpened();

  // Pause all sync timers on idle; restart them (plus an immediate sync) on resume.
  void setIdlePaused(bool paused);

  // Cleans up all cached data for one account (cache events, in-progress/backoff tracking, model
  // refresh) without touching the live provider list or in-flight sync state for other accounts.
  // See CalendarService::onCalendarConfigChanged() for the only production caller.
  void removeAccount(const QString& provider_type, const QString& account_name);

  // IActivityGate — pauses timers on lid close, resumes (with immediate sync) on lid open.
  void pauseActivity() override;
  void resumeActivity() override;

 Q_SIGNALS:
  void eventsUpdated();
  void syncError(SyncError::Kind kind, const QString& message);

 private Q_SLOTS:
  void onCalDavTimer();
  void onIcsTimer();
  void onNotificationTimer();
  void onBatteryChanged();

 private:
  struct BackoffState {
    int backoff_ms{0};
    QDateTime backoff_until;
  };

  using SyncKey = std::pair<QString, QString>;

  [[nodiscard]] static SyncKey syncKey(const QString& provider_type, const QString& account_name) {
    return {provider_type, account_name};
  }

  void runTestConnections();
  void runProviderSync(ICalendarProvider& provider, const QString& provider_type);
  void runCalDavSync(ICalendarProvider& provider);
  void runIcsSync(ICalendarProvider& provider);
  void onSyncFinished(const QString& provider_type, const QString& account,
                      std::expected<QList<CalendarEvent>, SyncError> result);
  void refreshModel();
  void dispatchNotifications();
  static void sendNotification(const QString& summary, const QString& body, int expire_timeout_ms);
  [[nodiscard]] bool isBatteryLow() const;

  std::vector<std::unique_ptr<ICalendarProvider>> caldav_providers_;
  std::vector<std::unique_ptr<ICalendarProvider>> ics_providers_;
  BatteryService* battery_service_{nullptr};
  CalendarCache cache_;
  CalendarEventModel model_;

  QTimer caldav_timer_;
  QTimer ics_timer_;
  QTimer notification_timer_;

  QHash<SyncKey, bool> in_progress_;
  QHash<SyncKey, BackoffState> backoff_;
  QList<QFutureWatcher<void>*> sync_watchers_;

  // Dedup keys: (uid + ":" + account_name, "15min" or "now")
  QSet<std::pair<QString, QString>> fired_notifications_;

  bool timers_paused_{false};
  bool idle_paused_{false};
  bool lid_paused_{false};

  static constexpr int kCalDavIntervalMs{15 * 60 * 1000};
  static constexpr int kIcsIntervalMs{60 * 60 * 1000};
  static constexpr int kNotificationIntervalMs{60 * 1000};
  static constexpr int kBackoffInitialMs{5 * 60 * 1000};
  static constexpr int kBackoffCalDavMaxMs{kCalDavIntervalMs};
  static constexpr int kBackoffIcsMaxMs{kIcsIntervalMs};
  static constexpr int kLowBatteryThreshold{10};
  static constexpr int kBatteryResumeThreshold{20};
  static constexpr int kNotifyPreEventMinutes{15};
  static constexpr int kNotifyWindowSecs{kNotifyPreEventMinutes * 60};
  static constexpr int kModelEventLimit{3};
};
