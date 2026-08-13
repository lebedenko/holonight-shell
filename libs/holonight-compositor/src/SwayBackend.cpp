#include "SwayBackend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <limits>

SwayBackend::SwayBackend(QString socket_path, QObject* parent)
    : CompositorBackend(parent),
      socket_path_(socket_path.isEmpty() ? qEnvironmentVariable("SWAYSOCK") : std::move(socket_path)) {
  refresh_timer_.setSingleShot(true);
  reconnect_timer_.setSingleShot(true);
  connect(&refresh_timer_, &QTimer::timeout, this, &SwayBackend::beginRefresh);
  connect(&reconnect_timer_, &QTimer::timeout, this, &SwayBackend::connectSockets);
  connect(&request_socket_, &QLocalSocket::readyRead, this, &SwayBackend::handleRequestData);
  connect(&subscription_socket_, &QLocalSocket::readyRead, this, &SwayBackend::handleSubscriptionData);
  connect(&request_socket_, &QLocalSocket::connected, this, &SwayBackend::scheduleRefresh);
  connect(&subscription_socket_, &QLocalSocket::connected, this, [this] {
    subscription_socket_.write(
        encodeSwayIpcFrame(kSubscribe, QByteArrayLiteral(R"(["workspace","window","output","shutdown"])")));
  });
  const auto disconnected = [this] {
    if (request_socket_.state() == QLocalSocket::UnconnectedState &&
        subscription_socket_.state() == QLocalSocket::UnconnectedState) {
      fail(QStringLiteral("Sway IPC disconnected"));
      scheduleReconnect();
    }
  };
  connect(&request_socket_, &QLocalSocket::disconnected, this, disconnected);
  connect(&subscription_socket_, &QLocalSocket::disconnected, this, disconnected);
}

void SwayBackend::start() {
  if (socket_path_.isEmpty()) {
    fail(QStringLiteral("SWAYSOCK is not set"));
    return;
  }
  connectSockets();
}

void SwayBackend::connectSockets() {
  request_decoder_ = {};
  subscription_decoder_ = {};
  subscription_ready_ = false;
  request_socket_.connectToServer(socket_path_, QIODevice::ReadWrite);
  subscription_socket_.connectToServer(socket_path_, QIODevice::ReadWrite);
}

void SwayBackend::activateWorkspace(const QString& workspace_id) {
  if (workspace_id.isEmpty() || request_socket_.state() != QLocalSocket::ConnectedState) return;
  if (phase_ != RequestPhase::Idle) {
    refresh_dirty_ = true;
    return;
  }
  phase_ = RequestPhase::Command;
  const QString command = QStringLiteral("workspace \"") + escapeSwayWorkspaceName(workspace_id) + QStringLiteral("\"");
  sendRequest(kCommand, command.toUtf8());
}

void SwayBackend::sendRequest(quint32 type, const QByteArray& payload) {
  request_socket_.write(encodeSwayIpcFrame(type, payload));
}

void SwayBackend::scheduleRefresh() {
  if (phase_ != RequestPhase::Idle) {
    refresh_dirty_ = true;
    return;
  }
  if (!refresh_timer_.isActive()) refresh_timer_.start(0);
}

void SwayBackend::beginRefresh() {
  if (request_socket_.state() != QLocalSocket::ConnectedState || phase_ != RequestPhase::Idle) return;
  refresh_dirty_ = false;
  workspaces_.clear();
  outputs_.clear();
  phase_ = RequestPhase::Workspaces;
  sendRequest(kGetWorkspaces);
}

void SwayBackend::handleRequestData() {
  if (!request_decoder_.append(request_socket_.readAll())) {
    fail(request_decoder_.error());
    request_socket_.abort();
    return;
  }
  for (const SwayIpcFrame& frame : request_decoder_.takeFrames()) {
    const quint32 expected = phase_ == RequestPhase::Workspaces ? kGetWorkspaces
                             : phase_ == RequestPhase::Outputs  ? kGetOutputs
                             : phase_ == RequestPhase::Tree     ? kGetTree
                             : phase_ == RequestPhase::Command  ? kCommand
                                                                : std::numeric_limits<quint32>::max();
    if (frame.type != expected) {
      fail(QStringLiteral("unexpected Sway IPC response type"));
      request_socket_.abort();
      return;
    }
    if (phase_ == RequestPhase::Workspaces) {
      workspaces_ = frame.payload;
      phase_ = RequestPhase::Outputs;
      sendRequest(kGetOutputs);
    } else if (phase_ == RequestPhase::Outputs) {
      outputs_ = frame.payload;
      phase_ = RequestPhase::Tree;
      sendRequest(kGetTree);
    } else if (phase_ == RequestPhase::Tree) {
      phase_ = RequestPhase::Idle;
      if (auto snapshot = parseSwaySnapshot(workspaces_, outputs_, frame.payload)) {
        reconnect_delay_ms_ = 1000;
        emit snapshotReady(std::move(*snapshot));
      } else {
        fail(QStringLiteral("invalid Sway refresh response"));
      }
      if (refresh_dirty_) scheduleRefresh();
    } else if (phase_ == RequestPhase::Command) {
      phase_ = RequestPhase::Idle;
      const QJsonDocument response = QJsonDocument::fromJson(frame.payload);
      if (!response.isArray() || response.array().isEmpty() ||
          !response.array().first().toObject().value(QStringLiteral("success")).toBool(false)) {
        fail(QStringLiteral("Sway workspace activation failed"));
      }
      scheduleRefresh();
    }
  }
}

void SwayBackend::handleSubscriptionData() {
  if (!subscription_decoder_.append(subscription_socket_.readAll())) {
    fail(subscription_decoder_.error());
    subscription_socket_.abort();
    return;
  }
  for (const SwayIpcFrame& frame : subscription_decoder_.takeFrames()) {
    if (!subscription_ready_) {
      const QJsonDocument response = QJsonDocument::fromJson(frame.payload);
      if (frame.type != kSubscribe || !response.isObject() ||
          !response.object().value(QStringLiteral("success")).toBool(false)) {
        fail(QStringLiteral("Sway IPC subscription rejected"));
        subscription_socket_.abort();
        return;
      }
      subscription_ready_ = true;
      continue;
    }
    if ((frame.type & kEventBit) == 0) {
      fail(QStringLiteral("non-event traffic on Sway subscription socket"));
      subscription_socket_.abort();
      return;
    }
    scheduleRefresh();
  }
}

void SwayBackend::fail(const QString& diagnostic) {
  emit snapshotReady({.connected = false, .diagnostic = diagnostic});
}

void SwayBackend::scheduleReconnect() {
  if (reconnect_timer_.isActive()) return;
  reconnect_timer_.start(reconnect_delay_ms_);
  reconnect_delay_ms_ = std::min(reconnect_delay_ms_ * 2, 30000);
}
