#include "ApplicationLaunchService.h"
#include "PackageUpdatesService.h"

#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QTest>

#include <gtest/gtest.h>

namespace {
class PackageProvider : public QDBusVirtualObject {
 public:
  QVariantMap updates{{"State", "ready"},           {"Count", quint32(123)},
                      {"IgnoredCount", quint32(2)}, {"DownloadSizeBytes", quint64(4096)},
                      {"DataAsOf", qint64(1234)},   {"LastError", ""}};
  QVariantMap checks{{"Checking", false}, {"LastCheckSucceeded", true}, {"LastCheckTime", qint64(1234)}};
  bool defer_updates = false;
  QList<QDBusMessage> pending_updates;
  QString introspect(const QString&) const override { return {}; }
  bool handleMessage(const QDBusMessage& message, const QDBusConnection& bus) override {
    if (message.member() != "GetAll") return false;
    if (defer_updates && message.arguments().first().toString().endsWith(".Updates")) {
      pending_updates.append(message);
      return true;
    }
    bus.send(message.createReply(
        QVariantList{message.arguments().first().toString().endsWith(".Updates") ? updates : checks}));
    return true;
  }
};
bool waitFor(const std::function<bool()>& predicate) {
  for (int attempt = 0; attempt < 200 && !predicate(); ++attempt) QTest::qWait(10);
  return predicate();
}
}  // namespace
TEST(PackageUpdatesService, ReadsInvalidationsPreservesFailuresAndClearsOwner) {
  // The test runner supplies a private bus; never start the service on the host bus.
  ASSERT_TRUE(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS").contains("/tmp/"));
  auto bus = QDBusConnection::sessionBus();
  PackageProvider provider;
  ASSERT_TRUE(bus.registerVirtualObject("/org/holonight/Packages1", &provider));
  ASSERT_TRUE(bus.registerService("org.holonight.Packages1"));
  PackageUpdatesService service(bus, nullptr);
  service.start();
  ASSERT_TRUE(waitFor([&] { return service.count() == 123; }));
  EXPECT_TRUE(service.available());
  EXPECT_EQ(service.ignoredCount(), 2);
  auto notify = [&](const QString& interface) {
    auto signal =
        QDBusMessage::createSignal("/org/holonight/Packages1", "org.freedesktop.DBus.Properties", "PropertiesChanged");
    signal << interface << QVariantMap{} << QStringList{"State"};
    bus.send(signal);
  };
  provider.updates["State"] = "error";
  provider.updates["LastError"] = "evaluation failed";
  notify("org.holonight.Packages1.Updates");
  ASSERT_TRUE(waitFor([&] { return !service.errorMessage().isEmpty(); }));
  EXPECT_EQ(service.count(), 123);
  provider.checks["Checking"] = true;
  notify("org.holonight.Packages1.UpdateCheck");
  ASSERT_TRUE(waitFor([&] { return service.checking(); }));
  provider.updates["State"] = "ready";
  provider.updates["LastError"] = "";
  provider.updates["Count"] = quint32(0);
  notify("org.holonight.Packages1.Updates");
  ASSERT_TRUE(waitFor([&] { return service.count() == 0 && service.errorMessage().isEmpty(); }));
  provider.updates["Count"] = "malformed";
  notify("org.holonight.Packages1.Updates");
  ASSERT_TRUE(waitFor([&] { return !service.errorMessage().isEmpty(); }));
  bus.unregisterService("org.holonight.Packages1");
  ASSERT_TRUE(waitFor([&] { return !service.available(); }));
  EXPECT_FALSE(service.checking());
  EXPECT_TRUE(service.errorMessage().isEmpty());
  provider.updates["Count"] = quint32(7);
  ASSERT_TRUE(bus.registerService("org.holonight.Packages1"));
  ASSERT_TRUE(waitFor([&] { return service.count() == 7; }));
  bus.unregisterService("org.holonight.Packages1");
  bus.unregisterObject("/org/holonight/Packages1");
}

TEST(PackageUpdatesService, LaunchFailureAndSuccessfulRetry) {
  int calls = 0;
  bool fail = true;
  ApplicationLaunchService launcher(
      nullptr,
      [&](ApplicationLaunchService::Backend, const LauncherCommand& command, const DesktopEntry*, const QString&) {
        EXPECT_EQ(command.program, QStringLiteral("holonight-packages"));
        ++calls;
        return fail ? QStringLiteral("launch failed") : QString{};
      },
      [] { return ApplicationLaunchService::Capabilities{}; });
  PackageUpdatesService service;
  service.setApplicationLaunchService(&launcher);
  service.openPackages();
  service.openPackages();
  ASSERT_TRUE(waitFor([&] { return !service.launchError().isEmpty(); }));
  EXPECT_EQ(calls, 1);
  fail = false;
  service.openPackages();
  ASSERT_TRUE(waitFor([&] { return service.launchError().isEmpty(); }));
  EXPECT_EQ(calls, 2);
}

TEST(PackageUpdatesService, DiscardsSupersededAndPreviousOwnerRepliesWithOptionalChecks) {
  ASSERT_TRUE(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS").contains("/tmp/"));
  auto bus = QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), "package-provider");
  PackageProvider provider;
  provider.defer_updates = true;
  provider.checks.clear();
  ASSERT_TRUE(bus.registerVirtualObject("/org/holonight/Packages1", &provider));
  ASSERT_TRUE(bus.registerService("org.holonight.Packages1"));
  PackageUpdatesService service(QDBusConnection::sessionBus(), nullptr);
  service.start();
  service.start();
  ASSERT_TRUE(waitFor([&] { return provider.pending_updates.size() == 1; }));
  EXPECT_FALSE(service.checkMetadataAvailable());
  auto signal =
      QDBusMessage::createSignal("/org/holonight/Packages1", "org.freedesktop.DBus.Properties", "PropertiesChanged");
  signal << QString("org.holonight.Packages1.Updates") << QVariantMap{} << QStringList{"Count"};
  bus.send(signal);
  ASSERT_TRUE(waitFor([&] { return provider.pending_updates.size() == 2; }));
  provider.updates["Count"] = quint32(9);
  bus.send(provider.pending_updates.at(1).createReply(QVariantList{provider.updates}));
  ASSERT_TRUE(waitFor([&] { return service.count() == 9; }));
  provider.updates["Count"] = quint32(99);
  bus.send(provider.pending_updates.at(0).createReply(QVariantList{provider.updates}));
  QTest::qWait(30);
  EXPECT_EQ(service.count(), 9);
  bus.send(signal);
  ASSERT_TRUE(waitFor([&] { return provider.pending_updates.size() == 3; }));
  bus.unregisterService("org.holonight.Packages1");
  ASSERT_TRUE(waitFor([&] { return !service.available(); }));
  provider.defer_updates = false;
  provider.updates["Count"] = quint32(5);
  ASSERT_TRUE(bus.registerService("org.holonight.Packages1"));
  ASSERT_TRUE(waitFor([&] { return service.count() == 5; }));
  provider.updates["Count"] = quint32(88);
  bus.send(provider.pending_updates.at(2).createReply(QVariantList{provider.updates}));
  QTest::qWait(30);
  EXPECT_EQ(service.count(), 5);
  EXPECT_FALSE(service.checkMetadataAvailable());
  EXPECT_TRUE(service.errorMessage().isEmpty());
  bus.unregisterService("org.holonight.Packages1");
  bus.unregisterObject("/org/holonight/Packages1");
}
