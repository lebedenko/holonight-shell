#pragma once

#include <QObject>
#include <QString>

class QDBusPendingCall;
class QDBusMessage;

// Thin client for the standalone holonight-chat process. The chat application owns
// all panel state and presentation; the shell only requests lifecycle changes.
class AiChatService : public QObject {
 public:
  explicit AiChatService(QObject* parent = nullptr);
  ~AiChatService() override = default;

  AiChatService(const AiChatService&) = delete;
  AiChatService& operator=(const AiChatService&) = delete;
  AiChatService(AiChatService&&) = delete;
  AiChatService& operator=(AiChatService&&) = delete;

  void togglePanel(const QString& monitor_name);
  [[nodiscard]] static QDBusMessage togglePanelMessage(const QString& monitor_name);

 protected:
  virtual QDBusPendingCall requestTogglePanel(const QString& monitor_name);
};
