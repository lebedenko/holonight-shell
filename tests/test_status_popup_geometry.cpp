#include "ShellConstants.h"
#include "StatusPopupGeometry.h"
#include "TooltipGeometry.h"

#include <gtest/gtest.h>

TEST(StatusPopupGeometry, SurfaceFillsTheBudgetAndLocalizesTheAnchor) {
  const QRect screen(0, 0, 1920, 1080);
  const QRect available(0, 64, 1920, 1016);
  const auto placement = statusPopupSurfacePlacement(screen, available, 900, 80);
  EXPECT_EQ(placement.surface_size, statusPopupMaximumSize(screen, available));
  EXPECT_EQ(placement.left_margin, kScreenEdgeMargin);
  EXPECT_EQ(placement.anchor_center, 940 - kScreenEdgeMargin);
}

TEST(StatusPopupGeometry, SurfaceDoesNotDependOnTheAnchor) {
  const QRect screen(0, 0, 800, 600);
  const QRect available(0, 64, 800, 536);
  const auto left = statusPopupSurfacePlacement(screen, available, 0, 40);
  const auto right = statusPopupSurfacePlacement(screen, available, 790, 40);
  EXPECT_EQ(left.surface_size, right.surface_size);
  EXPECT_EQ(left.left_margin, right.left_margin);
  EXPECT_EQ(right.left_margin + right.surface_size.width(), 800 - kScreenEdgeMargin);
  EXPECT_EQ(right.anchor_center - left.anchor_center, 790);
}

TEST(StatusPopupGeometry, LocalizesNonPrimaryMonitorAndAvailableHorizontalBounds) {
  const auto placement =
      statusPopupSurfacePlacement(QRect(1920, 100, 1920, 1080), QRect(2020, 164, 1820, 1016), 1920, 40);
  EXPECT_EQ(placement.left_margin, 108);
  EXPECT_EQ(placement.anchor_center, 20 - 108);
}

TEST(StatusPopupGeometry, HandlesTinyAllocationsWithoutInvalidSizes) {
  const auto placement = statusPopupSurfacePlacement(QRect(0, 0, 32, 32), QRect(0, 0, 32, 32), 0, 0);
  EXPECT_EQ(placement.surface_size, QSize(16, 1));
}

TEST(TooltipGeometry, CentersUnderAnchorWhenThereIsRoom) {
  EXPECT_EQ(TooltipGeometry::leftMargin(1920, 0, 900, 80), 778);
}

TEST(TooltipGeometry, LocalizesGlobalAnchorOnNonPrimaryScreen) {
  EXPECT_EQ(TooltipGeometry::leftMargin(1920, 1920, 2820, 80), 778);
}

TEST(TooltipGeometry, ClampsToScreenEdges) {
  EXPECT_EQ(TooltipGeometry::leftMargin(1920, 1920, 1920, 48), 8);
  EXPECT_EQ(TooltipGeometry::leftMargin(1920, 1920, 3790, 48), 1588);
}

TEST(StatusPopupGeometry, InputRectangleIncludesPanelAndCornerCutoutsOnly) {
  const QRect panel = statusPopupInputRect(QSize(348, 250));
  EXPECT_EQ(panel, QRect(24, 15, 300, 211));
  EXPECT_TRUE(panel.contains(QPoint(174, 100)));
  EXPECT_TRUE(panel.contains(panel.topLeft()));
  EXPECT_TRUE(panel.contains(panel.bottomRight()));
  EXPECT_FALSE(panel.contains(QPoint(23, 100)));
  EXPECT_FALSE(panel.contains(QPoint(324, 100)));
  EXPECT_FALSE(panel.contains(QPoint(174, 14)));
  EXPECT_FALSE(panel.contains(QPoint(174, 226)));
  EXPECT_FALSE(panel.contains(QPoint(174, 6)));  // Decorative notch tip.
}

TEST(StatusPopupGeometry, InputRectangleTracksFrameResizing) {
  EXPECT_EQ(statusPopupInputRect(QSize(650, 800)), QRect(24, 15, 602, 761));
  EXPECT_EQ(statusPopupInputRect(QSize(49, 40)), QRect(24, 15, 1, 1));
}

TEST(StatusPopupGeometry, InputRectangleClampsUndersizedSurfacesToEmpty) {
  for (const QSize size : {QSize(48, 250), QSize(348, 39), QSize(1, 1), QSize(0, 0), QSize(-1, -1)}) {
    const QRect panel = statusPopupInputRect(size);
    EXPECT_TRUE(panel.isEmpty());
    EXPECT_GE(panel.width(), 0);
    EXPECT_GE(panel.height(), 0);
  }
}

TEST(StatusPopupGeometry, MaximumSizePreservesScreenAndBarAllowances) {
  EXPECT_EQ(statusPopupMaximumSize(QRect(0, 0, 800, 600), QRect(0, 64, 800, 536)),
            QSize(800 - 2 * kScreenEdgeMargin, 536 - kStatusPopupTopGap - kScreenEdgeMargin));
  EXPECT_EQ(statusPopupMaximumSize(QRect(0, 0, 800, 600), QRect(0, 0, 800, 600)),
            QSize(800 - 2 * kScreenEdgeMargin, 600 - kBarHeight - kStatusPopupTopGap - kScreenEdgeMargin));
  EXPECT_EQ(statusPopupMaximumSize(QRect(0, 0, 1, 1), QRect()), QSize(1, 1));
}
