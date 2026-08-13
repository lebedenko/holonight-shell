#include "SidebarSurfacePolicy.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QScreen>

#include <gtest/gtest.h>

using Holonight::Wayland::Anchor;
using Holonight::Wayland::KeyboardInteractivity;
using Holonight::Wayland::Layer;

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

TEST(SidebarSurfacePolicy, ProducesCompleteFullscreenInteractiveSpec) {
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);
  const auto spec = sidebarSurfaceSpec(screen, QStringLiteral("DP-3"), 720, 4);

  EXPECT_EQ(spec.output, screen);
  EXPECT_EQ(spec.name_space, QStringLiteral("sidebar"));
  EXPECT_EQ(spec.layer, Layer::Top);
  EXPECT_EQ(spec.anchors, Anchor::Top | Anchor::Bottom | Anchor::Left | Anchor::Right);
  EXPECT_EQ(spec.width, 0);
  EXPECT_EQ(spec.height, 0);
  EXPECT_EQ(spec.margin_top, 0);
  EXPECT_EQ(spec.margin_right, 0);
  EXPECT_EQ(spec.margin_bottom, 0);
  EXPECT_EQ(spec.margin_left, 0);
  EXPECT_EQ(spec.exclusive_zone, 0);
  EXPECT_EQ(spec.keyboard_interactivity, KeyboardInteractivity::Exclusive);
  EXPECT_EQ(spec.input_region_policy, Holonight::Wayland::InputRegionPolicy::Default);
  EXPECT_EQ(spec.color, Qt::transparent);
  EXPECT_TRUE(spec.window_flags.testFlag(Qt::BypassWindowManagerHint));
  EXPECT_EQ(spec.qml_url, QUrl(QStringLiteral("qrc:/HolonightShell/RightSidebar/RightSidebar.qml")));
  EXPECT_EQ(spec.initial_properties.value(QStringLiteral("barMonitorName")).toString(), QStringLiteral("DP-3"));
  EXPECT_FALSE(spec.initial_properties.value(QStringLiteral("active")).toBool());
  EXPECT_EQ(spec.initial_properties.value(QStringLiteral("panelHeight")).toInt(), 720);
  EXPECT_EQ(spec.initial_properties.value(QStringLiteral("currentTab")).toInt(), 4);
}

TEST(SidebarSurfacePolicy, InstallsIconImageProviderBeforeLoad) {
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);
  const auto spec = sidebarSurfaceSpec(screen, QStringLiteral("DP-3"), 600, 0);
  ASSERT_TRUE(static_cast<bool>(spec.before_load));
  QQmlEngine engine;
  spec.before_load(&engine);
  EXPECT_NE(engine.imageProvider(QStringLiteral("icon")), nullptr);
}
