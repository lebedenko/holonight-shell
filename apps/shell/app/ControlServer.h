#pragma once

#include "ControlCommandBuffer.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

class QLocalServer;

class ControlServer : public QObject {
  Q_OBJECT

 public:
  enum class CommandType : uint8_t {
    Unknown,
    ToggleLauncher,
    ToggleSidebar,
    ToggleChat,
  };

  struct DecodedCommand {
    CommandType type{CommandType::Unknown};
    QString argument;
  };

  explicit ControlServer(QObject* parent = nullptr);
  ~ControlServer() override;

  ControlServer(const ControlServer&) = delete;
  ControlServer& operator=(const ControlServer&) = delete;
  ControlServer(ControlServer&&) = delete;
  ControlServer& operator=(ControlServer&&) = delete;

  void start();

  [[nodiscard]] static QString socketPath();
  [[nodiscard]] static DecodedCommand decodeCommand(const QByteArray& command);

 Q_SIGNALS:
  void toggleLauncherRequested();
  void toggleSidebarRequested(const QString& monitor_name);
  void toggleChatRequested(const QString& monitor_name);

 private:
  void handleConnection();
  void handleCommand(const QByteArray& command);

  QLocalServer* server_ = nullptr;
  QHash<QObject*, ControlCommandBuffer> command_buffers_;
};
