#include "BatteryState.h"

#include <QtMath>

namespace {
constexpr uint kBatteryType = 2;
constexpr uint kStateCharging = 1;
constexpr uint kStateDischarging = 2;
constexpr uint kStateFullyCharged = 4;
constexpr uint kStatePendingDischarge = 6;
}  // namespace

bool isUPowerBatteryDevice(const QVariant& type) { return type.isValid() && type.toUInt() == kBatteryType; }

bool isUPowerBatteryCharging(uint state) { return state == kStateCharging; }

bool isUPowerBatteryDischarging(uint state) { return state == kStateDischarging || state == kStatePendingDischarge; }

bool isUPowerBatteryFullyCharged(uint state) { return state == kStateFullyCharged; }

BatteryStateUpdate batteryStateUpdateFromProperties(const QVariantMap& properties) {
  BatteryStateUpdate update;

  if (const auto property = properties.constFind(QStringLiteral("Percentage")); property != properties.constEnd()) {
    update.percent = qRound(property->toDouble());
  }
  if (const auto property = properties.constFind(QStringLiteral("State")); property != properties.constEnd()) {
    const uint state = property->toUInt();
    update.charging = isUPowerBatteryCharging(state);
    update.discharging = isUPowerBatteryDischarging(state);
    update.fully_charged = isUPowerBatteryFullyCharged(state);
  }
  if (const auto property = properties.constFind(QStringLiteral("IsPresent")); property != properties.constEnd()) {
    update.present = property->toBool();
  }
  if (const auto property = properties.constFind(QStringLiteral("TimeToEmpty")); property != properties.constEnd()) {
    update.time_to_empty = static_cast<int>(property->toLongLong());
  }
  if (const auto property = properties.constFind(QStringLiteral("TimeToFull")); property != properties.constEnd()) {
    update.time_to_full = static_cast<int>(property->toLongLong());
  }
  if (const auto property = properties.constFind(QStringLiteral("Capacity")); property != properties.constEnd()) {
    update.health = qRound(property->toDouble());
  }
  if (const auto property = properties.constFind(QStringLiteral("ChargeCycles")); property != properties.constEnd()) {
    update.charge_cycles = property->toInt();
  }
  if (const auto property = properties.constFind(QStringLiteral("ChargeEndThreshold"));
      property != properties.constEnd()) {
    const int limit = static_cast<int>(property->toUInt());
    update.charge_limit = (limit > 0) ? limit : -1;
  }

  return update;
}
