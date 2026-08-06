#pragma once

#include "DbusPropertyClient.h"

#include <QDBusObjectPath>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantMap>

struct BatteryStateUpdate;

class BatteryService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int percent READ percent NOTIFY percentChanged)
  Q_PROPERTY(bool charging READ charging NOTIFY chargingChanged)
  Q_PROPERTY(bool discharging READ discharging NOTIFY dischargingChanged)
  Q_PROPERTY(bool fullyCharged READ fullyCharged NOTIFY fullyChargedChanged)
  Q_PROPERTY(bool present READ present NOTIFY presentChanged)
  Q_PROPERTY(int timeRemaining READ timeRemaining NOTIFY timeRemainingChanged)
  Q_PROPERTY(int health READ health NOTIFY healthChanged)
  Q_PROPERTY(int chargeCycles READ chargeCycles NOTIFY chargeCyclesChanged)
  Q_PROPERTY(int chargeLimit READ chargeLimit NOTIFY chargeLimitChanged)

 public:
  struct SkipInitTag {};
  static constexpr SkipInitTag SkipInit{};

  explicit BatteryService(QObject* parent = nullptr);
  explicit BatteryService(DbusPropertyClientPtr dbus, QObject* parent = nullptr);
  explicit BatteryService(SkipInitTag /*tag*/, QObject* parent = nullptr);
  ~BatteryService() override = default;

  BatteryService(const BatteryService&) = delete;
  BatteryService& operator=(const BatteryService&) = delete;
  BatteryService(BatteryService&&) = delete;
  BatteryService& operator=(BatteryService&&) = delete;

  [[nodiscard]] int percent() const { return percent_; }
  [[nodiscard]] bool charging() const { return charging_; }
  [[nodiscard]] bool discharging() const { return discharging_; }
  [[nodiscard]] bool fullyCharged() const { return fully_charged_; }
  [[nodiscard]] bool present() const { return present_; }
  // Seconds until the relevant terminal state: time-to-empty while discharging, time-to-full
  // while charging, 0 otherwise (idle / fully charged / unknown).
  [[nodiscard]] int timeRemaining() const;
  [[nodiscard]] int health() const { return health_; }
  [[nodiscard]] int chargeCycles() const { return charge_cycles_; }
  [[nodiscard]] int chargeLimit() const { return charge_limit_; }

  void start();
  void applyStateUpdate(const BatteryStateUpdate& update);

  // Test seam: override sysfs paths used by probeChargeLimit(). Production code uses
  // /sys/class/power_supply/BAT{0,1}/charge_control_end_threshold by default.
  void setSysfsChargeLimitPaths(const QList<QString>& paths) { sysfs_charge_limit_paths_ = paths; }

 Q_SIGNALS:
  void percentChanged();
  void chargingChanged();
  void dischargingChanged();
  void fullyChargedChanged();
  void presentChanged();
  void timeRemainingChanged();
  void healthChanged();
  void chargeCyclesChanged();
  void chargeLimitChanged();

 private Q_SLOTS:
  void onPropertiesChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated);

 private:
  void readProperties();
  void setPercent(int value);
  void setCharging(bool value);
  void setDischarging(bool value);
  void setFullyCharged(bool value);
  void setPresent(bool value);
  void setTimeToEmpty(int value);
  void setTimeToFull(int value);
  void setHealth(int value);
  void setChargeCycles(int value);
  void setChargeLimit(int value);
  void probeChargeLimit();

  DbusPropertyClientPtr dbus_;
  QString device_path_;
  int percent_{0};
  bool charging_{false};
  bool discharging_{false};
  bool fully_charged_{false};
  bool present_{false};
  int time_to_empty_{0};
  int time_to_full_{0};
  int health_{0};
  int charge_cycles_{0};
  int charge_limit_{-1};
  QList<QString> sysfs_charge_limit_paths_{
      QStringLiteral("/sys/class/power_supply/BAT0/charge_control_end_threshold"),
      QStringLiteral("/sys/class/power_supply/BAT1/charge_control_end_threshold"),
  };
  bool started_{false};
};
