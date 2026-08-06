#include "BatteryService.h"
#include "BatteryState.h"

#include <QDBusObjectPath>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>

class FakeBatteryDbusClient final : public DbusPropertyClient {
 public:
  [[nodiscard]] bool systemBusConnected() const override { return system_bus_connected; }
  [[nodiscard]] bool serviceRegistered(const QString& /*service*/) const override { return true; }

  [[nodiscard]] std::optional<QVariant> property(const QString& /*service*/, const QString& path,
                                                 const QString& /*interface*/, const QString& name) const override {
    if (path == battery_path && name == QStringLiteral("Type")) {
      return QVariant::fromValue(2U);
    }
    if (path == battery_path && name == QStringLiteral("ChargeEndThreshold")) {
      if (charge_end_threshold >= 0) {
        return QVariant::fromValue(static_cast<uint>(charge_end_threshold));
      }
      return std::nullopt;
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<QVariantMap> allProperties(const QString& /*service*/, const QString& path,
                                                         const QString& /*interface*/) const override {
    if (path == battery_path) {
      return properties;
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<QList<QDBusObjectPath>> upowerDevices() const override {
    return QList<QDBusObjectPath>{QDBusObjectPath(battery_path)};
  }

  bool connectSignal(const QString& /*service*/, const QString& path, const QString& /*interface*/,
                     const QString& signal, QObject* /*receiver*/, const char* /*slot*/) const override {
    signal_connected = path == battery_path && signal == QStringLiteral("PropertiesChanged");
    return signal_connected;
  }

  bool disconnectSignal(const QString& /*service*/, const QString& /*path*/, const QString& /*interface*/,
                        const QString& /*signal*/, QObject* /*receiver*/, const char* /*slot*/) const override {
    return true;
  }

  QString battery_path{QStringLiteral("/org/freedesktop/UPower/devices/battery_BAT0")};
  QVariantMap properties;
  bool system_bus_connected{true};
  mutable bool signal_connected{false};
  int charge_end_threshold{-1};  // -1 = not available; >= 0 = expose via property()
};

TEST(BatteryService, ApplyStateUpdateChangesPropertiesAndEmitsSignals) {
  BatteryService service(std::make_unique<FakeBatteryDbusClient>());
  QSignalSpy percent_changed(&service, &BatteryService::percentChanged);
  QSignalSpy charging_changed(&service, &BatteryService::chargingChanged);
  QSignalSpy discharging_changed(&service, &BatteryService::dischargingChanged);
  QSignalSpy fully_charged_changed(&service, &BatteryService::fullyChargedChanged);
  QSignalSpy present_changed(&service, &BatteryService::presentChanged);

  BatteryStateUpdate update;
  update.percent = 83;
  update.charging = true;
  update.discharging = false;
  update.fully_charged = true;
  update.present = true;

  service.applyStateUpdate(update);

  EXPECT_EQ(service.percent(), 83);
  EXPECT_TRUE(service.charging());
  EXPECT_FALSE(service.discharging());
  EXPECT_TRUE(service.fullyCharged());
  EXPECT_TRUE(service.present());
  EXPECT_EQ(percent_changed.count(), 1);
  EXPECT_EQ(charging_changed.count(), 1);
  EXPECT_EQ(discharging_changed.count(), 0);
  EXPECT_EQ(fully_charged_changed.count(), 1);
  EXPECT_EQ(present_changed.count(), 1);
}

TEST(BatteryService, ApplyStateUpdateNoOpsDoNotEmitSignals) {
  BatteryService service(std::make_unique<FakeBatteryDbusClient>());
  BatteryStateUpdate update;
  update.percent = 50;
  update.charging = true;
  update.discharging = false;
  update.fully_charged = false;
  update.present = true;
  service.applyStateUpdate(update);

  QSignalSpy percent_changed(&service, &BatteryService::percentChanged);
  QSignalSpy charging_changed(&service, &BatteryService::chargingChanged);
  QSignalSpy discharging_changed(&service, &BatteryService::dischargingChanged);
  QSignalSpy fully_charged_changed(&service, &BatteryService::fullyChargedChanged);
  QSignalSpy present_changed(&service, &BatteryService::presentChanged);

  service.applyStateUpdate(update);

  EXPECT_EQ(percent_changed.count(), 0);
  EXPECT_EQ(charging_changed.count(), 0);
  EXPECT_EQ(discharging_changed.count(), 0);
  EXPECT_EQ(fully_charged_changed.count(), 0);
  EXPECT_EQ(present_changed.count(), 0);
}

TEST(BatteryService, ApplyStateUpdateLeavesUnspecifiedPropertiesUnchanged) {
  BatteryService service(std::make_unique<FakeBatteryDbusClient>());
  BatteryStateUpdate initial;
  initial.percent = 67;
  initial.charging = true;
  initial.present = true;
  service.applyStateUpdate(initial);

  BatteryStateUpdate partial;
  partial.discharging = true;

  service.applyStateUpdate(partial);

  EXPECT_EQ(service.percent(), 67);
  EXPECT_TRUE(service.charging());
  EXPECT_TRUE(service.discharging());
  EXPECT_TRUE(service.present());
}

TEST(BatteryService, ApplyStateUpdateClampsPercentToValidRange) {
  BatteryService service(std::make_unique<FakeBatteryDbusClient>());
  QSignalSpy percent_changed(&service, &BatteryService::percentChanged);

  BatteryStateUpdate update;
  update.percent = -1;
  service.applyStateUpdate(update);
  EXPECT_EQ(service.percent(), 0);
  EXPECT_EQ(percent_changed.count(), 0);

  update.percent = 101;
  service.applyStateUpdate(update);
  EXPECT_EQ(service.percent(), 100);
  EXPECT_EQ(percent_changed.count(), 1);
}

TEST(BatteryService, StartReadsBatteryPropertiesFromDbusAdapter) {
  auto dbus = std::make_unique<FakeBatteryDbusClient>();
  auto* dbus_ptr = dbus.get();
  dbus->properties.insert(QStringLiteral("Percentage"), 91.0);
  dbus->properties.insert(QStringLiteral("State"), 1U);
  dbus->properties.insert(QStringLiteral("IsPresent"), true);
  BatteryService service(std::move(dbus));
  QSignalSpy percent_changed(&service, &BatteryService::percentChanged);
  QSignalSpy charging_changed(&service, &BatteryService::chargingChanged);
  QSignalSpy present_changed(&service, &BatteryService::presentChanged);

  service.start();
  service.start();

  EXPECT_EQ(service.percent(), 91);
  EXPECT_TRUE(service.charging());
  EXPECT_TRUE(service.present());
  EXPECT_TRUE(dbus_ptr->signal_connected);
  EXPECT_EQ(percent_changed.count(), 1);
  EXPECT_EQ(charging_changed.count(), 1);
  EXPECT_EQ(present_changed.count(), 1);
}

TEST(BatteryService, TimeRemainingTracksChargeStateAndEmits) {
  BatteryService service(std::make_unique<FakeBatteryDbusClient>());
  QSignalSpy time_changed(&service, &BatteryService::timeRemainingChanged);
  QSignalSpy health_changed(&service, &BatteryService::healthChanged);
  QSignalSpy cycles_changed(&service, &BatteryService::chargeCyclesChanged);

  BatteryStateUpdate update;
  update.discharging = true;
  update.charging = false;
  update.time_to_empty = 13320;
  update.time_to_full = 3900;
  update.health = 91;
  update.charge_cycles = 287;
  service.applyStateUpdate(update);

  EXPECT_EQ(service.timeRemaining(), 13320);  // discharging -> time-to-empty
  EXPECT_EQ(service.health(), 91);
  EXPECT_EQ(service.chargeCycles(), 287);
  EXPECT_GT(time_changed.count(), 0);
  EXPECT_EQ(health_changed.count(), 1);
  EXPECT_EQ(cycles_changed.count(), 1);

  BatteryStateUpdate to_charging;
  to_charging.discharging = false;
  to_charging.charging = true;
  service.applyStateUpdate(to_charging);

  EXPECT_EQ(service.timeRemaining(), 3900);  // charging -> time-to-full
}

TEST(BatteryService, TimeRemainingIsZeroWhenIdle) {
  BatteryService service(std::make_unique<FakeBatteryDbusClient>());
  BatteryStateUpdate update;
  update.charging = false;
  update.discharging = false;
  update.time_to_empty = 5000;
  update.time_to_full = 4000;
  service.applyStateUpdate(update);

  EXPECT_EQ(service.timeRemaining(), 0);
}

TEST(BatteryService, StartFailureLeavesStateUnavailableWhenSystemBusIsDisconnected) {
  auto dbus = std::make_unique<FakeBatteryDbusClient>();
  auto* dbus_ptr = dbus.get();
  dbus->system_bus_connected = false;
  BatteryService service(std::move(dbus));
  QSignalSpy present_changed(&service, &BatteryService::presentChanged);

  service.start();
  service.start();

  EXPECT_FALSE(service.present());
  EXPECT_FALSE(dbus_ptr->signal_connected);
  EXPECT_EQ(present_changed.count(), 0);
}

// ─── T-043: Charge limit — UPower probe ──────────────────────────────────────

TEST(BatteryService, ChargeLimitFromUPowerChargeEndThreshold) {
  auto dbus = std::make_unique<FakeBatteryDbusClient>();
  dbus->charge_end_threshold = 80;
  BatteryService service(std::move(dbus));
  QSignalSpy limit_spy(&service, &BatteryService::chargeLimitChanged);

  service.start();

  EXPECT_EQ(service.chargeLimit(), 80);
  EXPECT_EQ(limit_spy.count(), 1);
}

TEST(BatteryService, ChargeLimitUnavailableWhenUPowerReturnsZero) {
  auto dbus = std::make_unique<FakeBatteryDbusClient>();
  dbus->charge_end_threshold = 0;
  BatteryService service(std::move(dbus));
  // Override sysfs paths with non-existent files so all probes fail.
  service.setSysfsChargeLimitPaths({QStringLiteral("/nonexistent/path/BAT0")});

  service.start();

  EXPECT_EQ(service.chargeLimit(), -1);
}

// ─── T-044: Charge limit — sysfs fallback ────────────────────────────────────

TEST(BatteryService, ChargeLimitFallsBackToSysfs) {
  auto dbus = std::make_unique<FakeBatteryDbusClient>();
  // UPower returns nothing (charge_end_threshold == -1).
  BatteryService service(std::move(dbus));

  // Write a fake sysfs file to a temp location.
  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());
  const QString fake_path = tmp.filePath(QStringLiteral("charge_control_end_threshold"));
  {
    QFile file(fake_path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("75\n");
  }
  service.setSysfsChargeLimitPaths({fake_path});

  service.start();

  EXPECT_EQ(service.chargeLimit(), 75);
}

TEST(BatteryService, ChargeLimitNegativeOneWhenAllProbesFail) {
  auto dbus = std::make_unique<FakeBatteryDbusClient>();
  BatteryService service(std::move(dbus));
  service.setSysfsChargeLimitPaths({QStringLiteral("/nonexistent/BAT0"), QStringLiteral("/nonexistent/BAT1")});

  service.start();

  EXPECT_EQ(service.chargeLimit(), -1);
}

TEST(BatteryService, ChargeLimitUpdatedViaPropertiesChanged) {
  auto dbus = std::make_unique<FakeBatteryDbusClient>();
  BatteryService service(std::move(dbus));
  service.setSysfsChargeLimitPaths({});

  service.start();
  EXPECT_EQ(service.chargeLimit(), -1);

  QSignalSpy limit_spy(&service, &BatteryService::chargeLimitChanged);
  BatteryStateUpdate upd;
  upd.charge_limit = 85;
  service.applyStateUpdate(upd);

  EXPECT_EQ(service.chargeLimit(), 85);
  EXPECT_EQ(limit_spy.count(), 1);
}
