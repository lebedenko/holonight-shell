#include "NotificationManager.h"

#include "NotificationService.h"
#include "NotificationToastSurface.h"

#include <QtGlobal>

NotificationManager::NotificationManager(NotificationService* service, QObject* parent)
    : QObject(parent), service_(service) {
  if (service_ == nullptr) {
    return;
  }
  connect(service_, &NotificationService::notificationAdded, this, &NotificationManager::onNotificationAdded);
  connect(service_, &NotificationService::queueChanged, this, &NotificationManager::onQueueChanged);
}

NotificationManager::~NotificationManager() {
  qDeleteAll(surfaces_);
  surfaces_.clear();
}

void NotificationManager::onNotificationAdded(uint notif_id, const QString& monitor_name) {
  Q_UNUSED(notif_id)

  NotificationToastSurface* surface = surfaces_.value(monitor_name, nullptr);
  if (surface == nullptr) {
    surface = new NotificationToastSurface(this);
    surfaces_.insert(monitor_name, surface);
  }
  surface->ensureSurface(monitor_name);
}

void NotificationManager::onQueueChanged(const QString& monitor_name) {
  if (service_->hasNotificationsForMonitor(monitor_name)) {
    return;
  }
  // Both the visible set and queue for this monitor are empty — tear the surface down.
  NotificationToastSurface* surface = surfaces_.take(monitor_name);
  if (surface != nullptr) {
    surface->deleteLater();
  }
}
