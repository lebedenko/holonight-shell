#include "BatteryService.h"

#include "BatteryState.h"

#include <QFile>
#include <QList>
#include <QLoggingCategory>

#include <algorithm>
#include <memory>

Q_LOGGING_CATEGORY(lcBattery, "holonight.battery")

static constexpr auto kUPowerService = "org.freedesktop.UPower";
static constexpr auto kDeviceIface = "org.freedesktop.UPower.Device";
static constexpr auto kPropertiesIface = "org.freedesktop.DBus.Properties";

BatteryService::BatteryService(QObject* parent) : BatteryService(std::make_unique<QtDbusPropertyClient>(), parent) {}

BatteryService::BatteryService(DbusPropertyClientPtr dbus, QObject* parent) : QObject(parent), dbus_(std::move(dbus)) {}

BatteryService::BatteryService(SkipInitTag /*tag*/, QObject* parent) : QObject(parent) {}

void BatteryService::start() {
  if (started_) {
    return;
  }
  started_ = true;

  if (!dbus_->systemBusConnected()) {
    qCWarning(lcBattery) << "BatteryService: system D-Bus not available";
    return;
  }

  const std::optional<QList<QDBusObjectPath>> devices = dbus_->upowerDevices();
  if (!devices.has_value()) {
    qCWarning(lcBattery) << "BatteryService: EnumerateDevices failed";
    return;
  }

  for (const QDBusObjectPath& path : *devices) {
    const std::optional<QVariant> typeVar = dbus_->property(QLatin1String(kUPowerService), path.path(),
                                                            QLatin1String(kDeviceIface), QStringLiteral("Type"));
    if (!typeVar.has_value()) {
      continue;
    }
    if (isUPowerBatteryDevice(*typeVar)) {
      device_path_ = path.path();
      break;
    }
  }

  if (device_path_.isEmpty()) {
    qCInfo(lcBattery) << "BatteryService: no battery device found (desktop?)";
    return;
  }

  readProperties();
  probeChargeLimit();

  dbus_->connectSignal(QLatin1String(kUPowerService), device_path_, QLatin1String(kPropertiesIface),
                       QStringLiteral("PropertiesChanged"), this,
                       SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
}

void BatteryService::readProperties() {
  const std::optional<QVariantMap> properties =
      dbus_->allProperties(QLatin1String(kUPowerService), device_path_, QLatin1String(kDeviceIface));
  if (!properties.has_value()) {
    qCWarning(lcBattery) << "BatteryService: GetAll failed";
    return;
  }

  applyStateUpdate(batteryStateUpdateFromProperties(*properties));
}

void BatteryService::applyStateUpdate(const BatteryStateUpdate& update) {
  // Apply reset-triggering state flags first so LowBatteryMonitor sees a consistent state
  // when percentChanged fires. Then emit discharging last so dischargingChanged (which also
  // triggers threshold checks) has an up-to-date percent_ to read.
  if (update.charging.has_value()) {
    setCharging(*update.charging);
  }
  if (update.fully_charged.has_value()) {
    setFullyCharged(*update.fully_charged);
  }
  if (update.present.has_value()) {
    setPresent(*update.present);
  }
  if (update.time_to_empty.has_value()) {
    setTimeToEmpty(*update.time_to_empty);
  }
  if (update.time_to_full.has_value()) {
    setTimeToFull(*update.time_to_full);
  }
  if (update.health.has_value()) {
    setHealth(*update.health);
  }
  if (update.charge_cycles.has_value()) {
    setChargeCycles(*update.charge_cycles);
  }
  if (update.charge_limit.has_value()) {
    setChargeLimit(*update.charge_limit);
  }
  if (update.percent.has_value()) {
    setPercent(*update.percent);
  }
  if (update.discharging.has_value()) {
    setDischarging(*update.discharging);
  }
}

int BatteryService::timeRemaining() const {
  if (discharging_) {
    return time_to_empty_;
  }
  if (charging_) {
    return time_to_full_;
  }
  return 0;
}

void BatteryService::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                         const QStringList& /*invalidated*/) {
  if (interface != QLatin1String(kDeviceIface)) {
    return;
  }

  applyStateUpdate(batteryStateUpdateFromProperties(changed));
}

void BatteryService::setPercent(int value) {
  const int clamped_value = std::clamp(value, 0, 100);
  if (percent_ == clamped_value) {
    return;
  }
  percent_ = clamped_value;
  emit percentChanged();
}

void BatteryService::setCharging(bool value) {
  if (charging_ == value) {
    return;
  }
  charging_ = value;
  emit chargingChanged();
  emit timeRemainingChanged();  // timeRemaining() selects time-to-full vs time-to-empty by state
}

void BatteryService::setDischarging(bool value) {
  if (discharging_ == value) {
    return;
  }
  discharging_ = value;
  emit dischargingChanged();
  emit timeRemainingChanged();  // timeRemaining() selects time-to-full vs time-to-empty by state
}

void BatteryService::setFullyCharged(bool value) {
  if (fully_charged_ == value) {
    return;
  }
  fully_charged_ = value;
  emit fullyChargedChanged();
}

void BatteryService::setPresent(bool value) {
  if (present_ == value) {
    return;
  }
  present_ = value;
  emit presentChanged();
}

void BatteryService::setTimeToEmpty(int value) {
  if (time_to_empty_ == value) {
    return;
  }
  time_to_empty_ = value;
  if (discharging_) {
    emit timeRemainingChanged();
  }
}

void BatteryService::setTimeToFull(int value) {
  if (time_to_full_ == value) {
    return;
  }
  time_to_full_ = value;
  if (charging_) {
    emit timeRemainingChanged();
  }
}

void BatteryService::setHealth(int value) {
  if (health_ == value) {
    return;
  }
  health_ = value;
  emit healthChanged();
}

void BatteryService::setChargeCycles(int value) {
  if (charge_cycles_ == value) {
    return;
  }
  charge_cycles_ = value;
  emit chargeCyclesChanged();
}

void BatteryService::setChargeLimit(int value) {
  if (charge_limit_ == value) {
    return;
  }
  charge_limit_ = value;
  emit chargeLimitChanged();
}

void BatteryService::probeChargeLimit() {
  // Probe 1: UPower ChargeEndThreshold on the battery device object.
  const std::optional<QVariant> upower_val = dbus_->property(
      QLatin1String(kUPowerService), device_path_, QLatin1String(kDeviceIface), QStringLiteral("ChargeEndThreshold"));
  if (upower_val.has_value() && upower_val->isValid()) {
    const int limit = static_cast<int>(upower_val->toUInt());
    if (limit > 0) {
      setChargeLimit(limit);
      return;
    }
  }

  // Probes 2–3: sysfs charge_control_end_threshold (paths configurable for tests).
  for (const QString& sysfs_path : sysfs_charge_limit_paths_) {
    QFile file{sysfs_path};
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }
    const int limit = file.readAll().trimmed().toInt();
    if (limit > 0) {
      setChargeLimit(limit);
      return;
    }
  }

  // All probes failed — charge_limit_ stays -1, ChargeLimitRow remains hidden.
  qCInfo(lcBattery) << "BatteryService: charge limit not available on this system";
}
