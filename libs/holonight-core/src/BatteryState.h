#pragma once

#include <QVariant>
#include <QVariantMap>

#include <optional>

struct BatteryStateUpdate {
  std::optional<int> percent;
  std::optional<bool> charging;
  std::optional<bool> discharging;
  std::optional<bool> fully_charged;
  std::optional<bool> present;
  std::optional<int> time_to_empty;  // seconds; UPower TimeToEmpty ('x')
  std::optional<int> time_to_full;   // seconds; UPower TimeToFull ('x')
  std::optional<int> health;         // percent of design capacity; UPower Capacity ('d'), rounded
  std::optional<int> charge_cycles;  // UPower ChargeCycles ('i'); <= 0 means unavailable
  std::optional<int> charge_limit;   // percent; UPower ChargeEndThreshold ('u'); -1 if unavailable
};

[[nodiscard]] bool isUPowerBatteryDevice(const QVariant& type);
[[nodiscard]] bool isUPowerBatteryCharging(uint state);
[[nodiscard]] bool isUPowerBatteryDischarging(uint state);
[[nodiscard]] bool isUPowerBatteryFullyCharged(uint state);
[[nodiscard]] BatteryStateUpdate batteryStateUpdateFromProperties(const QVariantMap& properties);
