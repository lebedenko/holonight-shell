#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class NotificationService;
class NotificationToastSurface;

// Thin orchestrator bridging NotificationService (business logic, holonight_services) and the
// per-monitor NotificationToastSurface instances (holonight_surfaces). Owns one toast surface per
// monitor, mapped at startup and retained while empty to avoid compositor refocus on toast arrival.
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

  void start();

 private:
  void ensureSurface(const QString& monitor_name);

  NotificationService* service_;
  bool started_ = false;
  QHash<QString, NotificationToastSurface*> surfaces_;  // monitor name → toast surface
};
