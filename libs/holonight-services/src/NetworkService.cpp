#include "NetworkService.h"

#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QLoggingCategory>
#include <QProcess>

#include <algorithm>
#include <memory>
#include <optional>

Q_LOGGING_CATEGORY(lcNetwork, "holonight.network")

static constexpr auto kNmService = "org.freedesktop.NetworkManager";
static constexpr auto kNmPath = "/org/freedesktop/NetworkManager";
static constexpr auto kNmIface = "org.freedesktop.NetworkManager";
static constexpr auto kActiveIface = "org.freedesktop.NetworkManager.Connection.Active";
static constexpr auto kApIface = "org.freedesktop.NetworkManager.AccessPoint";
static constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";
static constexpr uint kNmStateConnectedGlobal = 70;

bool isVpnConnectionType(const QString& connection_type) {
  return connection_type == QLatin1String("vpn") || connection_type == QLatin1String("wireguard") ||
         connection_type == QLatin1String("tun");
}

QList<QDBusObjectPath> activeConnectionPaths(const QVariant& value) {
  if (value.canConvert<QList<QDBusObjectPath>>()) {
    return value.value<QList<QDBusObjectPath>>();
  }

  if (!value.canConvert<QDBusArgument>()) {
    return {};
  }

  QList<QDBusObjectPath> paths;
  const auto arg = value.value<QDBusArgument>();
  arg.beginArray();
  while (!arg.atEnd()) {
    QDBusObjectPath path;
    arg >> path;
    paths.push_back(path);
  }
  arg.endArray();
  return paths;
}

NetworkStateUpdate buildNetworkStateUpdate(const NetworkQuerySnapshot& snapshot) {
  NetworkStateUpdate update{
      .online = snapshot.online,
  };

  if (snapshot.active_connection_types.has_value()) {
    update.vpn_active =
        std::any_of(snapshot.active_connection_types->cbegin(), snapshot.active_connection_types->cend(),
                    [](const QString& type) { return isVpnConnectionType(type); });
  }

  if (!snapshot.primary_connection_path.has_value() || snapshot.primary_connection_path->isEmpty() ||
      *snapshot.primary_connection_path == QLatin1String("/")) {
    update.type = NetworkService::None;
    update.ssid = QString();
    update.strength = 0;
    update.access_point_path = QString();
    return update;
  }

  if (!snapshot.connection_type.has_value()) {
    update.type = NetworkService::None;
    return update;
  }

  if (*snapshot.connection_type == QLatin1String("802-3-ethernet")) {
    update.type = NetworkService::Wired;
    update.ssid = QString();
    update.strength = 0;
    update.access_point_path = QString();
    return update;
  }

  if (*snapshot.connection_type != QLatin1String("802-11-wireless")) {
    update.type = NetworkService::None;
    update.ssid = QString();
    update.strength = 0;
    update.access_point_path = QString();
    return update;
  }

  update.type = NetworkService::WiFi;

  if (!snapshot.specific_object_path.has_value()) {
    return update;
  }

  if (snapshot.specific_object_path->isEmpty() || *snapshot.specific_object_path == QLatin1String("/")) {
    update.ssid = QString();
    update.strength = 0;
    update.access_point_path = QString();
    return update;
  }

  update.access_point_path = *snapshot.specific_object_path;

  if (!snapshot.access_point_properties.has_value()) {
    return update;
  }

  const QVariantMap& apMap = *snapshot.access_point_properties;
  if (apMap.contains(QStringLiteral("Ssid"))) {
    const QByteArray rawSsid = apMap.value(QStringLiteral("Ssid")).toByteArray();
    QString decoded = QString::fromUtf8(rawSsid);
    if (decoded.contains(QChar::ReplacementCharacter)) {
      decoded.clear();
    }
    update.ssid = decoded;
  }

  if (apMap.contains(QStringLiteral("Strength"))) {
    const auto raw = static_cast<int>(apMap.value(QStringLiteral("Strength")).toUInt());
    update.strength = std::clamp(raw, 0, 100);
  } else {
    qCWarning(lcNetwork) << "NetworkService: Strength key absent from AP map";
  }

  return update;
}

NetworkService::NetworkService(QObject* parent)
    : NetworkService(std::make_unique<QtDbusPropertyClient>(), std::make_unique<QtNetworkManagerBackend>(), parent) {}

NetworkService::NetworkService(DbusPropertyClientPtr dbus, QObject* parent)
    : NetworkService(std::move(dbus), nullptr, parent) {}

NetworkService::NetworkService(DbusPropertyClientPtr dbus, std::unique_ptr<NetworkManagerBackend> backend,
                               QObject* parent)
    : QObject(parent),
      dbus_(std::move(dbus)),
      backend_(std::move(backend)),
      wifi_networks_(new WifiNetworkModel(this)),
      poll_timer_(new QTimer(this)) {
  poll_timer_->setInterval(2000);
  connect(poll_timer_, &QTimer::timeout, this, &NetworkService::onPollTimer);
  if (backend_ != nullptr) {
    backend_->setParent(this);
    connect(backend_.get(), &NetworkManagerBackend::stateChanged, this, &NetworkService::onBackendStateChanged);
    connect(backend_.get(), &NetworkManagerBackend::operationError, this, &NetworkService::onBackendError);
  }
}

void NetworkService::start() {
  if (started_) {
    return;
  }
  started_ = true;

  if (!dbus_->systemBusConnected()) {
    qCWarning(lcNetwork) << "NetworkService: system D-Bus not available";
    poll_timer_->start();
    if (backend_ != nullptr) {
      backend_->start();
    }
    return;
  }

  if (!dbus_->serviceRegistered(QLatin1String(kNmService))) {
    qCInfo(lcNetwork) << "NetworkService: NetworkManager not running, will retry";
    poll_timer_->start();
    if (backend_ != nullptr) {
      backend_->start();
    }
    return;
  }

  connectToNm();
  if (backend_ != nullptr) {
    backend_->start();
  }
}

void NetworkService::rescanWifi() {
  clearLastError();
  if (backend_ != nullptr) {
    backend_->requestScan();
  }
}

void NetworkService::setWifiEnabled(bool enabled) {
  clearLastError();
  if (backend_ != nullptr) {
    backend_->setWirelessEnabled(enabled);
  }
}

void NetworkService::connectNetwork(int row) {
  clearLastError();
  if (backend_ == nullptr) {
    return;
  }
  const std::optional<WifiNetwork> network = wifi_networks_->networkAt(row);
  if (!network.has_value()) {
    setLastError(QStringLiteral("Wi-Fi network is no longer available"));
    return;
  }
  if (network->known && !network->connection_path.isEmpty()) {
    backend_->activateKnown(*network);
    return;
  }
  if (!network->secured) {
    backend_->activateOpen(*network);
    return;
  }
  setLastError(QStringLiteral("Password required"));
}

void NetworkService::connectNetworkWithPassword(int row, const QString& password) {
  clearLastError();
  if (backend_ == nullptr) {
    return;
  }
  const std::optional<WifiNetwork> network = wifi_networks_->networkAt(row);
  if (!network.has_value()) {
    setLastError(QStringLiteral("Wi-Fi network is no longer available"));
    return;
  }
  backend_->activateWithPassword(*network, password);
}

void NetworkService::disconnectActive() {
  clearLastError();
  if (backend_ != nullptr) {
    backend_->disconnectActive(active_connection_path_);
  }
}

void NetworkService::openNetworkSettings() {
  clearLastError();
  if (!QProcess::startDetached(QStringLiteral("nm-connection-editor"))) {
    setLastError(QStringLiteral("Could not launch nm-connection-editor"));
  }
}

void NetworkService::clearLastError() { setLastError({}); }

NetworkService::~NetworkService() {
  if (!current_ap_path_.isEmpty()) {
    dbus_->disconnectSignal(QLatin1String(kNmService), current_ap_path_, QLatin1String(kPropsIface),
                            QStringLiteral("PropertiesChanged"), this,
                            SLOT(onApPropertiesChanged(QString, QVariantMap, QStringList)));
  }
}

void NetworkService::connectToNm() {
  dbus_->connectSignal(QLatin1String(kNmService), QLatin1String(kNmPath), QLatin1String(kNmIface),
                       QStringLiteral("StateChanged"), this, SLOT(onNmStateChanged(uint)));
  setAvailable(true);
  queryAll();
  if (!poll_timer_->isActive()) {
    poll_timer_->start();
  }
}

void NetworkService::queryAll() {
  NetworkQuerySnapshot snapshot;

  const auto finish = [this, &snapshot]() {
    snapshot.active_connection_types = queryActiveConnectionTypes();
    applyStateUpdate(buildNetworkStateUpdate(snapshot));
  };

  const std::optional<QVariant> state =
      getDbusProperty(QLatin1String(kNmPath), QLatin1String(kNmIface), QStringLiteral("State"), nullptr);
  if (state.has_value()) {
    snapshot.online = state->toUInt() == kNmStateConnectedGlobal;
  }

  const std::optional<QVariant> primary =
      getDbusProperty(QLatin1String(kNmPath), QLatin1String(kNmIface), QStringLiteral("PrimaryConnection"),
                      "NetworkService: get PrimaryConnection failed");
  if (!primary.has_value()) {
    finish();
    return;
  }

  snapshot.primary_connection_path = primary->value<QDBusObjectPath>().path();
  if (snapshot.primary_connection_path->isEmpty() || *snapshot.primary_connection_path == QLatin1String("/")) {
    finish();
    return;
  }

  const std::optional<QVariant> connection_type =
      getDbusProperty(*snapshot.primary_connection_path, QLatin1String(kActiveIface), QStringLiteral("Type"),
                      "NetworkService: get connection Type failed");
  if (!connection_type.has_value()) {
    finish();
    return;
  }

  snapshot.connection_type = connection_type->toString();

  if (*snapshot.connection_type == QLatin1String("802-3-ethernet") ||
      *snapshot.connection_type != QLatin1String("802-11-wireless")) {
    finish();
    return;
  }

  const std::optional<QVariant> specific =
      getDbusProperty(*snapshot.primary_connection_path, QLatin1String(kActiveIface), QStringLiteral("SpecificObject"),
                      "NetworkService: get SpecificObject failed");
  if (!specific.has_value()) {
    finish();
    return;
  }

  snapshot.specific_object_path = specific->value<QDBusObjectPath>().path();
  if (snapshot.specific_object_path->isEmpty() || *snapshot.specific_object_path == QLatin1String("/")) {
    finish();
    return;
  }

  const std::optional<QVariantMap> ap_properties =
      dbus_->allProperties(QLatin1String(kNmService), *snapshot.specific_object_path, QLatin1String(kApIface));
  if (!ap_properties.has_value()) {
    qCWarning(lcNetwork) << "NetworkService: GetAll on AP failed";
    finish();
    return;
  }

  snapshot.access_point_properties = *ap_properties;
  finish();
}

std::optional<QVariant> NetworkService::getDbusProperty(const QString& path, const QString& interface,
                                                        const QString& name, const char* failure_message) const {
  const std::optional<QVariant> value = dbus_->property(QLatin1String(kNmService), path, interface, name);
  if (!value.has_value() && failure_message != nullptr) {
    qCWarning(lcNetwork) << failure_message;
  }
  return value;
}

void NetworkService::subscribeAp(const QString& apPath) {
  if (apPath == current_ap_path_) {
    return;
  }

  if (!current_ap_path_.isEmpty()) {
    dbus_->disconnectSignal(QLatin1String(kNmService), current_ap_path_, QLatin1String(kPropsIface),
                            QStringLiteral("PropertiesChanged"), this,
                            SLOT(onApPropertiesChanged(QString, QVariantMap, QStringList)));
  }

  current_ap_path_ = apPath;

  if (!apPath.isEmpty()) {
    dbus_->connectSignal(QLatin1String(kNmService), apPath, QLatin1String(kPropsIface),
                         QStringLiteral("PropertiesChanged"), this,
                         SLOT(onApPropertiesChanged(QString, QVariantMap, QStringList)));
  }
}

std::optional<QList<QString>> NetworkService::queryActiveConnectionTypes() {
  const std::optional<QVariant> active_connections = dbus_->property(
      QLatin1String(kNmService), QLatin1String(kNmPath), QLatin1String(kNmIface), QStringLiteral("ActiveConnections"));
  if (!active_connections.has_value()) {
    qCWarning(lcNetwork) << "NetworkService: get ActiveConnections failed";
    return std::nullopt;
  }

  const auto connections = activeConnectionPaths(*active_connections);

  QList<QString> connection_types;

  for (const QDBusObjectPath& connPath : connections) {
    const std::optional<QVariant> conn_type = dbus_->property(QLatin1String(kNmService), connPath.path(),
                                                              QLatin1String(kActiveIface), QStringLiteral("Type"));
    if (conn_type.has_value()) {
      connection_types.append(conn_type->toString());
    }
  }

  return connection_types;
}

void NetworkService::applyStateUpdate(const NetworkStateUpdate& update) {
  if (update.online.has_value()) {
    setOnline(*update.online);
  }
  if (update.type.has_value()) {
    setType(*update.type);
  }
  if (update.ssid.has_value()) {
    setSsid(*update.ssid);
  }
  if (update.strength.has_value()) {
    setStrength(*update.strength);
  }
  if (update.access_point_path.has_value()) {
    subscribeAp(*update.access_point_path);
  }
  if (update.vpn_active.has_value()) {
    setVpnActive(*update.vpn_active);
  }
}

void NetworkService::onNmStateChanged(uint newState) {
  setOnline(newState == kNmStateConnectedGlobal);
  queryAll();
}

void NetworkService::onApPropertiesChanged(const QString& iface, const QVariantMap& changed,
                                           const QStringList& /*invalidated*/) {
  if (iface != QLatin1String(kApIface)) {
    return;
  }
  if (changed.contains(QStringLiteral("Strength"))) {
    const auto raw = static_cast<int>(changed.value(QStringLiteral("Strength")).toUInt());
    setStrength(std::clamp(raw, 0, 100));
  }
}

void NetworkService::onPollTimer() {
  if (available_) {
    queryAll();
    if (backend_ != nullptr) {
      backend_->refresh();
    }
    return;
  }

  if (!dbus_->systemBusConnected()) {
    return;
  }
  if (!dbus_->serviceRegistered(QLatin1String(kNmService))) {
    return;
  }
  qCInfo(lcNetwork) << "NetworkService: NetworkManager found, connecting";
  connectToNm();
  if (backend_ != nullptr) {
    backend_->start();
  }
}

void NetworkService::onBackendStateChanged(const NetworkBackendState& state) {
  if (state.available) {
    setAvailable(true);
  }
  setWifiEnabledState(state.wifi_enabled);
  setWifiHardwareEnabled(state.wifi_hardware_enabled);
  setScanning(state.scanning);
  setActiveConnectionName(state.active_connection_name);
  setActiveIp4Address(state.active_ip4_address);
  setActiveFrequencyMhz(state.active_frequency_mhz);
  setActiveLinkSpeedMbps(state.active_link_speed_mbps);
  setDownloadSpeedText(state.download_speed_text);
  setUploadSpeedText(state.upload_speed_text);
  setConnectionStatus(state.connection_status);
  active_connection_path_ = state.active_connection_path;
  wifi_networks_->setNetworks(state.networks);
}

void NetworkService::onBackendError(const QString& message) { setLastError(message); }

void NetworkService::setAvailable(bool value) {
  if (available_ == value) {
    return;
  }
  available_ = value;
  emit availableChanged();
}

void NetworkService::setOnline(bool value) {
  if (online_ == value) {
    return;
  }
  online_ = value;
  emit onlineChanged();
}

void NetworkService::setType(int value) {
  if (type_ == value) {
    return;
  }
  type_ = value;
  emit typeChanged();
}

void NetworkService::setVpnActive(bool value) {
  if (vpn_active_ == value) {
    return;
  }
  vpn_active_ = value;
  emit vpnActiveChanged();
}

void NetworkService::setSsid(const QString& value) {
  if (ssid_ == value) {
    return;
  }
  ssid_ = value;
  emit ssidChanged();
}

void NetworkService::setStrength(int value) {
  if (strength_ == value) {
    return;
  }
  strength_ = value;
  emit strengthChanged();
}

void NetworkService::setWifiEnabledState(bool value) {
  if (wifi_enabled_ == value) {
    return;
  }
  wifi_enabled_ = value;
  emit wifiEnabledChanged();
}

void NetworkService::setWifiHardwareEnabled(bool value) {
  if (wifi_hardware_enabled_ == value) {
    return;
  }
  wifi_hardware_enabled_ = value;
  emit wifiHardwareEnabledChanged();
}

void NetworkService::setScanning(bool value) {
  if (scanning_ == value) {
    return;
  }
  scanning_ = value;
  emit scanningChanged();
}

void NetworkService::setActiveConnectionName(const QString& value) {
  if (active_connection_name_ == value) {
    return;
  }
  active_connection_name_ = value;
  emit activeConnectionNameChanged();
}

void NetworkService::setActiveIp4Address(const QString& value) {
  if (active_ip4_address_ == value) {
    return;
  }
  active_ip4_address_ = value;
  emit activeIp4AddressChanged();
}

void NetworkService::setActiveFrequencyMhz(uint value) {
  if (active_frequency_mhz_ == value) {
    return;
  }
  active_frequency_mhz_ = value;
  emit activeFrequencyMhzChanged();
}

void NetworkService::setActiveLinkSpeedMbps(uint value) {
  if (active_link_speed_mbps_ == value) {
    return;
  }
  active_link_speed_mbps_ = value;
  emit activeLinkSpeedMbpsChanged();
}

void NetworkService::setDownloadSpeedText(const QString& value) {
  if (download_speed_text_ == value) {
    return;
  }
  download_speed_text_ = value;
  emit downloadSpeedTextChanged();
}

void NetworkService::setUploadSpeedText(const QString& value) {
  if (upload_speed_text_ == value) {
    return;
  }
  upload_speed_text_ = value;
  emit uploadSpeedTextChanged();
}

void NetworkService::setConnectionStatus(const QString& value) {
  if (connection_status_ == value) {
    return;
  }
  connection_status_ = value;
  emit connectionStatusChanged();
}

void NetworkService::setLastError(const QString& value) {
  if (last_error_ == value) {
    return;
  }
  last_error_ = value;
  emit lastErrorChanged();
}
