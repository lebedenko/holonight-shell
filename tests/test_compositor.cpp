#include "CompositorSelection.h"
#include "CompositorService.h"

#include <QSignalSpy>

#include <gtest/gtest.h>

TEST(CompositorSelection, DeclaredDesktopWinsOverRuntimeMarkers) {
  EXPECT_EQ(selectCompositor({.current_desktop = "X:HYPERLAND:Hyprland", .sway_socket = "/run/sway.sock"}),
            CompositorKind::Hyprland);
  EXPECT_EQ(selectCompositor({.current_desktop = "sWaY", .hyprland_instance_signature = "instance"}),
            CompositorKind::Sway);
}

TEST(CompositorSelection, AmbiguousDesktopDoesNotUseMarkers) {
  EXPECT_EQ(selectCompositor({.current_desktop = "Hyprland:Sway", .hyprland_instance_signature = "instance"}),
            CompositorKind::Generic);
}

TEST(CompositorSelection, UnambiguousMarkerIsUsedOnlyWithoutDeclaration) {
  EXPECT_EQ(selectCompositor({.hyprland_instance_signature = "instance"}), CompositorKind::Hyprland);
  EXPECT_EQ(selectCompositor({.sway_socket = "/run/sway.sock"}), CompositorKind::Sway);
  EXPECT_EQ(selectCompositor({.hyprland_instance_signature = "instance", .sway_socket = "/run/sway.sock"}),
            CompositorKind::Generic);
  EXPECT_EQ(selectCompositor({}), CompositorKind::Generic);
}

TEST(CompositorService, PublishesOneAtomicRevisionWithOpaqueWorkspaceRoles) {
  CompositorService service(CompositorKind::Sway);
  QSignalSpy revisions(&service, &CompositorService::revisionChanged);
  CompositorSnapshot snapshot{
      .connected = true,
      .focused_output = QStringLiteral("DP-1"),
      .capabilities = {.workspace_listing = true,
                       .workspace_activation = true,
                       .active_window = true,
                       .focused_output = true,
                       .urgency = true,
                       .occupancy = true},
      .workspaces = {{.id = QStringLiteral("dev:web"),
                      .display_name = QStringLiteral("dev:web"),
                      .stable_order = 2,
                      .outputs = {QStringLiteral("DP-1")},
                      .active = true,
                      .focused = true,
                      .occupied = true},
                     {.id = QStringLiteral("1"),
                      .numeric_slot = 1,
                      .display_name = QStringLiteral("1"),
                      .stable_order = 1,
                      .occupied = false}},
      .active_windows = {{QStringLiteral("DP-1"), {.app_id = QStringLiteral("foot"), .title = QStringLiteral("vim")}}},
  };

  service.publishSnapshot(snapshot);

  EXPECT_EQ(revisions.count(), 1);
  EXPECT_EQ(service.revision(), 1);
  EXPECT_EQ(service.focusedOutput(), QStringLiteral("DP-1"));
  EXPECT_EQ(service.activeWindowTitle(QStringLiteral("DP-1")), QStringLiteral("vim"));
  auto* model = service.workspaces();
  EXPECT_EQ(model->rowCount(), 2);
  EXPECT_EQ(model->data(model->index(0, 0), CompositorWorkspaceModel::WorkspaceIdRole).toString(), QStringLiteral("1"));
  EXPECT_FALSE(model->data(model->index(1, 0), CompositorWorkspaceModel::NumericSlotRole).isValid());
  EXPECT_EQ(model->data(model->index(1, 0), CompositorWorkspaceModel::VisualStateRole).toString(),
            QStringLiteral("focused"));
  EXPECT_FALSE(service.isOutputEmpty(QStringLiteral("DP-1")));
}

TEST(CompositorService, GatesActivationAndUnknownOccupancy) {
  CompositorService service(CompositorKind::Generic);
  QSignalSpy activations(&service, &CompositorService::workspaceActivationRequested);
  service.publishSnapshot({.workspaces = {{.id = QStringLiteral("opaque"), .active = true}}});
  service.activateWorkspace(QStringLiteral("opaque"));
  EXPECT_EQ(activations.count(), 0);
  EXPECT_FALSE(service.isOutputEmpty(QStringLiteral("DP-1")));

  service.publishSnapshot(
      {.connected = true,
       .capabilities = {.workspace_activation = true, .occupancy = true},
       .workspaces = {
           {.id = QStringLiteral("opaque"), .outputs = {QStringLiteral("DP-1")}, .active = true, .occupied = false}}});
  service.activateWorkspace(QStringLiteral("opaque"));
  EXPECT_EQ(activations.count(), 1);
  EXPECT_TRUE(service.isOutputEmpty(QStringLiteral("DP-1")));
}

TEST(CompositorService, DisconnectClearsTransientStateAndCapabilities) {
  CompositorService service(CompositorKind::Sway);
  service.publishSnapshot({.connected = true,
                           .focused_output = QStringLiteral("DP-1"),
                           .capabilities = {.workspace_listing = true, .active_window = true},
                           .workspaces = {{.id = QStringLiteral("dev")}},
                           .active_windows = {{QStringLiteral("DP-1"), {.title = QStringLiteral("editor")}}}});

  service.publishSnapshot({.diagnostic = QStringLiteral("subscription disconnected")});

  EXPECT_FALSE(service.connected());
  EXPECT_FALSE(service.canListWorkspaces());
  EXPECT_TRUE(service.focusedOutput().isEmpty());
  EXPECT_EQ(service.workspaces()->rowCount(), 0);
  EXPECT_TRUE(service.activeWindowTitle(QStringLiteral("DP-1")).isEmpty());
  EXPECT_EQ(service.diagnostic(), QStringLiteral("subscription disconnected"));
}

TEST(CompositorService, SynthesizesNumericSlotsOnlyWhenCapabilityAllowsIt) {
  CompositorService hyprland(CompositorKind::Hyprland);
  hyprland.setWorkspaceDisplayCount(3);
  hyprland.publishSnapshot(
      {.connected = true,
       .capabilities = {.workspace_listing = true, .numeric_workspace_creation = true},
       .workspaces = {
           {.id = QStringLiteral("2"), .numeric_slot = 2, .display_name = QStringLiteral("2"), .stable_order = 2}}});
  EXPECT_EQ(hyprland.workspaces()->rowCount(), 3);

  CompositorService sway(CompositorKind::Sway);
  sway.setWorkspaceDisplayCount(3);
  sway.publishSnapshot(
      {.connected = true,
       .capabilities = {.workspace_listing = true},
       .workspaces = {{.id = QStringLiteral("9:mail"), .display_name = QStringLiteral("9:mail"), .stable_order = 0}}});
  EXPECT_EQ(sway.workspaces()->rowCount(), 1);
  EXPECT_FALSE(
      sway.workspaces()->data(sway.workspaces()->index(0, 0), CompositorWorkspaceModel::NumericSlotRole).isValid());
}

TEST(CompositorService, ComputesVisibleRowsByModelIndex) {
  CompositorService service(CompositorKind::Sway);
  service.setWorkspaceDisplayCount(3);
  CompositorSnapshot snapshot{.connected = true, .capabilities = {.workspace_listing = true}};
  for (int row = 0; row < 7; ++row) {
    snapshot.workspaces.append({.id = QStringLiteral("named-%1").arg(row),
                                .display_name = QStringLiteral("named-%1").arg(row),
                                .stable_order = row,
                                .focused = row == 5});
  }
  service.publishSnapshot(std::move(snapshot));
  EXPECT_EQ(service.focusedWorkspaceRow(), 5);
  EXPECT_EQ(service.firstVisibleWorkspaceRow(), 4);
}
