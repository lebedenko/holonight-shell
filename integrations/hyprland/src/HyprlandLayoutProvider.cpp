#include "HyprlandLayoutProvider.h"

#include "ShellHyprlandIpc.h"

#include <algorithm>
#include <memory>

HyprlandLayoutProvider::HyprlandLayoutProvider(QObject* parent)
    : HyprlandLayoutProvider(std::make_unique<ShellHyprlandIpcClient>(QStringLiteral("HyprlandLayoutProvider:")),
                             parent) {}

HyprlandLayoutProvider::HyprlandLayoutProvider(ShellHyprlandIpcTransportPtr ipc_client, QObject* parent)
    : KeyboardLayoutProvider(parent), ipc_client_(std::move(ipc_client)) {}

void HyprlandLayoutProvider::start() {
  if (started_) {
    return;
  }
  started_ = true;
  connectSocket();
}

void HyprlandLayoutProvider::connectSocket() {
  connect(ipc_client_.get(), &ShellHyprlandIpcTransport::eventStreamDisconnected, this, [this] { setLayoutName({}); });
  connect(ipc_client_.get(), &ShellHyprlandIpcTransport::eventStreamConnected, this,
          &HyprlandLayoutProvider::onEventSocketConnected, Qt::UniqueConnection);
  connect(ipc_client_.get(), &ShellHyprlandIpcTransport::eventLineReceived, this,
          &HyprlandLayoutProvider::processEventLine, Qt::UniqueConnection);
  connect(ipc_client_.get(), &ShellHyprlandIpcTransport::commandFinished, this,
          &HyprlandLayoutProvider::onCommandFinished, Qt::UniqueConnection);
  ipc_client_->connectEventStream();
}

void HyprlandLayoutProvider::queryCurrentLayout() {
  if (ipc_client_->hasRunningCommand()) {
    return;
  }

  const bool started = ipc_client_->runCommand(QByteArrayLiteral("j/devices"), [](const QByteArray& response) {
    return parseShellHyprlandKeyboardLayoutDevicesJson(response).has_value();
  });
  Q_UNUSED(started)
}

void HyprlandLayoutProvider::processEventLine(const QByteArray& line) {
  const std::optional<ShellHyprlandKeyboardLayout> layout = parseShellHyprlandKeyboardLayoutEvent(line);
  if (layout.has_value()) {
    setLayoutName(layout->layout_name);
  }
}

void HyprlandLayoutProvider::onEventSocketConnected() { queryCurrentLayout(); }

void HyprlandLayoutProvider::onCommandFinished(const QByteArray& response, bool success) {
  if (success) {
    const std::optional<QString> layout = parseShellHyprlandKeyboardLayoutDevicesJson(response);
    if (layout.has_value()) {
      setLayoutName(*layout);
    }
  }
}

void HyprlandLayoutProvider::setLayoutName(const QString& value) {
  // Order matters: the name is committed before the code. Consumers that react to either signal by
  // reading both properties (the OSD's keyboard-layout channel source does exactly this) would
  // otherwise see the new code paired with the previous name, and that stale pair is the one they
  // would keep — the corrected emission that follows carries an unchanged code and gets diffed away.
  if (layout_name_ != value) {
    layout_name_ = value;
    emit layoutNameChanged();
  }
  setLayoutCode(shellKeyboardLayoutCode(value));
}

void HyprlandLayoutProvider::setLayoutCode(const QString& value) {
  if (layout_code_ == value) {
    return;
  }
  layout_code_ = value;
  emit layoutCodeChanged();
}
