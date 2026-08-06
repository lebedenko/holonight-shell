#pragma once

#include "DbusPropertyClient.h"
#include "NetworkManagerBackend.h"
#include "WifiNetworkModel.h"

#include <QAbstractItemModel>
#include <QDBusObjectPath>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>

#include <memory>
#include <optional>

[[nodiscard]] bool isVpnConnectionType(const QString& connection_type);
[[nodiscard]] QList<QDBusObjectPath> activeConnectionPaths(const QVariant& value);

struct NetworkQuerySnapshot {
  std::optional<bool> online;
  std::optional<QString> primary_connection_path;
  std::optional<QString> connection_type;
  std::optional<QString> specific_object_path;
  std::optional<QVariantMap> access_point_properties;
  std::optional<QList<QString>> active_connection_types;
};

struct NetworkStateUpdate {
  std::optional<bool> online;
  std::optional<int> type;
  std::optional<bool> vpn_active;
  std::optional<QString> ssid;
  std::optional<int> strength;
  std::optional<QString> access_point_path;
};

[[nodiscard]] NetworkStateUpdate buildNetworkStateUpdate(const NetworkQuerySnapshot& snapshot);

class NetworkService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool available READ available NOTIFY availableChanged)
  Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
  Q_PROPERTY(int type READ type NOTIFY typeChanged)
  Q_PROPERTY(bool vpnActive READ vpnActive NOTIFY vpnActiveChanged)
  Q_PROPERTY(QString ssid READ ssid NOTIFY ssidChanged)
  Q_PROPERTY(int strength READ strength NOTIFY strengthChanged)
  Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY wifiEnabledChanged)
  Q_PROPERTY(bool wifiHardwareEnabled READ wifiHardwareEnabled NOTIFY wifiHardwareEnabledChanged)
  Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
  Q_PROPERTY(QString activeConnectionName READ activeConnectionName NOTIFY activeConnectionNameChanged)
  Q_PROPERTY(QString activeIp4Address READ activeIp4Address NOTIFY activeIp4AddressChanged)
  Q_PROPERTY(uint activeFrequencyMhz READ activeFrequencyMhz NOTIFY activeFrequencyMhzChanged)
  Q_PROPERTY(uint activeLinkSpeedMbps READ activeLinkSpeedMbps NOTIFY activeLinkSpeedMbpsChanged)
  Q_PROPERTY(QString downloadSpeedText READ downloadSpeedText NOTIFY downloadSpeedTextChanged)
  Q_PROPERTY(QString uploadSpeedText READ uploadSpeedText NOTIFY uploadSpeedTextChanged)
  Q_PROPERTY(QString connectionStatus READ connectionStatus NOTIFY connectionStatusChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
  Q_PROPERTY(QAbstractItemModel* wifiNetworks READ wifiNetworks CONSTANT)

 public:
  enum ConnectionType : uint8_t { None = 0, WiFi = 1, Wired = 2 };  // NOLINT(cppcoreguidelines-use-enum-class)
  Q_ENUM(ConnectionType)

  explicit NetworkService(QObject* parent = nullptr);
  explicit NetworkService(DbusPropertyClientPtr dbus, QObject* parent = nullptr);
  NetworkService(DbusPropertyClientPtr dbus, std::unique_ptr<NetworkManagerBackend> backend, QObject* parent = nullptr);
  ~NetworkService() override;

  NetworkService(const NetworkService&) = delete;
  NetworkService& operator=(const NetworkService&) = delete;
  NetworkService(NetworkService&&) = delete;
  NetworkService& operator=(NetworkService&&) = delete;

  [[nodiscard]] bool available() const { return available_; }
  [[nodiscard]] bool online() const { return online_; }
  [[nodiscard]] int type() const { return type_; }
  [[nodiscard]] bool vpnActive() const { return vpn_active_; }
  [[nodiscard]] QString ssid() const { return ssid_; }
  [[nodiscard]] int strength() const { return strength_; }
  [[nodiscard]] bool wifiEnabled() const { return wifi_enabled_; }
  [[nodiscard]] bool wifiHardwareEnabled() const { return wifi_hardware_enabled_; }
  [[nodiscard]] bool scanning() const { return scanning_; }
  [[nodiscard]] QString activeConnectionName() const { return active_connection_name_; }
  [[nodiscard]] QString activeIp4Address() const { return active_ip4_address_; }
  [[nodiscard]] uint activeFrequencyMhz() const { return active_frequency_mhz_; }
  [[nodiscard]] uint activeLinkSpeedMbps() const { return active_link_speed_mbps_; }
  [[nodiscard]] QString downloadSpeedText() const { return download_speed_text_; }
  [[nodiscard]] QString uploadSpeedText() const { return upload_speed_text_; }
  [[nodiscard]] QString connectionStatus() const { return connection_status_; }
  [[nodiscard]] QString lastError() const { return last_error_; }
  [[nodiscard]] QAbstractItemModel* wifiNetworks() const { return wifi_networks_; }

  void start();

  Q_INVOKABLE void rescanWifi();
  Q_INVOKABLE void setWifiEnabled(bool enabled);
  Q_INVOKABLE void connectNetwork(int row);
  Q_INVOKABLE void connectNetworkWithPassword(int row, const QString& password);
  Q_INVOKABLE void disconnectActive();
  Q_INVOKABLE void openNetworkSettings();
  Q_INVOKABLE void clearLastError();

 Q_SIGNALS:
  void availableChanged();
  void onlineChanged();
  void typeChanged();
  void vpnActiveChanged();
  void ssidChanged();
  void strengthChanged();
  void wifiEnabledChanged();
  void wifiHardwareEnabledChanged();
  void scanningChanged();
  void activeConnectionNameChanged();
  void activeIp4AddressChanged();
  void activeFrequencyMhzChanged();
  void activeLinkSpeedMbpsChanged();
  void downloadSpeedTextChanged();
  void uploadSpeedTextChanged();
  void connectionStatusChanged();
  void lastErrorChanged();

 private Q_SLOTS:
  void onNmStateChanged(uint newState);
  void onApPropertiesChanged(const QString& iface, const QVariantMap& changed, const QStringList& invalidated);
  void onPollTimer();
  void onBackendStateChanged(const NetworkBackendState& state);
  void onBackendError(const QString& message);

 private:
  void connectToNm();
  void queryAll();
  [[nodiscard]] std::optional<QVariant> getDbusProperty(const QString& path, const QString& interface,
                                                        const QString& name, const char* failure_message) const;
  std::optional<QList<QString>> queryActiveConnectionTypes();
  void subscribeAp(const QString& apPath);
  void applyStateUpdate(const NetworkStateUpdate& update);

  void setAvailable(bool value);
  void setOnline(bool value);
  void setType(int value);
  void setVpnActive(bool value);
  void setSsid(const QString& value);
  void setStrength(int value);
  void setWifiEnabledState(bool value);
  void setWifiHardwareEnabled(bool value);
  void setScanning(bool value);
  void setActiveConnectionName(const QString& value);
  void setActiveIp4Address(const QString& value);
  void setActiveFrequencyMhz(uint value);
  void setActiveLinkSpeedMbps(uint value);
  void setDownloadSpeedText(const QString& value);
  void setUploadSpeedText(const QString& value);
  void setConnectionStatus(const QString& value);
  void setLastError(const QString& value);

  QString current_ap_path_;
  DbusPropertyClientPtr dbus_;
  std::unique_ptr<NetworkManagerBackend> backend_;
  WifiNetworkModel* wifi_networks_{nullptr};
  QTimer* poll_timer_{nullptr};
  bool started_{false};

  bool available_{false};
  bool online_{false};
  int type_{0};
  bool vpn_active_{false};
  QString ssid_;
  int strength_{0};
  bool wifi_enabled_{false};
  bool wifi_hardware_enabled_{false};
  bool scanning_{false};
  QString active_connection_name_;
  QString active_ip4_address_;
  uint active_frequency_mhz_{0};
  uint active_link_speed_mbps_{0};
  QString download_speed_text_;
  QString upload_speed_text_;
  QString connection_status_;
  QString active_connection_path_;
  QString last_error_;
};
