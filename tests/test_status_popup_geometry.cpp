#include "ShellConstants.h"
#include "StatusPopupGeometry.h"
#include "TooltipGeometry.h"

#include <gtest/gtest.h>

TEST(StatusPopupGeometry, SizePoliciesCoverKnownPopupKinds) {
  EXPECT_EQ(statusPopupSizePolicy(QStringLiteral("audio")).minimum_content_size, QSize(600, 480));
  EXPECT_EQ(statusPopupSizePolicy(QStringLiteral("audio")).preferred_content_size, QSize(780, 820));
  EXPECT_EQ(statusPopupSizePolicy(QStringLiteral("audio")).overflow_mode, StatusPopupOverflowMode::InternalList);
  EXPECT_EQ(statusPopupSizePolicy(QStringLiteral("network")).preferred_content_size, QSize(600, 866));
  EXPECT_EQ(statusPopupSizePolicy(QStringLiteral("network")).overflow_mode, StatusPopupOverflowMode::InternalList);
  EXPECT_EQ(statusPopupSizePolicy(QStringLiteral("weather")).preferred_content_size, QSize(760, 960));
  EXPECT_EQ(statusPopupSizePolicy(QStringLiteral("weather")).maximum_content_size, QSize(760, 960));
  EXPECT_EQ(statusPopupSizePolicy(QStringLiteral("battery")).preferred_content_size, QSize(300, 360));
  EXPECT_EQ(statusPopupSizePolicy(QStringLiteral("missing")).preferred_content_size, QSize(480, 320));
}

TEST(StatusPopupGeometry, CentersPopupUnderAnchorWhenThereIsRoom) {
  const StatusPopupGeometry geometry =
      statusPopupGeometry(QStringLiteral("battery"), QRect(0, 0, 1920, 1080), QRect(0, 64, 1920, 1016), 900, 80);

  EXPECT_EQ(geometry.content_width, 300);
  EXPECT_EQ(geometry.content_height, 360);
  EXPECT_EQ(geometry.surface_width, 348);
  EXPECT_EQ(geometry.surface_height, 390);
  EXPECT_EQ(geometry.left_margin, 766);
  EXPECT_EQ(geometry.pointer_x, 174);
}

TEST(StatusPopupGeometry, UsesTallNetworkCompositionOnRoomyMonitor) {
  const StatusPopupGeometry geometry =
      statusPopupGeometry(QStringLiteral("network"), QRect(0, 0, 1920, 1080), QRect(0, 64, 1920, 1016), 900, 80);

  EXPECT_EQ(geometry.content_width, 600);
  EXPECT_EQ(geometry.content_height, 866);
  EXPECT_EQ(geometry.surface_width, 648);
  EXPECT_EQ(geometry.surface_height, 896);
}

TEST(StatusPopupGeometry, ClampsPopupToLeftScreenEdge) {
  const StatusPopupGeometry geometry =
      statusPopupGeometry(QStringLiteral("network"), QRect(0, 0, 1920, 1080), QRect(0, 64, 1920, 1016), 0, 48);

  EXPECT_EQ(geometry.left_margin, 8);
  EXPECT_EQ(geometry.pointer_x, 33);
}

TEST(StatusPopupGeometry, ClampsPointerInsideRightPanelCorner) {
  const StatusPopupGeometry geometry =
      statusPopupGeometry(QStringLiteral("battery"), QRect(0, 0, 1920, 1080), QRect(0, 64, 1920, 1016), 1900, 40);

  EXPECT_EQ(geometry.left_margin, 1564);
  EXPECT_EQ(geometry.pointer_x, 315);
}

TEST(StatusPopupGeometry, LocalizesGlobalAnchorOnNonPrimaryScreen) {
  const StatusPopupGeometry geometry =
      statusPopupGeometry(QStringLiteral("battery"), QRect(1920, 0, 1920, 1080), QRect(1920, 64, 1920, 1016), 2820, 80);

  EXPECT_EQ(geometry.left_margin, 766);
  EXPECT_EQ(geometry.pointer_x, 174);
}

TEST(StatusPopupGeometry, UsesPreferredWeatherSizeOnRoomyMonitor) {
  const StatusPopupGeometry geometry =
      statusPopupGeometry(QStringLiteral("weather"), QRect(0, 0, 1920, 1080), QRect(0, 64, 1920, 1016), 900, 80);

  EXPECT_EQ(geometry.content_width, 760);
  EXPECT_EQ(geometry.content_height, 960);
  EXPECT_EQ(geometry.surface_height, 990);
}

TEST(StatusPopupGeometry, CapsWeatherHeightToConstrainedMonitor) {
  const StatusPopupGeometry geometry =
      statusPopupGeometry(QStringLiteral("weather"), QRect(0, 0, 1920, 900), QRect(0, 64, 1920, 836), 900, 80);

  EXPECT_LT(geometry.content_height, 960);
  EXPECT_EQ(geometry.surface_height, 824);
  EXPECT_LE(geometry.surface_height + kStatusPopupTopGap + kScreenEdgeMargin, 836);
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
