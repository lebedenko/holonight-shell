#include "ToplevelModel.h"
#include "WindowCatalog.h"
#include "WindowPresentation.h"

#include <gtest/gtest.h>

TEST(WindowCatalog, IdentityCapabilitySearchGroupingAndHistory) {
  WindowCatalog catalog;
  CompositorWindow first{
      .id = "first",
      .title = "Editor",
      .app_id = "app",
      .activated = true,
      .operations = {WindowCommand::Activate, WindowCommand::Close},
  };
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
