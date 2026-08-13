#pragma once

#include "CompositorBackend.h"
#include "SwayIpc.h"

#include <QLocalSocket>
#include <QTimer>

#include <cstdint>

class SwayBackend final : public CompositorBackend {
  Q_OBJECT

 public:
  explicit SwayBackend(QString socket_path = {}, QObject* parent = nullptr);
  void start() override;
  void activateWorkspace(const QString& workspace_id) override;

 private:
  enum class RequestPhase : std::uint8_t { Idle, Workspaces, Outputs, Tree, Command };
  static constexpr quint32 kGetWorkspaces = 1;
  static constexpr quint32 kSubscribe = 2;
  static constexpr quint32 kGetOutputs = 3;
  static constexpr quint32 kGetTree = 4;
  static constexpr quint32 kCommand = 0;
  static constexpr quint32 kEventBit = 1U << 31U;

  void connectSockets();
  void sendRequest(quint32 type, const QByteArray& payload = {});
  void handleRequestData();
  void handleSubscriptionData();
  [[nodiscard]] quint32 expectedResponseType() const;
  bool handleRequestFrame(const SwayIpcFrame& frame);
  void finishRefresh(const QByteArray& tree);
  void finishActivation(const QByteArray& payload);
  void disconnectSession(const QString& diagnostic);
  void scheduleRefresh();
  void beginRefresh();
  void fail(const QString& diagnostic);
  void scheduleReconnect();

  QString socket_path_;
  QLocalSocket request_socket_;
  QLocalSocket subscription_socket_;
  SwayIpcDecoder request_decoder_;
  SwayIpcDecoder subscription_decoder_;
  QTimer refresh_timer_;
  QTimer reconnect_timer_;
  RequestPhase phase_{RequestPhase::Idle};
  QByteArray workspaces_;
  QByteArray outputs_;
  QString pending_activation_;
  bool refresh_dirty_{false};
  bool subscription_ready_{false};
  int reconnect_delay_ms_{1000};
};
