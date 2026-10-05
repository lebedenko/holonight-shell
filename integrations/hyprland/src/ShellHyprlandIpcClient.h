#pragma once

#include <QByteArray>
#include <QLocalSocket>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>

class ShellHyprlandIpcTransport : public QObject {
  Q_OBJECT

 public:
  using CommandCompletePredicate = std::function<bool(const QByteArray&)>;

  explicit ShellHyprlandIpcTransport(QObject* parent = nullptr) : QObject(parent) {}
  ~ShellHyprlandIpcTransport() override = default;

  ShellHyprlandIpcTransport(const ShellHyprlandIpcTransport&) = delete;
  ShellHyprlandIpcTransport& operator=(const ShellHyprlandIpcTransport&) = delete;
  ShellHyprlandIpcTransport(ShellHyprlandIpcTransport&&) = delete;
  ShellHyprlandIpcTransport& operator=(ShellHyprlandIpcTransport&&) = delete;

  virtual void connectEventStream() = 0;
  [[nodiscard]] virtual bool runCommand(const QByteArray& command, CommandCompletePredicate is_complete = {}) = 0;
  [[nodiscard]] virtual bool hasRunningCommand() const = 0;

 Q_SIGNALS:
  void eventStreamConnected();
  void eventStreamDisconnected();
  void eventLineReceived(const QByteArray& line);
  void commandFinished(const QByteArray& response, bool success);
};

using ShellHyprlandIpcTransportPtr = std::unique_ptr<ShellHyprlandIpcTransport>;

class ShellHyprlandIpcClient : public ShellHyprlandIpcTransport {
  Q_OBJECT

 public:
  explicit ShellHyprlandIpcClient(QString service_name, QObject* parent = nullptr);
  ShellHyprlandIpcClient(QString service_name, QString event_socket_path, QString command_socket_path,
                         bool use_abstract_namespace, QObject* parent = nullptr);
  ~ShellHyprlandIpcClient() override = default;

  ShellHyprlandIpcClient(const ShellHyprlandIpcClient&) = delete;
  ShellHyprlandIpcClient& operator=(const ShellHyprlandIpcClient&) = delete;
  ShellHyprlandIpcClient(ShellHyprlandIpcClient&&) = delete;
  ShellHyprlandIpcClient& operator=(ShellHyprlandIpcClient&&) = delete;

  void connectEventStream() override;
  [[nodiscard]] bool runCommand(const QByteArray& command, CommandCompletePredicate is_complete = {}) override;
  [[nodiscard]] bool hasRunningCommand() const override { return command_socket_ != nullptr; }

  [[nodiscard]] static QString socketBasePath();
  [[nodiscard]] static QString eventSocketPath();
  [[nodiscard]] static QString commandSocketPath();

#ifdef HOLONIGHT_TESTS
  void testSetEventSocket(QLocalSocket* socket) { event_socket_ = socket; }
  void testSetCommandSocket(QLocalSocket* socket) { command_socket_ = socket; }
  void testHandleEventSocketReadable() { onEventSocketReadable(); }
  void testHandleEventSocketConnected() { onEventSocketConnected(); }
  void testHandleEventSocketDisconnected() { onEventSocketDisconnected(); }
  void testHandleEventSocketError(QLocalSocket::LocalSocketError error) { onEventSocketError(error); }
  void testHandleCommandSocketConnected() { onCommandSocketConnected(); }
  void testHandleCommandSocketReadable() { onCommandSocketReadable(); }
  void testHandleCommandSocketDisconnected() { onCommandSocketDisconnected(); }
  void testHandleCommandSocketError(QLocalSocket::LocalSocketError error) { onCommandSocketError(error); }
  void testSetReconnectDelay(int delay_ms) { reconnect_delay_ms_ = delay_ms; }
  void testSetCommandTimeout(int timeout_ms) { command_timeout_ms_ = timeout_ms; }
#endif

 private:
  static constexpr int kConnectTimeoutMs = 1000;
  static constexpr int kDefaultCommandTimeoutMs = 2000;
  static constexpr int kInitialReconnectDelayMs = 1000;
  static constexpr int kMaxReconnectDelayMs = 30000;
  static constexpr qsizetype kEventReadChunkSize = 4096;

  void onEventSocketReadable();
  void onEventSocketConnected();
  void onEventSocketDisconnected();
  void onEventSocketError(QLocalSocket::LocalSocketError error);
  void onCommandSocketConnected();
  void onCommandSocketReadable();
  void onCommandSocketDisconnected();
  void onCommandSocketError(QLocalSocket::LocalSocketError error);
  void finishCommand(bool success);
  [[nodiscard]] QString resolvedEventSocketPath() const;
  [[nodiscard]] QString resolvedCommandSocketPath() const;
  void scheduleReconnect();
  void resetReconnectBackoff();

  QString service_name_;
  QString event_socket_path_;
  QString command_socket_path_;
  bool use_abstract_namespace_{false};
  QLocalSocket* event_socket_{nullptr};
  QLocalSocket* command_socket_{nullptr};
  QTimer* connect_timeout_{nullptr};
  QTimer* reconnect_timer_{nullptr};
  QTimer* command_timeout_{nullptr};
  int command_timeout_ms_{kDefaultCommandTimeoutMs};
  QByteArray event_buffer_;
  QByteArray active_command_;
  QByteArray command_buffer_;
  CommandCompletePredicate command_complete_;
  int reconnect_delay_ms_{kInitialReconnectDelayMs};
  bool reconnect_scheduled_{false};
};
