#include "ForeignToplevelState.h"
#include "ToplevelModel.h"
#include "WindowCatalog.h"
#include "WindowPresentation.h"

#include <gtest/gtest.h>

TEST(LabwcWindows, PropertiesCommitAtomically) {
  ForeignToplevelState state;
  quint64 order = 0;
  state.pending.window = {.app_id = "app", .title = "title"};
  state.setActivated(true, order);
  EXPECT_TRUE(state.committed.window.title.isEmpty());
  EXPECT_EQ(ForeignToplevelState::active({&state}), nullptr);
  state.commit();
  EXPECT_EQ(state.committed.window.title, "title");
  EXPECT_EQ(ForeignToplevelState::active({&state}), &state);
  state.pending.window.title = "new title";
  state.pending.window.app_id = "new app";
  EXPECT_EQ(state.committed.window.app_id, "app");
  state.commit();
  EXPECT_EQ(state.committed.window.title, "new title");
  EXPECT_EQ(state.committed.window.app_id, "new app");
}

TEST(LabwcWindows, FocusTransferInEitherOrderAndClosure) {
  for (bool deactivate_first : {false, true}) {
    ForeignToplevelState first, second;
    quint64 order = 0;
    first.setActivated(true, order);
    first.commit();
    if (deactivate_first) {
      first.setActivated(false, order);
      first.commit();
    }
    second.setActivated(true, order);
    second.commit();
    EXPECT_EQ(ForeignToplevelState::active({&first, &second}), &second);
    first.setActivated(false, order);
    first.commit();
    EXPECT_EQ(ForeignToplevelState::active({&first, &second}), &second);
    EXPECT_EQ(ForeignToplevelState::active({&first}), nullptr);
    EXPECT_EQ(ForeignToplevelState::active({}), nullptr);
  }
}

TEST(LabwcWindows, OutputMovementSpanningUnknownAndRemoval) {
  ForeignToplevelState state;
  int first_token, second_token, unknown_token;
  auto* first = reinterpret_cast<wl_output*>(&first_token);
  auto* second = reinterpret_cast<wl_output*>(&second_token);
  auto* unknown = reinterpret_cast<wl_output*>(&unknown_token);
  QHash<wl_output*, QString> names{{first, "DP-1"}, {second, "DP-2"}};
  state.pending.window.title = "title";
  state.pending.outputs = {first};
  state.commit();
  EXPECT_EQ(state.onOutputs(names).size(), 1);
  EXPECT_FALSE(state.onOutputs(names).contains("DP-2"));
  state.pending.outputs.insert(second);
  EXPECT_EQ(state.onOutputs(names).size(), 1);
  state.commit();
  EXPECT_EQ(state.onOutputs(names).size(), 2);
  state.pending.outputs.remove(first);
  state.pending.outputs.insert(unknown);
  state.commit();
  EXPECT_EQ(state.onOutputs(names).size(), 1);
  names.remove(second);
  EXPECT_TRUE(state.onOutputs(names).isEmpty());
}

TEST(LabwcWindows, SourcesMergeIndependentlyAndProtocolAvailabilitySurvivesEmptyWindows) {
  CompositorSnapshot workspace{.connected = true,
                               .capabilities = {.workspace_listing = true, .workspace_activation = true},
                               .workspaces = {{.id = "one"}}};
  QHash<QString, CompositorActiveWindow> windows{{"DP-1", {.app_id = "app", .title = "title"}}};
  auto snapshot = mergeLabwcSnapshot(workspace, true, windows);
  EXPECT_TRUE(snapshot.capabilities.active_window);
  EXPECT_EQ(snapshot.workspaces.size(), 1);
  EXPECT_EQ(snapshot.active_windows.value("DP-1").title, "title");
  workspace.workspaces[0].display_name = "renamed";
  snapshot = mergeLabwcSnapshot(workspace, true, windows);
  EXPECT_EQ(snapshot.active_windows.value("DP-1").title, "title");
  EXPECT_EQ(snapshot.workspaces[0].display_name, "renamed");
  windows["DP-1"].title = "changed";
  snapshot = mergeLabwcSnapshot(workspace, true, windows);
  EXPECT_EQ(snapshot.workspaces[0].display_name, "renamed");
  EXPECT_EQ(snapshot.active_windows.value("DP-1").title, "changed");
  snapshot = mergeLabwcSnapshot(workspace, false, windows);
  EXPECT_TRUE(snapshot.connected);
  EXPECT_FALSE(snapshot.capabilities.active_window);
  EXPECT_TRUE(snapshot.active_windows.isEmpty());
  EXPECT_TRUE(snapshot.capabilities.workspace_activation);
  snapshot = mergeLabwcSnapshot(workspace, true, {});
  EXPECT_TRUE(snapshot.capabilities.active_window);
  EXPECT_TRUE(snapshot.active_windows.isEmpty());
  workspace.connected = false;
  snapshot = mergeLabwcSnapshot(workspace, true, windows);
  EXPECT_TRUE(snapshot.connected);
  EXPECT_FALSE(snapshot.capabilities.workspace_listing);
  EXPECT_TRUE(snapshot.workspaces.isEmpty());
  EXPECT_EQ(snapshot.active_windows.size(), 1);
  snapshot = mergeLabwcSnapshot(workspace, false, windows);
  EXPECT_FALSE(snapshot.connected);
  EXPECT_TRUE(snapshot.active_windows.isEmpty());
}

TEST(LabwcWindows, IndependentFlagsAndUncommittedInventory) {
  ForeignToplevelState state;
  EXPECT_FALSE(state.ready);
  state.pending.minimized = true;
  state.pending.maximized = true;
  state.pending.fullscreen = true;
  EXPECT_FALSE(state.committed.minimized);
  state.commit();
  EXPECT_TRUE(state.ready);
  EXPECT_TRUE(state.committed.minimized);
  EXPECT_TRUE(state.committed.maximized);
  EXPECT_TRUE(state.committed.fullscreen);
  EXPECT_FALSE(state.committed.activated);
}

TEST(WindowCatalog, IdentityCapabilitySearchGroupingAndHistory) {
  WindowCatalog catalog;
  CompositorWindow first{.id = "first",
                         .title = "Editor",
                         .app_id = "app",
                         .activated = true,
                         .operations = {WindowCommand::Activate, WindowCommand::Close}};
  CompositorWindow second{.id = "second", .title = "Editor", .app_id = "app", .minimized = true};
  catalog.replace({first, second});
  EXPECT_EQ(catalog.validate("first", WindowCommand::Close, true), WindowCommandResult::Accepted);
  EXPECT_EQ(catalog.validate("first", WindowCommand::Minimize, true), WindowCommandResult::Unsupported);
  EXPECT_EQ(catalog.validate("first", WindowCommand::Close, false), WindowCommandResult::Disconnected);
  EXPECT_EQ(catalog.search("EDITOR").size(), 2);
  EXPECT_EQ(catalog.search("APP").size(), 2);
  EXPECT_TRUE(catalog.search("unknown").isEmpty());
  EXPECT_EQ(WindowCatalog::groupKey(first, true), WindowCatalog::groupKey(second, true));
  EXPECT_NE(WindowCatalog::groupKey(first, false), WindowCatalog::groupKey(second, false));
  second.app_id.clear();
  first.app_id.clear();
  EXPECT_NE(WindowCatalog::groupKey(first, true), WindowCatalog::groupKey(second, true));
  const auto order = catalog.activationOrder("first");
  catalog.replace({first, second});
  EXPECT_EQ(catalog.activationOrder("first"), order);
  first.activated = false;
  second.activated = true;
  catalog.replace({first, second});
  EXPECT_GT(catalog.activationOrder("second"), order);
  catalog.replace({second});
  EXPECT_EQ(catalog.validate("first", WindowCommand::Close, true), WindowCommandResult::InvalidWindow);
  EXPECT_EQ(catalog.activationOrder("first"), 0);
}

TEST(ToplevelModel, IncludesMinimizedWindowsAndIndependentFlags) {
  ToplevelModel model;
  model.replace({{.id = "one", .outputs = {"DP-1"}, .minimized = true, .maximized = true}});
  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_EQ(model.data(model.index(0), ToplevelModel::Outputs).toStringList(), QStringList{"DP-1"});
  EXPECT_TRUE(model.data(model.index(0), ToplevelModel::Minimized).toBool());
  EXPECT_TRUE(model.data(model.index(0), ToplevelModel::Maximized).toBool());
  EXPECT_FALSE(model.data(model.index(0), ToplevelModel::Activated).toBool());
  model.replace({});
  EXPECT_EQ(model.rowCount(), 0);
}

TEST(WindowPresentation, PolicyGroupingSearchAndFrozenOrdering) {
  CompositorService service;
  WindowPresentation legacy(&service, false);
  WindowPresentation tasks(&service, true);
  CompositorWindow first{.id = "first", .title = "One", .app_id = "app", .activated = true};
  CompositorWindow second{.id = "second", .title = "Two", .app_id = "app", .minimized = true};
  service.publishSnapshotForTest(
      {.connected = true, .capabilities = {.window_listing = true}, .windows = {first, second}});
  EXPECT_FALSE(legacy.enabled());
  EXPECT_TRUE(tasks.enabled());
  EXPECT_EQ(tasks.applications()->rowCount(), 1);
  tasks.setGrouped(false);
  EXPECT_EQ(tasks.applications()->rowCount(), 2);
  tasks.beginOverview();
  first.activated = false;
  second.activated = true;
  service.publishSnapshotForTest(
      {.connected = true, .capabilities = {.window_listing = true}, .windows = {first, second}});
  EXPECT_EQ(tasks.overview()[0].toMap().value("windowId").toString(), "first");
  tasks.setSearch("two");
  EXPECT_EQ(tasks.overview().size(), 1);
  tasks.setSearch("");
  service.publishSnapshotForTest({.connected = true, .capabilities = {.window_listing = true}, .windows = {second}});
  EXPECT_TRUE(tasks.window("first").isEmpty());
  EXPECT_EQ(tasks.overview().size(), 1);
  tasks.endOverview();
  EXPECT_EQ(tasks.overview()[0].toMap().value("windowId").toString(), "second");
  tasks.configure(false, true, false, false);
  EXPECT_FALSE(tasks.taskbarEnabled());
  EXPECT_FALSE(tasks.overviewAccess());
  EXPECT_FALSE(tasks.desktopMenu());
  service.publishSnapshotForTest({});
  EXPECT_TRUE(tasks.overview().isEmpty());
}

TEST(LabwcWindows, FullscreenRequiresVersionTwo) {
  for (int version : {1, 2, 3}) {
    const auto operations = foreignToplevelOperations(version);
    EXPECT_TRUE(operations.contains(WindowCommand::Activate));
    EXPECT_TRUE(operations.contains(WindowCommand::Close));
    EXPECT_EQ(operations.contains(WindowCommand::Fullscreen), version >= 2);
    EXPECT_EQ(operations.contains(WindowCommand::Unfullscreen), version >= 2);
  }
}
