#include "ConfigService.h"
#include "WidgetSurfacePolicy.h"
#include "qwayland-wlr-layer-shell-unstable-v1.h"

#include <gtest/gtest.h>

using namespace HoloNight::ShellConfig;

namespace {

using Surface = QtWayland::zwlr_layer_surface_v1;

}  // namespace

TEST(WidgetSurfacePolicy, PlacesTopAnchoredWidgetBelowBar) {
  const WidgetSurfacePlacement placement = widgetSurfacePlacement(WidgetPosition::RightTop, 32);

  EXPECT_EQ(placement.anchor_flags, Surface::anchor_right | Surface::anchor_top);
  EXPECT_EQ(placement.width, 460);
  EXPECT_EQ(placement.height, 200);
  EXPECT_EQ(placement.top_margin, 96);
  EXPECT_EQ(placement.right_margin, 32);
  EXPECT_EQ(placement.bottom_margin, 32);
  EXPECT_EQ(placement.left_margin, 32);
}

TEST(WidgetSurfacePolicy, FourArgOverloadUsesRequestedSize) {
  const WidgetSurfacePlacement placement =
      widgetSurfacePlacement(WidgetPosition::RightTop, 32, kMprisWidgetWidth, kMprisWidgetHeight);

  EXPECT_EQ(placement.anchor_flags, Surface::anchor_right | Surface::anchor_top);
  EXPECT_EQ(placement.width, 368);
  EXPECT_EQ(placement.height, 456);
  EXPECT_EQ(placement.top_margin, 96);
  EXPECT_EQ(placement.right_margin, 32);
  EXPECT_EQ(placement.bottom_margin, 32);
  EXPECT_EQ(placement.left_margin, 32);
}

TEST(WidgetSurfacePolicy, TwoArgOverloadDelegatesToDefaultClockSize) {
  const WidgetSurfacePlacement two_arg = widgetSurfacePlacement(WidgetPosition::LeftBottom, 16);
  const WidgetSurfacePlacement four_arg = widgetSurfacePlacement(WidgetPosition::LeftBottom, 16, 460, 200);

  EXPECT_EQ(two_arg.anchor_flags, four_arg.anchor_flags);
  EXPECT_EQ(two_arg.width, four_arg.width);
  EXPECT_EQ(two_arg.height, four_arg.height);
  EXPECT_EQ(two_arg.top_margin, four_arg.top_margin);
}

TEST(WidgetSurfacePolicy, KeepsCenterWidgetUnanchored) {
  const WidgetSurfacePlacement placement = widgetSurfacePlacement(WidgetPosition::CenterCenter, 24);

  EXPECT_EQ(placement.anchor_flags, 0U);
  EXPECT_EQ(placement.top_margin, 24);
  EXPECT_EQ(placement.right_margin, 24);
  EXPECT_EQ(placement.bottom_margin, 24);
  EXPECT_EQ(placement.left_margin, 24);
}

TEST(WidgetSurfacePolicy, MapsAllNinePositionsToExpectedAnchors) {
  EXPECT_EQ(widgetSurfacePlacement(WidgetPosition::LeftTop, 0).anchor_flags,
            Surface::anchor_left | Surface::anchor_top);
  EXPECT_EQ(widgetSurfacePlacement(WidgetPosition::CenterTop, 0).anchor_flags, Surface::anchor_top);
  EXPECT_EQ(widgetSurfacePlacement(WidgetPosition::RightTop, 0).anchor_flags,
            Surface::anchor_right | Surface::anchor_top);
  EXPECT_EQ(widgetSurfacePlacement(WidgetPosition::LeftCenter, 0).anchor_flags, Surface::anchor_left);
  EXPECT_EQ(widgetSurfacePlacement(WidgetPosition::CenterCenter, 0).anchor_flags, 0U);
  EXPECT_EQ(widgetSurfacePlacement(WidgetPosition::RightCenter, 0).anchor_flags, Surface::anchor_right);
  EXPECT_EQ(widgetSurfacePlacement(WidgetPosition::LeftBottom, 0).anchor_flags,
            Surface::anchor_left | Surface::anchor_bottom);
  EXPECT_EQ(widgetSurfacePlacement(WidgetPosition::CenterBottom, 0).anchor_flags, Surface::anchor_bottom);
  EXPECT_EQ(widgetSurfacePlacement(WidgetPosition::RightBottom, 0).anchor_flags,
            Surface::anchor_right | Surface::anchor_bottom);
}

TEST(WidgetSurfacePolicy, EmptyMonitorListTargetsEveryMonitor) {
  EXPECT_TRUE(widgetTargetsMonitor({}, QStringLiteral("DP-1")));
  EXPECT_TRUE(shouldCreateWidgetSurface({}, {}, QStringLiteral("HDMI-A-1")));
}

TEST(WidgetSurfacePolicy, ExplicitMonitorListTargetsOnlyNamedMonitor) {
  const QStringList monitors{QStringLiteral("DP-1"), QStringLiteral("eDP-1")};

  EXPECT_TRUE(widgetTargetsMonitor(monitors, QStringLiteral("DP-1")));
  EXPECT_FALSE(widgetTargetsMonitor(monitors, QStringLiteral("HDMI-A-1")));
  EXPECT_FALSE(shouldCreateWidgetSurface(monitors, {}, QStringLiteral("HDMI-A-1")));
}

TEST(WidgetSurfacePolicy, EmptyBlockerClaimsEveryMonitor) {
  const QList<QStringList> blockers{{}};

  EXPECT_TRUE(widgetBlockedOnMonitor(blockers, QStringLiteral("DP-1")));
  EXPECT_FALSE(shouldCreateWidgetSurface({}, blockers, QStringLiteral("DP-1")));
}

TEST(WidgetSurfacePolicy, NamedBlockerClaimsOnlyMatchingMonitor) {
  const QList<QStringList> blockers{{QStringLiteral("DP-1")}};

  EXPECT_TRUE(widgetBlockedOnMonitor(blockers, QStringLiteral("DP-1")));
  EXPECT_FALSE(widgetBlockedOnMonitor(blockers, QStringLiteral("HDMI-A-1")));
  EXPECT_FALSE(shouldCreateWidgetSurface({}, blockers, QStringLiteral("DP-1")));
  EXPECT_TRUE(shouldCreateWidgetSurface({}, blockers, QStringLiteral("HDMI-A-1")));
}
