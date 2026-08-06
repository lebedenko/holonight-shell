#include "LayerShell.h"
#include "SidebarManager.h"

#include <QGuiApplication>
#include <QScreen>
#include <QSignalSpy>
#include <QString>

#include <gtest/gtest.h>

// SidebarManager only touches Wayland when it creates a surface, so construction and the
// pure state accessors are unit-testable here; anything that opens a real layer surface
// (toggle()/closeAll() sequencing) needs a live compositor (REQ-C-3) and is covered by the manual
// verification checklist instead.

TEST(SidebarManagerMonitorValidation, RejectsUnknownMonitorName) {
  EXPECT_FALSE(SidebarManager::isKnownMonitor(QStringLiteral("definitely-not-a-real-monitor-xyz123")));
}

TEST(SidebarManagerMonitorValidation, RejectsEmptyMonitorName) {
  EXPECT_FALSE(SidebarManager::isKnownMonitor(QString{}));
}

TEST(SidebarManagerMonitorValidation, AcceptsAnyCurrentlyConnectedNonEmptyNamedScreen) {
  // The offscreen QPA platform used by this test harness names its QScreen "" — a genuinely
  // empty name is correctly rejected (see RejectsEmptyMonitorName), so this only exercises
  // screens with a real name, mirroring what a live Wayland session always provides.
  const QList<QScreen*> screens = QGuiApplication::screens();
  ASSERT_FALSE(screens.isEmpty()) << "test harness offscreen platform should expose at least one QScreen";

  int named_screens = 0;
  for (QScreen* screen : screens) {
    if (screen->name().isEmpty()) {
      continue;
    }
    ++named_screens;
    EXPECT_TRUE(SidebarManager::isKnownMonitor(screen->name()));
  }
  if (named_screens == 0) {
    GTEST_SKIP() << "offscreen platform exposed no non-empty-named QScreen in this environment";
  }
}

TEST(SidebarManagerMonitorValidation, RejectsWhitespaceOnlyName) {
  EXPECT_FALSE(SidebarManager::isKnownMonitor(QStringLiteral("   ")));
}

TEST(SidebarManagerCurrentTab, DefaultsToZeroForUnreportedMonitor) {
  LayerShell shell;
  SidebarManager manager(shell);

  // Must match what createSurface() seeds the QML root's currentTab with, or a monitor whose
  // sidebar has never been opened would read back a tab the QML never selected.
  EXPECT_EQ(manager.currentTabForMonitor(QStringLiteral("DP-3")), 0);
}

TEST(SidebarManagerCurrentTab, ReadsBackTheTabReportedForThatMonitor) {
  LayerShell shell;
  SidebarManager manager(shell);

  manager.onCurrentTabChanged(QStringLiteral("DP-3"), 4);
  manager.onCurrentTabChanged(QStringLiteral("HDMI-A-1"), 2);

  // Per-monitor, not global: brightness suppression asks every monitor in turn, so one monitor's
  // tab must never answer for another's (REQ-F-006).
  EXPECT_EQ(manager.currentTabForMonitor(QStringLiteral("DP-3")), 4);
  EXPECT_EQ(manager.currentTabForMonitor(QStringLiteral("HDMI-A-1")), 2);
}

TEST(SidebarManagerCurrentTab, EmitsCurrentTabChangedWithMonitorAndIndex) {
  LayerShell shell;
  SidebarManager manager(shell);
  QSignalSpy spy(&manager, &SidebarManager::currentTabChanged);
  ASSERT_TRUE(spy.isValid());

  manager.onCurrentTabChanged(QStringLiteral("DP-3"), 4);

  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("DP-3"));
  EXPECT_EQ(spy.at(0).at(1).toInt(), 4);
}

TEST(SidebarManagerCurrentTab, EmitsEvenWhenTheTabIndexRepeats) {
  LayerShell shell;
  SidebarManager manager(shell);
  QSignalSpy spy(&manager, &SidebarManager::currentTabChanged);

  manager.onCurrentTabChanged(QStringLiteral("DP-3"), 4);
  manager.onCurrentTabChanged(QStringLiteral("DP-3"), 4);

  // No de-duplication here on purpose: the only caller is RightSidebar.qml's onCurrentTabChanged
  // handler, which Qt already fires on real changes only, and the suppression consumer is
  // idempotent. Filtering here would add a second, redundant notion of "changed".
  EXPECT_EQ(spy.count(), 2);
}
