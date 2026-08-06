#include "ExtWorkspaceManager.h"
#include "WorkspaceModel.h"

#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>
#include <memory>

// Regression test for the Q_ASSERT(config != nullptr) previously in the constructor, which aborted
// the process (even in default GTest Debug builds) when config was null. The guarded early return
// leaves the object fully inert but still safely destructible (REQ-F-A.2).
TEST(ExtWorkspaceManagerTest, NullConfigDoesNotAbortAndStaysDestructible) {
  WorkspaceModel model;
  QSignalSpy display_count_changed(&model, &WorkspaceModel::displayCountChanged);

  QTest::ignoreMessage(QtCriticalMsg,
                       "ExtWorkspaceManager: constructed with null ConfigService; workspace manager will remain inert");
  auto manager = std::make_unique<ExtWorkspaceManager>(&model, /*config=*/nullptr, /*parent=*/nullptr);

  EXPECT_EQ(model.displayCount(), 5);
  EXPECT_EQ(display_count_changed.size(), 0);
  manager.reset();
}
