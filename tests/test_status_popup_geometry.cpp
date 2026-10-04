#include "ShellConstants.h"
#include "StatusPopupGeometry.h"
#include "TooltipGeometry.h"

#include <gtest/gtest.h>

TEST(StatusPopupGeometry, FitsRequestedSurfaceAndCentersUnderAnchor) {
  const auto geometry =
      statusPopupGeometry(QSize(348, 250), QRect(0, 0, 1920, 1080), QRect(0, 64, 1920, 1016), 900, 80);
  EXPECT_EQ(geometry.surface_width, 348);
  EXPECT_EQ(geometry.surface_height, 250);
  EXPECT_EQ(geometry.left_margin, 766);
  EXPECT_EQ(geometry.pointer_x, 174);
}

TEST(StatusPopupGeometry, ResizesWithoutMovingTheAnchor) {
  const QRect screen(0, 0, 1920, 1080);
  const QRect available(0, 64, 1920, 1016);
  const auto small = statusPopupGeometry(QSize(348, 250), screen, available, 900, 80);
  const auto large = statusPopupGeometry(QSize(650, 800), screen, available, 900, 80);
  EXPECT_GT(large.surface_height, small.surface_height);
  EXPECT_GT(large.surface_width, small.surface_width);
  EXPECT_EQ(small.left_margin + small.pointer_x, large.left_margin + large.pointer_x);
  EXPECT_EQ(statusPopupGeometry(QSize(348, 250), screen, available, 900, 80), small);
}

TEST(StatusPopupGeometry, ClampsToMonitorAndKeepsPointerInsideFrame) {
  const QRect screen(0, 0, 800, 600);
  const QRect available(0, 64, 800, 536);
  for (const int anchor : {0, 400, 790}) {
    const auto geometry = statusPopupGeometry(QSize(2000, 2000), screen, available, anchor, 40);
    EXPECT_GE(geometry.left_margin, kScreenEdgeMargin);
    EXPECT_LE(geometry.left_margin + geometry.surface_width, 800 - kScreenEdgeMargin);
    EXPECT_LE(geometry.surface_height + kStatusPopupTopGap + kScreenEdgeMargin, available.height());
    EXPECT_GE(geometry.pointer_x, 33);
    EXPECT_LE(geometry.pointer_x, geometry.surface_width - 33);
  }
}

TEST(StatusPopupGeometry, LocalizesNonPrimaryMonitorAndAvailableHorizontalBounds) {
  const auto geometry =
      statusPopupGeometry(QSize(348, 250), QRect(1920, 100, 1920, 1080), QRect(2020, 164, 1820, 1016), 1920, 40);
  EXPECT_EQ(geometry.left_margin, 108);
  EXPECT_EQ(geometry.pointer_x, 33);
}

TEST(StatusPopupGeometry, HandlesTinyAllocationsWithoutInvalidClampBounds) {
  const auto geometry = statusPopupGeometry(QSize(0, 0), QRect(0, 0, 32, 32), QRect(0, 0, 32, 32), 0, 0);
  EXPECT_EQ(geometry.surface_width, 1);
  EXPECT_EQ(geometry.surface_height, 1);
  EXPECT_GE(geometry.pointer_x, 0);
  EXPECT_LE(geometry.pointer_x, geometry.surface_width);
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
