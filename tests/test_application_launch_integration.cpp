#include "ApplicationLaunchService.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QTest>
#include <QTimer>

#include <gtest/gtest.h>

struct TestSystemdUnit {
  QString name, description, loadState, activeState, subState, following;
  QDBusObjectPath path;
  uint jobId{0};
  QString jobType;
  QDBusObjectPath jobPath;
};
using TestSystemdUnits = QList<TestSystemdUnit>;
Q_DECLARE_METATYPE(TestSystemdUnit)
Q_DECLARE_METATYPE(TestSystemdUnits)
QDBusArgument& operator<<(QDBusArgument& argument, const TestSystemdUnit& unit) {
  argument.beginStructure();
  argument << unit.name << unit.description << unit.loadState << unit.activeState << unit.subState << unit.following
           << unit.path << unit.jobId << unit.jobType << unit.jobPath;
  argument.endStructure();
  return argument;
}
const QDBusArgument& operator>>(const QDBusArgument& argument, TestSystemdUnit& unit) {
  argument.beginStructure();
  argument >> unit.name >> unit.description >> unit.loadState >> unit.activeState >> unit.subState >> unit.following >>
      unit.path >> unit.jobId >> unit.jobType >> unit.jobPath;
  argument.endStructure();
  return argument;
}

namespace {
class SystemdManager final : public QDBusVirtualObject {
 public:
  QString name;
  QHash<QString, QVariant> properties;
  bool reject{false};
  bool targetActive{true};
  bool withholdResult{false};
  int starts{0};
  int stops{0};

  QString introspect(const QString&) const override {
    return QStringLiteral(R"xml(<interface name="org.freedesktop.systemd1.Manager">
      <method name="Subscribe"/>
      <method name="ListUnitsByPatterns"><arg type="as" direction="in"/><arg type="as" direction="in"/><arg type="a(ssssssouso)" direction="out"/></method>
      <method name="GetUnit"><arg type="s" direction="in"/><arg type="o" direction="out"/></method>
      <method name="StartTransientUnit"><arg type="s" direction="in"/><arg type="s" direction="in"/><arg type="a(sv)" direction="in"/><arg type="a(sa(sv))" direction="in"/><arg type="o" direction="out"/></method>
      <method name="StopUnit"><arg type="s" direction="in"/><arg type="s" direction="in"/><arg type="o" direction="out"/></method>
      <signal name="JobRemoved"><arg type="u"/><arg type="o"/><arg type="s"/><arg type="s"/></signal>
    </interface>
    <interface name="org.freedesktop.systemd1.Unit"><property name="ActiveState" type="s" access="read"/></interface>)xml");
  }

  bool handleMessage(const QDBusMessage& message, const QDBusConnection& bus) override {
    if (message.member() == "Introspect") {
      bus.send(message.createReply(QStringLiteral("<node>") + introspect(message.path()) + "</node>"));
    } else if (message.interface() == "org.freedesktop.DBus.Properties" && message.member() == "Get") {
      bus.send(message.createReply(QVariant::fromValue(QDBusVariant(QString(targetActive ? "active" : "inactive")))));
    } else if (message.member() == "ListUnitsByPatterns") {
      bus.send(message.createReply(QVariant::fromValue(TestSystemdUnits{})));
    } else if (message.member() == "Subscribe") {
      bus.send(message.createReply());
    } else if (message.member() == "GetUnit") {
      bus.send(message.createReply(QVariant::fromValue(QDBusObjectPath("/org/freedesktop/systemd1/unit/target"))));
    } else if (message.member() == "StartTransientUnit") {
      ++starts;
      name = message.arguments().at(0).toString();
      const auto argument = qvariant_cast<QDBusArgument>(message.arguments().at(2));
      argument.beginArray();
      while (!argument.atEnd()) {
        QString key;
        QDBusVariant value;
        argument.beginStructure();
        argument >> key >> value;
        argument.endStructure();
        properties.insert(key, value.variant());
      }
      argument.endArray();
      if (reject) {
        bus.send(message.createErrorReply("org.freedesktop.systemd1.AccessDenied", "Rejected test unit"));
        return true;
      }
      const QDBusObjectPath job("/org/freedesktop/systemd1/job/1");
      bus.send(message.createReply(QVariant::fromValue(job)));
      if (!withholdResult)
        QTimer::singleShot(10, this, [bus, job, unit = name] {
          auto signal =
              QDBusMessage::createSignal("/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager", "JobRemoved");
          signal.setArguments({QVariant::fromValue(uint(1)), QVariant::fromValue(job), unit, "done"});
          bus.send(signal);
        });
    } else if (message.member() == "StopUnit") {
      ++stops;
      bus.send(message.createReply(QVariant::fromValue(QDBusObjectPath("/org/freedesktop/systemd1/job/2"))));
    } else {
      bus.send(message.createErrorReply("org.freedesktop.DBus.Error.UnknownMethod", message.member()));
    }
    return true;
  }
};

class ApplicationLaunchPrivateBus : public testing::Test {
 protected:
  void SetUp() override {
    if (qEnvironmentVariable("HOLONIGHT_LAUNCH_PRIVATE_BUS") != "1")
      GTEST_SKIP() << "Requires disposable D-Bus session";
    qDBusRegisterMetaType<TestSystemdUnit>();
    qDBusRegisterMetaType<TestSystemdUnits>();
    ASSERT_TRUE(bus.registerService("org.freedesktop.systemd1"));
    ASSERT_TRUE(bus.registerVirtualObject("/org/freedesktop/systemd1", &manager, QDBusConnection::SubPath));
  }
  void TearDown() override {
    if (qEnvironmentVariable("HOLONIGHT_LAUNCH_PRIVATE_BUS") != "1") return;
    bus.unregisterObject("/org/freedesktop/systemd1", QDBusConnection::UnregisterTree);
    bus.unregisterService("org.freedesktop.systemd1");
  }
  QDBusConnection bus{QDBusConnection::sessionBus()};
  SystemdManager manager;
};

TEST_F(ApplicationLaunchPrivateBus, NativeUnitPreservesArgumentsDirectoryAndLifecycle) {
  ApplicationLaunchService launcher(nullptr, {},
                                    [] { return ApplicationLaunchService::Capabilities{.managerAvailable = true}; });
  QObject context;
  int completions = 0;
  QString error;
  launcher.launch(
      {.program = "/usr/bin/true", .arguments = {"argument with spaces", "--literal=$HOME"}, .working_dir = "/tmp"},
      &context, [&](const QString&, const QString& message) {
        ++completions;
        error = message;
      });
  ASSERT_TRUE(QTest::qWaitFor([&] { return completions == 1; }, 5000));
  EXPECT_TRUE(error.isEmpty()) << error.toStdString();
  EXPECT_EQ(manager.starts, 1);
  EXPECT_TRUE(manager.name.startsWith("app-holonight-true-"));
  EXPECT_EQ(manager.properties.value("Type").toString(), "exec");
  EXPECT_EQ(manager.properties.value("Slice").toString(), "app.slice");
  EXPECT_EQ(manager.properties.value("CollectMode").toString(), "inactive-or-failed");
  EXPECT_EQ(manager.properties.value("Restart").toString(), "no");
  EXPECT_EQ(manager.properties.value("WorkingDirectory").toString(), "/tmp");
  EXPECT_EQ(manager.properties.value("StandardOutput").toString(), "journal");
  EXPECT_EQ(qdbus_cast<QStringList>(manager.properties.value("PartOf")), QStringList{"graphical-session.target"});
  EXPECT_EQ(qdbus_cast<QStringList>(manager.properties.value("After")), QStringList{"graphical-session.target"});
  const auto exec = qvariant_cast<QDBusArgument>(manager.properties.value("ExecStart"));
  QString executable;
  QStringList argv;
  bool ignore = true;
  exec.beginArray();
  exec.beginStructure();
  exec >> executable >> argv >> ignore;
  exec.endStructure();
  exec.endArray();
  EXPECT_EQ(executable, "/usr/bin/true");
  EXPECT_EQ(argv, (QStringList{"/usr/bin/true", "argument with spaces", "--literal=$HOME"}));
  EXPECT_FALSE(ignore);
  const auto environment = qdbus_cast<QStringList>(manager.properties.value("Environment"));
  EXPECT_TRUE(environment.contains("HOLONIGHT_LAUNCH_PRIVATE_BUS=1"));
  for (const auto& item : environment) {
    EXPECT_FALSE(item.startsWith("INVOCATION_ID="));
    EXPECT_FALSE(item.startsWith("NOTIFY_SOCKET="));
    EXPECT_FALSE(item.startsWith("LISTEN_"));
  }
  QTest::qWait(20);
  EXPECT_EQ(completions, 1);
}

TEST_F(ApplicationLaunchPrivateBus, RejectedJobDoesNotRetry) {
  manager.reject = true;
  ApplicationLaunchService launcher(nullptr, {},
                                    [] { return ApplicationLaunchService::Capabilities{.managerAvailable = true}; });
  QObject context;
  int completions = 0;
  QString error;
  launcher.launch({.program = "/usr/bin/true"}, &context, [&](const QString&, const QString& message) {
    ++completions;
    error = message;
  });
  ASSERT_TRUE(QTest::qWaitFor([&] { return completions == 1; }, 5000));
  EXPECT_TRUE(error.contains("Rejected test unit"));
  EXPECT_EQ(manager.starts, 1);
  EXPECT_EQ(manager.stops, 0);
}

TEST_F(ApplicationLaunchPrivateBus, TimeoutStopsOnlyTheNewUnitAndCompletesOnce) {
  manager.withholdResult = true;
  ApplicationLaunchService launcher(nullptr, {},
                                    [] { return ApplicationLaunchService::Capabilities{.managerAvailable = true}; });
  QObject context;
  int completions = 0;
  QString error;
  launcher.launch({.program = "/usr/bin/true"}, &context, [&](const QString&, const QString& message) {
    ++completions;
    error = message;
  });
  ASSERT_TRUE(QTest::qWaitFor([&] { return completions == 1 && manager.stops == 1; }, 18000));
  EXPECT_TRUE(error.contains("timed out"));
  EXPECT_EQ(manager.starts, 1);
  EXPECT_EQ(manager.stops, 1);
  QTest::qWait(20);
  EXPECT_EQ(completions, 1);
}
}  // namespace

TEST_F(ApplicationLaunchPrivateBus, InactiveGraphicalTargetDoesNotAttachApplication) {
  manager.targetActive = false;
  ApplicationLaunchService launcher;
  QObject context;
  int completions = 0;
  launcher.launch({.program = "/usr/bin/true"}, &context, [&](const QString&, const QString& error) {
    EXPECT_TRUE(error.isEmpty());
    ++completions;
  });
  ASSERT_TRUE(QTest::qWaitFor([&] { return completions == 1; }, 5000));
  EXPECT_FALSE(manager.properties.contains("PartOf"));
  EXPECT_FALSE(manager.properties.contains("After"));
}
