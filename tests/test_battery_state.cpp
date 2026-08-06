#include "BatteryState.h"

#include <array>
#include <gtest/gtest.h>

TEST(BatteryState, DetectsBatteryDeviceType) {
  EXPECT_TRUE(isUPowerBatteryDevice(QVariant::fromValue(2U)));
  EXPECT_FALSE(isUPowerBatteryDevice(QVariant::fromValue(1U)));
  EXPECT_FALSE(isUPowerBatteryDevice(QVariant{QStringLiteral("battery")}));
  EXPECT_FALSE(isUPowerBatteryDevice(QVariant{}));
}

TEST(BatteryState, MapsChargingStates) {
  EXPECT_TRUE(isUPowerBatteryCharging(1));
  EXPECT_FALSE(isUPowerBatteryCharging(5));
  EXPECT_FALSE(isUPowerBatteryCharging(4));
  EXPECT_FALSE(isUPowerBatteryCharging(2));
  EXPECT_FALSE(isUPowerBatteryCharging(0));
}

TEST(BatteryState, MapsDischargingStates) {
  EXPECT_TRUE(isUPowerBatteryDischarging(2));
  EXPECT_TRUE(isUPowerBatteryDischarging(6));
  EXPECT_FALSE(isUPowerBatteryDischarging(1));
  EXPECT_FALSE(isUPowerBatteryDischarging(4));
}

TEST(BatteryState, MapsFullyChargedState) {
  EXPECT_TRUE(isUPowerBatteryFullyCharged(4));
  EXPECT_FALSE(isUPowerBatteryFullyCharged(1));
  EXPECT_FALSE(isUPowerBatteryFullyCharged(2));
}

TEST(BatteryState, MapsEveryRelevantUPowerState) {
  struct ExpectedState {
    uint state;
    bool charging;
    bool discharging;
    bool fully_charged;
  };

  constexpr std::array<ExpectedState, 8> kStates = {{
      {.state = 0, .charging = false, .discharging = false, .fully_charged = false},
      {.state = 1, .charging = true, .discharging = false, .fully_charged = false},
      {.state = 2, .charging = false, .discharging = true, .fully_charged = false},
      {.state = 3, .charging = false, .discharging = false, .fully_charged = false},
      {.state = 4, .charging = false, .discharging = false, .fully_charged = true},
      {.state = 5, .charging = false, .discharging = false, .fully_charged = false},
      {.state = 6, .charging = false, .discharging = true, .fully_charged = false},
      {.state = 99, .charging = false, .discharging = false, .fully_charged = false},
  }};

  for (const ExpectedState& expected : kStates) {
    SCOPED_TRACE(expected.state);
    EXPECT_EQ(isUPowerBatteryCharging(expected.state), expected.charging);
    EXPECT_EQ(isUPowerBatteryDischarging(expected.state), expected.discharging);
    EXPECT_EQ(isUPowerBatteryFullyCharged(expected.state), expected.fully_charged);
  }
}

TEST(BatteryState, BuildsFullStateUpdateFromProperties) {
  const BatteryStateUpdate update = batteryStateUpdateFromProperties({
      {QStringLiteral("Percentage"), 72.6},
      {QStringLiteral("State"), 1U},
      {QStringLiteral("IsPresent"), true},
  });

  ASSERT_TRUE(update.percent.has_value());
  ASSERT_TRUE(update.charging.has_value());
  ASSERT_TRUE(update.discharging.has_value());
  ASSERT_TRUE(update.fully_charged.has_value());
  ASSERT_TRUE(update.present.has_value());
  EXPECT_EQ(*update.percent, 73);
  EXPECT_TRUE(*update.charging);
  EXPECT_FALSE(*update.discharging);
  EXPECT_FALSE(*update.fully_charged);
  EXPECT_TRUE(*update.present);
}

TEST(BatteryState, RoundsPercentageWithoutClamping) {
  const BatteryStateUpdate update = batteryStateUpdateFromProperties({
      {QStringLiteral("Percentage"), 150.4},
  });
  const BatteryStateUpdate negative = batteryStateUpdateFromProperties({
      {QStringLiteral("Percentage"), -0.6},
  });

  ASSERT_TRUE(update.percent.has_value());
  ASSERT_TRUE(negative.percent.has_value());
  EXPECT_EQ(*update.percent, 150);
  EXPECT_EQ(*negative.percent, -1);
}

TEST(BatteryState, HandlesInvalidPropertyVariantsAsQtDefaults) {
  const BatteryStateUpdate update = batteryStateUpdateFromProperties({
      {QStringLiteral("Percentage"), QVariant{}},
      {QStringLiteral("State"), QVariant{}},
      {QStringLiteral("IsPresent"), QVariant{}},
  });

  ASSERT_TRUE(update.percent.has_value());
  ASSERT_TRUE(update.charging.has_value());
  ASSERT_TRUE(update.discharging.has_value());
  ASSERT_TRUE(update.fully_charged.has_value());
  ASSERT_TRUE(update.present.has_value());
  EXPECT_EQ(*update.percent, 0);
  EXPECT_FALSE(*update.charging);
  EXPECT_FALSE(*update.discharging);
  EXPECT_FALSE(*update.fully_charged);
  EXPECT_FALSE(*update.present);
}

TEST(BatteryState, BuildsFullyChargedStateUpdateFromProperties) {
  const BatteryStateUpdate update = batteryStateUpdateFromProperties({
      {QStringLiteral("State"), 4U},
  });

  ASSERT_TRUE(update.charging.has_value());
  ASSERT_TRUE(update.discharging.has_value());
  ASSERT_TRUE(update.fully_charged.has_value());
  EXPECT_FALSE(*update.charging);
  EXPECT_FALSE(*update.discharging);
  EXPECT_TRUE(*update.fully_charged);
}

TEST(BatteryState, PendingChargeDoesNotGlowAsCharging) {
  const BatteryStateUpdate update = batteryStateUpdateFromProperties({
      {QStringLiteral("State"), 5U},
  });

  ASSERT_TRUE(update.charging.has_value());
  ASSERT_TRUE(update.discharging.has_value());
  ASSERT_TRUE(update.fully_charged.has_value());
  EXPECT_FALSE(*update.charging);
  EXPECT_FALSE(*update.discharging);
  EXPECT_FALSE(*update.fully_charged);
}

TEST(BatteryState, ParsesTimeHealthAndCycleProperties) {
  const BatteryStateUpdate update = batteryStateUpdateFromProperties({
      {QStringLiteral("TimeToEmpty"), QVariant::fromValue<qlonglong>(13320)},
      {QStringLiteral("TimeToFull"), QVariant::fromValue<qlonglong>(3900)},
      {QStringLiteral("Capacity"), 84.7},
      {QStringLiteral("ChargeCycles"), 287},
  });

  ASSERT_TRUE(update.time_to_empty.has_value());
  ASSERT_TRUE(update.time_to_full.has_value());
  ASSERT_TRUE(update.health.has_value());
  ASSERT_TRUE(update.charge_cycles.has_value());
  EXPECT_EQ(*update.time_to_empty, 13320);
  EXPECT_EQ(*update.time_to_full, 3900);
  EXPECT_EQ(*update.health, 85);
  EXPECT_EQ(*update.charge_cycles, 287);
}

TEST(BatteryState, PreservesUnavailableChargeCyclesSentinel) {
  const BatteryStateUpdate update = batteryStateUpdateFromProperties({
      {QStringLiteral("ChargeCycles"), -1},
  });

  ASSERT_TRUE(update.charge_cycles.has_value());
  EXPECT_EQ(*update.charge_cycles, -1);
}

TEST(BatteryState, LeavesMissingPropertiesUnset) {
  const BatteryStateUpdate update = batteryStateUpdateFromProperties({
      {QStringLiteral("Percentage"), 44.2},
  });

  ASSERT_TRUE(update.percent.has_value());
  EXPECT_EQ(*update.percent, 44);
  EXPECT_FALSE(update.charging.has_value());
  EXPECT_FALSE(update.discharging.has_value());
  EXPECT_FALSE(update.fully_charged.has_value());
  EXPECT_FALSE(update.present.has_value());
}

TEST(BatteryState, ParsesChargeEndThreshold) {
  const BatteryStateUpdate update = batteryStateUpdateFromProperties({
      {QStringLiteral("ChargeEndThreshold"), QVariant::fromValue<uint>(80U)},
  });

  ASSERT_TRUE(update.charge_limit.has_value());
  EXPECT_EQ(*update.charge_limit, 80);
}

TEST(BatteryState, ChargeEndThresholdZeroYieldsNegativeOne) {
  const BatteryStateUpdate update = batteryStateUpdateFromProperties({
      {QStringLiteral("ChargeEndThreshold"), QVariant::fromValue<uint>(0U)},
  });

  ASSERT_TRUE(update.charge_limit.has_value());
  EXPECT_EQ(*update.charge_limit, -1);
}

TEST(BatteryState, MissingChargeEndThresholdLeavesLimitUnset) {
  const BatteryStateUpdate update = batteryStateUpdateFromProperties({
      {QStringLiteral("Percentage"), 50.0},
  });

  EXPECT_FALSE(update.charge_limit.has_value());
}
