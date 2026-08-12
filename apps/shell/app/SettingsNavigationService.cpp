#include "SettingsNavigationService.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QLoggingCategory>
#include <QVariantList>
#include <QVariantMap>

Q_LOGGING_CATEGORY(lcSettingsNavigation, "holonight.settings.navigation")

namespace {
constexpr auto kSettingsService = "org.holonight.Settings";
constexpr auto kSettingsPath = "/org/holonight/Settings";
constexpr auto kApplicationInterface = "org.freedesktop.Application";
constexpr auto kActivateActionMethod = "ActivateAction";
constexpr int kCallTimeoutMs = 5000;
}  // namespace

SettingsNavigationService::SettingsNavigationService(QObject* parent) : QObject(parent) {}

void SettingsNavigationService::openPage(const QString& page_key) {
  auto* watcher = new QDBusPendingCallWatcher(requestOpenPage(page_key), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher](QDBusPendingCallWatcher* completed) {
    const QDBusPendingReply<> reply = *completed;
    if (reply.isError()) {
      qCWarning(lcSettingsNavigation) << "Failed to open holonight-settings page:" << reply.error().message();
    }
    watcher->deleteLater();
  });
}

QDBusMessage SettingsNavigationService::openPageMessage(const QString& page_key) {
  QDBusMessage message =
      QDBusMessage::createMethodCall(QLatin1String(kSettingsService), QLatin1String(kSettingsPath),
                                     QLatin1String(kApplicationInterface), QLatin1String(kActivateActionMethod));
  message << page_key << QVariantList{} << QVariantMap{};
  return message;
}

QDBusPendingCall SettingsNavigationService::requestOpenPage(const QString& page_key) {
  return QDBusConnection::sessionBus().asyncCall(openPageMessage(page_key), kCallTimeoutMs);
}
