#include "SidebarSurfacePolicy.h"

#include <gtest/gtest.h>

TEST(SidebarSurfacePolicy, DefaultHeightMatchesInitialPanelHeight) { EXPECT_EQ(sidebarDefaultHeight(), 600); }

TEST(SidebarSurfacePolicy, BoundsHeightToMinimumAndScreenAvailableSpace) {
  EXPECT_EQ(boundedSidebarHeight(100, 1080), 461);
  EXPECT_EQ(boundedSidebarHeight(600, 1080), 600);
  EXPECT_EQ(boundedSidebarHeight(2000, 1080), 984);
}

TEST(SidebarSurfacePolicy, BoundsHeightToDefaultWhenScreenIsMissing) {
  EXPECT_EQ(boundedSidebarHeight(100, 0), 461);
  EXPECT_EQ(boundedSidebarHeight(500, 0), 500);
  EXPECT_EQ(boundedSidebarHeight(900, 0), 600);
}
