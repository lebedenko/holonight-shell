#include "AiChatService.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAiChat, "holonight.aichat")

namespace {
constexpr auto kChatService = "org.holonight.Chat";
constexpr auto kChatPath = "/org/holonight/Chat";
constexpr auto kChatInterface = "org.holonight.Chat1";
constexpr auto kTogglePanelMethod = "TogglePanel";
constexpr int kCallTimeoutMs = 5000;
}  // namespace

AiChatService::AiChatService(QObject* parent) : QObject(parent) {}

void AiChatService::togglePanel(const QString& monitor_name) {
  auto* watcher = new QDBusPendingCallWatcher(requestTogglePanel(monitor_name), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher](QDBusPendingCallWatcher* completed) {
    const QDBusPendingReply<> reply = *completed;
    if (reply.isError()) {
      qCWarning(lcAiChat) << "Failed to toggle holonight-chat panel:" << reply.error().message();
    }
    watcher->deleteLater();
  });
}

QDBusMessage AiChatService::togglePanelMessage(const QString& monitor_name) {
  QDBusMessage message =
      QDBusMessage::createMethodCall(QLatin1String(kChatService), QLatin1String(kChatPath),
                                     QLatin1String(kChatInterface), QLatin1String(kTogglePanelMethod));
  message << monitor_name;
  return message;
}

QDBusPendingCall AiChatService::requestTogglePanel(const QString& monitor_name) {
  return QDBusConnection::sessionBus().asyncCall(togglePanelMessage(monitor_name), kCallTimeoutMs);
}
