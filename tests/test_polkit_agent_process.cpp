#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusVirtualObject>
#include <QFile>
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
  environment.remove(QStringLiteral("QML_IMPORT_PATH"));
  environment.remove(QStringLiteral("QML2_IMPORT_PATH"));
  agent.setProcessEnvironment(environment);
  const QString executable = qEnvironmentVariable("UQC_POLKIT_EXECUTABLE", QStringLiteral(TEST_POLKIT_AGENT_PATH));
  QStringList arguments;
  if (qEnvironmentVariableIsSet("UQC_STYLE_CLI")) {
    arguments = {QStringLiteral("-style"), QStringLiteral("Fusion")};
  }
  agent.start(executable, arguments);
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
  QTest::qWait(qEnvironmentVariableIsSet("UQC_POLKIT_LOG") ? 1500 : 250);
  ASSERT_FALSE(pending.isFinished()) << pending.error().message().toStdString();
  agent.terminate();
  ASSERT_TRUE(QTest::qWaitFor([&] { return agent.state() == QProcess::NotRunning; }, 5000))
      << "SIGTERM did not stop the persistent authentication window";
  EXPECT_EQ(agent.exitStatus(), QProcess::NormalExit);
  const QByteArray diagnostics = agent.readAllStandardError();
  EXPECT_EQ(agent.exitCode(), 0) << diagnostics.constData();
  if (qEnvironmentVariableIsSet("UQC_POLKIT_LOG")) {
    QFile log(qEnvironmentVariable("UQC_POLKIT_LOG"));
    ASSERT_TRUE(log.open(QIODevice::WriteOnly));
    EXPECT_EQ(log.write(diagnostics), diagnostics.size());
  }
  EXPECT_TRUE(authority.unregistered);
  ASSERT_TRUE(QTest::qWaitFor([&] { return pending.isFinished(); }, 3000));
  EXPECT_EQ(pending.reply().type(), QDBusMessage::ErrorMessage);
  EXPECT_EQ(pending.error().name(), QStringLiteral("org.freedesktop.PolicyKit1.Error.Cancelled"));
  EXPECT_TRUE(agent.readAllStandardOutput().isEmpty());
}

class PolkitCancellationProcess : public testing::TestWithParam<int> {};

TEST_P(PolkitCancellationProcess, RepliesAndRequesterExitsWhileAgentRemainsAvailable) {
  QProcess bus;
  QProcess agent;
  QProcess requester;
  const QString connection_name = QStringLiteral("polkit-cancellation-test");
  const auto cleanup = qScopeGuard([&] {
    for (auto* process : {&requester, &agent, &bus}) {
      if (process->state() != QProcess::NotRunning) {
        process->kill();
        process->waitForFinished();
      }
    }
    QDBusConnection::disconnectFromBus(connection_name);
  });
  bus.start(QStringLiteral("dbus-daemon"),
            {QStringLiteral("--session"), QStringLiteral("--nofork"), QStringLiteral("--print-address=1")});
  ASSERT_TRUE(bus.waitForStarted());
  ASSERT_TRUE(bus.waitForReadyRead());
  const QString address = QString::fromUtf8(bus.readLine()).trimmed();
  auto connection = QDBusConnection::connectToBus(address, connection_name);
  ASSERT_TRUE(connection.isConnected());
  RegistrationAuthority authority;
  ASSERT_TRUE(connection.registerVirtualObject(QStringLiteral("/org/freedesktop/PolicyKit1/Authority"), &authority));
  ASSERT_TRUE(connection.registerService(QStringLiteral("org.freedesktop.PolicyKit1")));
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), address);
  const bool production = GetParam() == 2;
  environment.insert(QStringLiteral("XDG_SESSION_ID"), QStringLiteral("isolated-cancellation"));
  environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
  agent.setProcessEnvironment(environment);
  agent.start(
      production ? qEnvironmentVariable("UQC_POLKIT_EXECUTABLE", QStringLiteral(TEST_POLKIT_AGENT_PATH))
                 : QStringLiteral(TEST_CANCELLATION_AGENT_PATH),
      production ? QStringList{} : QStringList{GetParam() == 1 ? QStringLiteral("failure") : QStringLiteral("direct")});
  ASSERT_TRUE(agent.waitForStarted());
  ASSERT_TRUE(QTest::qWaitFor([&] { return authority.registered; }, 5000));
  qDBusRegisterMetaType<AgentTestIdentity>();
  qDBusRegisterMetaType<QList<AgentTestIdentity>>();
  qDBusRegisterMetaType<QMap<QString, QString>>();
  QList<AgentTestIdentity> identities{
      {.kind = QStringLiteral("unix-user"), .details = {{QStringLiteral("uid"), 1000U}}}};
  if (production) {
    identities.append({.kind = QStringLiteral("unix-user"), .details = {{QStringLiteral("uid"), 0U}}});
  }
  const auto cancelFromAuthority = [&](const QString& cookie) {
    QTest::qWait(100);
    auto cancel = QDBusMessage::createMethodCall(authority.agent_service, QStringLiteral("/org/holonight/PolkitAgent"),
                                                 QStringLiteral("org.freedesktop.PolicyKit1.AuthenticationAgent"),
                                                 QStringLiteral("CancelAuthentication"));
    cancel.setArguments({cookie});
    QDBusPendingCallWatcher cancelled(connection.asyncCall(cancel));
    EXPECT_TRUE(QTest::qWaitFor([&] { return cancelled.isFinished(); }, 3000));
    EXPECT_EQ(cancelled.reply().type(), QDBusMessage::ReplyMessage);
  };
  auto request = QDBusMessage::createMethodCall(authority.agent_service, QStringLiteral("/org/holonight/PolkitAgent"),
                                                QStringLiteral("org.freedesktop.PolicyKit1.AuthenticationAgent"),
                                                QStringLiteral("BeginAuthentication"));
  request.setArguments({QStringLiteral("org.example.test"), QStringLiteral("Synthetic cancellation"), QString{},
                        QVariant::fromValue(QMap<QString, QString>{}), QStringLiteral("pending-cookie"),
                        QVariant::fromValue(identities)});
  QDBusPendingCallWatcher pending(connection.asyncCall(request, 10000));
  if (production) {
    cancelFromAuthority(QStringLiteral("pending-cookie"));
  }
  ASSERT_TRUE(QTest::qWaitFor([&] { return pending.isFinished(); }, 3000))
      << "BeginAuthentication never replied: " << agent.readAllStandardError().constData();
  EXPECT_EQ(pending.reply().type(), QDBusMessage::ErrorMessage);
  if (production) {
    EXPECT_TRUE(pending.error().message().contains(QStringLiteral("cancelled"), Qt::CaseInsensitive));
  } else {
    EXPECT_EQ(pending.error().name(), QStringLiteral("org.freedesktop.PolicyKit1.Error.Cancelled"));
  }

  // A second request from a real child process must exit on the reply, before its
  // own timeout, while the same persistent agent handles it without stale callbacks.
  requester.start(
      QStringLiteral("gdbus"),
      {QStringLiteral("call"), QStringLiteral("--address"), address, QStringLiteral("--dest"), authority.agent_service,
       QStringLiteral("--object-path"), QStringLiteral("/org/holonight/PolkitAgent"), QStringLiteral("--method"),
       QStringLiteral("org.freedesktop.PolicyKit1.AuthenticationAgent.BeginAuthentication"),
       QStringLiteral("--timeout"), QStringLiteral("10"), QStringLiteral("org.example.test"),
       QStringLiteral("Synthetic cancellation"), QStringLiteral(""), QStringLiteral("{}"),
       QStringLiteral("requester-cookie"),
       production ? QStringLiteral("[('unix-user', {'uid': <uint32 1000>}), ('unix-user', {'uid': <uint32 0>})]")
                  : QStringLiteral("[('unix-user', {'uid': <uint32 1000>})]")});
  ASSERT_TRUE(requester.waitForStarted());
  if (production) {
    cancelFromAuthority(QStringLiteral("requester-cookie"));
  }
  ASSERT_TRUE(QTest::qWaitFor([&] { return requester.state() == QProcess::NotRunning; }, 3000))
      << "Requester did not exit; cleanup kill is not cancellation completion";
  EXPECT_EQ(requester.exitStatus(), QProcess::NormalExit);
  EXPECT_EQ(requester.exitCode(), 1);
  const auto requester_error = requester.readAllStandardError();
  EXPECT_TRUE(production ? requester_error.toLower().contains("cancelled")
                         : requester_error.contains("org.freedesktop.PolicyKit1.Error.Cancelled"))
      << requester_error.constData();
  EXPECT_EQ(agent.state(), QProcess::Running);
}

INSTANTIATE_TEST_SUITE_P(DirectFailureAndAuthorityCancellation, PolkitCancellationProcess, testing::Values(0, 1, 2));
}  // namespace
