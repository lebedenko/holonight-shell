#include "HyprlandBackend.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include <gtest/gtest.h>

namespace {
class FakeHyprlandTransport final : public HyprlandIpcTransport {
 public:
  void connectEventStream() override { ++connect_count; }
  bool runCommand(const QByteArray& command, CommandCompletePredicate is_complete = {}) override {
    Q_UNUSED(is_complete)
    commands.append(command);
    running = true;
    return submit_succeeds;
  }
  [[nodiscard]] bool hasRunningCommand() const override { return running; }
  void connectStream() { emit eventStreamConnected(); }
  void sendEvent(const QByteArray& event) { emit eventLineReceived(event); }
  void finish(const QByteArray& response, bool success = true) {
    running = false;
    emit commandFinished(response, success);
  }

  int connect_count{0};
  QList<QByteArray> commands;
  bool running{false};
  bool submit_succeeds{true};
};

void processDeferred() { QCoreApplication::processEvents(); }

void finishRefresh(FakeHyprlandTransport* transport, const QByteArray& clients = QByteArrayLiteral("[]")) {
  transport->finish(QByteArrayLiteral(R"([{"name":"DP-1","focused":true,"activeWorkspace":{"id":1}}])"));
  transport->finish(QByteArrayLiteral(R"([{"id":1,"name":"1"}])"));
  transport->finish(clients);
}
}  // namespace

TEST(HyprlandBackend, PublishesCompleteRefreshOnlyAfterAllResponses) {
  auto transport = std::make_unique<FakeHyprlandTransport>();
  auto* fake = transport.get();
  HyprlandBackend backend(std::move(transport));
  QSignalSpy snapshots(&backend, &CompositorBackend::snapshotReady);

  backend.start();
  fake->connectStream();
  processDeferred();
  ASSERT_EQ(fake->commands, QList<QByteArray>{QByteArrayLiteral("j/monitors")});
  finishRefresh(fake,
                QByteArrayLiteral(
                    R"([{"address":"0x1","class":"kitty","title":"shell","workspace":{"id":1},"focusHistoryID":0}])"));

  ASSERT_EQ(snapshots.count(), 1);
  const auto snapshot = qvariant_cast<CompositorSnapshot>(snapshots.first().first());
  EXPECT_TRUE(snapshot.connected);
  ASSERT_EQ(snapshot.workspaces.size(), 1);
  EXPECT_TRUE(snapshot.workspaces.first().occupied.value_or(false));
  EXPECT_EQ(snapshot.active_windows.value(QStringLiteral("DP-1")).title, QStringLiteral("shell"));
}

TEST(HyprlandBackend, DiscardsPartialRefreshOnFailure) {
  auto transport = std::make_unique<FakeHyprlandTransport>();
  auto* fake = transport.get();
  HyprlandBackend backend(std::move(transport));
  QSignalSpy snapshots(&backend, &CompositorBackend::snapshotReady);
  fake->connectStream();
  processDeferred();
  fake->finish(QByteArrayLiteral("[]"));
  fake->finish({}, false);

  ASSERT_EQ(snapshots.count(), 1);
  EXPECT_FALSE(qvariant_cast<CompositorSnapshot>(snapshots.first().first()).connected);
}

TEST(HyprlandBackend, CoalescesDirtyRefreshAndQueuesActivation) {
  auto transport = std::make_unique<FakeHyprlandTransport>();
  auto* fake = transport.get();
  HyprlandBackend backend(std::move(transport));
  fake->connectStream();
  processDeferred();
  fake->sendEvent(QByteArrayLiteral("workspace>>2"));
  fake->sendEvent(QByteArrayLiteral("openwindow>>a,2,kitty,title"));
  backend.activateWorkspace(QStringLiteral("4"));
  finishRefresh(fake);

  EXPECT_EQ(fake->commands.last(), QByteArrayLiteral("dispatch workspace 4"));
}

TEST(HyprlandBackend, FallsBackToLuaActivationAndRefreshes) {
  auto transport = std::make_unique<FakeHyprlandTransport>();
  auto* fake = transport.get();
  HyprlandBackend backend(std::move(transport));
  backend.activateWorkspace(QStringLiteral("4"));
  ASSERT_EQ(fake->commands.last(), QByteArrayLiteral("dispatch workspace 4"));
  fake->finish(QByteArrayLiteral("error: unsupported dispatcher"));
  ASSERT_EQ(fake->commands.last(), QByteArrayLiteral("dispatch hl.dsp.focus({ workspace = 4 })"));
  fake->finish(QByteArrayLiteral("ok"));
  processDeferred();
  EXPECT_EQ(fake->commands.last(), QByteArrayLiteral("j/monitors"));
}

TEST(HyprlandBackend, ActivatesSpecialWorkspaceByName) {
  auto transport = std::make_unique<FakeHyprlandTransport>();
  auto* fake = transport.get();
  HyprlandBackend backend(std::move(transport));

  backend.activateWorkspace(QStringLiteral("special:magic"));

  ASSERT_EQ(fake->commands.last(), QByteArrayLiteral("dispatch togglespecialworkspace magic"));
  fake->finish(QByteArrayLiteral("error: unsupported dispatcher"));
  ASSERT_EQ(fake->commands.last(), QByteArrayLiteral("dispatch hl.dsp.workspace.toggle_special(\"magic\")"));
}

TEST(HyprlandBackend, ProjectsVisibleSpecialWorkspaceFromMonitorState) {
  auto transport = std::make_unique<FakeHyprlandTransport>();
  auto* fake = transport.get();
  HyprlandBackend backend(std::move(transport));
  QSignalSpy snapshots(&backend, &CompositorBackend::snapshotReady);
  fake->connectStream();
  processDeferred();
  fake->finish(QByteArrayLiteral(
      R"([{"name":"DP-5","focused":true,"activeWorkspace":{"id":6},"specialWorkspace":{"id":-98,"name":"special:magic"}}])"));
  fake->finish(QByteArrayLiteral(R"([{"id":6,"name":"6"},{"id":-98,"name":"special:magic"}])"));
  fake->sendEvent(QByteArrayLiteral("urgent>>0x1"));
  fake->finish(QByteArrayLiteral(
      R"([{"address":"0x1","class":"teams","title":"Calendar","workspace":{"id":-98},"focusHistoryID":0}])"));

  ASSERT_EQ(snapshots.count(), 1);
  const auto snapshot = qvariant_cast<CompositorSnapshot>(snapshots.first().first());
  const auto special =
      std::ranges::find(snapshot.workspaces, QStringLiteral("special:magic"), &CompositorWorkspace::id);
  ASSERT_NE(special, snapshot.workspaces.end());
  EXPECT_TRUE(special->active);
  EXPECT_TRUE(special->focused);
  EXPECT_FALSE(special->urgent);
  EXPECT_EQ(special->outputs, QStringList{QStringLiteral("DP-5")});
}

TEST(HyprlandBackend, ClearsUrgencyWhenEventAddressOmitsClientPrefix) {
  auto transport = std::make_unique<FakeHyprlandTransport>();
  auto* fake = transport.get();
  HyprlandBackend backend(std::move(transport));
  QSignalSpy snapshots(&backend, &CompositorBackend::snapshotReady);
  fake->connectStream();
  processDeferred();
  fake->sendEvent(QByteArrayLiteral("urgent>>1"));
  finishRefresh(fake,
                QByteArrayLiteral(
                    R"([{"address":"0x1","class":"kitty","title":"shell","workspace":{"id":1},"focusHistoryID":0}])"));

  fake->sendEvent(QByteArrayLiteral("workspace>>2"));
  processDeferred();
  fake->finish(QByteArrayLiteral(R"([{"name":"DP-1","focused":true,"activeWorkspace":{"id":2}}])"));
  fake->finish(QByteArrayLiteral(R"([{"id":1,"name":"1"},{"id":2,"name":"2"}])"));
  fake->finish(QByteArrayLiteral(
      R"([{"address":"0x1","class":"kitty","title":"shell","workspace":{"id":1},"focusHistoryID":0}])"));

  ASSERT_EQ(snapshots.count(), 2);
  const auto snapshot = qvariant_cast<CompositorSnapshot>(snapshots.last().first());
  const auto workspace = std::ranges::find(snapshot.workspaces, QStringLiteral("1"), &CompositorWorkspace::id);
  ASSERT_NE(workspace, snapshot.workspaces.end());
  EXPECT_FALSE(workspace->urgent);
}
