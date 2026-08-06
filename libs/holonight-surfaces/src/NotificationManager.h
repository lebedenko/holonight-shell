#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class NotificationService;
class NotificationToastSurface;

// Thin orchestrator bridging NotificationService (business logic, holonight_services) and the
// per-monitor NotificationToastSurface instances (holonight_surfaces). Owns one toast surface per
// monitor, created lazily on the first notification for that monitor and destroyed once the
// monitor's visible set and queue are both empty.
//
// Instantiated in ShellApplication alongside the other surface managers.
class NotificationManager : public QObject {
  Q_OBJECT

 public:
  explicit NotificationManager(NotificationService* service, QObject* parent = nullptr);
  ~NotificationManager() override;

  NotificationManager(const NotificationManager&) = delete;
  NotificationManager& operator=(const NotificationManager&) = delete;
  NotificationManager(NotificationManager&&) = delete;
  NotificationManager& operator=(NotificationManager&&) = delete;

 private:
  void onNotificationAdded(uint notif_id, const QString& monitor_name);
  void onQueueChanged(const QString& monitor_name);

  NotificationService* service_;
  QHash<QString, NotificationToastSurface*> surfaces_;  // monitor name → toast surface
};
