#include "LauncherSurfaceLifecycle.h"

#include <gtest/gtest.h>

TEST(LauncherSurfaceLifecycle, DefersShowUntilShellBecomesActive) {
  LauncherSurfaceLifecycle lifecycle;

  const LauncherSurfaceCommand pending = lifecycle.show(QStringLiteral("DP-1"), false);
  EXPECT_EQ(pending.type, LauncherSurfaceCommandType::None);
  EXPECT_TRUE(lifecycle.pendingShow());
  EXPECT_EQ(lifecycle.pendingScreen(), QStringLiteral("DP-1"));

  const LauncherSurfaceCommand create = lifecycle.shellActivated(true);
  EXPECT_EQ(create.type, LauncherSurfaceCommandType::CreateSurface);
  EXPECT_EQ(create.screen_name, QStringLiteral("DP-1"));
  EXPECT_FALSE(lifecycle.pendingShow());
  EXPECT_TRUE(lifecycle.pendingScreen().isEmpty());
}

TEST(LauncherSurfaceLifecycle, ActiveShowRequestsSurfaceCreationAndPublishesResult) {
  LauncherSurfaceLifecycle lifecycle;

  const LauncherSurfaceCommand create = lifecycle.show(QStringLiteral("HDMI-A-1"), true);
  EXPECT_EQ(create.type, LauncherSurfaceCommandType::CreateSurface);
  EXPECT_EQ(create.screen_name, QStringLiteral("HDMI-A-1"));

  const LauncherSurfaceCommand visible = lifecycle.surfaceCreated(true);
  ASSERT_TRUE(visible.visible.has_value());
  EXPECT_TRUE(*visible.visible);
  EXPECT_TRUE(lifecycle.visible());
}

TEST(LauncherSurfaceLifecycle, ToggleVisibleSurfaceStartsCloseAnimationOnce) {
  LauncherSurfaceLifecycle lifecycle;
  const LauncherSurfaceCommand create = lifecycle.show(QString(), true);
  EXPECT_EQ(create.type, LauncherSurfaceCommandType::CreateSurface);
  const LauncherSurfaceCommand visible = lifecycle.surfaceCreated(true);
  ASSERT_TRUE(visible.visible.has_value());
  EXPECT_TRUE(*visible.visible);

  const LauncherSurfaceCommand close = lifecycle.toggle(QString(), true, true);
  EXPECT_EQ(close.type, LauncherSurfaceCommandType::StartCloseAnimation);
  EXPECT_TRUE(lifecycle.closing());

  const LauncherSurfaceCommand duplicate = lifecycle.hide(true);
  EXPECT_EQ(duplicate.type, LauncherSurfaceCommandType::None);
}

TEST(LauncherSurfaceLifecycle, NotifyHideReadyDestroysSurfaceAndClearsVisibility) {
  LauncherSurfaceLifecycle lifecycle;
  const LauncherSurfaceCommand create = lifecycle.show(QString(), true);
  EXPECT_EQ(create.type, LauncherSurfaceCommandType::CreateSurface);
  const LauncherSurfaceCommand visible = lifecycle.surfaceCreated(true);
  ASSERT_TRUE(visible.visible.has_value());
  EXPECT_TRUE(*visible.visible);
  const LauncherSurfaceCommand close = lifecycle.hide(true);
  EXPECT_EQ(close.type, LauncherSurfaceCommandType::StartCloseAnimation);

  const LauncherSurfaceCommand destroy = lifecycle.notifyHideReady(true);
  EXPECT_EQ(destroy.type, LauncherSurfaceCommandType::DestroySurface);
  ASSERT_TRUE(destroy.visible.has_value());
  EXPECT_FALSE(*destroy.visible);
  EXPECT_FALSE(lifecycle.visible());
  EXPECT_FALSE(lifecycle.closing());
}

TEST(LauncherSurfaceLifecycle, SurfaceClosedClearsClosingAndVisibility) {
  LauncherSurfaceLifecycle lifecycle;
  const LauncherSurfaceCommand create = lifecycle.show(QString(), true);
  EXPECT_EQ(create.type, LauncherSurfaceCommandType::CreateSurface);
  const LauncherSurfaceCommand visible = lifecycle.surfaceCreated(true);
  ASSERT_TRUE(visible.visible.has_value());
  EXPECT_TRUE(*visible.visible);
  const LauncherSurfaceCommand close = lifecycle.hide(true);
  EXPECT_EQ(close.type, LauncherSurfaceCommandType::StartCloseAnimation);

  const LauncherSurfaceCommand closed = lifecycle.surfaceClosed();
  EXPECT_EQ(closed.type, LauncherSurfaceCommandType::DestroySurface);
  ASSERT_TRUE(closed.visible.has_value());
  EXPECT_FALSE(*closed.visible);
  EXPECT_FALSE(lifecycle.visible());
  EXPECT_FALSE(lifecycle.closing());
}
