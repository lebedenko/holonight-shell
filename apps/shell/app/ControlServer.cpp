#include "ControlServer.h"

#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>

namespace {
constexpr qsizetype kSidebarTogglePrefixLength = 15;
constexpr qsizetype kChatTogglePrefixLength = 12;
}  // namespace

ControlServer::ControlServer(QObject* parent) : QObject(parent) {}

ControlServer::~ControlServer() = default;

void ControlServer::start() {
  if (server_ != nullptr) {
    return;
  }

  const QString path = socketPath();
  QDir().mkpath(QFileInfo(path).absolutePath());
  QLocalServer::removeServer(path);

  server_ = new QLocalServer(this);
  connect(server_, &QLocalServer::newConnection, this, &ControlServer::handleConnection);
  if (!server_->listen(path)) {
    qWarning() << "ControlServer: failed to listen on control socket:" << server_->errorString();
  }
}

QString ControlServer::socketPath() {
  const QString runtime_dir = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR")).trimmed();
  const QString base_dir = runtime_dir.isEmpty() ? QDir::tempPath() : runtime_dir;
  return base_dir + QStringLiteral("/holonight-shell/control.sock");
}

ControlServer::DecodedCommand ControlServer::decodeCommand(const QByteArray& command) {
  const QByteArray trimmed = command.trimmed();
  if (trimmed == QByteArrayLiteral("toggle-launcher")) {
    return {.type = CommandType::ToggleLauncher};
  }
  if (trimmed.startsWith(QByteArrayLiteral("sidebar:toggle:"))) {
    return {.type = CommandType::ToggleSidebar, .argument = QString::fromUtf8(trimmed.mid(kSidebarTogglePrefixLength))};
  }
  if (trimmed.startsWith(QByteArrayLiteral("chat:toggle:"))) {
    return {.type = CommandType::ToggleChat, .argument = QString::fromUtf8(trimmed.mid(kChatTogglePrefixLength))};
  }
  return {};
}

void ControlServer::handleConnection() {
  while (server_ != nullptr && server_->hasPendingConnections()) {
    QLocalSocket* socket = server_->nextPendingConnection();
    command_buffers_.insert(socket, {});
    connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
      auto buffer_it = command_buffers_.find(socket);
      if (buffer_it == command_buffers_.end()) {
        return;
      }

      if (!buffer_it->append(socket->readAll())) {
        command_buffers_.erase(buffer_it);
        socket->disconnectFromServer();
      }
    });
    connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
      const QByteArray command = command_buffers_.take(socket).take();
      if (!command.isEmpty()) {
        handleCommand(command);
      }
      socket->deleteLater();
    });
  }
}

void ControlServer::handleCommand(const QByteArray& command) {
  const DecodedCommand decoded = decodeCommand(command);
  switch (decoded.type) {
    case CommandType::ToggleLauncher:
      emit toggleLauncherRequested();
      break;
    case CommandType::ToggleSidebar:
      emit toggleSidebarRequested(decoded.argument);
      break;
    case CommandType::ToggleChat:
      emit toggleChatRequested(decoded.argument);
      break;
    case CommandType::Unknown:
      break;
  }
}
