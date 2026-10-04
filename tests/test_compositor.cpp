#include "CompositorBackend.h"
#include "CompositorService.h"
#include "SwayIpc.h"
#include "WindowPresentation.h"
#include "WorkspacePresentation.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>

#include <gtest/gtest.h>

namespace {
class NumberedFake : public NumberedWorkspaceProvider {
 public:
  NumberedWorkspaceState state{.eligible = true, .assignments = {{"opaque-one", 1}, {"opaque-eight", 8}}};
  int activated{0};
  [[nodiscard]] NumberedWorkspaceState numberedWorkspaces() const override { return state; }
  void activateNumberedSlot(int slot) override { activated = slot; }
};
}  // namespace
TEST(CompositorService, PublishesOneAtomicRevisionWithOpaqueWorkspaceRoles) {
  CompositorService service{};
  QSignalSpy revisions(&service, &CompositorService::revisionChanged);
  CompositorSnapshot snapshot{
      .connected = true,
      .focused_output = QStringLiteral("DP-1"),
      .capabilities =
          {
              .workspace_listing = true,
              .workspace_activation = true,
              .active_window = true,
              .focused_output = true,
              .urgency = true,
              .occupancy = true,
          },
      .workspaces =
          {
              {
                  .id = QStringLiteral("dev:web"),
                  .display_name = QStringLiteral("dev:web"),
                  .stable_order = 2,
                  .outputs = {QStringLiteral("DP-1")},
                  .active = true,
                  .focused = true,
                  .occupied = true,
              },
              {.id = QStringLiteral("1"), .display_name = QStringLiteral("1"), .stable_order = 1, .occupied = false},
          },
      .active_windows = {{QStringLiteral("DP-1"), {.app_id = QStringLiteral("foot"), .title = QStringLiteral("vim")}}},
  };

  service.publishSnapshotForTest(snapshot);

  EXPECT_EQ(revisions.count(), 1);
  EXPECT_EQ(service.revision(), 1);
  EXPECT_EQ(service.focusedOutput(), QStringLiteral("DP-1"));
  EXPECT_EQ(service.activeWindowTitle(QStringLiteral("DP-1")), QStringLiteral("vim"));
  auto* model = service.workspaces();
  EXPECT_EQ(model->rowCount(), 2);
  EXPECT_EQ(model->data(model->index(0, 0), CompositorWorkspaceModel::WorkspaceIdRole).toString(), QStringLiteral("1"));
  EXPECT_EQ(model->data(model->index(1, 0), CompositorWorkspaceModel::VisualStateRole).toString(),
            QStringLiteral("focused"));
  EXPECT_FALSE(service.isOutputEmpty(QStringLiteral("DP-1")));
}

TEST(CompositorService, GatesActivationAndUnknownOccupancy) {
  CompositorService service{};
  QSignalSpy activations(&service, &CompositorService::workspaceActivationRequested);
  service.publishSnapshotForTest({.workspaces = {{.id = QStringLiteral("opaque"), .active = true}}});
  service.activateWorkspace(QStringLiteral("opaque"));
  EXPECT_EQ(activations.count(), 0);
  EXPECT_FALSE(service.isOutputEmpty(QStringLiteral("DP-1")));

  service.publishSnapshotForTest({
      .connected = true,
      .capabilities = {.workspace_activation = true, .occupancy = true},
      .workspaces =
          {
              {.id = QStringLiteral("opaque"), .outputs = {QStringLiteral("DP-1")}, .active = true, .occupied = false},
          },
  });
  service.activateWorkspace(QStringLiteral("opaque"));
  EXPECT_EQ(activations.count(), 1);
  EXPECT_TRUE(service.isOutputEmpty(QStringLiteral("DP-1")));
}

TEST(CompositorService, DisconnectClearsTransientStateAndCapabilities) {
  CompositorService service{};
  service.publishSnapshotForTest({
      .connected = true,
      .focused_output = QStringLiteral("DP-1"),
      .capabilities = {.workspace_listing = true, .active_window = true},
      .workspaces = {{.id = QStringLiteral("dev")}},
      .active_windows = {{QStringLiteral("DP-1"), {.title = QStringLiteral("editor")}}},
  });

  service.publishSnapshotForTest({.diagnostic = QStringLiteral("subscription disconnected")});

  EXPECT_FALSE(service.connected());
  EXPECT_FALSE(service.canListWorkspaces());
  EXPECT_TRUE(service.focusedOutput().isEmpty());
  EXPECT_EQ(service.workspaces()->rowCount(), 0);
  EXPECT_TRUE(service.activeWindowTitle(QStringLiteral("DP-1")).isEmpty());
  EXPECT_EQ(service.diagnostic(), QStringLiteral("subscription disconnected"));
}

TEST(WorkspacePresentation, EmptySlotsNeverEnterActualSnapshotAndCountChangesImmediately) {
  CompositorService service;
  NumberedFake provider;
  WorkspacePresentation presentation(&service, &provider);
  CompositorSnapshot snapshot{
      .connected = true,
      .capabilities = {.workspace_listing = true, .workspace_activation = true},
      .workspaces =
          {
              {.id = "opaque-one", .display_name = "One", .outputs = {"DP-1"}, .active = true, .focused = true},
              {.id = "opaque-eight", .display_name = "Eight", .urgent = true, .occupied = true},
          },
  };
  service.publishSnapshotForTest(snapshot);
  EXPECT_TRUE(presentation.useNumericWorkspacePresentation());
  EXPECT_EQ(presentation.workspaceDisplayCount(), 5);
  EXPECT_EQ(service.workspaces()->rowCount(), 2);
  EXPECT_EQ(presentation.activeNumericWorkspaceForOutput("DP-1"), 1);
  EXPECT_EQ(presentation.numericWorkspaceVisualState(1), "focused-active");
  EXPECT_EQ(presentation.numericWorkspaceVisualState(5), "empty");
  EXPECT_EQ(presentation.numericWorkspaceVisualState(8), "urgent");
  EXPECT_TRUE(presentation.hasNavigableNumericWorkspaceAtOrBeyond(6));
  EXPECT_EQ(presentation.firstUrgentNumericWorkspaceAtOrBeyond(6), 8);
  EXPECT_EQ(presentation.lastUrgentNumericWorkspaceBefore(9), 8);
  QSignalSpy changes(&presentation, &WorkspacePresentation::workspaceDisplayCountChanged);
  presentation.setWorkspaceDisplayCount(9);
  presentation.setWorkspaceDisplayCount(3);
  EXPECT_EQ(changes.count(), 2);
  EXPECT_EQ(service.workspaces()->rowCount(), 2);
  EXPECT_EQ(provider.activated, 0);
  presentation.activateNumberedSlot(5);
  EXPECT_EQ(provider.activated, 5);
  QSignalSpy requests(&service, &CompositorService::workspaceActivationRequested);
  service.activateWorkspace("5");
  EXPECT_EQ(requests.count(), 0);
  snapshot.workspaces.last().urgent = false;
  service.publishSnapshotForTest(snapshot);
  EXPECT_EQ(presentation.numericWorkspaceVisualState(8), "occupied");
  provider.state.eligible = false;
  service.publishSnapshotForTest(snapshot);
  EXPECT_FALSE(presentation.useNumericWorkspacePresentation());
  presentation.activateNumberedSlot(6);
  EXPECT_EQ(provider.activated, 5);
  provider.state.eligible = true;
  service.publishSnapshotForTest({});
  EXPECT_FALSE(presentation.useNumericWorkspacePresentation());
  EXPECT_EQ(service.workspaces()->rowCount(), 0);
  service.publishSnapshotForTest({.connected = true});
  EXPECT_TRUE(presentation.useNumericWorkspacePresentation());
  EXPECT_EQ(service.workspaces()->rowCount(), 0);
}
TEST(WorkspacePresentation, NoOptionalProviderUsesNamedRows) {
  CompositorService service;
  WorkspacePresentation presentation(&service, nullptr);
  CompositorSnapshot snapshot{.connected = true};
  for (int row = 0; row < 7; ++row) {
    snapshot.workspaces.append(
        {.id = QString::number(row), .display_name = "same", .stable_order = row, .focused = row == 5});
  }
  service.publishSnapshotForTest(snapshot);
  presentation.setWorkspaceDisplayCount(3);
  EXPECT_FALSE(presentation.useNumericWorkspacePresentation());
  EXPECT_EQ(presentation.firstVisibleWorkspaceRow(), 4);
}
TEST(CompositorService, PerWorkspaceCapabilitiesGateExistingIdentity) {
  CompositorService service;
  QSignalSpy requests(&service, &CompositorService::workspaceActivationRequested);
  CompositorSnapshot snapshot{
      .connected = true,
      .capabilities = {.workspace_activation = true},
      .workspaces = {{.id = "opaque", .display_name = "mutable", .can_activate = false}},
  };
  service.publishSnapshotForTest(snapshot);
  service.activateWorkspace("opaque");
  EXPECT_EQ(requests.count(), 0);
  snapshot.workspaces.first().can_activate = true;
  snapshot.workspaces.first().display_name = "renamed";
  service.publishSnapshotForTest(snapshot);
  service.activateWorkspace("renamed");
  service.activateWorkspace("opaque");
  EXPECT_EQ(requests.count(), 1);
}
TEST(SwayNumberedProvider, RejectsNoncanonicalNamesZeroAndDuplicateNumbers) {
  for (const auto& name : {"dev", "2:web", "0", "02", "+2", "3"}) {
    const QJsonArray workspaces{QJsonObject{{"name", name}, {"num", 2}}};
    const auto refresh = parseSwayRefresh(QJsonDocument(workspaces).toJson(), "[]", "{}");
    ASSERT_TRUE(refresh);
    EXPECT_FALSE(refresh->numbered.eligible) << name;
  }
  const auto duplicate = parseSwayRefresh(R"([{"id":1,"name":"2","num":2},{"id":2,"name":"2","num":2}])", "[]", "{}");
  ASSERT_TRUE(duplicate);
  EXPECT_FALSE(duplicate->numbered.eligible);
  const auto sparse = parseSwayRefresh(R"([{"id":1,"name":"1","num":1},{"id":2,"name":"8","num":8}])", "[]", "{}");
  ASSERT_TRUE(sparse);
  EXPECT_TRUE(sparse->numbered.eligible);
  EXPECT_EQ(sparse->numbered.assignments.value("2"), 8);
  const auto empty = parseSwayRefresh("[]", "[]", "{}");
  ASSERT_TRUE(empty);
  EXPECT_TRUE(empty->numbered.eligible);
}

TEST(WorkspacePresentation, VisibilityUsesActualRowsAndCapabilitiesRatherThanDisplayLimit) {
  CompositorService service;
  NumberedFake provider;
  provider.state.eligible = false;
  WorkspacePresentation presentation(&service, &provider);
  QSignalSpy revisions(&presentation, &WorkspacePresentation::revisionChanged);
  CompositorSnapshot snapshot{.connected = true, .capabilities = {.workspace_listing = true}};
  service.publishSnapshotForTest(snapshot);
  EXPECT_FALSE(presentation.sectionVisible());
  snapshot.workspaces.append({.id = "opaque-one"});
  service.publishSnapshotForTest(snapshot);
  EXPECT_FALSE(presentation.sectionVisible());
  provider.state.eligible = true;
  service.publishSnapshotForTest(snapshot);
  EXPECT_TRUE(presentation.sectionVisible());
  presentation.activateNumberedSlot(5);
  EXPECT_EQ(provider.activated, 5);
  provider.state.eligible = false;
  snapshot.workspaces.append({.id = "opaque-two"});
  service.publishSnapshotForTest(snapshot);
  presentation.setWorkspaceDisplayCount(1);
  EXPECT_TRUE(presentation.sectionVisible());
  snapshot.capabilities.workspace_listing = false;
  service.publishSnapshotForTest(snapshot);
  EXPECT_FALSE(presentation.sectionVisible());
  snapshot.connected = false;
  service.publishSnapshotForTest(snapshot);
  EXPECT_FALSE(presentation.sectionVisible());
  EXPECT_EQ(revisions.count(), 7);
}

TEST(CompositorService, TitleUpdatesPreserveModelsAndCommitBeforeRowNotifications) {
  CompositorService service;
  WindowPresentation presentation(&service, true);
  CompositorSnapshot snapshot{
      .connected = true,
      .capabilities = {.window_listing = true, .workspace_activation = true},
      .workspaces = {{.id = "one"}, {.id = "two"}},
      .windows =
          {
              {.id = "first", .title = "Before", .app_id = "app"},
              {.id = "second", .title = "Second", .app_id = "app"},
          },
  };
  service.publishSnapshotForTest(snapshot);
  auto* workspaces = service.workspaces();
  auto* groups = presentation.applications();
  QSignalSpy workspace_resets(workspaces, &QAbstractItemModel::modelReset);
  QSignalSpy workspace_changes(workspaces, &QAbstractItemModel::dataChanged);
  QSignalSpy group_resets(groups, &QAbstractItemModel::modelReset);
  QSignalSpy group_changes(groups, &QAbstractItemModel::dataChanged);
  QSignalSpy revisions(&service, &CompositorService::revisionChanged);
  snapshot.windows[0].title = "After";
  service.publishSnapshotForTest(snapshot);
  EXPECT_EQ(workspace_resets.count(), 0);
  EXPECT_EQ(workspace_changes.count(), 0);
  EXPECT_EQ(group_resets.count(), 0);
  ASSERT_EQ(group_changes.count(), 1);
  EXPECT_EQ(group_changes[0][2].value<QList<int>>(),
            (QList<int>{GroupedApplicationModel::Title, GroupedApplicationModel::Windows}));
  EXPECT_EQ(groups->data(groups->index(0, 0), GroupedApplicationModel::Title).toString(), "After");
  service.publishSnapshotForTest(snapshot);
  EXPECT_EQ(group_changes.count(), 1);
  EXPECT_EQ(revisions.count(), 2);
  snapshot.windows[1].activated = true;
  snapshot.windows[0].minimized = true;
  snapshot.windows[1].minimized = true;
  snapshot.workspaces[0].active = true;
  bool committed = false;
  QObject::connect(workspaces, &QAbstractItemModel::dataChanged, &service, [&] {
    committed = service.snapshot().windows[1].activated && service.snapshot().workspaces[0].active;
  });
  service.publishSnapshotForTest(snapshot);
  EXPECT_TRUE(committed);
  EXPECT_EQ(workspace_resets.count(), 0);
  EXPECT_EQ(workspace_changes.count(), 1);
  EXPECT_TRUE(groups->data(groups->index(0, 0), GroupedApplicationModel::Active).toBool());
  EXPECT_TRUE(groups->data(groups->index(0, 0), GroupedApplicationModel::Minimized).toBool());
  EXPECT_TRUE(
      groups->data(groups->index(0, 0), GroupedApplicationModel::Windows).toList()[1].toMap()["activated"].toBool());
  snapshot.windows.removeLast();
  service.publishSnapshotForTest(snapshot);
  EXPECT_EQ(group_resets.count(), 0);
  EXPECT_EQ(groups->data(groups->index(0, 0), GroupedApplicationModel::Windows).toList().size(), 1);
  snapshot.windows.append({.id = "third", .app_id = "other"});
  service.publishSnapshotForTest(snapshot);
  EXPECT_EQ(group_resets.count(), 1);
  presentation.setGrouped(false);
  EXPECT_EQ(group_resets.count(), 2);
  std::swap(snapshot.windows[0], snapshot.windows[1]);
  snapshot.workspaces[0].stable_order = 2;
  service.publishSnapshotForTest(snapshot);
  EXPECT_EQ(group_resets.count(), 3);
  EXPECT_EQ(workspace_resets.count(), 1);
  service.publishSnapshotForTest({});
  EXPECT_EQ(groups->rowCount(), 0);
  EXPECT_EQ(workspaces->rowCount(), 0);
  EXPECT_EQ(group_resets.count(), 4);
  EXPECT_EQ(workspace_resets.count(), 2);
}
