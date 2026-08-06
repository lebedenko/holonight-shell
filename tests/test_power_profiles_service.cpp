#include "PowerProfilesService.h"

#include <QDBusServiceWatcher>
#include <QSignalSpy>

#include <gtest/gtest.h>

class FakePowerProfilesDbusClient final : public DbusPropertyClient {
 public:
  [[nodiscard]] bool systemBusConnected() const override { return system_bus_connected; }

  [[nodiscard]] bool serviceRegistered(const QString& service) const override { return registered.contains(service); }

  [[nodiscard]] std::optional<QVariant> property(const QString& /*service*/, const QString& /*path*/,
                                                 const QString& /*interface*/, const QString& /*name*/) const override {
    return std::nullopt;
  }

  [[nodiscard]] std::optional<QVariantMap> allProperties(const QString& service, const QString& /*path*/,
                                                         const QString& /*interface*/) const override {
    last_all_properties_service = service;
    if (!all_properties_available) {
      return std::nullopt;
    }
    return properties;
  }

  [[nodiscard]] bool setProperty(const QString& service, const QString& /*path*/, const QString& /*interface*/,
                                 const QString& name, const QVariant& value) const override {
    set_called = true;
    last_set_service = service;
    last_set_name = name;
    last_set_value = value.toString();
    return true;
  }

  [[nodiscard]] std::optional<QList<QDBusObjectPath>> upowerDevices() const override { return std::nullopt; }

  bool connectSignal(const QString& /*service*/, const QString& /*path*/, const QString& /*interface*/,
                     const QString& /*signal*/, QObject* /*receiver*/, const char* /*slot*/) const override {
    ++connect_signal_calls;
    return connect_signal_result;
  }

  bool disconnectSignal(const QString& service, const QString& /*path*/, const QString& /*interface*/,
                        const QString& /*signal*/, QObject* /*receiver*/, const char* /*slot*/) const override {
    ++disconnect_signal_calls;
    disconnected_services.append(service);
    return true;
  }

  QStringList registered;
  QVariantMap properties;
  bool system_bus_connected{true};
  bool all_properties_available{true};
  bool connect_signal_result{true};
  mutable QString last_all_properties_service;
  mutable int connect_signal_calls{0};
  mutable int disconnect_signal_calls{0};
  mutable QStringList disconnected_services;
  mutable bool set_called{false};
  mutable QString last_set_service;
  mutable QString last_set_name;
  mutable QString last_set_value;
};

TEST(PowerProfilesService, PrefersFreedesktopNameWhenBothRegistered) {
  auto dbus = std::make_unique<FakePowerProfilesDbusClient>();
  auto* dbus_ptr = dbus.get();
  dbus->registered = {QStringLiteral("org.freedesktop.UPower.PowerProfiles"),
                      QStringLiteral("net.hadess.PowerProfiles")};
  dbus->properties.insert(QStringLiteral("ActiveProfile"), QStringLiteral("balanced"));
  PowerProfilesService service(std::move(dbus));

  service.start();

  EXPECT_TRUE(service.available());
  EXPECT_EQ(service.activeProfile(), QStringLiteral("balanced"));
  EXPECT_EQ(dbus_ptr->last_all_properties_service, QStringLiteral("org.freedesktop.UPower.PowerProfiles"));
}

TEST(PowerProfilesService, FallsBackToHadessName) {
  auto dbus = std::make_unique<FakePowerProfilesDbusClient>();
  auto* dbus_ptr = dbus.get();
  dbus->registered = {QStringLiteral("net.hadess.PowerProfiles")};
  PowerProfilesService service(std::move(dbus));

  service.start();

  EXPECT_TRUE(service.available());
  EXPECT_EQ(dbus_ptr->last_all_properties_service, QStringLiteral("net.hadess.PowerProfiles"));
}

TEST(PowerProfilesService, StartAppliesProfilesFromPropertyList) {
  auto dbus = std::make_unique<FakePowerProfilesDbusClient>();
  dbus->registered = {QStringLiteral("org.freedesktop.UPower.PowerProfiles")};
  dbus->properties.insert(QStringLiteral("ActiveProfile"), QStringLiteral("balanced"));
  dbus->properties.insert(QStringLiteral("Profiles"),
                          QVariantList{QVariantMap{{QStringLiteral("Profile"), QStringLiteral("balanced")}},
                                       QVariantMap{{QStringLiteral("Profile"), QStringLiteral("performance")}}});
  PowerProfilesService service(std::move(dbus));

  service.start();

  EXPECT_TRUE(service.available());
  EXPECT_TRUE(service.hasBalanced());
  EXPECT_TRUE(service.hasPerformance());
  EXPECT_FALSE(service.hasPowerSaver());
}

TEST(PowerProfilesService, StartIgnoresUnsupportedProfileRepresentation) {
  auto dbus = std::make_unique<FakePowerProfilesDbusClient>();
  dbus->registered = {QStringLiteral("org.freedesktop.UPower.PowerProfiles")};
  dbus->properties.insert(QStringLiteral("Profiles"), 42);
  PowerProfilesService service(std::move(dbus));

  service.start();

  EXPECT_FALSE(service.hasBalanced());
  EXPECT_FALSE(service.hasPerformance());
  EXPECT_FALSE(service.hasPowerSaver());
}

TEST(PowerProfilesService, UnavailableWhenNeitherNameRegistered) {
  auto dbus = std::make_unique<FakePowerProfilesDbusClient>();
  dbus->registered = {};
  PowerProfilesService service(std::move(dbus));
  QSignalSpy available_changed(&service, &PowerProfilesService::availableChanged);

  service.start();

  EXPECT_FALSE(service.available());
  EXPECT_EQ(available_changed.count(), 0);
}

TEST(PowerProfilesService, UnavailableWhenInitialPropertiesFail) {
  auto dbus = std::make_unique<FakePowerProfilesDbusClient>();
  dbus->registered = {QStringLiteral("org.freedesktop.UPower.PowerProfiles")};
  dbus->all_properties_available = false;
  PowerProfilesService service(std::move(dbus));

  service.start();

  EXPECT_FALSE(service.available());
  EXPECT_TRUE(service.activeProfile().isEmpty());
}

TEST(PowerProfilesService, UnavailableWhenPropertiesChangedConnectionFails) {
  auto dbus = std::make_unique<FakePowerProfilesDbusClient>();
  auto* dbus_ptr = dbus.get();
  dbus->registered = {QStringLiteral("org.freedesktop.UPower.PowerProfiles")};
  dbus->properties.insert(QStringLiteral("ActiveProfile"), QStringLiteral("balanced"));
  dbus->connect_signal_result = false;
  PowerProfilesService service(std::move(dbus));

  service.start();

  EXPECT_FALSE(service.available());
  EXPECT_TRUE(service.activeProfile().isEmpty());
  EXPECT_EQ(dbus_ptr->disconnect_signal_calls, 0);
}

TEST(PowerProfilesService, DaemonRestartReplacesPropertiesChangedSubscription) {
  constexpr auto kFreedesktopService = "org.freedesktop.UPower.PowerProfiles";
  auto dbus = std::make_unique<FakePowerProfilesDbusClient>();
  auto* dbus_ptr = dbus.get();
  dbus->registered = {QLatin1String(kFreedesktopService)};
  dbus->properties.insert(QStringLiteral("ActiveProfile"), QStringLiteral("balanced"));
  PowerProfilesService service(std::move(dbus));

  service.start();
  ASSERT_TRUE(service.available());
  ASSERT_EQ(dbus_ptr->connect_signal_calls, 1);

  auto* watcher = service.findChild<QDBusServiceWatcher*>();
  ASSERT_NE(watcher, nullptr);
  ASSERT_TRUE(QMetaObject::invokeMethod(watcher, "serviceUnregistered", Qt::DirectConnection,
                                        Q_ARG(QString, QLatin1String(kFreedesktopService))));
  EXPECT_FALSE(service.available());
  EXPECT_EQ(dbus_ptr->disconnect_signal_calls, 1);
  EXPECT_EQ(dbus_ptr->disconnected_services, QStringList{QLatin1String(kFreedesktopService)});

  ASSERT_TRUE(QMetaObject::invokeMethod(watcher, "serviceRegistered", Qt::DirectConnection,
                                        Q_ARG(QString, QLatin1String(kFreedesktopService))));
  EXPECT_TRUE(service.available());
  EXPECT_EQ(dbus_ptr->connect_signal_calls, 2);
  EXPECT_EQ(dbus_ptr->disconnect_signal_calls, 1);
}

TEST(PowerProfilesService, ApplyProfilesSetsCapabilityFlags) {
  PowerProfilesService service(PowerProfilesService::SkipInit);
  QSignalSpy profiles_changed(&service, &PowerProfilesService::profilesChanged);

  service.applyProfiles({QStringLiteral("balanced"), QStringLiteral("performance")});

  EXPECT_TRUE(service.hasBalanced());
  EXPECT_TRUE(service.hasPerformance());
  EXPECT_FALSE(service.hasPowerSaver());
  EXPECT_EQ(profiles_changed.count(), 1);

  service.applyProfiles({QStringLiteral("power-saver"), QStringLiteral("balanced"), QStringLiteral("performance")});

  EXPECT_TRUE(service.hasPowerSaver());
  EXPECT_EQ(profiles_changed.count(), 2);
}

TEST(PowerProfilesService, ApplyProfilesNoOpDoesNotEmit) {
  PowerProfilesService service(PowerProfilesService::SkipInit);
  service.applyProfiles({QStringLiteral("balanced")});
  QSignalSpy profiles_changed(&service, &PowerProfilesService::profilesChanged);

  service.applyProfiles({QStringLiteral("balanced")});

  EXPECT_EQ(profiles_changed.count(), 0);
}

TEST(PowerProfilesService, SetProfileDoesNotUpdateOptimistically) {
  auto dbus = std::make_unique<FakePowerProfilesDbusClient>();
  auto* dbus_ptr = dbus.get();
  dbus->registered = {QStringLiteral("org.freedesktop.UPower.PowerProfiles")};
  dbus->properties.insert(QStringLiteral("ActiveProfile"), QStringLiteral("balanced"));
  PowerProfilesService service(std::move(dbus));
  service.start();
  ASSERT_EQ(service.activeProfile(), QStringLiteral("balanced"));

  service.setProfile(QStringLiteral("performance"));

  // The D-Bus write is issued, but the cached active profile stays put until the daemon echoes.
  EXPECT_TRUE(dbus_ptr->set_called);
  EXPECT_EQ(dbus_ptr->last_set_name, QStringLiteral("ActiveProfile"));
  EXPECT_EQ(dbus_ptr->last_set_value, QStringLiteral("performance"));
  EXPECT_EQ(service.activeProfile(), QStringLiteral("balanced"));

  // Simulate the daemon's PropertiesChanged echo.
  service.applyActiveProfile(QStringLiteral("performance"));
  EXPECT_EQ(service.activeProfile(), QStringLiteral("performance"));
}

TEST(PowerProfilesService, SetProfileIgnoredWhenUnavailable) {
  auto dbus = std::make_unique<FakePowerProfilesDbusClient>();
  auto* dbus_ptr = dbus.get();
  dbus->registered = {};
  PowerProfilesService service(std::move(dbus));
  service.start();

  service.setProfile(QStringLiteral("performance"));

  EXPECT_FALSE(dbus_ptr->set_called);
}
