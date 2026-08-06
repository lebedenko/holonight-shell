#include "WorkspaceModel.h"

#include <QSignalSpy>
#include <QVariantMap>

#include <gtest/gtest.h>

using WS = WorkspaceModel::WorkspaceState;
using Entry = WorkspaceModel::WorkspaceEntry;

static Entry e(int workspace_id, WS state, bool on_monitor = true) {
  return Entry{.id = workspace_id, .name = QString::number(workspace_id), .state = state, .on_monitor = on_monitor};
}

class WorkspaceModelTest : public ::testing::Test {
 protected:
  WorkspaceModel model;
};

// T-017: stateForId returns Urgent for a workspace with urgent state
TEST_F(WorkspaceModelTest, StateForIdReturnsUrgent) {
  model.applyBatchUpdate(
      {e(1, WS::Empty), e(2, WS::Empty), e(3, WS::Urgent), e(4, WS::Empty), e(5, WS::Empty), e(6, WS::Empty)});
  EXPECT_EQ(model.stateForId(3), static_cast<int>(WS::Urgent));
  EXPECT_EQ(model.stateForId(1), static_cast<int>(WS::Empty));
}

// T-017: Urgent and Active are distinct integer values
TEST_F(WorkspaceModelTest, UrgentAndActiveAreDistinct) {
  model.applyBatchUpdate({e(1, WS::Active), e(2, WS::Urgent)});
  EXPECT_EQ(model.stateForId(1), static_cast<int>(WS::Active));
  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::Urgent));
  EXPECT_NE(model.stateForId(1), model.stateForId(2));
}

// T-017: state update replaces urgent with empty on next batch
TEST_F(WorkspaceModelTest, UrgentStateCleared) {
  model.applyBatchUpdate({e(1, WS::Empty), e(2, WS::Urgent)});
  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::Urgent));
  model.applyBatchUpdate({e(1, WS::Active), e(2, WS::Empty)});
  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::Empty));
}

TEST_F(WorkspaceModelTest, HyprlandUrgentFallbackPromotesWorkspaceState) {
  model.applyBatchUpdate({e(1, WS::Active), e(2, WS::Occupied)});

  model.addUrgentWorkspaceId(2);

  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::Urgent));
}

TEST_F(WorkspaceModelTest, FocusedWorkspaceClearsHyprlandUrgentFallback) {
  model.applyBatchUpdate({e(1, WS::Active), e(2, WS::Occupied)});
  model.setFocusedWorkspaceId(1);
  model.addUrgentWorkspaceId(2);

  model.setFocusedWorkspaceId(2);

  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::Occupied));
}

TEST_F(WorkspaceModelTest, FocusedWorkspaceAcknowledgesStickyTerminalBellUrgencyUntilItClearsWhileAway) {
  model.applyBatchUpdate({e(2, WS::Urgent), e(6, WS::Active)});

  model.setFocusedWorkspaceId(2);

  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::FocusedActiveMonitor));

  model.setFocusedWorkspaceId(6);

  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::Occupied));

  model.applyBatchUpdate({e(2, WS::Urgent), e(6, WS::Active)});

  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::Occupied));

  model.applyBatchUpdate({e(2, WS::Occupied), e(6, WS::Active)});
  model.applyBatchUpdate({e(2, WS::Urgent), e(6, WS::Active)});

  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::Urgent));
}

TEST_F(WorkspaceModelTest, FocusedWorkspaceAcknowledgementSuppressesDelayedHyprlandUrgentFallback) {
  model.applyBatchUpdate({e(2, WS::Occupied), e(6, WS::Active)});
  model.setFocusedWorkspaceId(2);
  model.setFocusedWorkspaceId(6);

  model.addUrgentWorkspaceId(2);

  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::Occupied));
}

// activeWorkspaceForMonitor: unknown monitor name returns 0
TEST_F(WorkspaceModelTest, ActiveWorkspaceForMonitorReturnsZeroForUnknownMonitor) {
  Entry entry = e(1, WS::Active);
  entry.monitor_names = {"DP-3"};
  model.applyBatchUpdate({entry});

  EXPECT_EQ(model.activeWorkspaceForMonitor("HDMI-1"), 0);
}

// activeWorkspaceForMonitor: resolves per-monitor active id independently
TEST_F(WorkspaceModelTest, ActiveWorkspaceForMonitorResolvesEachMonitorIndependently) {
  Entry entry1 = e(2, WS::Active);
  entry1.monitor_names = {"DP-3"};
  Entry entry2 = e(5, WS::Active);
  entry2.monitor_names = {"HDMI-1"};
  model.applyBatchUpdate({entry1, entry2});

  EXPECT_EQ(model.activeWorkspaceForMonitor("DP-3"), 2);
  EXPECT_EQ(model.activeWorkspaceForMonitor("HDMI-1"), 5);
}

// activeWorkspaceForMonitor: Urgent state on the monitor's workspace is not treated as Active
// (documents the known limitation flagged in DESIGN.md §10 — Active+Urgent collapse to Urgent only).
TEST_F(WorkspaceModelTest, ActiveWorkspaceForMonitorReturnsZeroWhenEntryIsUrgentNotActive) {
  Entry entry = e(3, WS::Urgent);
  entry.monitor_names = {"DP-3"};
  model.applyBatchUpdate({entry});

  EXPECT_EQ(model.activeWorkspaceForMonitor("DP-3"), 0);
}

// maxWorkspaceId: empty model returns 0
TEST_F(WorkspaceModelTest, MaxWorkspaceIdReturnsZeroWhenEmpty) { EXPECT_EQ(model.maxWorkspaceId(), 0); }

// maxWorkspaceId: returns highest numbered id; specials (routed separately) don't affect it
TEST_F(WorkspaceModelTest, MaxWorkspaceIdReturnsHighestNumberedId) {
  model.applyBatchUpdate({e(1, WS::Empty), e(9, WS::Occupied), e(4, WS::Active)});
  model.applySpecialWorkspaces({WorkspaceModel::SpecialWorkspaceEntry{.name = "special:scratch", .active = true}});

  EXPECT_EQ(model.maxWorkspaceId(), 9);
}

// hasOccupiedOrUrgentBeyond: boundary at exactly minId, excluded below it
TEST_F(WorkspaceModelTest, HasOccupiedOrUrgentBeyondRespectsBoundary) {
  model.applyBatchUpdate({e(5, WS::Occupied)});

  EXPECT_TRUE(model.hasOccupiedOrUrgentBeyond(5));
  EXPECT_FALSE(model.hasOccupiedOrUrgentBeyond(6));
}

// hasOccupiedOrUrgentBeyond: Empty workspaces beyond the edge don't count
TEST_F(WorkspaceModelTest, HasOccupiedOrUrgentBeyondFalseWhenOnlyEmptyBeyond) {
  model.applyBatchUpdate({e(1, WS::Active), e(6, WS::Empty)});

  EXPECT_FALSE(model.hasOccupiedOrUrgentBeyond(6));
}

// hasVisibleOrNavigableBeyond: active/focused workspaces beyond the edge keep the right-pan affordance visible
TEST_F(WorkspaceModelTest, HasVisibleOrNavigableBeyondIncludesFocusedActiveWorkspace) {
  model.applyBatchUpdate({e(1, WS::Empty), e(7, WS::Active)});
  model.setFocusedWorkspaceId(7);

  EXPECT_FALSE(model.hasOccupiedOrUrgentBeyond(7));
  EXPECT_TRUE(model.hasVisibleOrNavigableBeyond(7));
}

// hasVisibleOrNavigableBeyond: still excludes empty workspaces beyond the edge
TEST_F(WorkspaceModelTest, HasVisibleOrNavigableBeyondFalseWhenOnlyEmptyBeyond) {
  model.applyBatchUpdate({e(1, WS::Active), e(7, WS::Empty)});

  EXPECT_FALSE(model.hasVisibleOrNavigableBeyond(7));
}

// hasUrgentBeyond / firstUrgentIdBeyond: multiple urgents beyond minId — nearest (lowest) wins
TEST_F(WorkspaceModelTest, FirstUrgentIdBeyondReturnsLowestMatchingId) {
  model.applyBatchUpdate({e(1, WS::Active), e(7, WS::Urgent), e(8, WS::Urgent), e(9, WS::Urgent)});

  EXPECT_TRUE(model.hasUrgentBeyond(6));
  EXPECT_EQ(model.firstUrgentIdBeyond(6), 7);
}

// hasUrgentBeyond / firstUrgentIdBeyond: no match returns false / 0
TEST_F(WorkspaceModelTest, FirstUrgentIdBeyondReturnsZeroWhenNoneMatch) {
  model.applyBatchUpdate({e(1, WS::Active), e(7, WS::Occupied)});

  EXPECT_FALSE(model.hasUrgentBeyond(6));
  EXPECT_EQ(model.firstUrgentIdBeyond(6), 0);
}

// hasUrgentBefore / lastUrgentIdBefore: mirror of the "beyond" predicates for the left edge —
// boundary at maxIdExclusive - 1 included, maxIdExclusive itself excluded
TEST_F(WorkspaceModelTest, HasUrgentBeforeRespectsBoundary) {
  model.applyBatchUpdate({e(4, WS::Urgent)});

  EXPECT_TRUE(model.hasUrgentBefore(5));
  EXPECT_FALSE(model.hasUrgentBefore(4));
}

// hasUrgentBefore / lastUrgentIdBefore: multiple urgents below maxIdExclusive — nearest (highest) wins
TEST_F(WorkspaceModelTest, LastUrgentIdBeforeReturnsHighestMatchingId) {
  model.applyBatchUpdate({e(1, WS::Urgent), e(2, WS::Urgent), e(3, WS::Urgent), e(9, WS::Active)});

  EXPECT_TRUE(model.hasUrgentBefore(4));
  EXPECT_EQ(model.lastUrgentIdBefore(4), 3);
}

// hasUrgentBefore / lastUrgentIdBefore: no match returns false / 0
TEST_F(WorkspaceModelTest, LastUrgentIdBeforeReturnsZeroWhenNoneMatch) {
  model.applyBatchUpdate({e(1, WS::Occupied), e(9, WS::Active)});

  EXPECT_FALSE(model.hasUrgentBefore(4));
  EXPECT_EQ(model.lastUrgentIdBefore(4), 0);
}

// Revision increments with every batch update
TEST_F(WorkspaceModelTest, RevisionIncrementsOnUpdate) {
  EXPECT_EQ(model.revision(), 0);
  model.applyBatchUpdate({e(1, WS::Empty)});
  EXPECT_EQ(model.revision(), 1);
  model.applyBatchUpdate({e(1, WS::Active)});
  EXPECT_EQ(model.revision(), 2);
}

// Unknown workspace id returns Empty
TEST_F(WorkspaceModelTest, StateForUnknownIdReturnsEmpty) {
  model.applyBatchUpdate({e(1, WS::Active)});
  EXPECT_EQ(model.stateForId(99), static_cast<int>(WS::Empty));
}

TEST_F(WorkspaceModelTest, DataReturnsEveryQmlRole) {
  model.applyBatchUpdate({Entry{.id = 7, .name = QStringLiteral("special"), .state = WS::Active, .on_monitor = true}});

  const QModelIndex index = model.index(0, 0);

  EXPECT_EQ(model.data(index, WorkspaceModel::WorkspaceIdRole).toInt(), 7);
  EXPECT_EQ(model.data(index, WorkspaceModel::WorkspaceNameRole).toString(), QStringLiteral("special"));
  EXPECT_EQ(model.data(index, WorkspaceModel::WorkspaceStateRole).toInt(), static_cast<int>(WS::FocusedActiveMonitor));
  EXPECT_TRUE(model.data(index, WorkspaceModel::WorkspaceOnMonitorRole).toBool());
}

TEST_F(WorkspaceModelTest, DataReturnsEmptyVariantForInvalidRequests) {
  model.applyBatchUpdate({e(1, WS::Active)});

  EXPECT_FALSE(model.data({}).isValid());
  EXPECT_FALSE(model.data(model.index(-1, 0), WorkspaceModel::WorkspaceIdRole).isValid());
  EXPECT_FALSE(model.data(model.index(1, 0), WorkspaceModel::WorkspaceIdRole).isValid());
  EXPECT_FALSE(model.data(model.index(0, 0), Qt::DisplayRole).isValid());
}

TEST_F(WorkspaceModelTest, RowCountRejectsParentIndexes) {
  model.applyBatchUpdate({e(1, WS::Active)});

  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_EQ(model.rowCount(model.index(0, 0)), 0);
}

TEST_F(WorkspaceModelTest, RoleNamesExposeQmlContract) {
  const QHash<int, QByteArray> roles = model.roleNames();

  EXPECT_EQ(roles.value(WorkspaceModel::WorkspaceIdRole), QByteArrayLiteral("wsId"));
  EXPECT_EQ(roles.value(WorkspaceModel::WorkspaceNameRole), QByteArrayLiteral("wsName"));
  EXPECT_EQ(roles.value(WorkspaceModel::WorkspaceStateRole), QByteArrayLiteral("wsState"));
  EXPECT_EQ(roles.value(WorkspaceModel::WorkspaceOnMonitorRole), QByteArrayLiteral("wsOnMonitor"));
}

TEST_F(WorkspaceModelTest, OccupiedWorkspaceIdsPromoteEmptyState) {
  model.applyBatchUpdate({e(1, WS::Empty), e(2, WS::Empty)});
  model.setOccupiedWorkspaceIds({2});

  EXPECT_EQ(model.stateForId(1), static_cast<int>(WS::Empty));
  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::Occupied));
}

TEST_F(WorkspaceModelTest, FocusedWorkspaceDistinguishesActiveMonitor) {
  model.applyBatchUpdate({e(1, WS::Active), e(2, WS::Active)});
  model.setFocusedWorkspaceId(1);

  EXPECT_EQ(model.stateForId(1), static_cast<int>(WS::FocusedActiveMonitor));
  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::FocusedInactiveMonitor));
}

using SpecialEntry = WorkspaceModel::SpecialWorkspaceEntry;

// specialWorkspaceList: round-trips SpecialWorkspaceEntry through the exact QVariantMap shape consumers read
TEST_F(WorkspaceModelTest, SpecialWorkspaceListRoundTripsQVariantMapShape) {
  model.applySpecialWorkspaces({SpecialEntry{.name = "special:scratch", .active = true, .urgent = false}});

  const QVariantList list = model.specialWorkspaceList();

  ASSERT_EQ(list.size(), 1);
  const QVariantMap entry = list.at(0).toMap();
  EXPECT_EQ(entry.value("name").toString(), QStringLiteral("special:scratch"));
  EXPECT_TRUE(entry.value("active").toBool());
  EXPECT_FALSE(entry.value("urgent").toBool());
  EXPECT_FALSE(entry.value("occupied").toBool());
  EXPECT_TRUE(entry.value("monitorNames").toStringList().isEmpty());
}

// applySpecialWorkspaces: identical content is a revision-bump no-op (operator== short-circuit)
TEST_F(WorkspaceModelTest, ApplySpecialWorkspacesNoOpsOnIdenticalContent) {
  model.applySpecialWorkspaces({SpecialEntry{.name = "special:scratch", .active = true, .urgent = false}});
  QSignalSpy revision_changed(&model, &WorkspaceModel::revisionChanged);

  model.applySpecialWorkspaces({SpecialEntry{.name = "special:scratch", .active = true, .urgent = false}});

  EXPECT_EQ(revision_changed.count(), 0);
}

// applySpecialWorkspaces: differing content bumps revision
TEST_F(WorkspaceModelTest, ApplySpecialWorkspacesBumpsRevisionOnChange) {
  model.applySpecialWorkspaces({SpecialEntry{.name = "special:scratch", .active = false, .urgent = false}});
  QSignalSpy revision_changed(&model, &WorkspaceModel::revisionChanged);

  model.applySpecialWorkspaces({SpecialEntry{.name = "special:scratch", .active = true, .urgent = false}});

  EXPECT_EQ(revision_changed.count(), 1);
}

// activateSpecialWorkspace: emits activateSpecialWorkspaceRequested with the given name
TEST_F(WorkspaceModelTest, ActivateSpecialWorkspaceEmitsRequest) {
  QSignalSpy activate_requested(&model, &WorkspaceModel::activateSpecialWorkspaceRequested);

  model.activateSpecialWorkspace("special:scratch");

  ASSERT_EQ(activate_requested.count(), 1);
  EXPECT_EQ(activate_requested.at(0).at(0).toString(), QStringLiteral("special:scratch"));
}

// activateSpecialWorkspace: empty name is ignored
TEST_F(WorkspaceModelTest, ActivateSpecialWorkspaceIgnoresEmptyName) {
  QSignalSpy activate_requested(&model, &WorkspaceModel::activateSpecialWorkspaceRequested);

  model.activateSpecialWorkspace("");

  EXPECT_EQ(activate_requested.count(), 0);
}

TEST_F(WorkspaceModelTest, RepeatedEmptyUpdatesDoNotEmitInvalidDataChanged) {
  model.applyBatchUpdate({});
  model.applyBatchUpdate({});
  EXPECT_EQ(model.rowCount(), 0);
}

TEST_F(WorkspaceModelTest, SetFocusedWorkspaceIdEmitsDataChangedAndRevisionChanged) {
  model.applyBatchUpdate({e(1, WS::Active), e(2, WS::Active)});
  QSignalSpy data_changed(&model, &WorkspaceModel::dataChanged);
  QSignalSpy revision_changed(&model, &WorkspaceModel::revisionChanged);

  model.setFocusedWorkspaceId(2);

  EXPECT_EQ(model.revision(), 2);
  EXPECT_EQ(data_changed.count(), 1);
  EXPECT_EQ(revision_changed.count(), 1);
  EXPECT_EQ(data_changed.at(0).at(0).toModelIndex().row(), 0);
  EXPECT_EQ(data_changed.at(0).at(1).toModelIndex().row(), 1);
  EXPECT_EQ(qvariant_cast<QList<int>>(data_changed.at(0).at(2)), QList<int>({WorkspaceModel::WorkspaceStateRole}));
  EXPECT_EQ(model.stateForId(1), static_cast<int>(WS::FocusedInactiveMonitor));
  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::FocusedActiveMonitor));
}

TEST_F(WorkspaceModelTest, SetFocusedWorkspaceIdNoOpDoesNotEmitOrRevise) {
  model.applyBatchUpdate({e(1, WS::Active)});
  model.setFocusedWorkspaceId(1);
  QSignalSpy data_changed(&model, &WorkspaceModel::dataChanged);
  QSignalSpy revision_changed(&model, &WorkspaceModel::revisionChanged);

  model.setFocusedWorkspaceId(1);

  EXPECT_EQ(model.revision(), 2);
  EXPECT_EQ(data_changed.count(), 0);
  EXPECT_EQ(revision_changed.count(), 0);
}

TEST_F(WorkspaceModelTest, SetOccupiedWorkspaceIdsEmitsDataChangedAndRevisionChanged) {
  model.applyBatchUpdate({e(1, WS::Empty), e(2, WS::Empty)});
  QSignalSpy data_changed(&model, &WorkspaceModel::dataChanged);
  QSignalSpy revision_changed(&model, &WorkspaceModel::revisionChanged);

  model.setOccupiedWorkspaceIds({2});

  EXPECT_EQ(model.revision(), 2);
  EXPECT_EQ(data_changed.count(), 1);
  EXPECT_EQ(revision_changed.count(), 1);
  EXPECT_EQ(data_changed.at(0).at(0).toModelIndex().row(), 0);
  EXPECT_EQ(data_changed.at(0).at(1).toModelIndex().row(), 1);
  EXPECT_EQ(qvariant_cast<QList<int>>(data_changed.at(0).at(2)), QList<int>({WorkspaceModel::WorkspaceStateRole}));
  EXPECT_EQ(model.stateForId(2), static_cast<int>(WS::Occupied));
}

TEST_F(WorkspaceModelTest, BatchUpdateEmitsAllChangedRoles) {
  model.applyBatchUpdate({e(1, WS::Empty)});
  QSignalSpy data_changed(&model, &WorkspaceModel::dataChanged);

  model.applyBatchUpdate({e(2, WS::Occupied, false)});

  ASSERT_EQ(data_changed.count(), 1);
  EXPECT_EQ(qvariant_cast<QList<int>>(data_changed.at(0).at(2)),
            QList<int>({WorkspaceModel::WorkspaceIdRole, WorkspaceModel::WorkspaceNameRole,
                        WorkspaceModel::WorkspaceStateRole, WorkspaceModel::WorkspaceOnMonitorRole}));
}

TEST_F(WorkspaceModelTest, SetOccupiedWorkspaceIdsNoOpDoesNotEmitOrRevise) {
  model.applyBatchUpdate({e(1, WS::Empty), e(2, WS::Empty)});
  model.setOccupiedWorkspaceIds({2});
  QSignalSpy data_changed(&model, &WorkspaceModel::dataChanged);
  QSignalSpy revision_changed(&model, &WorkspaceModel::revisionChanged);

  model.setOccupiedWorkspaceIds({2});

  EXPECT_EQ(model.revision(), 2);
  EXPECT_EQ(data_changed.count(), 0);
  EXPECT_EQ(revision_changed.count(), 0);
}

TEST_F(WorkspaceModelTest, ActivateWorkspaceEmitsRequestForKnownWorkspace) {
  QSignalSpy activate_requested(&model, &WorkspaceModel::activateWorkspaceRequested);

  model.applyBatchUpdate({e(3, WS::Empty)});
  model.activateWorkspace(3);

  ASSERT_EQ(activate_requested.count(), 1);
  EXPECT_EQ(activate_requested.at(0).at(0).toInt(), 3);
}

TEST_F(WorkspaceModelTest, ActivateWorkspaceEmitsRequestForEmptyFixedWorkspace) {
  QSignalSpy activate_requested(&model, &WorkspaceModel::activateWorkspaceRequested);

  model.applyBatchUpdate({e(3, WS::Empty)});
  model.activateWorkspace(4);

  ASSERT_EQ(activate_requested.count(), 1);
  EXPECT_EQ(activate_requested.at(0).at(0).toInt(), 4);
}

TEST_F(WorkspaceModelTest, ActivateWorkspaceIgnoresInvalidWorkspace) {
  QSignalSpy activate_requested(&model, &WorkspaceModel::activateWorkspaceRequested);

  model.activateWorkspace(0);

  EXPECT_EQ(activate_requested.count(), 0);
}
