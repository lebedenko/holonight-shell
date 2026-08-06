#include "NetworkManagerBackend.h"
#include "NetworkService.h"
#include "WifiNetworkModel.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QThread>

#include <gtest/gtest.h>
#include <optional>

class FakeNetworkDbusClient final : public DbusPropertyClient {
 public:
  [[nodiscard]] bool systemBusConnected() const override { return system_bus_connected; }
  [[nodiscard]] bool serviceRegistered(const QString& service) const override {
    return registered_services.contains(service);
  }

  [[nodiscard]] std::optional<QVariant> property(const QString& /*service*/, const QString& path,
                                                 const QString& /*interface*/, const QString& name) const override {
    const auto path_it = properties.constFind(path);
    if (path_it == properties.constEnd()) {
      return std::nullopt;
    }
    const auto value_it = path_it->constFind(name);
    if (value_it == path_it->constEnd()) {
      return std::nullopt;
    }
    return *value_it;
  }

  [[nodiscard]] std::optional<QVariantMap> allProperties(const QString& /*service*/, const QString& path,
                                                         const QString& /*interface*/) const override {
    const auto property_it = properties.constFind(path);
    if (property_it == properties.constEnd()) {
      return std::nullopt;
    }
    return *property_it;
  }

  [[nodiscard]] std::optional<QList<QDBusObjectPath>> upowerDevices() const override { return std::nullopt; }

  bool connectSignal(const QString& /*service*/, const QString& path, const QString& /*interface*/,
                     const QString& signal, QObject* /*receiver*/, const char* /*slot*/) const override {
    const QString entry = path + QStringLiteral(":") + signal;
    connected_signals.append(entry);
    return true;
  }

  bool disconnectSignal(const QString& /*service*/, const QString& path, const QString& /*interface*/,
                        const QString& signal, QObject* /*receiver*/, const char* /*slot*/) const override {
    const QString entry = path + QStringLiteral(":") + signal;
    disconnected_signals.append(entry);
    return true;
  }

  QSet<QString> registered_services{QStringLiteral("org.freedesktop.NetworkManager")};
  QHash<QString, QVariantMap> properties;
  bool system_bus_connected{true};
  mutable QStringList connected_signals;
  mutable QStringList disconnected_signals;
};

class FakeNetworkManagerBackend final : public NetworkManagerBackend {
 public:
  explicit FakeNetworkManagerBackend(QObject* parent = nullptr) : NetworkManagerBackend(parent) {}

  void start() override {
    start_count++;
    emit stateChanged(state);
  }
  void refresh() override {
    refresh_count++;
    emit stateChanged(state);
  }
  void requestScan() override {
    scan_count++;
    state.scanning = true;
    emit stateChanged(state);
  }
  void setWirelessEnabled(bool enabled) override {
    set_wifi_count++;
    last_wifi_enabled = enabled;
    state.wifi_enabled = enabled;
    emit stateChanged(state);
  }
  void activateKnown(const WifiNetwork& network) override {
    known_count++;
    last_ssid = network.ssid;
  }
  void activateOpen(const WifiNetwork& network) override {
    open_count++;
    last_ssid = network.ssid;
  }
  void activateWithPassword(const WifiNetwork& network, const QString& password) override {
    password_count++;
    last_ssid = network.ssid;
    last_password = password;
  }
  void disconnectActive(const QString& active_connection_path) override {
    disconnect_count++;
    last_active_path = active_connection_path;
  }

  NetworkBackendState state;
  int start_count{0};
  int refresh_count{0};
  int scan_count{0};
  int set_wifi_count{0};
  int known_count{0};
  int open_count{0};
  int password_count{0};
  int disconnect_count{0};
  bool last_wifi_enabled{false};
  QString last_ssid;
  QString last_password;
  QString last_active_path;
};

TEST(NetworkService, DetectsVpnConnectionTypes) {
  EXPECT_TRUE(isVpnConnectionType(QStringLiteral("vpn")));
  EXPECT_TRUE(isVpnConnectionType(QStringLiteral("wireguard")));
  EXPECT_TRUE(isVpnConnectionType(QStringLiteral("tun")));
  EXPECT_FALSE(isVpnConnectionType(QStringLiteral("802-3-ethernet")));
  EXPECT_FALSE(isVpnConnectionType(QStringLiteral("802-11-wireless")));
  EXPECT_FALSE(isVpnConnectionType(QString()));
}

TEST(NetworkService, ExtractsActiveConnectionPathsFromDirectVariant) {
  const QList<QDBusObjectPath> paths = {
      QDBusObjectPath(QStringLiteral("/org/freedesktop/NetworkManager/ActiveConnection/1")),
      QDBusObjectPath(QStringLiteral("/org/freedesktop/NetworkManager/ActiveConnection/2")),
  };

  const QList<QDBusObjectPath> parsed = activeConnectionPaths(QVariant::fromValue(paths));

  ASSERT_EQ(parsed.size(), 2);
  EXPECT_EQ(parsed.at(0).path(), paths.at(0).path());
  EXPECT_EQ(parsed.at(1).path(), paths.at(1).path());
}

TEST(NetworkService, ReturnsEmptyActiveConnectionPathsForInvalidVariant) {
  EXPECT_TRUE(activeConnectionPaths(QVariant::fromValue(QStringLiteral("not paths"))).isEmpty());
  EXPECT_TRUE(activeConnectionPaths(QVariant()).isEmpty());
}

TEST(NetworkService, AccessPointPropertiesChangedFiltersInterfaceAndClampsStrength) {
  NetworkService service(std::make_unique<FakeNetworkDbusClient>());
  QSignalSpy strength_changed(&service, &NetworkService::strengthChanged);

  QVariantMap changed;
  changed.insert(QStringLiteral("Strength"), 150U);

  EXPECT_TRUE(QMetaObject::invokeMethod(&service, "onApPropertiesChanged", Qt::DirectConnection,
                                        Q_ARG(QString, QStringLiteral("org.example.Other")),
                                        Q_ARG(QVariantMap, changed), Q_ARG(QStringList, QStringList())));
  EXPECT_EQ(service.strength(), 0);
  EXPECT_EQ(strength_changed.count(), 0);

  EXPECT_TRUE(QMetaObject::invokeMethod(&service, "onApPropertiesChanged", Qt::DirectConnection,
                                        Q_ARG(QString, QStringLiteral("org.freedesktop.NetworkManager.AccessPoint")),
                                        Q_ARG(QVariantMap, changed), Q_ARG(QStringList, QStringList())));
  EXPECT_EQ(service.strength(), 100);
  EXPECT_EQ(strength_changed.count(), 1);

  changed.insert(QStringLiteral("Strength"), 40U);
  EXPECT_TRUE(QMetaObject::invokeMethod(&service, "onApPropertiesChanged", Qt::DirectConnection,
                                        Q_ARG(QString, QStringLiteral("org.freedesktop.NetworkManager.AccessPoint")),
                                        Q_ARG(QVariantMap, changed), Q_ARG(QStringList, QStringList())));
  EXPECT_EQ(service.strength(), 40);
  EXPECT_EQ(strength_changed.count(), 2);
}

TEST(NetworkService, StartupFailureRetriesWhenNetworkManagerAppears) {
  auto dbus = std::make_unique<FakeNetworkDbusClient>();
  auto* dbus_ptr = dbus.get();
  dbus->system_bus_connected = false;
  NetworkService service(std::move(dbus));
  QSignalSpy available_changed(&service, &NetworkService::availableChanged);

  service.start();
  EXPECT_FALSE(service.available());
  EXPECT_TRUE(dbus_ptr->connected_signals.isEmpty());

  dbus_ptr->system_bus_connected = true;
  EXPECT_TRUE(QMetaObject::invokeMethod(&service, "onPollTimer", Qt::DirectConnection));

  EXPECT_TRUE(service.available());
  EXPECT_EQ(available_changed.count(), 1);
  EXPECT_TRUE(dbus_ptr->connected_signals.contains(QStringLiteral("/org/freedesktop/NetworkManager:StateChanged")));
}

TEST(NetworkService, BuildsNoPrimaryConnectionStateUpdate) {
  const NetworkStateUpdate update = buildNetworkStateUpdate(NetworkQuerySnapshot{
      .online = true,
      .primary_connection_path = QStringLiteral("/"),
      .active_connection_types = QList<QString>{QStringLiteral("802-3-ethernet")},
  });

  ASSERT_TRUE(update.online.has_value());
  EXPECT_TRUE(*update.online);
  ASSERT_TRUE(update.type.has_value());
  EXPECT_EQ(*update.type, NetworkService::None);
  ASSERT_TRUE(update.ssid.has_value());
  EXPECT_TRUE(update.ssid->isEmpty());
  ASSERT_TRUE(update.strength.has_value());
  EXPECT_EQ(*update.strength, 0);
  ASSERT_TRUE(update.access_point_path.has_value());
  EXPECT_TRUE(update.access_point_path->isEmpty());
  ASSERT_TRUE(update.vpn_active.has_value());
  EXPECT_FALSE(*update.vpn_active);
}

TEST(NetworkService, BuildsWiredConnectionStateUpdateWithVpn) {
  const NetworkStateUpdate update = buildNetworkStateUpdate(NetworkQuerySnapshot{
      .online = true,
      .primary_connection_path = QStringLiteral("/active/wired"),
      .connection_type = QStringLiteral("802-3-ethernet"),
      .active_connection_types = QList<QString>{QStringLiteral("802-3-ethernet"), QStringLiteral("wireguard")},
  });

  ASSERT_TRUE(update.type.has_value());
  EXPECT_EQ(*update.type, NetworkService::Wired);
  ASSERT_TRUE(update.ssid.has_value());
  EXPECT_TRUE(update.ssid->isEmpty());
  ASSERT_TRUE(update.strength.has_value());
  EXPECT_EQ(*update.strength, 0);
  ASSERT_TRUE(update.access_point_path.has_value());
  EXPECT_TRUE(update.access_point_path->isEmpty());
  ASSERT_TRUE(update.vpn_active.has_value());
  EXPECT_TRUE(*update.vpn_active);
}

TEST(NetworkService, BuildsWifiConnectionStateUpdateWithAccessPointDetails) {
  QVariantMap ap_properties;
  ap_properties.insert(QStringLiteral("Ssid"), QByteArray("Cafe WiFi"));
  ap_properties.insert(QStringLiteral("Strength"), 87U);

  const NetworkStateUpdate update = buildNetworkStateUpdate(NetworkQuerySnapshot{
      .primary_connection_path = QStringLiteral("/active/wifi"),
      .connection_type = QStringLiteral("802-11-wireless"),
      .specific_object_path = QStringLiteral("/access-point/1"),
      .access_point_properties = ap_properties,
      .active_connection_types = QList<QString>{QStringLiteral("802-11-wireless")},
  });

  ASSERT_TRUE(update.type.has_value());
  EXPECT_EQ(*update.type, NetworkService::WiFi);
  ASSERT_TRUE(update.ssid.has_value());
  EXPECT_EQ(*update.ssid, QStringLiteral("Cafe WiFi"));
  ASSERT_TRUE(update.strength.has_value());
  EXPECT_EQ(*update.strength, 87);
  ASSERT_TRUE(update.access_point_path.has_value());
  EXPECT_EQ(*update.access_point_path, QStringLiteral("/access-point/1"));
  ASSERT_TRUE(update.vpn_active.has_value());
  EXPECT_FALSE(*update.vpn_active);
}

TEST(NetworkService, BuildsWifiConnectionStateUpdateForMissingAccessPoint) {
  const NetworkStateUpdate update = buildNetworkStateUpdate(NetworkQuerySnapshot{
      .primary_connection_path = QStringLiteral("/active/wifi"),
      .connection_type = QStringLiteral("802-11-wireless"),
      .specific_object_path = QStringLiteral("/"),
  });

  ASSERT_TRUE(update.type.has_value());
  EXPECT_EQ(*update.type, NetworkService::WiFi);
  ASSERT_TRUE(update.ssid.has_value());
  EXPECT_TRUE(update.ssid->isEmpty());
  ASSERT_TRUE(update.strength.has_value());
  EXPECT_EQ(*update.strength, 0);
  ASSERT_TRUE(update.access_point_path.has_value());
  EXPECT_TRUE(update.access_point_path->isEmpty());
}

TEST(NetworkService, KeepsWifiDetailsUnsetWhenAccessPointQueryFails) {
  const NetworkStateUpdate update = buildNetworkStateUpdate(NetworkQuerySnapshot{
      .primary_connection_path = QStringLiteral("/active/wifi"),
      .connection_type = QStringLiteral("802-11-wireless"),
      .specific_object_path = QStringLiteral("/access-point/1"),
  });

  ASSERT_TRUE(update.type.has_value());
  EXPECT_EQ(*update.type, NetworkService::WiFi);
  EXPECT_FALSE(update.ssid.has_value());
  EXPECT_FALSE(update.strength.has_value());
  ASSERT_TRUE(update.access_point_path.has_value());
  EXPECT_EQ(*update.access_point_path, QStringLiteral("/access-point/1"));
}

TEST(NetworkService, ClearsInvalidUtf8SsidAndClampsWifiStrength) {
  QVariantMap ap_properties;
  ap_properties.insert(QStringLiteral("Ssid"), QByteArray::fromHex("fffe"));
  ap_properties.insert(QStringLiteral("Strength"), 250U);

  const NetworkStateUpdate update = buildNetworkStateUpdate(NetworkQuerySnapshot{
      .primary_connection_path = QStringLiteral("/active/wifi"),
      .connection_type = QStringLiteral("802-11-wireless"),
      .specific_object_path = QStringLiteral("/access-point/1"),
      .access_point_properties = ap_properties,
  });

  ASSERT_TRUE(update.ssid.has_value());
  EXPECT_TRUE(update.ssid->isEmpty());
  ASSERT_TRUE(update.strength.has_value());
  EXPECT_EQ(*update.strength, 100);
}

TEST(NetworkService, BuildsUnknownConnectionTypeAsDisconnectedState) {
  const NetworkStateUpdate update = buildNetworkStateUpdate(NetworkQuerySnapshot{
      .primary_connection_path = QStringLiteral("/active/unknown"),
      .connection_type = QStringLiteral("bluetooth"),
  });

  ASSERT_TRUE(update.type.has_value());
  EXPECT_EQ(*update.type, NetworkService::None);
  ASSERT_TRUE(update.ssid.has_value());
  EXPECT_TRUE(update.ssid->isEmpty());
  ASSERT_TRUE(update.strength.has_value());
  EXPECT_EQ(*update.strength, 0);
  ASSERT_TRUE(update.access_point_path.has_value());
  EXPECT_TRUE(update.access_point_path->isEmpty());
}

TEST(WifiNetworkModel, SortsConnectedFirstAndExposesRoles) {
  WifiNetworkModel model;
  model.setNetworks({
      WifiNetwork{
          .ssid = QStringLiteral("Cafe"), .strength = 45, .secured = false, .status_text = QStringLiteral("Open")},
      WifiNetwork{.ssid = QStringLiteral("Home"),
                  .strength = 80,
                  .secured = true,
                  .known = true,
                  .connected = true,
                  .active = true,
                  .access_point_path = QStringLiteral("/ap/home"),
                  .device_path = QStringLiteral("/dev/wlan0"),
                  .connection_path = QStringLiteral("/settings/home"),
                  .frequency = 5200,
                  .status_text = QStringLiteral("Connected")},
  });

  ASSERT_EQ(model.rowCount(), 2);
  const QModelIndex first = model.index(0);
  EXPECT_EQ(model.data(first, static_cast<int>(WifiNetworkModel::Role::Ssid)).toString(), QStringLiteral("Home"));
  EXPECT_TRUE(model.data(first, static_cast<int>(WifiNetworkModel::Role::Secured)).toBool());
  EXPECT_TRUE(model.data(first, static_cast<int>(WifiNetworkModel::Role::Known)).toBool());
  EXPECT_TRUE(model.data(first, static_cast<int>(WifiNetworkModel::Role::Connected)).toBool());
  EXPECT_EQ(model.data(first, static_cast<int>(WifiNetworkModel::Role::Frequency)).toUInt(), 5200U);
  EXPECT_EQ(model.roleNames().value(static_cast<int>(WifiNetworkModel::Role::AccessPointPath)), "accessPointPath");
}

TEST(WifiNetworkModel, UpdatesStableRowsWithoutResettingModel) {
  WifiNetworkModel model;
  model.setNetworks({
      WifiNetwork{.ssid = QStringLiteral("Home"),
                  .strength = 80,
                  .secured = true,
                  .known = true,
                  .access_point_path = QStringLiteral("/ap/home"),
                  .device_path = QStringLiteral("/dev/wlan0"),
                  .connection_path = QStringLiteral("/settings/home"),
                  .status_text = QStringLiteral("Known")},
  });
  QSignalSpy reset_spy(&model, &WifiNetworkModel::modelReset);
  QSignalSpy data_changed_spy(&model, &WifiNetworkModel::dataChanged);

  model.setNetworks({
      WifiNetwork{.ssid = QStringLiteral("Home"),
                  .strength = 72,
                  .secured = true,
                  .known = true,
                  .access_point_path = QStringLiteral("/ap/home"),
                  .device_path = QStringLiteral("/dev/wlan0"),
                  .connection_path = QStringLiteral("/settings/home"),
                  .status_text = QStringLiteral("Known")},
  });

  EXPECT_EQ(reset_spy.count(), 0);
  EXPECT_EQ(data_changed_spy.count(), 1);
  EXPECT_EQ(model.data(model.index(0), static_cast<int>(WifiNetworkModel::Role::Strength)).toInt(), 72);
}

TEST(NetworkService, AppliesBackendStateToPopupPropertiesAndModel) {
  auto backend = std::make_unique<FakeNetworkManagerBackend>();
  auto* backend_ptr = backend.get();
  backend->state = NetworkBackendState{
      .available = true,
      .wifi_enabled = true,
      .wifi_hardware_enabled = true,
      .scanning = false,
      .active_connection_name = QStringLiteral("Home"),
      .active_ip4_address = QStringLiteral("192.0.2.4"),
      .active_frequency_mhz = 5180,
      .active_link_speed_mbps = 866,
      .download_speed_text = QStringLiteral("24 Mbps"),
      .upload_speed_text = QStringLiteral("3 Mbps"),
      .connection_status = QStringLiteral("Connected"),
      .active_connection_path = QStringLiteral("/active/1"),
      .networks = {WifiNetwork{.ssid = QStringLiteral("Home"), .strength = 91, .connected = true}},
  };
  NetworkService service(std::make_unique<FakeNetworkDbusClient>(), std::move(backend));

  service.start();

  EXPECT_EQ(backend_ptr->start_count, 1);
  EXPECT_TRUE(service.wifiEnabled());
  EXPECT_TRUE(service.wifiHardwareEnabled());
  EXPECT_EQ(service.activeConnectionName(), QStringLiteral("Home"));
  EXPECT_EQ(service.activeIp4Address(), QStringLiteral("192.0.2.4"));
  EXPECT_EQ(service.activeFrequencyMhz(), 5180U);
  EXPECT_EQ(service.activeLinkSpeedMbps(), 866U);
  EXPECT_EQ(service.downloadSpeedText(), QStringLiteral("24 Mbps"));
  EXPECT_EQ(service.uploadSpeedText(), QStringLiteral("3 Mbps"));
  ASSERT_NE(service.wifiNetworks(), nullptr);
  EXPECT_EQ(service.wifiNetworks()->rowCount(), 1);
}

TEST(NetworkService, GuardsAndClearsActiveDiagnosticSignals) {
  auto backend = std::make_unique<FakeNetworkManagerBackend>();
  auto* backend_ptr = backend.get();
  backend->state = NetworkBackendState{
      .available = true,
      .active_frequency_mhz = 5180,
      .active_link_speed_mbps = 866,
  };
  NetworkService service(std::make_unique<FakeNetworkDbusClient>(), std::move(backend));
  QSignalSpy frequency_changed(&service, &NetworkService::activeFrequencyMhzChanged);
  QSignalSpy link_speed_changed(&service, &NetworkService::activeLinkSpeedMbpsChanged);

  service.start();
  EXPECT_EQ(frequency_changed.count(), 1);
  EXPECT_EQ(link_speed_changed.count(), 1);

  backend_ptr->refresh();
  EXPECT_EQ(frequency_changed.count(), 1);
  EXPECT_EQ(link_speed_changed.count(), 1);

  backend_ptr->state = NetworkBackendState{.available = true};
  backend_ptr->refresh();
  EXPECT_EQ(service.activeFrequencyMhz(), 0U);
  EXPECT_EQ(service.activeLinkSpeedMbps(), 0U);
  EXPECT_EQ(frequency_changed.count(), 2);
  EXPECT_EQ(link_speed_changed.count(), 2);
}

TEST(NetworkService, RoutesPopupActionsToBackend) {
  auto backend = std::make_unique<FakeNetworkManagerBackend>();
  auto* backend_ptr = backend.get();
  backend->state = NetworkBackendState{
      .available = true,
      .wifi_enabled = true,
      .wifi_hardware_enabled = true,
      .active_connection_path = QStringLiteral("/active/1"),
      .networks =
          {
              WifiNetwork{.ssid = QStringLiteral("Known"),
                          .known = true,
                          .access_point_path = QStringLiteral("/ap/known"),
                          .device_path = QStringLiteral("/dev/wlan0"),
                          .connection_path = QStringLiteral("/settings/known")},
              WifiNetwork{.ssid = QStringLiteral("Open"),
                          .secured = false,
                          .access_point_path = QStringLiteral("/ap/open"),
                          .device_path = QStringLiteral("/dev/wlan0")},
              WifiNetwork{.ssid = QStringLiteral("Secured"),
                          .secured = true,
                          .access_point_path = QStringLiteral("/ap/secured"),
                          .device_path = QStringLiteral("/dev/wlan0")},
          },
  };
  NetworkService service(std::make_unique<FakeNetworkDbusClient>(), std::move(backend));
  service.start();

  service.rescanWifi();
  service.setWifiEnabled(false);
  service.connectNetwork(0);
  service.connectNetwork(1);
  service.connectNetwork(2);
  EXPECT_EQ(service.lastError(), QStringLiteral("Password required"));
  service.connectNetworkWithPassword(2, QStringLiteral("secret123"));
  service.disconnectActive();

  EXPECT_EQ(backend_ptr->scan_count, 1);
  EXPECT_EQ(backend_ptr->set_wifi_count, 1);
  EXPECT_FALSE(backend_ptr->last_wifi_enabled);
  EXPECT_EQ(backend_ptr->known_count, 1);
  EXPECT_EQ(backend_ptr->open_count, 1);
  EXPECT_EQ(backend_ptr->password_count, 1);
  EXPECT_EQ(backend_ptr->last_password, QStringLiteral("secret123"));
  EXPECT_EQ(backend_ptr->disconnect_count, 1);
  EXPECT_EQ(backend_ptr->last_active_path, QStringLiteral("/active/1"));
}

class FakeNmAp : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.AccessPoint")
  Q_PROPERTY(QByteArray Ssid READ ssid)
  Q_PROPERTY(uint Strength READ strength)
  Q_PROPERTY(uint Flags READ flags)
  Q_PROPERTY(uint WpaFlags READ wpaFlags)
  Q_PROPERTY(uint RsnFlags READ rsnFlags)
  Q_PROPERTY(uint Frequency READ frequency)

 public:
  [[nodiscard]] static QByteArray ssid() { return "FakeWiFi"; }
  [[nodiscard]] static uint strength() { return 85; }
  [[nodiscard]] static uint flags() { return 1; }
  [[nodiscard]] static uint wpaFlags() { return 0; }
  [[nodiscard]] static uint rsnFlags() { return 0; }
  [[nodiscard]] static uint frequency() { return 2412; }
};

class FakeNmDevice : public QObject {
  Q_OBJECT

 public:
  explicit FakeNmDevice(QObject* parent = nullptr);

  [[nodiscard]] static uint deviceType() { return 2; }
  [[nodiscard]] static QString ipInterface() { return QStringLiteral("mock_wlan0"); }
  [[nodiscard]] static QList<QDBusObjectPath> accessPoints() {
    return {QDBusObjectPath("/org/freedesktop/NetworkManager/AccessPoint/1")};
  }
};

class DeviceAdaptor : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.Device")
  Q_PROPERTY(uint DeviceType READ deviceType)
  Q_PROPERTY(QString IpInterface READ ipInterface)

 public:
  explicit DeviceAdaptor(FakeNmDevice* parent) : QDBusAbstractAdaptor(parent), device_(parent) {}

  [[nodiscard]] static uint deviceType() { return FakeNmDevice::deviceType(); }
  [[nodiscard]] static QString ipInterface() { return FakeNmDevice::ipInterface(); }

 private:
  FakeNmDevice* device_;
};

class WirelessAdaptor : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.Device.Wireless")
  Q_PROPERTY(QList<QDBusObjectPath> AccessPoints READ accessPoints)

 public:
  explicit WirelessAdaptor(FakeNmDevice* parent) : QDBusAbstractAdaptor(parent), device_(parent) {}

  [[nodiscard]] static QList<QDBusObjectPath> accessPoints() { return FakeNmDevice::accessPoints(); }

 private:
  FakeNmDevice* device_;
};

inline FakeNmDevice::FakeNmDevice(QObject* parent) : QObject(parent) {
  new DeviceAdaptor(this);
  new WirelessAdaptor(this);
}

class FakeNmIp4 : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.IP4Config")
  Q_PROPERTY(QVariantList AddressData READ addressData)

 public:
  [[nodiscard]] static QVariantList addressData() {
    QVariantMap addr;
    addr["address"] = "192.168.1.50";
    return {addr};
  }
};

class FakeNmActive : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.Connection.Active")
  Q_PROPERTY(QDBusObjectPath Connection READ connection)
  Q_PROPERTY(QDBusObjectPath Ip4Config READ ip4Config)
  Q_PROPERTY(QDBusObjectPath SpecificObject READ specificObject)
  Q_PROPERTY(QList<QDBusObjectPath> Devices READ devices)

 public:
  [[nodiscard]] static QDBusObjectPath connection() {
    return QDBusObjectPath("/org/freedesktop/NetworkManager/Settings/1");
  }
  [[nodiscard]] static QDBusObjectPath ip4Config() {
    return QDBusObjectPath("/org/freedesktop/NetworkManager/IP4Config/1");
  }
  [[nodiscard]] static QDBusObjectPath specificObject() {
    return QDBusObjectPath("/org/freedesktop/NetworkManager/AccessPoint/1");
  }
  [[nodiscard]] static QList<QDBusObjectPath> devices() {
    return {QDBusObjectPath("/org/freedesktop/NetworkManager/Devices/1")};
  }
};

class FakeNmSettingsConnection : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.Settings.Connection")

 public Q_SLOTS:
  [[nodiscard]] QMap<QString, QVariantMap> GetSettings() const {  // NOLINT(readability-identifier-naming)
    QMap<QString, QVariantMap> settings;
    QVariantMap connection;
    connection["id"] = connection_id;
    connection["type"] = "802-11-wireless";
    settings["connection"] = connection;

    QVariantMap wifi;
    wifi["ssid"] = QByteArray("FakeWiFi");
    settings["802-11-wireless"] = wifi;
    return settings;
  }

 public:
  QString connection_id{QStringLiteral("FakeWiFi")};
};

class FakeNmSettings : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.Settings")

 public Q_SLOTS:
  QList<QDBusObjectPath> ListConnections() {  // NOLINT(readability-identifier-naming)
    ++list_connections_call_count;
    return {QDBusObjectPath("/org/freedesktop/NetworkManager/Settings/1")};
  }

 public:
  int list_connections_call_count{0};  // NOLINT(readability-identifier-naming)
};

class FakeNetworkManager : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager")
  Q_PROPERTY(bool WirelessEnabled READ wirelessEnabled WRITE setWirelessEnabled)
  Q_PROPERTY(bool WirelessHardwareEnabled READ wirelessHardwareEnabled)
  Q_PROPERTY(uint State READ state)
  Q_PROPERTY(QDBusObjectPath PrimaryConnection READ primaryConnection)

 public:
  [[nodiscard]] bool wirelessEnabled() const { return wifi_enabled_; }
  void setWirelessEnabled(bool enabled) { wifi_enabled_ = enabled; }
  [[nodiscard]] static bool wirelessHardwareEnabled() { return true; }
  [[nodiscard]] static uint state() { return 70; }
  [[nodiscard]] static QDBusObjectPath primaryConnection() {
    return QDBusObjectPath("/org/freedesktop/NetworkManager/ActiveConnection/1");
  }

 public Q_SLOTS:
  [[nodiscard]] QList<QDBusObjectPath> GetDevices() const {  // NOLINT(readability-identifier-naming)
    if (!has_wireless_device_) {
      return {};
    }
    return {QDBusObjectPath("/org/freedesktop/NetworkManager/Devices/1")};
  }

  QDBusObjectPath ActivateConnection(const QDBusObjectPath& /*connection*/,  // NOLINT(readability-identifier-naming)
                                     const QDBusObjectPath& /*device*/, const QDBusObjectPath& /*specific_object*/) {
    activate_called = true;
    return QDBusObjectPath("/org/freedesktop/NetworkManager/ActiveConnection/1");
  }

  QDBusObjectPath AddAndActivateConnection2(  // NOLINT(readability-identifier-naming)
      const QMap<QString, QVariantMap>& settings, const QDBusObjectPath& /*device*/,
      const QDBusObjectPath& /*specific_object*/, const QVariantMap& /*options*/) {
    add_and_activate_called = true;
    added_settings = settings;
    return QDBusObjectPath("/org/freedesktop/NetworkManager/ActiveConnection/1");
  }

  void DeactivateConnection(const QDBusObjectPath& /*active_connection*/) {  // NOLINT(readability-identifier-naming)
    deactivate_called = true;
  }

 public:
  bool wifi_enabled_{true};         // NOLINT(readability-identifier-naming)
  bool has_wireless_device_{true};  // NOLINT(readability-identifier-naming)
  bool activate_called{false};
  bool add_and_activate_called{false};
  bool deactivate_called{false};
  QMap<QString, QVariantMap> added_settings;
};

class QtNetworkManagerBackendTest : public ::testing::Test {
 protected:
  FakeNetworkManager mock_nm;                   // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  FakeNmSettings mock_settings;                 // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  FakeNmSettingsConnection mock_settings_conn;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  FakeNmActive mock_active;                     // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  FakeNmIp4 mock_ip4;                           // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  FakeNmDevice mock_device;                     // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  FakeNmAp mock_ap;                             // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

  void SetUp() override {  // NOLINT(readability-function-cognitive-complexity)
    qDBusRegisterMetaType<QMap<QString, QVariantMap>>();
    QtNetworkManagerBackend::setDbusConnection(QDBusConnection::sessionBus());

    QDBusConnection bus = QDBusConnection::sessionBus();
    ASSERT_TRUE(bus.registerService(QStringLiteral("org.freedesktop.NetworkManager")));

    const auto export_flags =
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSignals;
    ASSERT_TRUE(bus.registerObject(QStringLiteral("/org/freedesktop/NetworkManager"), &mock_nm, export_flags));
    ASSERT_TRUE(
        bus.registerObject(QStringLiteral("/org/freedesktop/NetworkManager/Settings"), &mock_settings, export_flags));
    ASSERT_TRUE(bus.registerObject(QStringLiteral("/org/freedesktop/NetworkManager/Settings/1"), &mock_settings_conn,
                                   export_flags));
    ASSERT_TRUE(bus.registerObject(QStringLiteral("/org/freedesktop/NetworkManager/ActiveConnection/1"), &mock_active,
                                   export_flags));
    ASSERT_TRUE(
        bus.registerObject(QStringLiteral("/org/freedesktop/NetworkManager/IP4Config/1"), &mock_ip4, export_flags));
    ASSERT_TRUE(bus.registerObject(QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"), &mock_device,
                                   QDBusConnection::ExportAdaptors));
    ASSERT_TRUE(
        bus.registerObject(QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/1"), &mock_ap, export_flags));

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
      auto* bus_iface = bus.interface();
      const QDBusReply<bool> registered =
          bus_iface != nullptr ? bus_iface->isServiceRegistered(QStringLiteral("org.freedesktop.NetworkManager"))
                               : QDBusReply<bool>();
      if (registered.isValid() && registered.value()) {
        return;
      }
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
      QThread::msleep(10);
    }
    FAIL() << "Fake NetworkManager service was not visible on the test bus";
  }

  void TearDown() override {
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject(QStringLiteral("/org/freedesktop/NetworkManager"));
    bus.unregisterObject(QStringLiteral("/org/freedesktop/NetworkManager/Settings"));
    bus.unregisterObject(QStringLiteral("/org/freedesktop/NetworkManager/Settings/1"));
    bus.unregisterObject(QStringLiteral("/org/freedesktop/NetworkManager/ActiveConnection/1"));
    bus.unregisterObject(QStringLiteral("/org/freedesktop/NetworkManager/IP4Config/1"));
    bus.unregisterObject(QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"));
    bus.unregisterObject(QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/1"));
    bus.unregisterService(QStringLiteral("org.freedesktop.NetworkManager"));

    QtNetworkManagerBackend::resetDbusConnection();
  }

  static void waitForSignal(QSignalSpy& spy, int timeout_ms = 1000) {
    QElapsedTimer timer;
    timer.start();
    while (spy.isEmpty() && timer.elapsed() < timeout_ms) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
      QThread::msleep(10);
    }
  }

  template <typename Predicate>
  std::optional<NetworkBackendState> waitForState(QSignalSpy* spy, Predicate predicate, int timeout_ms = 5000) {
    QElapsedTimer timer;
    timer.start();
    int inspected_count = 0;
    while (timer.elapsed() < timeout_ms) {
      while (inspected_count < spy->count()) {
        auto state = spy->at(inspected_count).at(0).value<NetworkBackendState>();
        ++inspected_count;
        if (predicate(state)) {
          return state;
        }
      }
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
      QThread::msleep(10);
    }
    return std::nullopt;
  }
};

TEST_F(QtNetworkManagerBackendTest,  // NOLINT(readability-function-cognitive-complexity)
       StartQueriesInitialStateAndEmitsSignals) {
  QtNetworkManagerBackend backend;
  QSignalSpy spy(&backend, &QtNetworkManagerBackend::stateChanged);

  backend.start();
  const std::optional<NetworkBackendState> maybe_state = waitForState(&spy, [](const NetworkBackendState& state) {
    return state.available && state.wifi_enabled && state.wifi_hardware_enabled &&
           state.active_connection_name == QStringLiteral("FakeWiFi") &&
           state.active_ip4_address == QStringLiteral("192.168.1.50") &&
           state.connection_status == QStringLiteral("Connected") && state.networks.size() == 1 &&
           state.networks.first().ssid == QStringLiteral("FakeWiFi") && state.networks.first().connected;
  });

  ASSERT_TRUE(maybe_state.has_value()) << "Timed out waiting for populated NetworkManager backend state";
  const NetworkBackendState& state = *maybe_state;

  EXPECT_TRUE(state.available);
  EXPECT_TRUE(state.wifi_enabled);
  EXPECT_TRUE(state.wifi_hardware_enabled);
  EXPECT_EQ(state.active_connection_name, QStringLiteral("FakeWiFi"));
  EXPECT_EQ(state.active_ip4_address, QStringLiteral("192.168.1.50"));
  EXPECT_EQ(state.connection_status, QStringLiteral("Connected"));

  ASSERT_EQ(state.networks.size(), 1);
  EXPECT_EQ(state.networks.first().ssid, QStringLiteral("FakeWiFi"));
  EXPECT_TRUE(state.networks.first().known);
  EXPECT_EQ(state.networks.first().connection_path, QStringLiteral("/org/freedesktop/NetworkManager/Settings/1"));
  EXPECT_TRUE(state.networks.first().connected);
}

TEST_F(QtNetworkManagerBackendTest, ActiveConnectionNameReadsMapEncodedSettings) {
  mock_settings_conn.connection_id = QStringLiteral("MapEncodedWiFi");

  QtNetworkManagerBackend backend;
  QSignalSpy spy(&backend, &QtNetworkManagerBackend::stateChanged);

  backend.start();
  const std::optional<NetworkBackendState> maybe_state = waitForState(&spy, [](const NetworkBackendState& state) {
    return state.active_connection_name == QStringLiteral("MapEncodedWiFi");
  });

  ASSERT_TRUE(maybe_state.has_value()) << "Timed out waiting for active connection name from map-encoded settings";
  EXPECT_EQ(maybe_state->active_connection_name, QStringLiteral("MapEncodedWiFi"));
}

TEST_F(QtNetworkManagerBackendTest, NoWirelessDeviceSkipsSavedConnectionEnumeration) {
  mock_nm.has_wireless_device_ = false;

  QtNetworkManagerBackend backend;
  QSignalSpy spy(&backend, &QtNetworkManagerBackend::stateChanged);

  backend.start();
  const std::optional<NetworkBackendState> maybe_state = waitForState(
      &spy, [](const NetworkBackendState& state) { return state.available && state.wifi_hardware_enabled; });

  ASSERT_TRUE(maybe_state.has_value()) << "Timed out waiting for NetworkManager backend state";
  EXPECT_TRUE(maybe_state->networks.isEmpty());
  EXPECT_EQ(mock_settings.list_connections_call_count, 0);
}

TEST_F(QtNetworkManagerBackendTest, SetWirelessEnabledSendsDbusMessage) {
  QtNetworkManagerBackend backend;
  backend.start();

  QSignalSpy spy(&backend, &QtNetworkManagerBackend::stateChanged);
  waitForSignal(spy);

  mock_nm.wifi_enabled_ = true;
  backend.setWirelessEnabled(false);

  QElapsedTimer timer;
  timer.start();
  while (mock_nm.wifi_enabled_ && timer.elapsed() < 1000) {
    QCoreApplication::processEvents();
    QThread::msleep(10);
  }

  EXPECT_FALSE(mock_nm.wifi_enabled_);
}

TEST_F(QtNetworkManagerBackendTest, ActivateKnownSendsDbusMessage) {
  QtNetworkManagerBackend backend;
  backend.start();

  QSignalSpy spy(&backend, &QtNetworkManagerBackend::stateChanged);
  waitForSignal(spy);

  WifiNetwork net;
  net.ssid = QStringLiteral("FakeWiFi");
  net.connection_path = QStringLiteral("/org/freedesktop/NetworkManager/Settings/1");
  net.device_path = QStringLiteral("/org/freedesktop/NetworkManager/Devices/1");
  net.access_point_path = QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/1");

  mock_nm.activate_called = false;
  backend.activateKnown(net);

  QElapsedTimer timer;
  timer.start();
  while (!mock_nm.activate_called && timer.elapsed() < 1000) {
    QCoreApplication::processEvents();
    QThread::msleep(10);
  }

  EXPECT_TRUE(mock_nm.activate_called);
}

TEST_F(QtNetworkManagerBackendTest, ActivateKnownIncludesDbusFailureDetail) {
  QDBusConnection bus = QDBusConnection::sessionBus();
  bus.unregisterObject(QStringLiteral("/org/freedesktop/NetworkManager"));

  QtNetworkManagerBackend backend;
  QSignalSpy error_spy(&backend, &QtNetworkManagerBackend::operationError);

  WifiNetwork net;
  net.ssid = QStringLiteral("FakeWiFi");
  net.connection_path = QStringLiteral("/org/freedesktop/NetworkManager/Settings/1");
  net.device_path = QStringLiteral("/org/freedesktop/NetworkManager/Devices/1");
  net.access_point_path = QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/1");

  backend.activateKnown(net);
  waitForSignal(error_spy);

  ASSERT_EQ(error_spy.count(), 1);
  const QString error = error_spy.first().first().toString();
  const QString context = QStringLiteral("Could not activate saved Wi-Fi network");
  EXPECT_TRUE(error.contains(context));
  EXPECT_NE(error, context);
}

TEST_F(QtNetworkManagerBackendTest, ActivateWithPasswordSendsNestedSettingsMap) {
  QtNetworkManagerBackend backend;

  WifiNetwork net;
  net.ssid = QStringLiteral("NewWiFi");
  net.secured = true;
  net.device_path = QStringLiteral("/org/freedesktop/NetworkManager/Devices/1");
  net.access_point_path = QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/1");

  backend.activateWithPassword(net, QStringLiteral("secret123"));

  QElapsedTimer timer;
  timer.start();
  while (!mock_nm.add_and_activate_called && timer.elapsed() < 1000) {
    QCoreApplication::processEvents();
    QThread::msleep(10);
  }

  ASSERT_TRUE(mock_nm.add_and_activate_called);
  EXPECT_EQ(mock_nm.added_settings.value(QStringLiteral("connection")).value(QStringLiteral("id")).toString(),
            QStringLiteral("NewWiFi"));
  EXPECT_EQ(
      mock_nm.added_settings.value(QStringLiteral("802-11-wireless-security")).value(QStringLiteral("psk")).toString(),
      QStringLiteral("secret123"));
}

TEST_F(QtNetworkManagerBackendTest, DisconnectActiveSendsDbusMessage) {
  QtNetworkManagerBackend backend;
  backend.start();

  QSignalSpy spy(&backend, &QtNetworkManagerBackend::stateChanged);
  waitForSignal(spy);

  mock_nm.deactivate_called = false;
  backend.disconnectActive(QStringLiteral("/org/freedesktop/NetworkManager/ActiveConnection/1"));

  QElapsedTimer timer;
  timer.start();
  while (!mock_nm.deactivate_called && timer.elapsed() < 1000) {
    QCoreApplication::processEvents();
    QThread::msleep(10);
  }

  EXPECT_TRUE(mock_nm.deactivate_called);
}

#include "test_network_service.moc"
