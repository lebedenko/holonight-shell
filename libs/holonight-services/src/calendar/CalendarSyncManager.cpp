#include "CalendarSyncManager.h"

#include "../BatteryService.h"
#include "ICalendarProvider.h"

#include <QDBusInterface>
#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <memory>

Q_LOGGING_CATEGORY(lcCalendarSync, "holonight.calendar.sync")

namespace {

QString calendarDbPath() {
  const QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  return cache_dir + QStringLiteral("/calendar/calendar.sqlite");
}

}  // namespace

CalendarSyncManager::CalendarSyncManager(std::vector<std::unique_ptr<ICalendarProvider>> caldav_providers,
                                         std::vector<std::unique_ptr<ICalendarProvider>> ics_providers,
                                         BatteryService* battery_service, QObject* parent)
    : QObject(parent),
      caldav_providers_(std::move(caldav_providers)),
      ics_providers_(std::move(ics_providers)),
      battery_service_(battery_service) {
  const QString db_path = calendarDbPath();
  if (!cache_.open(db_path)) {
    qCWarning(lcCalendarSync) << "Failed to open calendar cache at" << db_path;
  }

  connect(&caldav_timer_, &QTimer::timeout, this, &CalendarSyncManager::onCalDavTimer);
  connect(&ics_timer_, &QTimer::timeout, this, &CalendarSyncManager::onIcsTimer);
  connect(&notification_timer_, &QTimer::timeout, this, &CalendarSyncManager::onNotificationTimer);

  caldav_timer_.setInterval(kCalDavIntervalMs);
  ics_timer_.setInterval(kIcsIntervalMs);
  notification_timer_.setInterval(kNotificationIntervalMs);

  if (battery_service_ != nullptr) {
    connect(battery_service_, &BatteryService::percentChanged, this, &CalendarSyncManager::onBatteryChanged);
    connect(battery_service_, &BatteryService::chargingChanged, this, &CalendarSyncManager::onBatteryChanged);
  }

  QTimer::singleShot(0, this, &CalendarSyncManager::runTestConnections);
}

CalendarSyncManager::~CalendarSyncManager() {
  caldav_timer_.stop();
  ics_timer_.stop();
  notification_timer_.stop();
  // Wait for any in-flight sync workers so they finish before providers are destroyed.
  const auto watchers = findChildren<QFutureWatcherBase*>();
  for (auto* watcher : watchers) {
    watcher->waitForFinished();
  }
}

void CalendarSyncManager::notifySidebarOpened() {
  refreshModel();

  const QDateTime now = QDateTime::currentDateTimeUtc();
  for (const auto& provider : caldav_providers_) {
    const auto state = cache_.loadSyncState(QStringLiteral("caldav"), provider->accountName());
    if (!state.next_sync_time.isValid() || state.next_sync_time <= now) {
      runCalDavSync(*provider);
    }
  }
  for (const auto& provider : ics_providers_) {
    const auto state = cache_.loadSyncState(QStringLiteral("ics"), provider->accountName());
    if (!state.next_sync_time.isValid() || state.next_sync_time <= now) {
      runIcsSync(*provider);
    }
  }
}

void CalendarSyncManager::runTestConnections() {
  auto shared_errors = std::make_shared<QList<SyncError>>();
  auto* watcher = new QFutureWatcher<void>(this);
  sync_watchers_.append(watcher);
  connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, shared_errors]() {
    sync_watchers_.removeOne(watcher);
    watcher->deleteLater();
    for (const SyncError& err : *shared_errors) {
      qCWarning(lcCalendarSync) << "testConnection failed:" << err.message;
      emit syncError(err.kind, err.message);
    }
    if (!timers_paused_) {
      caldav_timer_.start();
      ics_timer_.start();
      notification_timer_.start();
    }
    for (const auto& provider : caldav_providers_) {
      runCalDavSync(*provider);
    }
    for (const auto& provider : ics_providers_) {
      runIcsSync(*provider);
    }
  });

  watcher->setFuture(QtConcurrent::run([this, shared_errors]() {
    for (const auto& provider : caldav_providers_) {
      const auto result = provider->testConnection();
      if (!result) {
        *shared_errors << result.error();
      }
    }
    for (const auto& provider : ics_providers_) {
      const auto result = provider->testConnection();
      if (!result) {
        *shared_errors << result.error();
      }
    }
  }));
}

void CalendarSyncManager::runCalDavSync(ICalendarProvider& provider) {
  runProviderSync(provider, QStringLiteral("caldav"));
}

void CalendarSyncManager::runIcsSync(ICalendarProvider& provider) { runProviderSync(provider, QStringLiteral("ics")); }

void CalendarSyncManager::runProviderSync(ICalendarProvider& provider, const QString& provider_type) {
  if (isBatteryLow()) {
    return;
  }

  const QString account = provider.accountName();
  const SyncKey key = syncKey(provider_type, account);
  if (in_progress_.value(key, false)) {
    return;
  }

  const auto& backoff_state = backoff_.value(key);
  if (backoff_state.backoff_ms > 0 && QDateTime::currentDateTimeUtc() < backoff_state.backoff_until) {
    return;
  }

  in_progress_.insert(key, true);

  const QDateTime now = QDateTime::currentDateTimeUtc();
  const DateRange range{.start_utc = now.addDays(-CalendarCache::kRetainPastDays),
                        .end_utc = now.addDays(CalendarCache::kRetainFutureDays)};

  auto shared_result = std::make_shared<std::expected<QList<CalendarEvent>, SyncError>>();
  auto* watcher = new QFutureWatcher<void>(this);
  sync_watchers_.append(watcher);
  auto* ptr = &provider;

  connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, account, provider_type, shared_result]() {
    sync_watchers_.removeOne(watcher);
    watcher->deleteLater();
    onSyncFinished(provider_type, account, std::move(*shared_result));
  });
  watcher->setFuture(QtConcurrent::run([ptr, range, shared_result]() { *shared_result = ptr->fetchEvents(range); }));
}

void CalendarSyncManager::onSyncFinished(const QString& provider_type, const QString& account,
                                         std::expected<QList<CalendarEvent>, SyncError> result) {
  const SyncKey key = syncKey(provider_type, account);
  in_progress_.insert(key, false);

  const QDateTime now = QDateTime::currentDateTimeUtc();

  const auto handle_error = [this, &provider_type, &account, &key, &now](const SyncError& err) {
    qCWarning(lcCalendarSync) << provider_type << "sync error for" << account << ":" << err.message;

    auto& state = backoff_[key];
    if (state.backoff_ms == 0) {
      state.backoff_ms = kBackoffInitialMs;
    } else {
      const int max_ms = (provider_type == QLatin1String("caldav")) ? kBackoffCalDavMaxMs : kBackoffIcsMaxMs;
      state.backoff_ms = std::min(state.backoff_ms * 2, max_ms);
    }
    state.backoff_until = now.addMSecs(state.backoff_ms);

    cache_.storeSyncState(provider_type, account, now, err.message, state.backoff_until);
    emit syncError(err.kind, err.message);
  };

  if (!result) {
    handle_error(result.error());
    return;
  }

  if (!cache_.upsertEvents(*result) || !cache_.reconcileAccountEvents(provider_type, account, *result)) {
    handle_error(SyncError{.kind = SyncError::Kind::StorageError,
                           .message = QStringLiteral("Failed to persist calendar sync results")});
    return;
  }

  backoff_.remove(key);
  cache_.pruneExpired();

  const int interval_ms = (provider_type == QLatin1String("caldav")) ? kCalDavIntervalMs : kIcsIntervalMs;
  cache_.storeSyncState(provider_type, account, now, {}, now.addMSecs(interval_ms));

  refreshModel();
  emit eventsUpdated();
}

void CalendarSyncManager::removeAccount(const QString& provider_type, const QString& account_name) {
  cache_.clearAccountEvents(provider_type, account_name);
  const SyncKey key = syncKey(provider_type, account_name);
  in_progress_.remove(key);
  backoff_.remove(key);
  refreshModel();
}

void CalendarSyncManager::refreshModel() {
  const QDateTime now = QDateTime::currentDateTimeUtc();
  QList<CalendarEvent> events = cache_.queryRange(now, now.addDays(7));
  if (events.size() > kModelEventLimit) {
    events = events.first(kModelEventLimit);
  }
  model_.setEvents(events);
}

void CalendarSyncManager::onCalDavTimer() {
  for (const auto& provider : caldav_providers_) {
    runCalDavSync(*provider);
  }
}

void CalendarSyncManager::onIcsTimer() {
  for (const auto& provider : ics_providers_) {
    runIcsSync(*provider);
  }
}

void CalendarSyncManager::onNotificationTimer() { dispatchNotifications(); }

void CalendarSyncManager::dispatchNotifications() {
  const QDateTime now = QDateTime::currentDateTimeUtc();
  const QList<CalendarEvent> events = cache_.queryRange(now.addSecs(-60), now.addSecs(kNotifyWindowSecs));

  for (const auto& evt : events) {
    if (evt.is_all_day) {
      continue;
    }

    const qint64 secs_to_start = now.secsTo(evt.start_time);
    const QString dedup_key = evt.uid + QLatin1Char(':') + evt.account_name;

    if (secs_to_start > 0 && secs_to_start <= kNotifyWindowSecs) {
      const auto key = std::make_pair(dedup_key, QStringLiteral("15min"));
      if (!fired_notifications_.contains(key)) {
        fired_notifications_.insert(key);
        sendNotification(QStringLiteral("Calendar reminder"),
                         QStringLiteral("%1 starts in %2 min").arg(evt.title).arg(secs_to_start / 60), 5000);
      }
    }

    if (secs_to_start >= -60 && secs_to_start <= 0) {
      const auto key = std::make_pair(dedup_key, QStringLiteral("now"));
      if (!fired_notifications_.contains(key)) {
        fired_notifications_.insert(key);
        sendNotification(QStringLiteral("Calendar reminder"), QStringLiteral("%1 starts in 0 min").arg(evt.title), 0);
      }
    }
  }
}

void CalendarSyncManager::sendNotification(const QString& summary, const QString& body, int expire_timeout_ms) {
  QDBusInterface notif(QStringLiteral("org.freedesktop.Notifications"),
                       QStringLiteral("/org/freedesktop/Notifications"),
                       QStringLiteral("org.freedesktop.Notifications"));
  if (!notif.isValid()) {
    qCWarning(lcCalendarSync) << "org.freedesktop.Notifications not available";
    return;
  }
  notif.call(QStringLiteral("Notify"), QStringLiteral("Calendar reminder"), static_cast<uint>(0),
             QStringLiteral("calendar"), summary, body, QStringList{}, QVariantMap{}, expire_timeout_ms);
}

void CalendarSyncManager::onBatteryChanged() {
  if (battery_service_ == nullptr) {
    return;
  }

  const bool charging = battery_service_->charging();
  const int pct = battery_service_->percent();

  const bool should_pause = !charging && pct < kLowBatteryThreshold;
  const bool should_resume = charging || pct >= kBatteryResumeThreshold;

  if (!timers_paused_ && should_pause) {
    timers_paused_ = true;
    caldav_timer_.stop();
    ics_timer_.stop();
    notification_timer_.stop();
    qCInfo(lcCalendarSync) << "Calendar sync paused: battery" << pct << "% discharging";
  } else if (timers_paused_ && should_resume) {
    timers_paused_ = false;
    if (!idle_paused_ && !lid_paused_) {
      caldav_timer_.start();
      ics_timer_.start();
      notification_timer_.start();
    }
    qCInfo(lcCalendarSync) << "Calendar sync resumed: battery" << pct << "%";
  }
}

void CalendarSyncManager::setIdlePaused(bool paused) {
  if (paused == idle_paused_) {
    return;
  }
  idle_paused_ = paused;
  if (paused) {
    caldav_timer_.stop();
    ics_timer_.stop();
    notification_timer_.stop();
    qCInfo(lcCalendarSync) << "Calendar sync paused: idle";
  } else if (!timers_paused_ && !lid_paused_) {
    onCalDavTimer();
    onIcsTimer();
    caldav_timer_.start();
    ics_timer_.start();
    notification_timer_.start();
    qCInfo(lcCalendarSync) << "Calendar sync resumed: idle";
  }
}

void CalendarSyncManager::pauseActivity() {
  if (lid_paused_) {
    return;
  }
  lid_paused_ = true;
  caldav_timer_.stop();
  ics_timer_.stop();
  notification_timer_.stop();
  qCInfo(lcCalendarSync) << "Calendar sync paused: lid";
}

void CalendarSyncManager::resumeActivity() {
  if (!lid_paused_) {
    return;
  }
  lid_paused_ = false;
  if (!timers_paused_ && !idle_paused_) {
    onCalDavTimer();
    onIcsTimer();
    caldav_timer_.start();
    ics_timer_.start();
    notification_timer_.start();
    qCInfo(lcCalendarSync) << "Calendar sync resumed: lid";
  }
}

bool CalendarSyncManager::isBatteryLow() const {
  if (battery_service_ == nullptr) {
    return false;
  }
  return !battery_service_->charging() && battery_service_->percent() < kLowBatteryThreshold;
}
