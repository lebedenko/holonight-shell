#pragma once

#include "IActivityGate.h"

#include <QAbstractListModel>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QString>

#include <cstdint>
#include <memory>

class CalendarSyncManager;
class LibsecretCredentialStorage;

// Exposes calendar data and configuration to QML. Wraps ConfigService and CalendarSyncManager
// so QML components never need to import the core config or calendar-sync layers directly.
class CalendarService : public QObject, public IActivityGate {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(QString weekStartDay READ weekStartDay NOTIFY weekStartDayChanged FINAL)
  Q_PROPERTY(QAbstractListModel* upcomingEvents READ upcomingEvents NOTIFY upcomingEventsChanged FINAL)
  Q_PROPERTY(UpcomingState upcomingState READ upcomingState NOTIFY upcomingStateChanged FINAL)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

 public:
  enum class UpcomingState : uint8_t {
    Loading = 0,
    Ready = 1,
    ConnectError = 2,
    Offline = 3,
  };
  Q_ENUM(UpcomingState)

  explicit CalendarService(QObject* parent = nullptr);
  ~CalendarService() override;

  CalendarService(const CalendarService&) = delete;
  CalendarService& operator=(const CalendarService&) = delete;
  CalendarService(CalendarService&&) = delete;
  CalendarService& operator=(CalendarService&&) = delete;

  [[nodiscard]] QString weekStartDay() const;
  [[nodiscard]] QAbstractListModel* upcomingEvents() const;
  [[nodiscard]] UpcomingState upcomingState() const { return upcoming_state_; }
  [[nodiscard]] QString lastError() const { return last_error_; }

  Q_INVOKABLE void notifySidebarOpened();

 public Q_SLOTS:
  void onIdleChanged(bool idle);

  // IActivityGate — delegates to CalendarSyncManager when it exists.
  void pauseActivity() override;
  void resumeActivity() override;

 Q_SIGNALS:
  void weekStartDayChanged();
  void upcomingEventsChanged();
  void upcomingStateChanged();
  void lastErrorChanged();

 private Q_SLOTS:
  void onCalendarConfigChanged();
  void onEventsUpdated();
  void onSyncError(int kind_int, const QString& message);

 private:
  void initSyncManager();

  QString week_start_day_{"Mon"};
  // "provider_type:account_name" composite keys, diffed on every onCalendarConfigChanged() call
  // to detect account removal (see the REQ-F-013 comment at that call site).
  QSet<QString> known_account_keys_;
  UpcomingState upcoming_state_{UpcomingState::Loading};
  QString last_error_;
  // credentials_ must outlive sync_manager_ (CalDavProviders hold a raw pointer to it).
  // Members are destroyed in reverse declaration order, so declare credentials_ first.
  std::unique_ptr<LibsecretCredentialStorage> credentials_;
  std::unique_ptr<CalendarSyncManager> sync_manager_;
};
