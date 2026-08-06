#include "ActivityGateManager.h"
#include "IActivityGate.h"
#include "LidStateMonitor.h"

#include <QMetaObject>
#include <QStringList>
#include <QVariantMap>

#include <gtest/gtest.h>

namespace {

class FakeLidDbusClient final : public DbusPropertyClient {
 public:
  [[nodiscard]] bool systemBusConnected() const override { return connected; }
  [[nodiscard]] bool serviceRegistered(const QString& /*service*/) const override { return true; }

  [[nodiscard]] std::optional<QVariant> property(const QString& /*s*/, const QString& /*p*/, const QString& /*i*/,
                                                 const QString& /*n*/) const override {
    return std::nullopt;
  }

  [[nodiscard]] std::optional<QVariantMap> allProperties(const QString& /*service*/, const QString& path,
                                                         const QString& /*iface*/) const override {
    if (path != QStringLiteral("/org/freedesktop/UPower")) {
      return std::nullopt;
    }
    return props;
  }

  [[nodiscard]] std::optional<QList<QDBusObjectPath>> upowerDevices() const override { return std::nullopt; }

  bool connectSignal(const QString& /*s*/, const QString& /*p*/, const QString& /*i*/, const QString& signal,
                     QObject* /*r*/, const char* /*slot*/) const override {
    signal_connected = (signal == QStringLiteral("PropertiesChanged"));
    return signal_connected;
  }

  bool disconnectSignal(const QString& /*s*/, const QString& /*p*/, const QString& /*i*/, const QString& /*sig*/,
                        QObject* /*r*/, const char* /*slot*/) const override {
    return true;
  }

  bool connected{true};
  QVariantMap props;
  mutable bool signal_connected{false};
};

}  // namespace

// ─── T-037 ──────────────────────────────────────────────────────────────────

TEST(LidStateMonitorTest, DesktopNoLid_SkipsSignalSubscription) {
  auto dbus = std::make_unique<FakeLidDbusClient>();
  auto* ptr = dbus.get();
  ptr->props.insert(QStringLiteral("LidIsPresent"), false);
  ptr->props.insert(QStringLiteral("LidIsClosed"), false);

  LidStateMonitor monitor(std::move(dbus));
  monitor.start();

  EXPECT_FALSE(monitor.lidPresent());
  EXPECT_FALSE(monitor.lidClosed());
  EXPECT_FALSE(ptr->signal_connected);
}

TEST(LidStateMonitorTest, LaptopLidClosed_ConnectsAndReflectsState) {
  auto dbus = std::make_unique<FakeLidDbusClient>();
  auto* ptr = dbus.get();
  ptr->props.insert(QStringLiteral("LidIsPresent"), true);
  ptr->props.insert(QStringLiteral("LidIsClosed"), true);

  LidStateMonitor monitor(std::move(dbus));
  monitor.start();

  EXPECT_TRUE(monitor.lidPresent());
  EXPECT_TRUE(monitor.lidClosed());
  EXPECT_TRUE(ptr->signal_connected);
}

TEST(LidStateMonitorTest, LaptopLidOpen_ConnectsAndReflectsState) {
  auto dbus = std::make_unique<FakeLidDbusClient>();
  auto* ptr = dbus.get();
  ptr->props.insert(QStringLiteral("LidIsPresent"), true);
  ptr->props.insert(QStringLiteral("LidIsClosed"), false);

  LidStateMonitor monitor(std::move(dbus));
  monitor.start();

  EXPECT_TRUE(monitor.lidPresent());
  EXPECT_FALSE(monitor.lidClosed());
  EXPECT_TRUE(ptr->signal_connected);
}

TEST(LidStateMonitorTest, StartIsIdempotent) {
  auto dbus = std::make_unique<FakeLidDbusClient>();
  auto* ptr = dbus.get();
  ptr->props.insert(QStringLiteral("LidIsPresent"), true);
  ptr->props.insert(QStringLiteral("LidIsClosed"), false);

  LidStateMonitor monitor(std::move(dbus));
  monitor.start();
  monitor.start();

  EXPECT_TRUE(ptr->signal_connected);
}

TEST(LidStateMonitorTest, SystemBusDisconnected_NoSignalAndNoError) {
  auto dbus = std::make_unique<FakeLidDbusClient>();
  auto* ptr = dbus.get();
  ptr->connected = false;

  LidStateMonitor monitor(std::move(dbus));
  monitor.start();

  EXPECT_FALSE(monitor.lidPresent());
  EXPECT_FALSE(ptr->signal_connected);
}

// ─── T-038 ──────────────────────────────────────────────────────────────────

namespace {

class MockGate final : public IActivityGate {
 public:
  void pauseActivity() override { ++pause_calls; }
  void resumeActivity() override { ++resume_calls; }

  int pause_calls{0};
  int resume_calls{0};
};

}  // namespace

TEST(ActivityGateManagerTest, LidClose_PausesAllGates) {
  ActivityGateManager manager;
  MockGate gate1;
  MockGate gate2;
  manager.registerGate(&gate1);
  manager.registerGate(&gate2);

  manager.onLidStateChanged(true);

  EXPECT_EQ(gate1.pause_calls, 1);
  EXPECT_EQ(gate2.pause_calls, 1);
  EXPECT_EQ(gate1.resume_calls, 0);
  EXPECT_EQ(gate2.resume_calls, 0);
}

TEST(ActivityGateManagerTest, LidOpen_ResumesAllGates) {
  ActivityGateManager manager;
  MockGate gate1;
  MockGate gate2;
  manager.registerGate(&gate1);
  manager.registerGate(&gate2);

  manager.onLidStateChanged(true);
  manager.onLidStateChanged(false);

  EXPECT_EQ(gate1.pause_calls, 1);
  EXPECT_EQ(gate2.pause_calls, 1);
  EXPECT_EQ(gate1.resume_calls, 1);
  EXPECT_EQ(gate2.resume_calls, 1);
}

TEST(ActivityGateManagerTest, NoGates_DoesNotCrash) {
  ActivityGateManager manager;
  manager.onLidStateChanged(true);
  manager.onLidStateChanged(false);
}

TEST(ActivityGateManagerTest, MultipleCloseEvents_EachPauseCallIssued) {
  ActivityGateManager manager;
  MockGate gate;
  manager.registerGate(&gate);

  manager.onLidStateChanged(true);
  manager.onLidStateChanged(true);

  EXPECT_EQ(gate.pause_calls, 2);
}

// ─── T-047: Full integration — LidStateMonitor ↔ ActivityGateManager ─────────

static void simulateLidPropertiesChanged(LidStateMonitor* monitor, bool closed) {
  QVariantMap changed;
  changed.insert(QStringLiteral("LidIsClosed"), closed);
  QMetaObject::invokeMethod(monitor, "onPropertiesChanged", Q_ARG(QString, QStringLiteral("org.freedesktop.UPower")),
                            Q_ARG(QVariantMap, changed), Q_ARG(QStringList, QStringList{}));
}

TEST(IntegrationTest, LidClose_PausesGatesThroughSignalChain) {
  auto dbus = std::make_unique<FakeLidDbusClient>();
  dbus->props.insert(QStringLiteral("LidIsPresent"), true);
  dbus->props.insert(QStringLiteral("LidIsClosed"), false);

  LidStateMonitor monitor(std::move(dbus));
  ActivityGateManager manager;
  MockGate gate;
  manager.registerGate(&gate);
  QObject::connect(&monitor, &LidStateMonitor::lidStateChanged, &manager, &ActivityGateManager::onLidStateChanged);
  monitor.start();

  simulateLidPropertiesChanged(&monitor, true);

  EXPECT_EQ(gate.pause_calls, 1);
  EXPECT_EQ(gate.resume_calls, 0);
}

TEST(IntegrationTest, LidOpen_ResumesGatesThroughSignalChain) {
  auto dbus = std::make_unique<FakeLidDbusClient>();
  dbus->props.insert(QStringLiteral("LidIsPresent"), true);
  dbus->props.insert(QStringLiteral("LidIsClosed"), true);

  LidStateMonitor monitor(std::move(dbus));
  ActivityGateManager manager;
  MockGate gate;
  manager.registerGate(&gate);
  QObject::connect(&monitor, &LidStateMonitor::lidStateChanged, &manager, &ActivityGateManager::onLidStateChanged);
  monitor.start();

  simulateLidPropertiesChanged(&monitor, false);

  EXPECT_EQ(gate.pause_calls, 0);
  EXPECT_EQ(gate.resume_calls, 1);
}

TEST(IntegrationTest, LidCloseOpen_CompleteCyclePauseThenResume) {
  auto dbus = std::make_unique<FakeLidDbusClient>();
  dbus->props.insert(QStringLiteral("LidIsPresent"), true);
  dbus->props.insert(QStringLiteral("LidIsClosed"), false);

  LidStateMonitor monitor(std::move(dbus));
  ActivityGateManager manager;
  MockGate gate;
  manager.registerGate(&gate);
  QObject::connect(&monitor, &LidStateMonitor::lidStateChanged, &manager, &ActivityGateManager::onLidStateChanged);
  monitor.start();

  simulateLidPropertiesChanged(&monitor, true);
  simulateLidPropertiesChanged(&monitor, false);

  EXPECT_EQ(gate.pause_calls, 1);
  EXPECT_EQ(gate.resume_calls, 1);
}
