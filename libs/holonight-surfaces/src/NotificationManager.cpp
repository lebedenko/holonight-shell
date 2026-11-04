#include "NotificationManager.h"

#include "NotificationService.h"
#include "NotificationToastSurface.h"

#include <QGuiApplication>
#include <QScreen>

NotificationManager::NotificationManager(NotificationService* service, QObject* parent)
    : QObject(parent), service_(service) {
  if (service_ == nullptr) {
    return;
  }
  connect(service_, &NotificationService::notificationAdded, this,
          [this](uint /*notif_id*/, const QString& monitor_name) { ensureSurface(monitor_name); });
}

void NotificationManager::start() {
  if (started_ || service_ == nullptr) {
    return;
  }
  started_ = true;
  connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen* screen) { ensureSurface(screen->name()); });
  connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen* screen) {
    if (auto* surface = surfaces_.take(screen->name())) {
      surface->deleteLater();
    }
  });
  for (QScreen* screen : QGuiApplication::screens()) {
    ensureSurface(screen->name());
  }
}

NotificationManager::~NotificationManager() {
  qDeleteAll(surfaces_);
  surfaces_.clear();
}

void NotificationManager::ensureSurface(const QString& monitor_name) {
  NotificationToastSurface* surface = surfaces_.value(monitor_name, nullptr);
  if (surface == nullptr) {
    surface = new NotificationToastSurface(this);
    surfaces_.insert(monitor_name, surface);
  }
  surface->ensureSurface(monitor_name);
}
