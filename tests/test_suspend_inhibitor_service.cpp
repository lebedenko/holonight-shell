#include "InhibitorModel.h"
#include "SuspendInhibitorService.h"

#include <QSignalSpy>

#include <gtest/gtest.h>
#include <type_traits>

namespace {

// Override listInhibitors() to inject fake entries without a real logind connection.
class FakeSuspendInhibitorService final : public SuspendInhibitorService {
 public:
  using SuspendInhibitorService::SuspendInhibitorService;

  QList<InhibitorEntry> listInhibitors() override { return injected_entries; }

  QList<InhibitorEntry> injected_entries;
};

}  // namespace

// ─── T-041: ListInhibitors parsing and filtering ─────────────────────────────

TEST(SuspendInhibitorServiceTest, OnlyKeepsSleepInhibitors) {
  FakeSuspendInhibitorService service(SuspendInhibitorService::SkipInit);
  service.injected_entries = {
      {.who = QStringLiteral("gnome-session"), .why = QStringLiteral("user is active")},
      {.who = QStringLiteral("NetworkManager"), .why = QStringLiteral("suspend inhibited")},
  };

  service.start();

  EXPECT_EQ(service.inhibitorModel()->count(), 2);
}

TEST(SuspendInhibitorServiceTest, EmptyInhibitorListGivesZeroCount) {
  FakeSuspendInhibitorService service(SuspendInhibitorService::SkipInit);
  service.injected_entries.clear();

  service.start();

  EXPECT_EQ(service.inhibitorModel()->count(), 0);
}

TEST(SuspendInhibitorServiceTest, ModelReflectsWhoAndWhyFields) {
  FakeSuspendInhibitorService service(SuspendInhibitorService::SkipInit);
  service.injected_entries = {
      {.who = QStringLiteral("teams"), .why = QStringLiteral("video call in progress")},
  };

  service.start();

  ASSERT_EQ(service.inhibitorModel()->count(), 1);
  const QModelIndex idx = service.inhibitorModel()->index(0);
  EXPECT_EQ(service.inhibitorModel()->data(idx, InhibitorModel::WhoRole).toString(), QStringLiteral("teams"));
  EXPECT_EQ(service.inhibitorModel()->data(idx, InhibitorModel::WhyRole).toString(),
            QStringLiteral("video call in progress"));
}

TEST(SuspendInhibitorServiceTest, MultipleEntriesAllStored) {
  FakeSuspendInhibitorService service(SuspendInhibitorService::SkipInit);
  service.injected_entries = {
      {.who = QStringLiteral("zoom"), .why = QStringLiteral("meeting")},
      {.who = QStringLiteral("vlc"), .why = QStringLiteral("playing video")},
      {.who = QStringLiteral("steam"), .why = QStringLiteral("game running")},
  };

  service.start();

  EXPECT_EQ(service.inhibitorModel()->count(), 3);
}

TEST(SuspendInhibitorServiceTest, ConstServiceExposesReadOnlyModel) {
  static_assert(
      std::is_same_v<decltype(std::declval<const SuspendInhibitorService&>().inhibitorModel()), const InhibitorModel*>);
}

// ─── T-042: InhibitorModel Qt roles and data access ──────────────────────────

TEST(InhibitorModelTest, EmptyModelHasZeroCount) {
  InhibitorModel model;
  EXPECT_EQ(model.count(), 0);
  EXPECT_EQ(model.rowCount(), 0);
}

TEST(InhibitorModelTest, SetEntriesUpdatesCount) {
  InhibitorModel model;
  QSignalSpy count_spy(&model, &InhibitorModel::countChanged);

  model.setEntries({{.who = QStringLiteral("a"), .why = QStringLiteral("b")}});

  EXPECT_EQ(model.count(), 1);
  EXPECT_EQ(count_spy.count(), 1);
}

TEST(InhibitorModelTest, SetEntriesClearsOldEntries) {
  InhibitorModel model;
  model.setEntries({
      {.who = QStringLiteral("x"), .why = QStringLiteral("y")},
      {.who = QStringLiteral("p"), .why = QStringLiteral("q")},
  });

  model.setEntries({{.who = QStringLiteral("only"), .why = QStringLiteral("one")}});

  EXPECT_EQ(model.count(), 1);
}

TEST(InhibitorModelTest, WhoRoleReturnsWhoField) {
  InhibitorModel model;
  model.setEntries({{.who = QStringLiteral("systemd-sleep"), .why = QStringLiteral("reason")}});

  const QVariant val = model.data(model.index(0), InhibitorModel::WhoRole);
  EXPECT_EQ(val.toString(), QStringLiteral("systemd-sleep"));
}

TEST(InhibitorModelTest, WhyRoleReturnsWhyField) {
  InhibitorModel model;
  model.setEntries({{.who = QStringLiteral("app"), .why = QStringLiteral("downloading file")}});

  const QVariant val = model.data(model.index(0), InhibitorModel::WhyRole);
  EXPECT_EQ(val.toString(), QStringLiteral("downloading file"));
}

TEST(InhibitorModelTest, InvalidIndexReturnsEmptyVariant) {
  InhibitorModel model;
  model.setEntries({{.who = QStringLiteral("x"), .why = QStringLiteral("y")}});

  EXPECT_FALSE(model.data(model.index(-1)).isValid());
  EXPECT_FALSE(model.data(model.index(5)).isValid());
}

TEST(InhibitorModelTest, RoleNamesContainExpectedKeys) {
  InhibitorModel model;
  const QHash<int, QByteArray> roles = model.roleNames();

  EXPECT_EQ(roles.value(InhibitorModel::WhoRole), "who");
  EXPECT_EQ(roles.value(InhibitorModel::WhyRole), "why");
}

TEST(InhibitorModelTest, SetSameEntriesDoesNotEmitCountChanged) {
  InhibitorModel model;
  model.setEntries({{.who = QStringLiteral("a"), .why = QStringLiteral("b")}});

  QSignalSpy count_spy(&model, &InhibitorModel::countChanged);
  model.setEntries({{.who = QStringLiteral("different"), .why = QStringLiteral("content")}});

  // Count is still 1 → countChanged must NOT emit again.
  EXPECT_EQ(count_spy.count(), 0);
}

TEST(InhibitorModelTest, SetIdenticalEntriesDoesNotEmitModelReset) {
  InhibitorModel model;
  const QList<InhibitorEntry> entries{{.who = QStringLiteral("a"), .why = QStringLiteral("b")}};
  model.setEntries(entries);

  QSignalSpy reset_spy(&model, &QAbstractItemModel::modelReset);
  model.setEntries(entries);

  EXPECT_EQ(reset_spy.count(), 0);
}

TEST(InhibitorModelTest, SetDifferentEntriesEmitsModelReset) {
  InhibitorModel model;
  model.setEntries({{.who = QStringLiteral("a"), .why = QStringLiteral("b")}});

  QSignalSpy reset_spy(&model, &QAbstractItemModel::modelReset);
  model.setEntries({{.who = QStringLiteral("c"), .why = QStringLiteral("d")}});

  EXPECT_EQ(reset_spy.count(), 1);
}
