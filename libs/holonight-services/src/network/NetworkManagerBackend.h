#pragma once

#include "WifiNetworkModel.h"

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QFuture>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>

struct NetworkBackendState {
  bool available{false};
  bool wifi_enabled{false};
  bool wifi_hardware_enabled{false};
  bool scanning{false};
  QString active_connection_name;
  QString active_ip4_address;
  uint active_frequency_mhz{0};
  uint active_link_speed_mbps{0};
  QString download_speed_text;
  QString upload_speed_text;
  QString connection_status;
  QString active_connection_path;
  QList<WifiNetwork> networks;
};

class NetworkManagerBackend : public QObject {
  Q_OBJECT

 public:
  explicit NetworkManagerBackend(QObject* parent = nullptr) : QObject(parent) {}
  ~NetworkManagerBackend() override = default;

  NetworkManagerBackend(const NetworkManagerBackend&) = delete;
  NetworkManagerBackend& operator=(const NetworkManagerBackend&) = delete;
  NetworkManagerBackend(NetworkManagerBackend&&) = delete;
  NetworkManagerBackend& operator=(NetworkManagerBackend&&) = delete;

  virtual void start() = 0;
  virtual void refresh() = 0;
  virtual void requestScan() = 0;
  virtual void setWirelessEnabled(bool enabled) = 0;
  virtual void activateKnown(const WifiNetwork& network) = 0;
  virtual void activateOpen(const WifiNetwork& network) = 0;
  virtual void activateWithPassword(const WifiNetwork& network, const QString& password) = 0;
  virtual void disconnectActive(const QString& active_connection_path) = 0;

 Q_SIGNALS:
  void stateChanged(const NetworkBackendState& state);
  void operationError(const QString& message);
};

#include <QFutureWatcher>

class QtNetworkManagerBackend final : public NetworkManagerBackend {
  Q_OBJECT

 public:
  struct NetworkStats {
    QHash<QString, quint64> rx_bytes;
    QHash<QString, quint64> tx_bytes;
    QHash<QString, qint64> stat_time_ms;
  };

  explicit QtNetworkManagerBackend(QObject* parent = nullptr);
  ~QtNetworkManagerBackend() override;

  QtNetworkManagerBackend(const QtNetworkManagerBackend&) = delete;
  QtNetworkManagerBackend& operator=(const QtNetworkManagerBackend&) = delete;
  QtNetworkManagerBackend(QtNetworkManagerBackend&&) = delete;
  QtNetworkManagerBackend& operator=(QtNetworkManagerBackend&&) = delete;

  [[nodiscard]] static QDBusConnection dbusConnection();
  static void setDbusConnection(const QDBusConnection& connection);
  static void resetDbusConnection();

  void start() override;
  void refresh() override;
  void requestScan() override;
  void setWirelessEnabled(bool enabled) override;
  void activateKnown(const WifiNetwork& network) override;
  void activateOpen(const WifiNetwork& network) override;
  void activateWithPassword(const WifiNetwork& network, const QString& password) override;
  void disconnectActive(const QString& active_connection_path) override;

 private Q_SLOTS:
  void onPollTimer();
  void onNmStateChanged(uint state);
  void onQueryFinished();

 private:
  void emitError(const QString& message);
  [[nodiscard]] static std::pair<NetworkBackendState, NetworkStats> doQueryState(NetworkStats stats);
  static void updateActiveConnectionState(NetworkBackendState* state, NetworkStats* stats);
  static void updateVisibleWifiNetworks(NetworkBackendState* state);
  [[nodiscard]] static WifiNetwork networkFromAccessPoint(const QString& device, const QDBusObjectPath& access_point,
                                                          const QHash<QString, QString>& saved,
                                                          const QString& active_access_point,
                                                          const QString& active_connection_name);
  [[nodiscard]] static QList<QString> wirelessDevicePaths();
  [[nodiscard]] static QList<QDBusObjectPath> objectPathListProperty(const QString& path, const QString& interface,
                                                                     const QString& property);
  [[nodiscard]] static QVariant property(const QString& path, const QString& interface, const QString& property);
  [[nodiscard]] static QVariantMap allProperties(const QString& path, const QString& interface);
  [[nodiscard]] static QHash<QString, QString> savedWifiConnections();
  [[nodiscard]] static QString connectionId(const QString& settings_path);
  [[nodiscard]] static QString activeConnectionId(const QString& active_path);
  [[nodiscard]] static QString activeConnectionIp4(const QString& active_path);
  [[nodiscard]] static QString deviceIp4Address(const QString& device_path);
  [[nodiscard]] static std::optional<quint64> interfaceCounter(const QString& interface_name, const QString& counter);
  [[nodiscard]] static QString speedText(double bits_per_second);
  static void updateDeviceStatistics(NetworkBackendState* state, const QList<QDBusObjectPath>& devices,
                                     NetworkStats* stats);
  [[nodiscard]] static bool callNoReply(const QString& path, const QString& interface, const QString& method,
                                        const QVariantList& args = {});
  void trackOperation(const QFuture<void>& future);
  void addAndActivate(const WifiNetwork& network, const QString& password);

  QTimer* poll_timer_{nullptr};
  QFutureWatcher<std::pair<NetworkBackendState, NetworkStats>>* watcher_{nullptr};
  QList<QFutureWatcher<void>*> operation_watchers_;
  bool query_in_progress_{false};
  bool query_queued_{false};
  QHash<QString, quint64> previous_rx_bytes_;
  QHash<QString, quint64> previous_tx_bytes_;
  QHash<QString, qint64> previous_stat_time_ms_;
  bool shutting_down_{false};
};
