#include "ActiveWindowService.h"
#include "HyprlandIpcClient.h"
#include "MonitorOccupancyService.h"
#include "WorkspaceModel.h"

#include <QSignalSpy>

#include <gtest/gtest.h>

namespace {

class FakeHyprlandIpcTransport final : public HyprlandIpcTransport {
 public:
  explicit FakeHyprlandIpcTransport(QObject* parent = nullptr) : HyprlandIpcTransport(parent) {}

  void connectEventStream() override { ++connect_count; }

  bool runCommand(const QByteArray& command, CommandCompletePredicate /*is_complete*/ = {}) override {
    last_command = command;
    ++run_command_count;
    return true;
  }

  [[nodiscard]] bool hasRunningCommand() const override { return false; }

  void fireEventLine(const QByteArray& line) { emit eventLineReceived(line); }

  int connect_count{0};
  int run_command_count{0};
  QByteArray last_command;
};

struct MonitorOccupancyFixture {
  MonitorOccupancyFixture() {
    auto transport = std::make_unique<FakeHyprlandIpcTransport>();
    fake_transport = transport.get();
    active_window = std::make_unique<ActiveWindowService>(std::move(transport));
    occupancy = std::make_unique<MonitorOccupancyService>(&workspace_model, active_window.get());
    active_window->start();
  }

  WorkspaceModel workspace_model;
  FakeHyprlandIpcTransport* fake_transport{nullptr};
  std::unique_ptr<ActiveWindowService> active_window;
  std::unique_ptr<MonitorOccupancyService> occupancy;
};

}  // namespace

TEST(MonitorOccupancyService, UnknownMonitorDefaultsToEmpty) {
  MonitorOccupancyFixture fixture;

  EXPECT_TRUE(fixture.occupancy->isMonitorEmpty(QStringLiteral("DP-1")));
}

TEST(MonitorOccupancyService, EmitsWhenMonitorVisibleWorkspaceBecomesOccupied) {
  MonitorOccupancyFixture fixture;
  fixture.workspace_model.setOccupiedWorkspaceIds({2});
  QSignalSpy occupancy_changed(fixture.occupancy.get(), &MonitorOccupancyService::occupancyChanged);

  fixture.fake_transport->fireEventLine(QByteArrayLiteral("focusedmon>>DP-1,2"));

  ASSERT_EQ(occupancy_changed.count(), 1);
  EXPECT_EQ(occupancy_changed.at(0).at(0).toString(), QStringLiteral("DP-1"));
  EXPECT_FALSE(occupancy_changed.at(0).at(1).toBool());
  EXPECT_FALSE(fixture.occupancy->isMonitorEmpty(QStringLiteral("DP-1")));
}

TEST(MonitorOccupancyService, SuppressesDuplicateSignalsWhenOccupancyDoesNotChange) {
  MonitorOccupancyFixture fixture;
  fixture.workspace_model.setOccupiedWorkspaceIds({2});
  QSignalSpy occupancy_changed(fixture.occupancy.get(), &MonitorOccupancyService::occupancyChanged);

  fixture.fake_transport->fireEventLine(QByteArrayLiteral("focusedmon>>DP-1,2"));
  fixture.fake_transport->fireEventLine(QByteArrayLiteral("activewindow>>kitty,build output"));

  ASSERT_EQ(occupancy_changed.count(), 1);
  EXPECT_FALSE(occupancy_changed.at(0).at(1).toBool());
}

TEST(MonitorOccupancyService, EmitsWhenFocusedMonitorMovesToEmptyWorkspace) {
  MonitorOccupancyFixture fixture;
  fixture.workspace_model.setOccupiedWorkspaceIds({2});
  QSignalSpy occupancy_changed(fixture.occupancy.get(), &MonitorOccupancyService::occupancyChanged);

  fixture.fake_transport->fireEventLine(QByteArrayLiteral("focusedmon>>DP-1,2"));
  fixture.fake_transport->fireEventLine(QByteArrayLiteral("focusedmon>>DP-1,3"));

  ASSERT_EQ(occupancy_changed.count(), 2);
  EXPECT_EQ(occupancy_changed.at(1).at(0).toString(), QStringLiteral("DP-1"));
  EXPECT_TRUE(occupancy_changed.at(1).at(1).toBool());
  EXPECT_TRUE(fixture.occupancy->isMonitorEmpty(QStringLiteral("DP-1")));
}
