#include "BatteryService.h"
#include "BatteryState.h"
#include "LowBatteryMonitor.h"

#include <QSettings>

#include <gtest/gtest.h>

namespace {

// Counts sendNotification calls without requiring a real notification daemon.
class SpyLowBatteryMonitor final : public LowBatteryMonitor {
 public:
  explicit SpyLowBatteryMonitor(BatteryService* battery, QObject* parent = nullptr)
      : LowBatteryMonitor(battery, parent) {}

  void sendNotification(const QString& /*summary*/, const QString& body, int urgency,
                        int /*expire_timeout_ms*/) override {
    ++total_calls;
    last_body = body;
    if (urgency >= 2) {
      ++critical_calls;
    } else {
      ++warning_calls;
    }
  }

  int total_calls{0};
  int warning_calls{0};
  int critical_calls{0};
  QString last_body;
};

// Drive the battery service using applyStateUpdate without a real D-Bus.
BatteryStateUpdate dischargingAt(int pct) {
  BatteryStateUpdate upd;
  upd.percent = pct;
  upd.discharging = true;
  upd.charging = false;
  upd.fully_charged = false;
  upd.present = true;
  return upd;
}

BatteryStateUpdate chargingAt(int pct) {
  BatteryStateUpdate upd;
  upd.percent = pct;
  upd.discharging = false;
  upd.charging = true;
  upd.fully_charged = false;
  upd.present = true;
  return upd;
}

}  // namespace

// ─── T-039: Threshold state machine ─────────────────────────────────────────

TEST(LowBatteryMonitorTest, WarningFiresOncePerCycle) {
  BatteryService battery(BatteryService::SkipInit);
  SpyLowBatteryMonitor monitor(&battery);
  monitor.setThresholds(20, 10);

  battery.applyStateUpdate(dischargingAt(25));
  EXPECT_EQ(monitor.warning_calls, 0);

  battery.applyStateUpdate(dischargingAt(20));
  EXPECT_EQ(monitor.warning_calls, 1);

  // Second crossing at same level — must not re-fire in same discharge cycle.
  battery.applyStateUpdate(dischargingAt(19));
  EXPECT_EQ(monitor.warning_calls, 1);
}

TEST(LowBatteryMonitorTest, CriticalFiresOncePerCycle) {
  BatteryService battery(BatteryService::SkipInit);
  SpyLowBatteryMonitor monitor(&battery);
  monitor.setThresholds(20, 10);

  battery.applyStateUpdate(dischargingAt(15));
  battery.applyStateUpdate(dischargingAt(10));
  EXPECT_EQ(monitor.critical_calls, 1);

  battery.applyStateUpdate(dischargingAt(9));
  EXPECT_EQ(monitor.critical_calls, 1);
}

TEST(LowBatteryMonitorTest, CriticalAlsoSuppressesWarning) {
  BatteryService battery(BatteryService::SkipInit);
  SpyLowBatteryMonitor monitor(&battery);
  monitor.setThresholds(20, 10);

  // Jump straight to critical without passing through warning threshold separately.
  battery.applyStateUpdate(dischargingAt(10));
  EXPECT_EQ(monitor.warning_calls, 0);
  EXPECT_EQ(monitor.critical_calls, 1);

  // No warning should fire now either.
  battery.applyStateUpdate(dischargingAt(15));
  EXPECT_EQ(monitor.warning_calls, 0);
}

TEST(LowBatteryMonitorTest, ResetOnCharge_AllowsNewNotificationsNextCycle) {
  BatteryService battery(BatteryService::SkipInit);
  SpyLowBatteryMonitor monitor(&battery);
  monitor.setThresholds(20, 10);

  battery.applyStateUpdate(dischargingAt(20));
  EXPECT_EQ(monitor.warning_calls, 1);

  // Plug in, then discharge again — warning should fire a second time.
  battery.applyStateUpdate(chargingAt(21));
  battery.applyStateUpdate(dischargingAt(20));
  EXPECT_EQ(monitor.warning_calls, 2);
}

TEST(LowBatteryMonitorTest, ResetOnFullyCharged_AllowsNewNotifications) {
  BatteryService battery(BatteryService::SkipInit);
  SpyLowBatteryMonitor monitor(&battery);
  monitor.setThresholds(20, 10);

  battery.applyStateUpdate(dischargingAt(20));
  EXPECT_EQ(monitor.warning_calls, 1);

  BatteryStateUpdate full;
  full.fully_charged = true;
  full.charging = false;
  full.discharging = false;
  full.present = true;
  battery.applyStateUpdate(full);

  battery.applyStateUpdate(dischargingAt(20));
  EXPECT_EQ(monitor.warning_calls, 2);
}

TEST(LowBatteryMonitorTest, ResetOnBatteryInsert_AllowsNewNotifications) {
  BatteryService battery(BatteryService::SkipInit);
  SpyLowBatteryMonitor monitor(&battery);
  monitor.setThresholds(20, 10);

  battery.applyStateUpdate(dischargingAt(20));
  EXPECT_EQ(monitor.warning_calls, 1);

  // Simulate remove+reinsert.
  BatteryStateUpdate removed;
  removed.present = false;
  battery.applyStateUpdate(removed);

  battery.applyStateUpdate(dischargingAt(20));
  EXPECT_EQ(monitor.warning_calls, 2);
}

TEST(LowBatteryMonitorTest, NoNotificationWhileNotDischarging) {
  BatteryService battery(BatteryService::SkipInit);
  SpyLowBatteryMonitor monitor(&battery);
  monitor.setThresholds(20, 10);

  // Percent below threshold but not discharging — no notification.
  battery.applyStateUpdate(chargingAt(5));
  EXPECT_EQ(monitor.total_calls, 0);
}

TEST(LowBatteryMonitorTest, NegativePercentIsClampedBeforeThresholdEvaluation) {
  BatteryService battery(BatteryService::SkipInit);
  SpyLowBatteryMonitor monitor(&battery);
  monitor.setThresholds(20, 10);

  battery.applyStateUpdate(dischargingAt(-1));

  EXPECT_EQ(battery.percent(), 0);
  EXPECT_EQ(monitor.critical_calls, 1);
  EXPECT_EQ(monitor.last_body, QStringLiteral("0% remaining — plug in now"));
}

// ─── T-040: QSettings loading and clamping ───────────────────────────────────

TEST(LowBatteryMonitorTest, LoadsThresholdsFromQSettings) {
  QSettings settings;
  settings.setValue(QStringLiteral("holonight/power/warningThreshold"), 30);
  settings.setValue(QStringLiteral("holonight/power/criticalThreshold"), 15);

  BatteryService battery(BatteryService::SkipInit);
  SpyLowBatteryMonitor monitor(&battery);

  EXPECT_EQ(monitor.warningThreshold(), 30);
  EXPECT_EQ(monitor.criticalThreshold(), 15);

  settings.remove(QStringLiteral("holonight/power/warningThreshold"));
  settings.remove(QStringLiteral("holonight/power/criticalThreshold"));
}

TEST(LowBatteryMonitorTest, DefaultThresholdsWhenSettingsAbsent) {
  QSettings settings;
  settings.remove(QStringLiteral("holonight/power/warningThreshold"));
  settings.remove(QStringLiteral("holonight/power/criticalThreshold"));

  BatteryService battery(BatteryService::SkipInit);
  SpyLowBatteryMonitor monitor(&battery);

  EXPECT_EQ(monitor.warningThreshold(), 20);
  EXPECT_EQ(monitor.criticalThreshold(), 10);
}

TEST(LowBatteryMonitorTest, ClampsInvalidCriticalThreshold) {
  QSettings settings;
  settings.setValue(QStringLiteral("holonight/power/warningThreshold"), 20);
  settings.setValue(QStringLiteral("holonight/power/criticalThreshold"), 20);  // same as warning — invalid

  BatteryService battery(BatteryService::SkipInit);
  SpyLowBatteryMonitor monitor(&battery);

  // Critical must be < warning after clamping.
  EXPECT_EQ(monitor.warningThreshold(), 20);
  EXPECT_LT(monitor.criticalThreshold(), monitor.warningThreshold());

  settings.remove(QStringLiteral("holonight/power/warningThreshold"));
  settings.remove(QStringLiteral("holonight/power/criticalThreshold"));
}

TEST(LowBatteryMonitorTest, SetThresholdsOverridesQSettings) {
  BatteryService battery(BatteryService::SkipInit);
  SpyLowBatteryMonitor monitor(&battery);
  monitor.setThresholds(35, 12);

  EXPECT_EQ(monitor.warningThreshold(), 35);
  EXPECT_EQ(monitor.criticalThreshold(), 12);
}
