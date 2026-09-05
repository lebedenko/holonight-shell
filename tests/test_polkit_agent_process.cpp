#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusVirtualObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QTest>

#include <gtest/gtest.h>

struct AgentTestIdentity {
  QString kind;
  QVariantMap details;
};
Q_DECLARE_METATYPE(AgentTestIdentity)

QDBusArgument& operator<<(QDBusArgument& argument, const AgentTestIdentity& identity) {
  argument.beginStructure();
  argument << identity.kind << identity.details;
  argument.endStructure();
  return argument;
}
const QDBusArgument& operator>>(const QDBusArgument& argument, AgentTestIdentity& identity) {
  argument.beginStructure();
  argument >> identity.kind >> identity.details;
  argument.endStructure();
  return argument;
}

namespace {
class RegistrationAuthority final : public QDBusVirtualObject {
 public:
  [[nodiscard]] QString introspect(const QString& /*path*/) const override { return {}; }
  bool handleMessage(const QDBusMessage& message, const QDBusConnection& connection) override {
    if (message.interface() == QStringLiteral("org.freedesktop.DBus.Properties") &&
        message.member() == QStringLiteral("GetAll")) {
      return connection.send(message.createReply(QVariantList{QVariantMap{}}));
    }
    if (message.member() == QStringLiteral("RegisterAuthenticationAgent")) {
      registered = true;
      agent_service = message.service();
      return connection.send(message.createReply());
    }
    if (message.member() == QStringLiteral("UnregisterAuthenticationAgent")) {
      unregistered = true;
      return connection.send(message.createReply());
    }
    return false;
  }
  bool registered = false;
  bool unregistered = false;
  QString agent_service;
};

TEST(PolkitAgentProcess, SigtermExitsPersistentDialogAndUnregisters) {
  // A private bus replaces only the authority. The production executable,
  // signal handler, QML window, and listener teardown all run unchanged.
  QProcess bus;
  QProcess agent;
  const QString connection_name = QStringLiteral("polkit-process-test");
  const auto cleanup = qScopeGuard([&] {
    if (agent.state() != QProcess::NotRunning) {
      agent.kill();
      agent.waitForFinished();
    }
    QDBusConnection::disconnectFromBus(connection_name);
    bus.terminate();
    bus.waitForFinished();
  });
  bus.start(QStringLiteral("dbus-daemon"),
            {QStringLiteral("--session"), QStringLiteral("--nofork"), QStringLiteral("--print-address=1")});
  ASSERT_TRUE(bus.waitForStarted());
  ASSERT_TRUE(bus.waitForReadyRead());
  const QString address = QString::fromUtf8(bus.readLine()).trimmed();
  ASSERT_FALSE(address.isEmpty());
  QDBusConnection connection = QDBusConnection::connectToBus(address, connection_name);
  ASSERT_TRUE(connection.isConnected());
  RegistrationAuthority authority;
  ASSERT_TRUE(connection.registerVirtualObject(QStringLiteral("/org/freedesktop/PolicyKit1/Authority"), &authority));
  ASSERT_TRUE(connection.registerService(QStringLiteral("org.freedesktop.PolicyKit1")));

  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), address);
  environment.insert(QStringLiteral("XDG_SESSION_ID"), QStringLiteral("isolated-test-session"));
  environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
  environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
  agent.setProcessEnvironment(environment);
  agent.start(QStringLiteral(TEST_POLKIT_AGENT_PATH), {});
  ASSERT_TRUE(agent.waitForStarted());
  ASSERT_TRUE(QTest::qWaitFor([&] { return authority.registered; }, 5000)) << agent.readAllStandardError().constData();
  // Present identity selection without starting PAM. Allow the offscreen window
  // to render before sending SIGTERM; registration precedes loading the QML.
  qDBusRegisterMetaType<AgentTestIdentity>();
  qDBusRegisterMetaType<QList<AgentTestIdentity>>();
  qDBusRegisterMetaType<QMap<QString, QString>>();
  const QList<AgentTestIdentity> identities{
      {.kind = QStringLiteral("unix-user"), .details = {{QStringLiteral("uid"), 0U}}},
      {.kind = QStringLiteral("unix-user"), .details = {{QStringLiteral("uid"), 65534U}}}};
  auto request = QDBusMessage::createMethodCall(authority.agent_service, QStringLiteral("/org/holonight/PolkitAgent"),
                                                QStringLiteral("org.freedesktop.PolicyKit1.AuthenticationAgent"),
                                                QStringLiteral("BeginAuthentication"));
  request.setArguments({QStringLiteral("org.example.test"), QStringLiteral("Synthetic shutdown check"), QString{},
                        QVariant::fromValue(QMap<QString, QString>{}), QStringLiteral("test-cookie"),
                        QVariant::fromValue(identities)});
  QDBusPendingCallWatcher pending(connection.asyncCall(request));
  QTest::qWait(250);
  ASSERT_FALSE(pending.isFinished()) << pending.error().message().toStdString();
  agent.terminate();
  ASSERT_TRUE(QTest::qWaitFor([&] { return agent.state() == QProcess::NotRunning; }, 5000))
      << "SIGTERM did not stop the persistent authentication window";
  EXPECT_EQ(agent.exitStatus(), QProcess::NormalExit);
  EXPECT_EQ(agent.exitCode(), 0) << agent.readAllStandardError().constData();
  EXPECT_TRUE(authority.unregistered);
  EXPECT_TRUE(agent.readAllStandardOutput().isEmpty());
}
}  // namespace
