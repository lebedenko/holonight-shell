// Integration test (T-029): the workspace display count flows from ConfigService
// through ExtWorkspaceManager into WorkspaceModel, and the overflow threshold
// tracks live config changes.
#include "ConfigService.h"
#include "ExtWorkspaceManager.h"
#include "WorkspaceModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {

using WS = WorkspaceModel::WorkspaceState;
using Entry = WorkspaceModel::WorkspaceEntry;

QString setTempXdg(const QTemporaryDir& tmp) {
  qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
  return tmp.path() + "/holonight/config.toml";
}

void writeConfig(const QString& path, const QByteArray& content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write(content);
}

Entry entry(int workspace_id, WS state, bool on_monitor = true) {
  return Entry{.id = workspace_id, .name = QString::number(workspace_id), .state = state, .on_monitor = on_monitor};
}

}  // namespace

class WorkspaceConfigIntegrationTest : public ::testing::Test {
 protected:
  QTemporaryDir tmp;    // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  QString config_path;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

  void SetUp() override { config_path = setTempXdg(tmp); }
  void TearDown() override { qunsetenv("XDG_CONFIG_HOME"); }
};

// REQ-F-018, REQ-C-005: WorkspaceModel.displayCount mirrors the configured count at startup.
TEST_F(WorkspaceConfigIntegrationTest, StartupDisplayCountMatchesConfig) {
  writeConfig(config_path,
              "[bar.workspaces]\n"
              "count = 8\n");

  ConfigService config;
  WorkspaceModel model;
  ExtWorkspaceManager manager(&model, &config);

  EXPECT_EQ(model.displayCount(), 8);
  EXPECT_EQ(model.displayCount(), config.barWorkspaces().count);
}

// REQ-F-018: a live count change updates displayCount and bumps the model revision.
TEST_F(WorkspaceConfigIntegrationTest, LiveReloadUpdatesDisplayCountAndRevision) {
  writeConfig(config_path,
              "[bar.workspaces]\n"
              "count = 5\n");

  ConfigService config;
  WorkspaceModel model;
  ExtWorkspaceManager manager(&model, &config);
  ASSERT_EQ(model.displayCount(), 5);

  QSignalSpy count_spy(&model, &WorkspaceModel::displayCountChanged);
  QSignalSpy revision_spy(&model, &WorkspaceModel::revisionChanged);

  writeConfig(config_path,
              "[bar.workspaces]\n"
              "count = 9\n");
  QMetaObject::invokeMethod(&config, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(model.displayCount(), 9);
  EXPECT_EQ(count_spy.count(), 1);
  EXPECT_EQ(revision_spy.count(), 1);
}

// REQ-F-018, REQ-F-004: the edge-arrow "occupied beyond" threshold follows displayCount, so a
// workspace at id 7 is beyond the visible window under count=5 but in-range under count=10.
TEST_F(WorkspaceConfigIntegrationTest, OccupiedBeyondThresholdTracksConfiguredCount) {
  writeConfig(config_path,
              "[bar.workspaces]\n"
              "count = 5\n");

  ConfigService config;
  WorkspaceModel model;
  ExtWorkspaceManager manager(&model, &config);

  model.applyBatchUpdate({entry(1, WS::Empty), entry(7, WS::Occupied, /*on_monitor=*/true)});
  EXPECT_TRUE(model.hasOccupiedOrUrgentBeyond(model.displayCount() + 1));

  writeConfig(config_path,
              "[bar.workspaces]\n"
              "count = 10\n");
  QMetaObject::invokeMethod(&config, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(model.displayCount(), 10);
  EXPECT_FALSE(model.hasOccupiedOrUrgentBeyond(model.displayCount() + 1));
}
