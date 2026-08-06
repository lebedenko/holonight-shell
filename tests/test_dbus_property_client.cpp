#include "DbusPropertyClient.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

#include <gtest/gtest.h>

class MockDbusObject : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.example.MockInterface")
  Q_PROPERTY(QString TestProp READ testProp WRITE setTestProp)

 public:
  [[nodiscard]] QString testProp() const { return test_prop_; }
  void setTestProp(const QString& val) { test_prop_ = val; }

 Q_SIGNALS:
  void someSignal(QString msg);  // NOLINT(readability-inconsistent-declaration-parameter-name)

 private:
  QString test_prop_{"initial_val"};
};

class MockUPowerObject : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.UPower")

 public Q_SLOTS:
  [[nodiscard]] static QList<QDBusObjectPath> EnumerateDevices() {  // NOLINT(readability-identifier-naming)
    return {QDBusObjectPath("/org/freedesktop/UPower/devices/battery_BAT0")};
  }
};

class SignalReceiver : public QObject {
  Q_OBJECT
 public:
  QString received_msg;
 public Q_SLOTS:
  void handleSignal(QString msg) { received_msg = std::move(msg); }
};

class DbusPropertyClientTest : public ::testing::Test {
 protected:
  MockDbusObject mock_obj;       // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  MockUPowerObject mock_upower;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

  void SetUp() override {
    QtDbusPropertyClient::setDbusConnection(QDBusConnection::sessionBus());

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.registerService("org.example.MockService");
    bus.registerService("org.freedesktop.UPower");

    const auto export_flags =
        QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals;
    bus.registerObject("/org/example/mock", &mock_obj, export_flags);
    bus.registerObject("/org/freedesktop/UPower", &mock_upower, export_flags);
  }

  void TearDown() override {
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject("/org/example/mock");
    bus.unregisterObject("/org/freedesktop/UPower");
    bus.unregisterService("org.example.MockService");
    bus.unregisterService("org.freedesktop.UPower");

    QtDbusPropertyClient::resetDbusConnection();
  }
};

TEST_F(DbusPropertyClientTest, ChecksSystemBusConnected) {
  QtDbusPropertyClient client;
  EXPECT_TRUE(client.systemBusConnected());
}

TEST_F(DbusPropertyClientTest, ChecksServiceRegistered) {
  QtDbusPropertyClient client;
  EXPECT_TRUE(client.serviceRegistered("org.example.MockService"));
  EXPECT_FALSE(client.serviceRegistered("org.example.NonExistentService"));
}

TEST_F(DbusPropertyClientTest, WarnsWhenServiceRegistrationCheckFails) {
  QtDbusPropertyClient client;

  QtDbusPropertyClient::setDbusConnection(QDBusConnection(QString()));
  QTest::ignoreMessage(QtWarningMsg, "D-Bus service registration interface is unavailable: org.example.MockService");
  EXPECT_FALSE(client.serviceRegistered("org.example.MockService"));
}

TEST_F(DbusPropertyClientTest, GetsPropertySuccessAndFailure) {
  QtDbusPropertyClient client;
  auto val = client.property("org.example.MockService", "/org/example/mock", "org.example.MockInterface", "TestProp");
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(val->toString(), "initial_val");

  auto val_fail =
      client.property("org.example.MockService", "/org/example/mock", "org.example.MockInterface", "NonExistentProp");
  EXPECT_FALSE(val_fail.has_value());
}

TEST_F(DbusPropertyClientTest, GetsAllPropertiesSuccessAndFailure) {
  QtDbusPropertyClient client;
  auto props = client.allProperties("org.example.MockService", "/org/example/mock", "org.example.MockInterface");
  ASSERT_TRUE(props.has_value());
  EXPECT_EQ(props->value("TestProp").toString(), "initial_val");

  auto props_fail =
      client.allProperties("org.example.NonExistentService", "/org/example/mock", "org.example.MockInterface");
  EXPECT_FALSE(props_fail.has_value());
}

TEST_F(DbusPropertyClientTest, SetsPropertySuccessAndFailure) {
  QtDbusPropertyClient client;
  bool parse_ok = client.setProperty("org.example.MockService", "/org/example/mock", "org.example.MockInterface",
                                     "TestProp", "new_val");
  EXPECT_TRUE(parse_ok);
  EXPECT_EQ(mock_obj.testProp(), "new_val");

  bool ok_fail = client.setProperty("org.example.MockService", "/org/example/mock", "org.example.MockInterface",
                                    "NonExistentProp", "val");
  EXPECT_FALSE(ok_fail);
}

TEST_F(DbusPropertyClientTest, GetsUpowerDevicesSuccess) {
  QtDbusPropertyClient client;
  auto devices = client.upowerDevices();
  ASSERT_TRUE(devices.has_value());
  ASSERT_EQ(devices->size(), 1);
  EXPECT_EQ(devices->first().path(), "/org/freedesktop/UPower/devices/battery_BAT0");
}

TEST_F(DbusPropertyClientTest, ConnectsAndDisconnectsSignals) {
  QtDbusPropertyClient client;
  SignalReceiver receiver;

  bool connected = client.connectSignal("org.example.MockService", "/org/example/mock", "org.example.MockInterface",
                                        "someSignal", &receiver, SLOT(handleSignal(QString)));
  ASSERT_TRUE(connected);

  emit mock_obj.someSignal("hello");

  // Wait for the signal to be delivered asynchronously
  for (int i = 0; i < 50 && receiver.received_msg.isEmpty(); ++i) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QThread::msleep(10);
  }

  EXPECT_EQ(receiver.received_msg, "hello");

  receiver.received_msg.clear();

  bool disconnected =
      client.disconnectSignal("org.example.MockService", "/org/example/mock", "org.example.MockInterface", "someSignal",
                              &receiver, SLOT(handleSignal(QString)));
  ASSERT_TRUE(disconnected);

  emit mock_obj.someSignal("world");

  // Wait to confirm it was NOT delivered
  for (int i = 0; i < 10; ++i) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QThread::msleep(10);
  }

  EXPECT_TRUE(receiver.received_msg.isEmpty());
}

#include "test_dbus_property_client.moc"
