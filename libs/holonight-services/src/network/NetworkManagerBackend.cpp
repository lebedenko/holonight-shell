#include "NetworkManagerBackend.h"

#include <QAbstractSocket>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDateTime>
#include <QFile>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QNetworkInterface>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcNetworkBackend, "holonight.network.backend")

namespace {
constexpr auto kNmService = "org.freedesktop.NetworkManager";
constexpr auto kNmPath = "/org/freedesktop/NetworkManager";
constexpr auto kNmIface = "org.freedesktop.NetworkManager";
constexpr auto kDeviceIface = "org.freedesktop.NetworkManager.Device";
constexpr auto kWirelessIface = "org.freedesktop.NetworkManager.Device.Wireless";
constexpr auto kWiredIface = "org.freedesktop.NetworkManager.Device.Wired";
constexpr auto kApIface = "org.freedesktop.NetworkManager.AccessPoint";
constexpr auto kActiveIface = "org.freedesktop.NetworkManager.Connection.Active";
constexpr auto kSettingsIface = "org.freedesktop.NetworkManager.Settings";
constexpr auto kSettingsConnectionIface = "org.freedesktop.NetworkManager.Settings.Connection";
constexpr auto kIp4ConfigIface = "org.freedesktop.NetworkManager.IP4Config";
constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";
constexpr auto kSettingsPath = "/org/freedesktop/NetworkManager/Settings";
constexpr uint kNmDeviceTypeWifi = 2;
constexpr uint kNmDeviceTypeEthernet = 1;
constexpr uint kNmStateConnectedGlobal = 70;
constexpr uint kApFlagsPrivacy = 0x1;
using ConnectionSettings = QMap<QString, QVariantMap>;

QString decodeSsid(const QVariant& value) {
  QByteArray bytes = value.toByteArray();
  if (bytes.isEmpty() && value.canConvert<QDBusArgument>()) {
    const auto argument = value.value<QDBusArgument>();
    argument.beginArray();
    while (!argument.atEnd()) {
      uchar byte = 0;
      argument >> byte;
      bytes.append(static_cast<char>(byte));
    }
    argument.endArray();
  }
  QString decoded = QString::fromUtf8(bytes);
  if (decoded.contains(QChar::ReplacementCharacter)) {
    decoded.clear();
  }
  return decoded;
}

QList<QDBusObjectPath> parseObjectPathList(const QVariant& value) {
  if (value.canConvert<QList<QDBusObjectPath>>()) {
    return value.value<QList<QDBusObjectPath>>();
  }
  if (!value.canConvert<QDBusArgument>()) {
    return {};
  }

  QList<QDBusObjectPath> paths;
  const auto argument = value.value<QDBusArgument>();
  argument.beginArray();
  while (!argument.atEnd()) {
    QDBusObjectPath path;
    argument >> path;
    paths.append(path);
  }
  argument.endArray();
  return paths;
}

ConnectionSettings makeConnectionSettings(const WifiNetwork& network, const QString& password) {
  QVariantMap connection;
  connection.insert(QStringLiteral("id"), network.ssid);
  connection.insert(QStringLiteral("type"), QStringLiteral("802-11-wireless"));
  connection.insert(QStringLiteral("autoconnect"), true);

  QVariantMap wireless;
  wireless.insert(QStringLiteral("ssid"), network.ssid.toUtf8());
  wireless.insert(QStringLiteral("mode"), QStringLiteral("infrastructure"));

  QVariantMap ipv4;
  ipv4.insert(QStringLiteral("method"), QStringLiteral("auto"));
  QVariantMap ipv6;
  ipv6.insert(QStringLiteral("method"), QStringLiteral("auto"));

  ConnectionSettings settings;
  settings.insert(QStringLiteral("connection"), connection);
  settings.insert(QStringLiteral("802-11-wireless"), wireless);
  settings.insert(QStringLiteral("ipv4"), ipv4);
  settings.insert(QStringLiteral("ipv6"), ipv6);

  if (!password.isEmpty()) {
    QVariantMap security;
    security.insert(QStringLiteral("key-mgmt"), QStringLiteral("wpa-psk"));
    security.insert(QStringLiteral("psk"), password);
    settings.insert(QStringLiteral("802-11-wireless-security"), security);
  }

  return settings;
}

QString operationErrorMessage(const QString& context, const QString& dbus_detail) {
  return dbus_detail.isEmpty() ? context : context + QStringLiteral(": ") + dbus_detail;
}

}  // namespace

namespace {
QDBusConnection s_custom_connection{QString()};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
bool s_has_custom = false;                       // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace

QDBusConnection QtNetworkManagerBackend::dbusConnection() {
  return s_has_custom ? s_custom_connection : QDBusConnection::systemBus();
}

void QtNetworkManagerBackend::setDbusConnection(const QDBusConnection& connection) {
  s_custom_connection = connection;
  s_has_custom = true;
}

void QtNetworkManagerBackend::resetDbusConnection() {
  s_has_custom = false;
  s_custom_connection = QDBusConnection(QString());
}

QtNetworkManagerBackend::QtNetworkManagerBackend(QObject* parent)
    : NetworkManagerBackend(parent),
      poll_timer_(new QTimer(this)),
      watcher_(new QFutureWatcher<std::pair<NetworkBackendState, NetworkStats>>(this)) {
  qDBusRegisterMetaType<ConnectionSettings>();
  poll_timer_->setInterval(2500);
  connect(poll_timer_, &QTimer::timeout, this, &QtNetworkManagerBackend::onPollTimer);
  connect(watcher_, &QFutureWatcher<std::pair<NetworkBackendState, NetworkStats>>::finished, this,
          &QtNetworkManagerBackend::onQueryFinished);
}

QtNetworkManagerBackend::~QtNetworkManagerBackend() {
  shutting_down_ = true;
  if (poll_timer_ != nullptr) {
    poll_timer_->stop();
  }

  if (watcher_ != nullptr) {
    disconnect(watcher_, nullptr, this, nullptr);
    watcher_->cancel();
    watcher_->future().waitForFinished();
  }

  while (!operation_watchers_.isEmpty()) {
    QFutureWatcher<void>* operation_watcher = operation_watchers_.takeLast();
    disconnect(operation_watcher, nullptr, this, nullptr);
    operation_watcher->cancel();
    operation_watcher->future().waitForFinished();
    delete operation_watcher;
  }
}

void QtNetworkManagerBackend::start() {
  QDBusConnection bus = dbusConnection();
  if (!bus.isConnected()) {
    emit stateChanged(NetworkBackendState{.connection_status = QStringLiteral("System D-Bus unavailable")});
    poll_timer_->start();
    return;
  }

  auto* bus_iface = bus.interface();
  const QDBusReply<bool> registered =
      bus_iface != nullptr ? bus_iface->isServiceRegistered(kNmService) : QDBusReply<bool>();
  if (!registered.isValid() || !registered.value()) {
    emit stateChanged(NetworkBackendState{.connection_status = QStringLiteral("NetworkManager unavailable")});
    poll_timer_->start();
    return;
  }

  bus.connect(kNmService, kNmPath, kNmIface, QStringLiteral("StateChanged"), this, SLOT(onNmStateChanged(uint)));
  refresh();
  poll_timer_->start();
}

void QtNetworkManagerBackend::refresh() {
  if (shutting_down_) {
    return;
  }
  if (query_in_progress_) {
    query_queued_ = true;
    return;
  }
  query_in_progress_ = true;
  query_queued_ = false;

  NetworkStats stats{
      .rx_bytes = previous_rx_bytes_, .tx_bytes = previous_tx_bytes_, .stat_time_ms = previous_stat_time_ms_};
  watcher_->setFuture(QtConcurrent::run([stats]() { return doQueryState(stats); }));
}

void QtNetworkManagerBackend::requestScan() {
  trackOperation(QtConcurrent::run([this]() {
    const QList<QString> devices = wirelessDevicePaths();
    if (devices.isEmpty()) {
      QMetaObject::invokeMethod(
          this, [this]() { emitError(QStringLiteral("No Wi-Fi device found")); }, Qt::QueuedConnection);
      return;
    }

    bool requested = false;
    QString error_detail;
    for (const QString& device : devices) {
      QDBusInterface iface(kNmService, device, kWirelessIface, dbusConnection());
      const QDBusReply<void> reply = iface.call(QStringLiteral("RequestScan"), QVariantMap{});
      if (reply.isValid()) {
        requested = true;
      } else if (!reply.error().message().isEmpty()) {
        error_detail = reply.error().message();
      }
    }
    if (!requested) {
      const QString message = operationErrorMessage(QStringLiteral("Wi-Fi scan request failed"), error_detail);
      QMetaObject::invokeMethod(this, [this, message]() { emitError(message); }, Qt::QueuedConnection);
    }
    QMetaObject::invokeMethod(this, &QtNetworkManagerBackend::refresh, Qt::QueuedConnection);
  }));
}

void QtNetworkManagerBackend::setWirelessEnabled(bool enabled) {
  trackOperation(QtConcurrent::run([this, enabled]() {
    QDBusInterface props(kNmService, kNmPath, kPropsIface, dbusConnection());
    const QDBusReply<void> reply =
        props.call(QStringLiteral("Set"), QLatin1String(kNmIface), QStringLiteral("WirelessEnabled"),
                   QVariant::fromValue(QDBusVariant(QVariant(enabled))));
    if (!reply.isValid()) {
      const QString message =
          operationErrorMessage(QStringLiteral("Could not change Wi-Fi radio state"), reply.error().message());
      QMetaObject::invokeMethod(this, [this, message]() { emitError(message); }, Qt::QueuedConnection);
    }
    QMetaObject::invokeMethod(this, &QtNetworkManagerBackend::refresh, Qt::QueuedConnection);
  }));
}

void QtNetworkManagerBackend::activateKnown(const WifiNetwork& network) {
  trackOperation(QtConcurrent::run([this, network]() {
    QDBusInterface network_manager(kNmService, kNmPath, kNmIface, dbusConnection());
    const QDBusReply<QDBusObjectPath> reply =
        network_manager.call(QStringLiteral("ActivateConnection"), QDBusObjectPath(network.connection_path),
                             QDBusObjectPath(network.device_path), QDBusObjectPath(network.access_point_path));
    if (!reply.isValid()) {
      const QString message =
          operationErrorMessage(QStringLiteral("Could not activate saved Wi-Fi network"), reply.error().message());
      QMetaObject::invokeMethod(this, [this, message]() { emitError(message); }, Qt::QueuedConnection);
    }
    QMetaObject::invokeMethod(this, &QtNetworkManagerBackend::refresh, Qt::QueuedConnection);
  }));
}

void QtNetworkManagerBackend::activateOpen(const WifiNetwork& network) { addAndActivate(network, {}); }

void QtNetworkManagerBackend::activateWithPassword(const WifiNetwork& network, const QString& password) {
  if (password.isEmpty()) {
    emitError(QStringLiteral("Wi-Fi password is required"));
    return;
  }
  addAndActivate(network, password);
}

void QtNetworkManagerBackend::disconnectActive(const QString& active_connection_path) {
  if (active_connection_path.isEmpty() || active_connection_path == QLatin1String("/")) {
    emitError(QStringLiteral("No active network connection to disconnect"));
    return;
  }
  trackOperation(QtConcurrent::run([this, active_connection_path]() {
    QDBusInterface network_manager(kNmService, kNmPath, kNmIface, dbusConnection());
    const QDBusReply<void> reply =
        network_manager.call(QStringLiteral("DeactivateConnection"), QDBusObjectPath(active_connection_path));
    if (!reply.isValid()) {
      const QString message =
          operationErrorMessage(QStringLiteral("Could not disconnect active network"), reply.error().message());
      // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
      QMetaObject::invokeMethod(this, [this, message]() { emitError(message); }, Qt::QueuedConnection);
    }
    QMetaObject::invokeMethod(this, &QtNetworkManagerBackend::refresh, Qt::QueuedConnection);
  }));
}

void QtNetworkManagerBackend::onPollTimer() { refresh(); }

void QtNetworkManagerBackend::onNmStateChanged(uint /*state*/) { refresh(); }

void QtNetworkManagerBackend::onQueryFinished() {
  if (shutting_down_) {
    return;
  }
  query_in_progress_ = false;

  const auto result = watcher_->result();
  previous_rx_bytes_ = result.second.rx_bytes;
  previous_tx_bytes_ = result.second.tx_bytes;
  previous_stat_time_ms_ = result.second.stat_time_ms;

  emit stateChanged(result.first);

  if (query_queued_) {
    refresh();
  }
}

void QtNetworkManagerBackend::emitError(const QString& message) {
  qCWarning(lcNetworkBackend) << message;
  emit operationError(message);
}

std::pair<NetworkBackendState, QtNetworkManagerBackend::NetworkStats> QtNetworkManagerBackend::doQueryState(
    NetworkStats stats) {
  NetworkBackendState state;
  QDBusConnection bus = dbusConnection();
  state.available =
      bus.isConnected() && bus.interface() != nullptr && bus.interface()->isServiceRegistered(kNmService).value();
  if (!state.available) {
    state.connection_status = QStringLiteral("NetworkManager unavailable");
    return {state, stats};
  }

  state.wifi_enabled = property(kNmPath, kNmIface, QStringLiteral("WirelessEnabled")).toBool();
  state.wifi_hardware_enabled = property(kNmPath, kNmIface, QStringLiteral("WirelessHardwareEnabled")).toBool();
  const uint nm_state = property(kNmPath, kNmIface, QStringLiteral("State")).toUInt();
  state.connection_status =
      nm_state >= kNmStateConnectedGlobal ? QStringLiteral("Connected") : QStringLiteral("Disconnected");

  updateActiveConnectionState(&state, &stats);
  updateVisibleWifiNetworks(&state);
  return {state, stats};
}

void QtNetworkManagerBackend::updateActiveConnectionState(NetworkBackendState* state, NetworkStats* stats) {
  if (state == nullptr) {
    return;
  }

  const QString primary_connection =
      property(kNmPath, kNmIface, QStringLiteral("PrimaryConnection")).value<QDBusObjectPath>().path();
  state->active_connection_path = primary_connection;
  if (primary_connection.isEmpty() || primary_connection == QLatin1String("/")) {
    return;
  }

  state->active_connection_name = activeConnectionId(primary_connection);
  state->active_ip4_address = activeConnectionIp4(primary_connection);
  const QList<QDBusObjectPath> devices =
      objectPathListProperty(primary_connection, kActiveIface, QStringLiteral("Devices"));
  const QString active_access_point =
      property(primary_connection, kActiveIface, QStringLiteral("SpecificObject")).value<QDBusObjectPath>().path();
  if (!active_access_point.isEmpty() && active_access_point != QLatin1String("/")) {
    state->active_frequency_mhz = property(active_access_point, kApIface, QStringLiteral("Frequency")).toUInt();
  }

  for (const QDBusObjectPath& device : devices) {
    const uint device_type = property(device.path(), kDeviceIface, QStringLiteral("DeviceType")).toUInt();
    uint speed_mbps = 0;
    if (device_type == kNmDeviceTypeWifi) {
      const uint bitrate_kbps = property(device.path(), kWirelessIface, QStringLiteral("Bitrate")).toUInt();
      speed_mbps = static_cast<uint>(std::lround(static_cast<double>(bitrate_kbps) / 1000.0));
    } else if (device_type == kNmDeviceTypeEthernet) {
      speed_mbps = property(device.path(), kWiredIface, QStringLiteral("Speed")).toUInt();
    }
    state->active_link_speed_mbps = std::max(state->active_link_speed_mbps, speed_mbps);
  }

  updateDeviceStatistics(state, devices, stats);
}

void QtNetworkManagerBackend::updateVisibleWifiNetworks(NetworkBackendState* state) {
  if (state == nullptr) {
    return;
  }

  const QList<QString> devices = wirelessDevicePaths();
  if (devices.isEmpty()) {
    return;
  }

  const QHash<QString, QString> saved = savedWifiConnections();
  QHash<QString, WifiNetwork> merged;
  const QString active_access_point =
      property(state->active_connection_path, kActiveIface, QStringLiteral("SpecificObject"))
          .value<QDBusObjectPath>()
          .path();

  for (const QString& device : devices) {
    for (const QDBusObjectPath& access_point :
         objectPathListProperty(device, kWirelessIface, QStringLiteral("AccessPoints"))) {
      WifiNetwork network =
          networkFromAccessPoint(device, access_point, saved, active_access_point, state->active_connection_name);
      if (network.ssid.isEmpty()) {
        continue;
      }

      const QString key =
          network.ssid + QLatin1Char('|') + (network.secured ? QStringLiteral("secured") : QStringLiteral("open"));
      if (!merged.contains(key) || merged.value(key).strength < network.strength) {
        merged.insert(key, network);
      }
    }
  }

  state->networks = merged.values();
}

WifiNetwork QtNetworkManagerBackend::networkFromAccessPoint(const QString& device, const QDBusObjectPath& access_point,
                                                            const QHash<QString, QString>& saved,
                                                            const QString& active_access_point,
                                                            const QString& active_connection_name) {
  const QVariantMap props = allProperties(access_point.path(), kApIface);
  WifiNetwork network;
  network.ssid = decodeSsid(props.value(QStringLiteral("Ssid")));
  if (network.ssid.isEmpty()) {
    return network;
  }

  network.strength = std::clamp(static_cast<int>(props.value(QStringLiteral("Strength")).toUInt()), 0, 100);
  network.secured = (props.value(QStringLiteral("Flags")).toUInt() & kApFlagsPrivacy) != 0 ||
                    props.value(QStringLiteral("WpaFlags")).toUInt() != 0 ||
                    props.value(QStringLiteral("RsnFlags")).toUInt() != 0;
  network.known = saved.contains(network.ssid);
  network.connected = access_point.path() == active_access_point ||
                      (!active_connection_name.isEmpty() && active_connection_name == network.ssid);
  network.active = network.connected;
  network.access_point_path = access_point.path();
  network.device_path = device;
  network.connection_path = saved.value(network.ssid);
  network.frequency = props.value(QStringLiteral("Frequency")).toUInt();
  if (network.connected) {
    network.status_text = QStringLiteral("Connected");
  } else if (network.known) {
    network.status_text = QStringLiteral("Known");
  } else if (network.secured) {
    network.status_text = QStringLiteral("Secured");
  } else {
    network.status_text = QStringLiteral("Open");
  }
  return network;
}

QList<QString> QtNetworkManagerBackend::wirelessDevicePaths() {
  QList<QString> paths;
  QDBusInterface network_manager(kNmService, kNmPath, kNmIface, dbusConnection());
  const QDBusReply<QList<QDBusObjectPath>> reply = network_manager.call(QStringLiteral("GetDevices"));
  if (!reply.isValid()) {
    return paths;
  }
  for (const QDBusObjectPath& path : reply.value()) {
    if (property(path.path(), kDeviceIface, QStringLiteral("DeviceType")).toUInt() == kNmDeviceTypeWifi) {
      paths.append(path.path());
    }
  }
  return paths;
}

QList<QDBusObjectPath> QtNetworkManagerBackend::objectPathListProperty(const QString& path, const QString& interface,
                                                                       const QString& property_name) {
  return parseObjectPathList(property(path, interface, property_name));
}

QVariant QtNetworkManagerBackend::property(const QString& path, const QString& interface,
                                           const QString& property_name) {
  if (path.isEmpty() || path == QLatin1String("/")) {
    return {};
  }
  QDBusInterface props(kNmService, path, kPropsIface, dbusConnection());
  const QDBusReply<QDBusVariant> reply = props.call(QStringLiteral("Get"), interface, property_name);
  if (!reply.isValid()) {
    qWarning() << "DBUS GET PROPERTY FAILED:" << path << interface << property_name << reply.error().message();
  }
  return reply.isValid() ? reply.value().variant() : QVariant{};
}

QVariantMap QtNetworkManagerBackend::allProperties(const QString& path, const QString& interface) {
  QDBusInterface props(kNmService, path, kPropsIface, dbusConnection());
  const QDBusReply<QVariantMap> reply = props.call(QStringLiteral("GetAll"), interface);
  if (!reply.isValid()) {
    qWarning() << "DBUS GETALL PROPERTIES FAILED:" << path << interface << reply.error().message();
  }
  return reply.isValid() ? reply.value() : QVariantMap{};
}

QHash<QString, QString> QtNetworkManagerBackend::savedWifiConnections() {
  QHash<QString, QString> saved;
  QDBusInterface settings(kNmService, kSettingsPath, kSettingsIface, dbusConnection());
  const QDBusReply<QList<QDBusObjectPath>> reply = settings.call(QStringLiteral("ListConnections"));
  if (!reply.isValid()) {
    return saved;
  }

  for (const QDBusObjectPath& path : reply.value()) {
    QDBusInterface connection(kNmService, path.path(), kSettingsConnectionIface, dbusConnection());
    const QDBusReply<ConnectionSettings> settings_reply = connection.call(QStringLiteral("GetSettings"));
    if (!settings_reply.isValid()) {
      continue;
    }
    const QVariantMap connection_map = settings_reply.value().value(QStringLiteral("connection"));
    if (connection_map.value(QStringLiteral("type")).toString() != QLatin1String("802-11-wireless")) {
      continue;
    }
    const QVariantMap wifi = settings_reply.value().value(QStringLiteral("802-11-wireless"));
    QString ssid = decodeSsid(wifi.value(QStringLiteral("ssid")));
    if (ssid.isEmpty()) {
      ssid = connection_map.value(QStringLiteral("id")).toString();
    }
    if (!ssid.isEmpty()) {
      saved.insert(ssid, path.path());
    }
  }
  return saved;
}

QString QtNetworkManagerBackend::connectionId(const QString& settings_path) {
  QDBusInterface connection(kNmService, settings_path, kSettingsConnectionIface, dbusConnection());
  const QDBusMessage reply = connection.call(QStringLiteral("GetSettings"));
  if (reply.type() == QDBusMessage::ErrorMessage) {
    return {};
  }
  if (reply.arguments().isEmpty()) {
    return {};
  }

  const QVariant arg = reply.arguments().first();
  QVariant connection_value;

  if (arg.metaType() == QMetaType::fromType<QVariantMap>()) {
    connection_value = arg.toMap().value(QStringLiteral("connection"));
  } else if (arg.userType() == qMetaTypeId<QDBusArgument>()) {
    const auto dbus_arg = arg.value<QDBusArgument>();
    if (dbus_arg.currentType() == QDBusArgument::MapType) {
      QMap<QString, QVariantMap> map;
      dbus_arg >> map;
      connection_value = map.value(QStringLiteral("connection"));
    }
  }

  QVariantMap connection_map;
  if (connection_value.metaType() == QMetaType::fromType<QVariantMap>()) {
    connection_map = connection_value.toMap();
  } else if (connection_value.userType() == qMetaTypeId<QDBusArgument>()) {
    const auto dbus_arg = connection_value.value<QDBusArgument>();
    dbus_arg >> connection_map;
  }

  return connection_map.value(QStringLiteral("id")).toString();
}

QString QtNetworkManagerBackend::activeConnectionId(const QString& active_path) {
  const QString settings_path =
      property(active_path, kActiveIface, QStringLiteral("Connection")).value<QDBusObjectPath>().path();
  return connectionId(settings_path);
}

static void extractAddressesFromVariantList(const QVariantList& list, QList<QVariantMap>& addresses) {
  for (const QVariant& item : list) {
    if (item.canConvert<QVariantMap>()) {
      addresses.append(item.toMap());
    } else if (item.canConvert<QDBusArgument>()) {
      auto dbus_arg = item.value<QDBusArgument>();
      QVariantMap map;
      dbus_arg >> map;
      addresses.append(map);
    }
  }
}

static void extractAddressesFromDbusArg(const QDBusArgument& dbus_arg, QList<QVariantMap>& addresses) {
  QDBusArgument arg_copy1 = dbus_arg;  // NOLINT(performance-unnecessary-copy-initialization)
  QList<QVariantMap> direct_maps;
  arg_copy1 >> direct_maps;
  for (const auto& map : direct_maps) {
    if (map.contains(QStringLiteral("address"))) {
      addresses.append(map);
    }
  }
  if (!addresses.isEmpty()) {
    return;
  }
  QDBusArgument arg_copy2 = dbus_arg;  // NOLINT(performance-unnecessary-copy-initialization)
  QVariantList list;
  arg_copy2 >> list;
  for (const QVariant& item : list) {
    if (item.canConvert<QVariantMap>()) {
      const QVariantMap map = item.toMap();
      if (map.contains(QStringLiteral("address"))) {
        addresses.append(map);
      }
    } else if (item.canConvert<QDBusArgument>()) {
      auto item_arg = item.value<QDBusArgument>();
      QVariantMap map;
      item_arg >> map;
      if (map.contains(QStringLiteral("address"))) {
        addresses.append(map);
      }
    }
  }
}

QString QtNetworkManagerBackend::activeConnectionIp4(const QString& active_path) {
  const QString ip4_path =
      property(active_path, kActiveIface, QStringLiteral("Ip4Config")).value<QDBusObjectPath>().path();
  const QVariantMap props = allProperties(ip4_path, kIp4ConfigIface);

  QList<QVariantMap> addresses;
  const QVariant address_data_var = props.value(QStringLiteral("AddressData"));
  if (address_data_var.canConvert<QVariantList>()) {
    extractAddressesFromVariantList(address_data_var.toList(), addresses);
  } else if (address_data_var.canConvert<QList<QVariantMap>>()) {
    addresses = address_data_var.value<QList<QVariantMap>>();
  } else if (address_data_var.canConvert<QDBusArgument>()) {
    extractAddressesFromDbusArg(address_data_var.value<QDBusArgument>(), addresses);
  }

  if (!addresses.isEmpty()) {
    return addresses.first().value(QStringLiteral("address")).toString();
  }
  for (const QDBusObjectPath& device : objectPathListProperty(active_path, kActiveIface, QStringLiteral("Devices"))) {
    const QString address = deviceIp4Address(device.path());
    if (!address.isEmpty()) {
      return address;
    }
  }
  return {};
}

QString QtNetworkManagerBackend::deviceIp4Address(const QString& device_path) {
  const QString interface_name = property(device_path, kDeviceIface, QStringLiteral("IpInterface")).toString();
  if (interface_name.isEmpty()) {
    return {};
  }
  const QNetworkInterface iface = QNetworkInterface::interfaceFromName(interface_name);
  for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
    const QHostAddress address = entry.ip();
    if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
      return address.toString();
    }
  }
  return {};
}

std::optional<quint64> QtNetworkManagerBackend::interfaceCounter(const QString& interface_name,
                                                                 const QString& counter) {
  if (interface_name.isEmpty() || counter.isEmpty()) {
    return std::nullopt;
  }

  QFile file(QStringLiteral("/sys/class/net/%1/statistics/%2").arg(interface_name, counter));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return std::nullopt;
  }

  bool converted = false;
  const quint64 value = QString::fromLatin1(file.readAll()).trimmed().toULongLong(&converted);
  return converted ? std::optional<quint64>{value} : std::nullopt;
}

QString QtNetworkManagerBackend::speedText(double bits_per_second) {
  if (bits_per_second <= 0.0) {
    return QStringLiteral("0 Mbps");
  }
  const double mbps = bits_per_second / 1000000.0;
  if (mbps >= 10.0) {
    return QStringLiteral("%1 Mbps").arg(std::round(mbps), 0, 'f', 0);
  }
  if (mbps >= 1.0) {
    return QStringLiteral("%1 Mbps").arg(mbps, 0, 'f', 1);
  }
  return QStringLiteral("%1 Kbps").arg(std::round(bits_per_second / 1000.0), 0, 'f', 0);
}

void QtNetworkManagerBackend::updateDeviceStatistics(NetworkBackendState* state, const QList<QDBusObjectPath>& devices,
                                                     NetworkStats* stats) {
  if (state == nullptr || stats == nullptr) {
    return;
  }

  const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
  double download_bits = 0.0;
  double upload_bits = 0.0;

  for (const QDBusObjectPath& device : devices) {
    const QString path = device.path();
    const QString interface_name = property(path, kDeviceIface, QStringLiteral("IpInterface")).toString();
    const std::optional<quint64> receive_counter = interfaceCounter(interface_name, QStringLiteral("rx_bytes"));
    const std::optional<quint64> transmit_counter = interfaceCounter(interface_name, QStringLiteral("tx_bytes"));
    if (!receive_counter.has_value() || !transmit_counter.has_value()) {
      continue;
    }
    const quint64 receive_bytes = *receive_counter;
    const quint64 transmit_bytes = *transmit_counter;
    const qint64 previous_ms = stats->stat_time_ms.value(path, 0);
    const quint64 previous_receive_bytes = stats->rx_bytes.value(path, receive_bytes);
    const quint64 previous_transmit_bytes = stats->tx_bytes.value(path, transmit_bytes);

    if (previous_ms > 0 && now_ms > previous_ms) {
      const double seconds = static_cast<double>(now_ms - previous_ms) / 1000.0;
      if (receive_bytes >= previous_receive_bytes) {
        download_bits += static_cast<double>(receive_bytes - previous_receive_bytes) * 8.0 / seconds;
      }
      if (transmit_bytes >= previous_transmit_bytes) {
        upload_bits += static_cast<double>(transmit_bytes - previous_transmit_bytes) * 8.0 / seconds;
      }
    }

    stats->rx_bytes.insert(path, receive_bytes);
    stats->tx_bytes.insert(path, transmit_bytes);
    stats->stat_time_ms.insert(path, now_ms);
  }

  state->download_speed_text = speedText(download_bits);
  state->upload_speed_text = speedText(upload_bits);
}

bool QtNetworkManagerBackend::callNoReply(const QString& path, const QString& interface, const QString& method,
                                          const QVariantList& args) {
  QDBusInterface iface(kNmService, path, interface, dbusConnection());
  const QDBusMessage reply = iface.callWithArgumentList(QDBus::Block, method, args);
  return reply.type() != QDBusMessage::ErrorMessage;
}

void QtNetworkManagerBackend::trackOperation(const QFuture<void>& future) {
  auto* operation_watcher = new QFutureWatcher<void>(this);
  operation_watchers_.append(operation_watcher);
  connect(operation_watcher, &QFutureWatcher<void>::finished, this, [this, operation_watcher] {
    operation_watchers_.removeOne(operation_watcher);
    operation_watcher->deleteLater();
  });
  operation_watcher->setFuture(future);
}

void QtNetworkManagerBackend::addAndActivate(const WifiNetwork& network, const QString& password) {
  trackOperation(QtConcurrent::run([this, network, password]() {
    QDBusInterface network_manager(kNmService, kNmPath, kNmIface, dbusConnection());
    QVariantMap options;
    options.insert(QStringLiteral("persist"), QStringLiteral("disk"));
    const ConnectionSettings settings = makeConnectionSettings(network, password);
    const QDBusMessage reply = network_manager.callWithArgumentList(
        QDBus::Block, QStringLiteral("AddAndActivateConnection2"),
        {QVariant::fromValue(settings), QVariant::fromValue(QDBusObjectPath(network.device_path)),
         QVariant::fromValue(QDBusObjectPath(network.access_point_path)), QVariant::fromValue(options)});
    if (reply.type() == QDBusMessage::ErrorMessage) {
      const QString message =
          operationErrorMessage(QStringLiteral("Could not add and activate Wi-Fi network"), reply.errorMessage());
      QMetaObject::invokeMethod(this, [this, message]() { emitError(message); }, Qt::QueuedConnection);
    }
    QMetaObject::invokeMethod(this, &QtNetworkManagerBackend::refresh, Qt::QueuedConnection);
  }));
}
