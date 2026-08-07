#include "CalendarService.h"

#include "BatteryService.h"
#include "ConfigService.h"
#include "calendar/CalDavProvider.h"
#include "calendar/CalendarSyncManager.h"
#include "calendar/CalendarTypes.h"
#include "calendar/ICalendarProvider.h"
#include "calendar/IcsProvider.h"
#include "calendar/LibsecretCredentialStorage.h"

#include <QLoggingCategory>
#include <QQmlEngine>
#include <QStandardPaths>

#include <memory>

Q_LOGGING_CATEGORY(lcCalendarService, "holonight.calendar.service")

CalendarService::~CalendarService() = default;

CalendarService::CalendarService(QObject* parent) : QObject(parent) {
  if (auto* config = ConfigService::instance()) {
    onCalendarConfigChanged();
    connect(config, &ConfigService::calendarConfigChanged, this, &CalendarService::onCalendarConfigChanged);
  }
}

QString CalendarService::weekStartDay() const { return week_start_day_; }

QAbstractListModel* CalendarService::upcomingEvents() const {
  if (sync_manager_ == nullptr) {
    return nullptr;
  }
  return sync_manager_->eventModel();
}

void CalendarService::notifySidebarOpened() {
  if (sync_manager_ != nullptr) {
    sync_manager_->notifySidebarOpened();
  }
}

void CalendarService::onIdleChanged(bool idle) {
  if (sync_manager_ != nullptr) {
    sync_manager_->setIdlePaused(idle);
  }
}

void CalendarService::pauseActivity() {
  if (sync_manager_ != nullptr) {
    sync_manager_->pauseActivity();
  }
}

void CalendarService::resumeActivity() {
  if (sync_manager_ != nullptr) {
    sync_manager_->resumeActivity();
  }
}

void CalendarService::initSyncManager() {
  const auto* config = ConfigService::instance();
  if (config == nullptr) {
    return;
  }

  const auto& cal_cfg = config->calendarConfig();
  if (cal_cfg.caldav_accounts.isEmpty() && cal_cfg.ics_accounts.isEmpty()) {
    return;
  }

  const LibsecretCredentialStorage* cred_ptr = nullptr;
  if (!cal_cfg.caldav_accounts.isEmpty()) {
    if (!credentials_) {
      credentials_ = std::make_unique<LibsecretCredentialStorage>();
    }
    cred_ptr = credentials_.get();
  }

  std::vector<std::unique_ptr<ICalendarProvider>> caldav_providers;
  for (const auto& account_cfg : cal_cfg.caldav_accounts) {
    if (account_cfg.url.isEmpty() || account_cfg.username.isEmpty()) {
      qCWarning(lcCalendarService) << "Skipping CalDAV account with missing url/username:" << account_cfg.account_name;
      continue;
    }
    caldav_providers.push_back(std::make_unique<CalDavProvider>(account_cfg, cred_ptr));
  }

  std::vector<std::unique_ptr<ICalendarProvider>> ics_providers;
  for (const auto& account_cfg : cal_cfg.ics_accounts) {
    if (account_cfg.url.isEmpty()) {
      qCWarning(lcCalendarService) << "Skipping ICS account with missing url:" << account_cfg.account_name;
      continue;
    }
    ics_providers.push_back(std::make_unique<IcsProvider>(account_cfg));
  }

  if (caldav_providers.empty() && ics_providers.empty()) {
    return;
  }

  auto* engine = qmlEngine(this);
  auto* battery = (engine != nullptr) ? qobject_cast<BatteryService*>(engine->singletonInstance<BatteryService*>(
                                            QStringLiteral("HolonightShell"), QStringLiteral("BatteryService")))
                                      : nullptr;

  sync_manager_ =
      std::make_unique<CalendarSyncManager>(std::move(caldav_providers), std::move(ics_providers), battery, this);
  connect(sync_manager_.get(), &CalendarSyncManager::eventsUpdated, this, &CalendarService::onEventsUpdated);
  connect(sync_manager_.get(), &CalendarSyncManager::syncError, this,
          [this](SyncError::Kind kind, const QString& msg) { onSyncError(static_cast<int>(kind), msg); });

  emit upcomingEventsChanged();
}

void CalendarService::onCalendarConfigChanged() {
  const auto* config = ConfigService::instance();
  if (config == nullptr) {
    return;
  }

  const QString new_day = (config->calendarConfig().week_start_day == HoloNight::ShellConfig::WeekStartDay::Sunday)
                              ? QStringLiteral("Sun")
                              : QStringLiteral("Mon");
  if (new_day != week_start_day_) {
    week_start_day_ = new_day;
    emit weekStartDayChanged();
  }

  const auto& cal_cfg = config->calendarConfig();
  QSet<QString> current_keys;
  for (const auto& acct : cal_cfg.caldav_accounts) {
    current_keys.insert(QStringLiteral("caldav:") + acct.account_name);
  }
  for (const auto& acct : cal_cfg.ics_accounts) {
    current_keys.insert(QStringLiteral("ics:") + acct.account_name);
  }

  // POC Remediation Phase 4 REQ-F-013 (SPEC.md): this method previously only acted on the FIRST
  // call where sync_manager_ was still null (see the initSyncManager() guard below); every later
  // config change was silently a no-op beyond the week-start-day check. This was a genuine latent
  // bug, not intentional caching — REQ-F-012's account-removal wiring cannot function without
  // detecting removals on every call, so the diff below now runs unconditionally, independent of
  // sync_manager_'s state. Scope note: only REMOVAL is detected here; a newly added second account
  // after the first sync is still not hot-picked-up (initSyncManager()'s guard is unchanged) — see
  // Phase 4 DESIGN.md Item 4 for why that narrower gap is deliberately out of scope.
  QSet<QString> removed_keys = known_account_keys_;
  removed_keys.subtract(current_keys);
  for (const QString& removed_key : removed_keys) {
    const qsizetype sep = removed_key.indexOf(QLatin1Char(':'));
    const QString provider_type = removed_key.left(static_cast<int>(sep));
    const QString account_name = removed_key.mid(static_cast<int>(sep) + 1);
    if (sync_manager_ != nullptr) {
      sync_manager_->removeAccount(provider_type, account_name);
    }
  }
  known_account_keys_ = current_keys;

  const bool has_accounts = !current_keys.isEmpty();
  if (sync_manager_ == nullptr && has_accounts) {
    initSyncManager();
  }
}

void CalendarService::onEventsUpdated() {
  if (upcoming_state_ != UpcomingState::Ready) {
    upcoming_state_ = UpcomingState::Ready;
    emit upcomingStateChanged();
  }
}

void CalendarService::onSyncError(int kind_int, const QString& message) {
  using Kind = SyncError::Kind;
  const auto kind = static_cast<Kind>(kind_int);

  UpcomingState new_state = upcoming_state_;
  if (kind == Kind::ConnectError || kind == Kind::ParseError || kind == Kind::StorageError) {
    new_state = UpcomingState::ConnectError;
  } else if (kind == Kind::NetworkError) {
    new_state = UpcomingState::Offline;
  }

  if (upcoming_state_ != new_state) {
    upcoming_state_ = new_state;
    emit upcomingStateChanged();
  }

  if (last_error_ != message) {
    last_error_ = message;
    emit lastErrorChanged();
  }
}
