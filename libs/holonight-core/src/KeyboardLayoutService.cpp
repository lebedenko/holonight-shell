#include "KeyboardLayoutService.h"

#include "HyprlandIpc.h"

#include <algorithm>
#include <memory>

KeyboardLayoutService::KeyboardLayoutService(QObject* parent)
    : KeyboardLayoutService(std::make_unique<HyprlandIpcClient>(QStringLiteral("KeyboardLayoutService:")), parent) {}

KeyboardLayoutService::KeyboardLayoutService(HyprlandIpcTransportPtr ipc_client, QObject* parent)
    : QObject(parent), ipc_client_(std::move(ipc_client)) {}

void KeyboardLayoutService::start() {
  if (started_) {
    return;
  }
  started_ = true;
  connectSocket();
}

void KeyboardLayoutService::connectSocket() {
  connect(ipc_client_.get(), &HyprlandIpcTransport::eventStreamConnected, this,
          &KeyboardLayoutService::onEventSocketConnected, Qt::UniqueConnection);
  connect(ipc_client_.get(), &HyprlandIpcTransport::eventLineReceived, this, &KeyboardLayoutService::processEventLine,
          Qt::UniqueConnection);
  connect(ipc_client_.get(), &HyprlandIpcTransport::commandFinished, this, &KeyboardLayoutService::onCommandFinished,
          Qt::UniqueConnection);
  ipc_client_->connectEventStream();
}

void KeyboardLayoutService::queryCurrentLayout() {
  if (ipc_client_->hasRunningCommand()) {
    return;
  }

  const bool started = ipc_client_->runCommand(QByteArrayLiteral("j/devices"), [](const QByteArray& response) {
    return parseHyprlandKeyboardLayoutDevicesJson(response).has_value();
  });
  Q_UNUSED(started)
}

void KeyboardLayoutService::processEventLine(const QByteArray& line) {
  const std::optional<HyprlandKeyboardLayout> layout = parseHyprlandKeyboardLayoutEvent(line);
  if (layout.has_value()) {
    setLayoutName(layout->layout_name);
  }
}

void KeyboardLayoutService::onEventSocketConnected() { queryCurrentLayout(); }

void KeyboardLayoutService::onCommandFinished(const QByteArray& response, bool success) {
  if (success) {
    const std::optional<QString> layout = parseHyprlandKeyboardLayoutDevicesJson(response);
    if (layout.has_value()) {
      setLayoutName(*layout);
    }
  }
}

void KeyboardLayoutService::setLayoutName(const QString& value) {
  // Order matters: the name is committed before the code. Consumers that react to either signal by
  // reading both properties (the OSD's keyboard-layout channel source does exactly this) would
  // otherwise see the new code paired with the previous name, and that stale pair is the one they
  // would keep — the corrected emission that follows carries an unchanged code and gets diffed away.
  if (layout_name_ != value) {
    layout_name_ = value;
    emit layoutNameChanged();
  }
  setLayoutCode(keyboardLayoutCode(value));
}

void KeyboardLayoutService::setLayoutCode(const QString& value) {
  if (layout_code_ == value) {
    return;
  }
  layout_code_ = value;
  emit layoutCodeChanged();
}
