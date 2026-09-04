#include "CompositorBackend.h"
#include "CompositorService.h"
#include "WindowActivationServer.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>

#include <gtest/gtest.h>
#include <memory>

namespace {

class FakeActivationBackend final : public CompositorBackend {
 public:
  void start() override {}
  void activateWorkspace(const QString& /*workspace_id*/) override {}
  WindowActivationResult requestWindowActivation(const WindowActivationRequest& request) override {
    requests.append(request);
    return result;
  }

  WindowActivationResult result{WindowActivationResult::Accepted};
  QList<WindowActivationRequest> requests;
};

struct ServerFixture {
  ServerFixture() {
    auto backend = std::make_unique<FakeActivationBackend>();
    fake = backend.get();
    compositor = std::make_unique<CompositorService>(CompositorKind::Sway, std::move(backend));
    compositor->publishSnapshotForTest({.connected = true, .capabilities = {.window_activation = true}});
  }

  FakeActivationBackend* fake{};
  std::unique_ptr<CompositorService> compositor;
};

}  // namespace

TEST(WindowActivationServerTest, MapsOnlyAcceptedBackendResultToTrue) {
  ServerFixture fixture;
  WindowActivationServer server(fixture.compositor.get());

  const QList rejected_results{WindowActivationResult::InvalidRequest, WindowActivationResult::Unsupported,
                               WindowActivationResult::Disconnected,   WindowActivationResult::Missing,
                               WindowActivationResult::Ambiguous,      WindowActivationResult::Busy,
                               WindowActivationResult::Failed};
  for (const WindowActivationResult result : rejected_results) {
    fixture.fake->result = result;
    EXPECT_FALSE(server.requestWindowActivation({42U}, QStringLiteral("terminal")));
  }
  fixture.fake->result = WindowActivationResult::Accepted;
  EXPECT_TRUE(server.requestWindowActivation({42U}, QStringLiteral("terminal")));
}

TEST(WindowActivationServerTest, RejectsInvalidRequestsBeforeBackend) {
  ServerFixture fixture;
  WindowActivationServer server(fixture.compositor.get());
  const QString oversized_title(WindowActivationRequest::kMaximumTitleHintBytes + 1, QLatin1Char('x'));
  QList<quint32> oversized_lineage(WindowActivationRequest::kMaximumLineageEntries + 1, 42U);

  EXPECT_FALSE(server.requestWindowActivation({}, {}));
  EXPECT_FALSE(server.requestWindowActivation({0U}, {}));
  EXPECT_FALSE(server.requestWindowActivation(oversized_lineage, {}));
  EXPECT_FALSE(server.requestWindowActivation({42U}, oversized_title));
  EXPECT_FALSE(server.requestWindowActivation({42U}, QString(QChar(u'\0'))));
  EXPECT_TRUE(fixture.fake->requests.isEmpty());
}

TEST(WindowActivationServerTest, DBusExportsExactContractAndPreservesPayload) {
  ServerFixture fixture;
  WindowActivationServer server(fixture.compositor.get());
  ASSERT_TRUE(server.start());
  EXPECT_TRUE(server.start());

  QDBusInterface introspection(QLatin1String(WindowActivationServer::kServiceName),
                               QLatin1String(WindowActivationServer::kObjectPath),
                               QStringLiteral("org.freedesktop.DBus.Introspectable"));
  const QDBusReply<QString> xml = introspection.call(QStringLiteral("Introspect"));
  ASSERT_TRUE(xml.isValid()) << qPrintable(xml.error().message());
  EXPECT_TRUE(xml.value().contains(QStringLiteral("org.holonight.Shell.WindowActivation1")));
  EXPECT_TRUE(xml.value().contains(QStringLiteral("name=\"RequestWindowActivation\"")));
  EXPECT_TRUE(xml.value().contains(QStringLiteral("type=\"au\" direction=\"in\"")));
  EXPECT_TRUE(xml.value().contains(QStringLiteral("type=\"s\" direction=\"in\"")));
  EXPECT_TRUE(xml.value().contains(QStringLiteral("type=\"b\" direction=\"out\"")));
  const qsizetype interface_start = xml.value().indexOf(QStringLiteral("org.holonight.Shell.WindowActivation1"));
  ASSERT_NE(interface_start, -1);
  const qsizetype interface_end = xml.value().indexOf(QStringLiteral("</interface>"), interface_start);
  ASSERT_NE(interface_end, -1);
  EXPECT_FALSE(xml.value().mid(interface_start, interface_end - interface_start).contains(QStringLiteral("signal")));

  QDBusMessage call = QDBusMessage::createMethodCall(
      QLatin1String(WindowActivationServer::kServiceName), QLatin1String(WindowActivationServer::kObjectPath),
      QLatin1String(WindowActivationServer::kInterfaceName), QStringLiteral("RequestWindowActivation"));
  const QList<quint32> lineage{91U, 73U, 11U};
  const QString title = QStringLiteral("Exact — Terminal [α]");
  call << QVariant::fromValue(lineage) << title;
  const QDBusMessage reply = QDBusConnection::sessionBus().call(call);
  ASSERT_EQ(reply.type(), QDBusMessage::ReplyMessage) << qPrintable(reply.errorMessage());
  ASSERT_EQ(reply.arguments().size(), 1);
  EXPECT_TRUE(reply.arguments().constFirst().toBool());
  ASSERT_EQ(fixture.fake->requests.size(), 1);
  EXPECT_EQ(fixture.fake->requests.constFirst().process_lineage, lineage);
  EXPECT_EQ(fixture.fake->requests.constFirst().title_hint, title);
}

TEST(WindowActivationServerTest, ConflictsFailAndDestructionReleasesRegistrations) {
  ServerFixture fixture;
  auto first = std::make_unique<WindowActivationServer>(fixture.compositor.get());
  ASSERT_TRUE(first->start());
  WindowActivationServer conflict(fixture.compositor.get());
  EXPECT_FALSE(conflict.start());
  EXPECT_FALSE(conflict.start());

  first.reset();
  WindowActivationServer replacement(fixture.compositor.get());
  EXPECT_TRUE(replacement.start());
}

TEST(WindowActivationServerTest, ObjectConflictRollsBackClaimedService) {
  ServerFixture fixture;
  QObject blocker;
  QDBusConnection bus = QDBusConnection::sessionBus();
  ASSERT_TRUE(bus.registerObject(QLatin1String(WindowActivationServer::kObjectPath), &blocker));

  WindowActivationServer conflicted(fixture.compositor.get());
  EXPECT_FALSE(conflicted.start());
  EXPECT_FALSE(bus.interface()->isServiceRegistered(QLatin1String(WindowActivationServer::kServiceName)));

  bus.unregisterObject(QLatin1String(WindowActivationServer::kObjectPath));
  WindowActivationServer replacement(fixture.compositor.get());
  EXPECT_TRUE(replacement.start());
}
