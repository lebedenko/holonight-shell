#include "BluetoothService.h"

#include <algorithm>
using namespace HoloNight::System;
namespace {
QString label(const BluetoothDevice& d) {
  return !d.alias.isEmpty() ? d.alias : !d.name.isEmpty() ? d.name : d.address;
}
QVariantMap adapterMap(const BluetoothAdapter& a) {
  return {{"id", a.id},
          {"alias", a.alias},
          {"address", a.address},
          {"powered", a.powered},
          {"discoverable", a.discoverable},
          {"discovering", a.discovering}};
}
QString interactionName(BluetoothInteraction kind) {
  switch (kind) {
    case BluetoothInteraction::PinEntry:
      return QStringLiteral("pin-entry");
    case BluetoothInteraction::PinDisplay:
      return QStringLiteral("pin-display");
    case BluetoothInteraction::PasskeyEntry:
      return QStringLiteral("passkey-entry");
    case BluetoothInteraction::PasskeyDisplay:
      return QStringLiteral("passkey-display");
    case BluetoothInteraction::Confirmation:
      return QStringLiteral("confirmation");
    case BluetoothInteraction::PairingAuthorization:
      return QStringLiteral("pairing-authorization");
    case BluetoothInteraction::ServiceAuthorization:
      return QStringLiteral("service-authorization");
  }
  return {};
}
QString serviceName(const QString& uuid) {
  const auto shortId = uuid.toLower().left(8);
  if (shortId == "0000110b") return QObject::tr("Audio sink");
  if (shortId == "0000110a") return QObject::tr("Audio source");
  if (shortId == "0000110e") return QObject::tr("Media remote control");
  if (shortId == "0000111e") return QObject::tr("Hands-free audio");
  if (shortId == "00001124" || shortId == "00001812") return QObject::tr("Keyboard / pointing device");
  if (shortId == "0000180f") return QObject::tr("Battery service");
  if (shortId == "0000180a") return QObject::tr("Device information");
  return QObject::tr("Other service");
}
}  // namespace
BluetoothService::BluetoothService(QObject* parent) : BluetoothService(nullptr, parent) {}
BluetoothService::BluetoothService(BluetoothBackend* backend, QObject* parent)
    : QObject(parent),
      controller_(backend ? std::make_unique<BluetoothController>(backend) : std::make_unique<BluetoothController>()) {
  release_.setSingleShot(true);
  release_.setInterval(0);
  connect(&release_, &QTimer::timeout, this, [this] {
    scan_requested_ = false;
    reconcileDiscovery();
    emit changed();
  });
  connect(controller_->adapters(), &QAbstractItemModel::modelReset, this, &BluetoothService::update);
  connect(controller_->adapters(), &QAbstractItemModel::dataChanged, this, &BluetoothService::update);
  connect(controller_->devices(), &QAbstractItemModel::modelReset, this, &BluetoothService::update);
  connect(controller_->devices(), &QAbstractItemModel::dataChanged, this, &BluetoothService::update);
  connect(controller_.get(), &BluetoothController::availableChanged, this, &BluetoothService::update);
  connect(controller_.get(), &BluetoothController::operationStateChanged, this, &BluetoothService::changed);
  connect(controller_.get(), &BluetoothController::operationFinished, this, &BluetoothService::completed);
  connect(controller_.get(), &BluetoothController::diagnostic, this,
          [this](const QString& name, const QString& message) {
            error_ = tr("Bluetooth agent: %1 (%2)").arg(message, name);
            emit changed();
          });
  connect(controller_.get(), &BluetoothController::agentRequested, this, [this](const BluetoothAgentRequest& r) {
    interaction_ = {{"id", r.requestId},
                    {"deviceId", r.deviceId},
                    {"kind", interactionName(r.interaction)},
                    {"code", r.code},
                    {"service", serviceName(r.serviceUuid)},
                    {"entered", r.entered}};
    emit changed();
    emit pairingRequested();
  });
  connect(controller_.get(), &BluetoothController::agentCanceled, this, [this](const QString& id) {
    if (interaction_.value("id").toString() == id) {
      interaction_.clear();
      emit changed();
    }
  });
  update();
}
BluetoothService::~BluetoothService() {
  disconnect(controller_.get(), nullptr, this, nullptr);
  disconnect(controller_->adapters(), nullptr, this, nullptr);
  disconnect(controller_->devices(), nullptr, this, nullptr);
}
QVariantList BluetoothService::adapters() const {
  QVariantList out;
  for (const auto& a : controller_->adapters()->items()) out.append(adapterMap(a));
  return out;
}
QVariantMap BluetoothService::selectedAdapter() const {
  const auto a = controller_->adapters()->find(selected_);
  auto result = a ? adapterMap(*a) : QVariantMap{};
  result.insert("busy", controller_->busy(selected_));
  return result;
}
QVariantList BluetoothService::devices() const {
  auto items = controller_->devices()->items();
  std::stable_sort(items.begin(), items.end(), [this](const auto& a, const auto& b) {
    const int sa = a.connected ? 0 : a.paired ? 1 : 2, sb = b.connected ? 0 : b.paired ? 1 : 2;
    if (sa != sb) return sa < sb;
    if (active_) {
      const auto ka = QString::number(sa) + a.id, kb = QString::number(sb) + b.id;
      const int ia = display_order_.indexOf(ka), ib = display_order_.indexOf(kb);
      if (ia >= 0 || ib >= 0) {
        if (ia < 0) return false;
        if (ib < 0) return true;
        return ia < ib;
      }
    }
    if (sa == 2 && a.rssi != b.rssi) return a.rssi.value_or(-32768) > b.rssi.value_or(-32768);
    const int comparison = QString::localeAwareCompare(label(a), label(b));
    return comparison ? comparison < 0 : a.id < b.id;
  });
  QVariantList out;
  display_order_.clear();
  for (const auto& d : items) {
    if (d.adapterId != selected_) continue;
    display_order_.append(QString::number(d.connected ? 0 : d.paired ? 1 : 2) + d.id);
    QStringList services;
    for (const auto& id : d.uuids) {
      const auto name = serviceName(id);
      if (!services.contains(name)) services.append(name);
    }
    out.append(QVariantMap{{"id", d.id},
                           {"name", label(d)},
                           {"originalName", d.name},
                           {"alias", d.alias},
                           {"address", d.address},
                           {"addressType", d.addressType},
                           {"paired", d.paired},
                           {"bonded", d.bonded},
                           {"connected", d.connected},
                           {"trusted", d.trusted},
                           {"blocked", d.blocked},
                           {"icon", d.icon.isEmpty() ? QStringLiteral("bluetooth") : d.icon},
                           {"battery", d.battery ? QVariant(*d.battery) : QVariant{}},
                           {"rssi", d.rssi ? QVariant(*d.rssi) : QVariant{}},
                           {"uuids", d.uuids},
                           {"services", services},
                           {"deviceClass", d.deviceClass},
                           {"appearance", d.appearance},
                           {"busy", controller_->busy(d.id)},
                           {"section", d.connected ? tr("Connected")
                                       : d.paired  ? tr("Paired")
                                                   : tr("Available")}});
  }
  return out;
}
int BluetoothService::connectedCount() const {
  int count = 0;
  for (const auto& d : controller_->devices()->items())
    if (d.connected) ++count;
  return count;
}
QString BluetoothService::tooltip() const {
  QStringList lines;
  for (const auto& d : controller_->devices()->items())
    if (d.connected) lines.append(d.battery ? tr("%1 — %2%").arg(label(d)).arg(*d.battery) : label(d));
  return lines.isEmpty() ? (selectedAdapter().value("powered").toBool() ? tr("Bluetooth enabled") : tr("Bluetooth off"))
                         : lines.join(QLatin1Char('\n'));
}
void BluetoothService::update() {
  if (!explicit_selection_ || !controller_->adapters()->find(selected_)) {
    explicit_selection_ = false;
    const auto items = controller_->adapters()->items();
    selected_.clear();
    for (const auto& a : items)
      if (a.powered) {
        selected_ = a.id;
        break;
      }
    if (selected_.isEmpty() && !items.isEmpty()) selected_ = items.first().id;
  }
  for (auto it = owned_.begin(); it != owned_.end();) {
    const auto adapter = controller_->adapters()->find(*it);
    if (!adapter || !adapter->powered)
      it = owned_.erase(it);
    else
      ++it;
  }
  if (!pairing_.isEmpty() && !controller_->devices()->find(pairing_)) cancelPairing();
  reconcileDiscovery();
  emit changed();
}
void BluetoothService::selectAdapter(const QString& id) {
  if (!controller_->adapters()->find(id)) return;
  explicit_selection_ = true;
  if (selected_ == id) return;
  cancelPairing();
  selected_ = id;
  explicit_selection_ = true;
  reconcileDiscovery();
  emit changed();
}
void BluetoothService::setActive(bool active) {
  if (active_ == active) return;
  active_ = active;
  if (active) {
    release_.stop();
    scan_requested_ = true;
    reconcileDiscovery();
  } else {
    cancelPairing();
    release_.start();
  }
}
void BluetoothService::scan(bool enabled) {
  error_.clear();
  release_.stop();
  scan_requested_ = enabled && active_;
  reconcileDiscovery();
  emit changed();
}
void BluetoothService::reconcileDiscovery() {
  if (!discovery_request_.isEmpty()) return;
  QString desired;
  const auto a = controller_->adapters()->find(selected_);
  if (scan_requested_ && a && a->powered) desired = selected_;
  for (const auto& id : owned_)
    if (id != desired && !controller_->busy(id)) {
      discovery_target_ = id;
      discovery_request_ = controller_->execute(BluetoothOperation::StopDiscovery, id);
      return;
    }
  if (!desired.isEmpty() && !owned_.contains(desired) && !controller_->busy(desired)) {
    discovery_target_ = desired;
    discovery_request_ = controller_->execute(BluetoothOperation::StartDiscovery, desired);
  }
}
void BluetoothService::power(bool enabled) {
  error_.clear();
  controller_->execute(BluetoothOperation::Power, selected_, enabled);
}
void BluetoothService::discoverable(bool enabled) {
  error_.clear();
  controller_->execute(BluetoothOperation::Discoverable, selected_, enabled);
}
void BluetoothService::renameAdapter(const QString& alias) {
  error_.clear();
  controller_->execute(BluetoothOperation::AdapterAlias, selected_, alias);
}
void BluetoothService::action(const QString& id, const QString& action, const QVariant& value) {
  error_.clear();
  BluetoothOperation operation;
  if (action == "pair") {
    cancelPairing();
    pairing_ = id;
    operation = BluetoothOperation::Pair;
  } else if (action == "connect")
    operation = BluetoothOperation::Connect;
  else if (action == "disconnect")
    operation = BluetoothOperation::Disconnect;
  else if (action == "rename")
    operation = BluetoothOperation::DeviceAlias;
  else if (action == "trust")
    operation = BluetoothOperation::Trust;
  else if (action == "block")
    operation = BluetoothOperation::Block;
  else if (action == "forget")
    operation = BluetoothOperation::Forget;
  else
    return;
  const auto request = controller_->execute(operation, id, value);
  if (operation == BluetoothOperation::Pair) {
    chain_request_ = request;
    chain_operation_ = operation;
  }
}
void BluetoothService::completed(const BluetoothResult& result) {
  if (!result.succeeded()) {
    QString advice = tr("Try the operation again.");
    if (result.errorName.endsWith("NotReady"))
      advice = tr("Turn Bluetooth on and try again.");
    else if (result.errorName.contains("Authentication"))
      advice = tr("Put the device in pairing mode and check the pairing code.");
    else if (result.errorName.endsWith("Busy") || result.errorName.endsWith("InProgress"))
      advice = tr("Wait for the current operation to finish.");
    else if (result.errorName.endsWith("NotAuthorized"))
      advice = tr("Bluetooth permission was denied.");
    error_ = tr("Bluetooth operation failed. %1\n%2 (%3)").arg(advice, result.errorMessage, result.errorName);
    if (result.requestId == chain_request_ && chain_operation_ != BluetoothOperation::Pair)
      error_ =
          (chain_operation_ == BluetoothOperation::Trust ? tr("Device paired, but trusting failed. ")
                                                         : tr("Device paired and trusted, but connecting failed. ")) +
          error_;
  }
  if (result.requestId == chain_request_) {
    chain_request_.clear();
    if (result.succeeded() && chain_operation_ != BluetoothOperation::Connect) {
      chain_operation_ =
          chain_operation_ == BluetoothOperation::Pair ? BluetoothOperation::Trust : BluetoothOperation::Connect;
      chain_request_ = controller_->execute(
          chain_operation_, pairing_, chain_operation_ == BluetoothOperation::Trust ? QVariant(true) : QVariant{});
    } else
      pairing_.clear();
  }
  if (result.requestId == discovery_request_) {
    discovery_request_.clear();
    if (result.succeeded()) {
      if (result.operation == BluetoothOperation::StartDiscovery)
        owned_.insert(discovery_target_);
      else
        owned_.remove(discovery_target_);
    } else {
      scan_requested_ = false;
      owned_.remove(discovery_target_);
    }
  }
  reconcileDiscovery();
  emit changed();
}
void BluetoothService::respond(bool accept, const QString& input) {
  controller_->respond(interaction_.value("id").toString(), accept, input);
}
void BluetoothService::cancelPairing() {
  QString target = pairing_;
  const auto kind = interaction_.value("kind").toString();
  if (target.isEmpty() && (kind == "pin-display" || kind == "passkey-display"))
    target = interaction_.value("deviceId").toString();
  if (!interaction_.isEmpty()) respond(false);
  if (!target.isEmpty()) {
    controller_->execute(BluetoothOperation::CancelPairing, target);
    pairing_.clear();
  }
  chain_request_.clear();
  interaction_.clear();
  emit changed();
}
