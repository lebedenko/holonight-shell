#include "HyprlandWorkspaceService.h"
#include "WorkspaceModel.h"

#include <gtest/gtest.h>

namespace {
class FakeHyprlandIpcTransport final : public HyprlandIpcTransport {
 public:
  explicit FakeHyprlandIpcTransport(QObject* parent = nullptr) : HyprlandIpcTransport(parent) {}

  void connectEventStream() override { ++connect_count; }

  bool runCommand(const QByteArray& command, CommandCompletePredicate /*predicate*/ = {}) override {
    commands.append(command);
    return submit_succeeds;
  }

  [[nodiscard]] bool hasRunningCommand() const override { return running; }

  void finishCommand(const QByteArray& response = QByteArrayLiteral("ok"), bool success = true) {
    emit commandFinished(response, success);
  }

  int connect_count{0};
  QList<QByteArray> commands;
  bool running{false};
  bool submit_succeeds{true};
};
}  // namespace

TEST(HyprlandWorkspaceService, WorkspaceEventUpdatesFocusedWorkspaceAndRequestsRefresh) {
  const HyprlandWorkspaceEventUpdate update = hyprlandWorkspaceEventUpdate(QByteArrayLiteral("workspace>>4"));

  ASSERT_TRUE(update.focused_workspace_id.has_value());
  EXPECT_EQ(*update.focused_workspace_id, 4);
  EXPECT_TRUE(update.refresh_workspaces);
}

TEST(HyprlandWorkspaceService, FocusedMonitorEventUsesNumericWorkspaceName) {
  const HyprlandWorkspaceEventUpdate update = hyprlandWorkspaceEventUpdate(QByteArrayLiteral("focusedmon>>DP-1, 7 "));

  ASSERT_TRUE(update.focused_workspace_id.has_value());
  EXPECT_EQ(*update.focused_workspace_id, 7);
  EXPECT_TRUE(update.refresh_workspaces);
}

TEST(HyprlandWorkspaceService, FocusedMonitorEventIgnoresNonnumericWorkspaceNameButRefreshes) {
  const HyprlandWorkspaceEventUpdate update =
      hyprlandWorkspaceEventUpdate(QByteArrayLiteral("focusedmon>>DP-1,special:magic"));

  EXPECT_FALSE(update.focused_workspace_id.has_value());
  EXPECT_TRUE(update.refresh_workspaces);
}

TEST(HyprlandWorkspaceService, NonWorkspaceEventsDoNotUpdateOrRefresh) {
  const HyprlandWorkspaceEventUpdate update = hyprlandWorkspaceEventUpdate(QByteArrayLiteral("activewindow>>kitty,sh"));

  EXPECT_FALSE(update.focused_workspace_id.has_value());
  EXPECT_FALSE(update.urgent_window_address.has_value());
  EXPECT_FALSE(update.refresh_workspaces);
}

TEST(HyprlandWorkspaceService, UrgentEventCapturesWindowAddressWithoutWorkspaceRefresh) {
  const HyprlandWorkspaceEventUpdate update = hyprlandWorkspaceEventUpdate(QByteArrayLiteral("urgent>>abc123"));

  EXPECT_FALSE(update.focused_workspace_id.has_value());
  ASSERT_TRUE(update.urgent_window_address.has_value());
  EXPECT_EQ(*update.urgent_window_address, QStringLiteral("abc123"));
  EXPECT_FALSE(update.refresh_workspaces);
}

TEST(HyprlandWorkspaceService, QueuedCommandsPreferLatestDispatchThenActiveThenUrgentThenWorkspaces) {
  HyprlandWorkspaceCommandQueue queue;

  queueHyprlandWorkspaceCommand(&queue, HyprlandWorkspacePendingQuery::ActiveWorkspace,
                                hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery::ActiveWorkspace));
  queueHyprlandWorkspaceCommand(&queue, HyprlandWorkspacePendingQuery::Workspaces,
                                hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery::Workspaces));
  queueHyprlandWorkspaceCommand(&queue, HyprlandWorkspacePendingQuery::UrgentClient, QByteArrayLiteral("abc123"));
  queueHyprlandWorkspaceCommand(&queue, HyprlandWorkspacePendingQuery::DispatchWorkspace,
                                hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery::DispatchWorkspace, 3));
  queueHyprlandWorkspaceCommand(&queue, HyprlandWorkspacePendingQuery::DispatchWorkspace,
                                hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery::DispatchWorkspace, 8));

  std::optional<HyprlandWorkspaceQueuedCommand> next = takeNextHyprlandWorkspaceCommand(&queue);
  ASSERT_TRUE(next.has_value());
  EXPECT_EQ(next->query, HyprlandWorkspacePendingQuery::DispatchWorkspace);
  EXPECT_EQ(next->command, QByteArrayLiteral("dispatch workspace 8"));

  next = takeNextHyprlandWorkspaceCommand(&queue);
  ASSERT_TRUE(next.has_value());
  EXPECT_EQ(next->query, HyprlandWorkspacePendingQuery::ActiveWorkspace);
  EXPECT_EQ(next->command, QByteArrayLiteral("j/activeworkspace"));

  next = takeNextHyprlandWorkspaceCommand(&queue);
  ASSERT_TRUE(next.has_value());
  EXPECT_EQ(next->query, HyprlandWorkspacePendingQuery::UrgentClient);
  EXPECT_EQ(next->command, QByteArrayLiteral("abc123"));

  next = takeNextHyprlandWorkspaceCommand(&queue);
  ASSERT_TRUE(next.has_value());
  EXPECT_EQ(next->query, HyprlandWorkspacePendingQuery::Workspaces);
  EXPECT_EQ(next->command, QByteArrayLiteral("j/workspaces"));

  EXPECT_FALSE(takeNextHyprlandWorkspaceCommand(&queue).has_value());
}

TEST(HyprlandWorkspaceService, UrgentClientCommandUsesClientsJson) {
  EXPECT_EQ(hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery::UrgentClient), QByteArrayLiteral("j/clients"));
}

TEST(HyprlandWorkspaceService, LuaWorkspaceFocusCommandUsesCurrentDispatcherSyntax) {
  EXPECT_EQ(hyprlandLuaWorkspaceFocusCommand(4), QByteArrayLiteral("dispatch hl.dsp.focus({ workspace = 4 })"));
}

TEST(HyprlandWorkspaceService, IpcErrorResponseIsRecognized) {
  EXPECT_TRUE(hyprlandIpcResponseIsError(QByteArrayLiteral("error: unsupported dispatcher")));
  EXPECT_FALSE(hyprlandIpcResponseIsError(QByteArrayLiteral("ok")));
}

TEST(HyprlandWorkspaceService, ActiveWorkspaceCommandResultAppliesFocusedWorkspace) {
  const HyprlandWorkspaceCommandResult result = hyprlandWorkspaceCommandResult(
      HyprlandWorkspacePendingQuery::ActiveWorkspace, QByteArrayLiteral(R"({"id":5})"), true);

  ASSERT_TRUE(result.focused_workspace_id.has_value());
  EXPECT_EQ(*result.focused_workspace_id, 5);
  EXPECT_FALSE(result.occupied_workspace_ids.has_value());
  EXPECT_FALSE(result.refresh_active_workspace);
  EXPECT_FALSE(result.refresh_workspaces);
}

TEST(HyprlandWorkspaceService, WorkspacesCommandResultAppliesOccupiedWorkspaces) {
  const HyprlandWorkspaceCommandResult result = hyprlandWorkspaceCommandResult(
      HyprlandWorkspacePendingQuery::Workspaces,
      QByteArrayLiteral(R"([{"id":1,"windows":2},{"id":2,"windows":0},{"id":9,"windows":1}])"), true);

  ASSERT_TRUE(result.occupied_workspace_ids.has_value());
  EXPECT_TRUE(result.occupied_workspace_ids->contains(1));
  EXPECT_FALSE(result.occupied_workspace_ids->contains(2));
  EXPECT_TRUE(result.occupied_workspace_ids->contains(9));
}

TEST(HyprlandWorkspaceService, UrgentClientCommandResultFindsWorkspace) {
  const HyprlandWorkspaceCommandResult result = hyprlandWorkspaceCommandResult(
      HyprlandWorkspacePendingQuery::UrgentClient,
      QByteArrayLiteral(
          R"([{"address":"0xabc123","class":"Code","title":"ping","workspace":{"id":7},"focusHistoryID":0}])"),
      true, QStringLiteral("abc123"));

  ASSERT_TRUE(result.urgent_workspace_id.has_value());
  EXPECT_EQ(*result.urgent_workspace_id, 7);
}

TEST(HyprlandWorkspaceService, DispatchCommandResultTriggersBothRefreshes) {
  const HyprlandWorkspaceCommandResult result =
      hyprlandWorkspaceCommandResult(HyprlandWorkspacePendingQuery::DispatchWorkspace, QByteArrayLiteral("ok"), true);

  EXPECT_FALSE(result.focused_workspace_id.has_value());
  EXPECT_FALSE(result.occupied_workspace_ids.has_value());
  EXPECT_TRUE(result.refresh_active_workspace);
  EXPECT_TRUE(result.refresh_workspaces);
}

TEST(HyprlandWorkspaceService, CommandResultIgnoresUnparsedBuffers) {
  const HyprlandWorkspaceCommandResult result = hyprlandWorkspaceCommandResult(
      HyprlandWorkspacePendingQuery::ActiveWorkspace, QByteArrayLiteral(R"({"id":5})"), false);

  EXPECT_FALSE(result.focused_workspace_id.has_value());
  EXPECT_FALSE(result.occupied_workspace_ids.has_value());
  EXPECT_FALSE(result.refresh_active_workspace);
  EXPECT_FALSE(result.refresh_workspaces);
}

TEST(HyprlandWorkspaceService, EmptyWorkspaceActivationSubmitsHyprlandDispatch) {
  WorkspaceModel model;
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  HyprlandWorkspaceService service(&model, std::move(transport));

  model.activateWorkspace(4);

  ASSERT_EQ(fake->commands.size(), 1);
  EXPECT_EQ(fake->commands.front(), QByteArrayLiteral("dispatch workspace 4"));
}

TEST(HyprlandWorkspaceService, QueuedWorkspaceActivationSubmitsAfterCurrentCommandCompletes) {
  WorkspaceModel model;
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  HyprlandWorkspaceService service(&model, std::move(transport));
  service.start();

  fake->running = true;
  model.activateWorkspace(4);
  EXPECT_TRUE(fake->commands.isEmpty());

  fake->running = false;
  fake->finishCommand();

  ASSERT_EQ(fake->commands.size(), 1);
  EXPECT_EQ(fake->commands.front(), QByteArrayLiteral("dispatch workspace 4"));
}

TEST(HyprlandWorkspaceService, LegacyWorkspaceDispatchErrorRetriesWithLuaDispatcher) {
  WorkspaceModel model;
  auto transport = std::make_unique<FakeHyprlandIpcTransport>();
  FakeHyprlandIpcTransport* fake = transport.get();
  HyprlandWorkspaceService service(&model, std::move(transport));
  service.start();

  model.activateWorkspace(4);
  ASSERT_EQ(fake->commands.size(), 1);
  EXPECT_EQ(fake->commands.at(0), QByteArrayLiteral("dispatch workspace 4"));

  fake->finishCommand(QByteArrayLiteral("error: legacy dispatch syntax is unavailable"));

  ASSERT_EQ(fake->commands.size(), 2);
  EXPECT_EQ(fake->commands.at(1), QByteArrayLiteral("dispatch hl.dsp.focus({ workspace = 4 })"));
}
