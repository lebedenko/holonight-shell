#include "ActiveWindowService.h"

#include <gtest/gtest.h>

TEST(ActiveWindowService, FocusedMonitorThenActiveWindowUpdatesThatMonitor) {
  ActiveWindowState state;

  ActiveWindowStateChange focus_change = applyActiveWindowEvent(state, "focusedmon>>DP-1,1");
  EXPECT_EQ(focus_change.changed_monitors, QStringList({QStringLiteral("DP-1")}));
  EXPECT_EQ(state.focused_monitor_name, QStringLiteral("DP-1"));
  EXPECT_EQ(state.monitor_workspaces.value(QStringLiteral("DP-1")), 1);

  ActiveWindowStateChange window_change = applyActiveWindowEvent(state, "activewindow>>kitty,build output");
  EXPECT_EQ(window_change.changed_monitors, QStringList({QStringLiteral("DP-1")}));
  EXPECT_TRUE(window_change.classes_to_resolve.contains(QStringLiteral("kitty")));
  EXPECT_EQ(state.monitor_windows.value(QStringLiteral("DP-1")).app_class, QStringLiteral("kitty"));
  EXPECT_EQ(state.monitor_windows.value(QStringLiteral("DP-1")).title, QStringLiteral("build output"));
}

TEST(ActiveWindowService, ActiveWindowWithoutFocusedMonitorDoesNothing) {
  ActiveWindowState state;

  const ActiveWindowStateChange change = applyActiveWindowEvent(state, "activewindow>>kitty,build output");

  EXPECT_TRUE(change.changed_monitors.isEmpty());
  EXPECT_TRUE(state.monitor_windows.isEmpty());
}

TEST(ActiveWindowService, FocusedMonitorUpdatesVisibleWorkspace) {
  ActiveWindowState state;
  state.monitor_workspaces.insert(QStringLiteral("DP-1"), 6);

  const ActiveWindowStateChange change = applyActiveWindowEvent(state, "focusedmon>>DP-1,8");

  EXPECT_EQ(change.changed_monitors, QStringList({QStringLiteral("DP-1")}));
  EXPECT_EQ(change.workspace_changed_monitors, QStringList({QStringLiteral("DP-1")}));
  EXPECT_EQ(state.focused_monitor_name, QStringLiteral("DP-1"));
  EXPECT_EQ(state.monitor_workspaces.value(QStringLiteral("DP-1")), 8);
}

TEST(ActiveWindowService, FocusedMonitorSameWorkspaceDoesNotEmitMonitorChange) {
  ActiveWindowState state;
  state.monitor_workspaces.insert(QStringLiteral("DP-1"), 8);

  const ActiveWindowStateChange change = applyActiveWindowEvent(state, "focusedmon>>DP-1,8");

  EXPECT_TRUE(change.changed_monitors.isEmpty());
  EXPECT_TRUE(change.workspace_changed_monitors.isEmpty());
  EXPECT_EQ(state.focused_monitor_name, QStringLiteral("DP-1"));
  EXPECT_EQ(state.monitor_workspaces.value(QStringLiteral("DP-1")), 8);
}

TEST(ActiveWindowService, OpenWindowUpdatesMonitorMatchingWorkspaceMap) {
  ActiveWindowState state;
  state.monitor_workspaces.insert(QStringLiteral("DP-1"), 1);
  state.monitor_workspaces.insert(QStringLiteral("HDMI-A-1"), 2);

  const ActiveWindowStateChange change = applyActiveWindowEvent(state, "openwindow>>abc123,2,firefox,Issue #1, review");

  EXPECT_EQ(change.changed_monitors, QStringList({QStringLiteral("HDMI-A-1")}));
  EXPECT_EQ(state.monitor_windows.value(QStringLiteral("HDMI-A-1")).app_class, QStringLiteral("firefox"));
  EXPECT_EQ(state.monitor_windows.value(QStringLiteral("HDMI-A-1")).title, QStringLiteral("Issue #1, review"));
  EXPECT_FALSE(state.monitor_windows.contains(QStringLiteral("DP-1")));
}

TEST(ActiveWindowService, OpenWindowWithNonnumericWorkspaceDoesNothing) {
  ActiveWindowState state;
  state.monitor_workspaces.insert(QStringLiteral("DP-1"), 1);

  const ActiveWindowStateChange change =
      applyActiveWindowEvent(state, "openwindow>>abc123,special:magic,firefox,title");

  EXPECT_TRUE(change.changed_monitors.isEmpty());
  EXPECT_TRUE(state.monitor_windows.isEmpty());
}

TEST(ActiveWindowService, RefreshEventsRequestRequery) {
  for (const QByteArray& line : {QByteArray("closewindow>>abc123"), QByteArray("movewindow>>abc123,2"),
                                 QByteArray("workspace>>2"), QByteArray("destroyworkspace>>2")}) {
    ActiveWindowState state;
    EXPECT_TRUE(applyActiveWindowEvent(state, line).requery_requested);
  }

  ActiveWindowState state;
  EXPECT_FALSE(applyActiveWindowEvent(state, "createworkspace>>3").requery_requested);
}

TEST(ActiveWindowService, SnapshotChoosesLowestFocusHistoryIdPerMonitor) {
  ActiveWindowState state;
  const QHash<QString, int> monitor_workspaces = {
      {QStringLiteral("DP-1"), 1},
      {QStringLiteral("HDMI-A-1"), 2},
  };
  const QList<HyprlandClientInfo> clients = {
      HyprlandClientInfo{.app_class = QStringLiteral("stale"),
                         .title = QStringLiteral("Old"),
                         .workspace_id = 1,
                         .focus_history_id = 4},
      HyprlandClientInfo{.app_class = QStringLiteral("Code"),
                         .title = QStringLiteral("holonight-shell"),
                         .workspace_id = 1,
                         .focus_history_id = 0},
      HyprlandClientInfo{.app_class = QStringLiteral("firefox"),
                         .title = QStringLiteral("Docs"),
                         .workspace_id = 2,
                         .focus_history_id = 2},
  };

  const ActiveWindowStateChange change = applyActiveWindowSnapshot(state, monitor_workspaces, clients);

  EXPECT_TRUE(change.changed_monitors.contains(QStringLiteral("DP-1")));
  EXPECT_TRUE(change.changed_monitors.contains(QStringLiteral("HDMI-A-1")));
  EXPECT_EQ(state.monitor_workspaces, monitor_workspaces);
  EXPECT_EQ(state.monitor_windows.value(QStringLiteral("DP-1")).app_class, QStringLiteral("Code"));
  EXPECT_EQ(state.monitor_windows.value(QStringLiteral("DP-1")).title, QStringLiteral("holonight-shell"));
  EXPECT_EQ(state.monitor_windows.value(QStringLiteral("HDMI-A-1")).app_class, QStringLiteral("firefox"));
}

TEST(ActiveWindowService, SnapshotEmitsWhenVisibleWorkspaceChangesWithoutWindowChange) {
  ActiveWindowState state;
  state.monitor_workspaces.insert(QStringLiteral("DP-1"), 6);

  const QHash<QString, int> monitor_workspaces = {{QStringLiteral("DP-1"), 8}};
  const QList<HyprlandClientInfo> clients;

  const ActiveWindowStateChange change = applyActiveWindowSnapshot(state, monitor_workspaces, clients);

  EXPECT_EQ(change.changed_monitors, QStringList({QStringLiteral("DP-1")}));
  EXPECT_EQ(state.monitor_workspaces.value(QStringLiteral("DP-1")), 8);
  EXPECT_TRUE(state.monitor_windows.value(QStringLiteral("DP-1")).app_class.isEmpty());
  EXPECT_TRUE(state.monitor_windows.value(QStringLiteral("DP-1")).title.isEmpty());
}

TEST(ActiveWindowService, SnapshotDoesNotEmitChangesWhenStateIsUnchanged) {
  ActiveWindowState state;
  const QHash<QString, int> monitor_workspaces = {{QStringLiteral("DP-1"), 1}};
  const QList<HyprlandClientInfo> clients = {
      HyprlandClientInfo{.app_class = QStringLiteral("Code"),
                         .title = QStringLiteral("holonight-shell"),
                         .workspace_id = 1,
                         .focus_history_id = 0},
  };

  EXPECT_FALSE(applyActiveWindowSnapshot(state, monitor_workspaces, clients).changed_monitors.isEmpty());
  EXPECT_TRUE(applyActiveWindowSnapshot(state, monitor_workspaces, clients).changed_monitors.isEmpty());
}

TEST(ActiveWindowService, SnapshotRemovesStateForDisconnectedMonitor) {
  ActiveWindowState state;
  state.focused_monitor_name = QStringLiteral("HDMI-A-1");
  state.monitor_workspaces.insert(QStringLiteral("DP-1"), 1);
  state.monitor_workspaces.insert(QStringLiteral("HDMI-A-1"), 2);
  state.monitor_windows.insert(QStringLiteral("DP-1"), HyprlandActiveWindow{.app_class = QStringLiteral("Code"),
                                                                            .title = QStringLiteral("Shell")});
  state.monitor_windows.insert(
      QStringLiteral("HDMI-A-1"),
      HyprlandActiveWindow{.app_class = QStringLiteral("firefox"), .title = QStringLiteral("Documentation")});

  const ActiveWindowStateChange change = applyActiveWindowSnapshot(state, {{QStringLiteral("DP-1"), 1}}, {});

  EXPECT_FALSE(state.monitor_workspaces.contains(QStringLiteral("HDMI-A-1")));
  EXPECT_FALSE(state.monitor_windows.contains(QStringLiteral("HDMI-A-1")));
  EXPECT_TRUE(state.focused_monitor_name.isEmpty());
  EXPECT_EQ(change.workspace_changed_monitors, QStringList({QStringLiteral("HDMI-A-1")}));
  EXPECT_EQ(change.changed_monitors, QStringList({QStringLiteral("HDMI-A-1"), QStringLiteral("DP-1")}));
}

TEST(ActiveWindowService, MapsDesktopCategoriesToIconCategories) {
  EXPECT_EQ(activeWindowCategoryForDesktopCategories(QStringLiteral("Network;WebBrowser;")), QStringLiteral("browser"));
  EXPECT_EQ(activeWindowCategoryForDesktopCategories(QStringLiteral("Development;IDE;")), QStringLiteral("editor"));
  EXPECT_EQ(activeWindowCategoryForDesktopCategories(QStringLiteral("TerminalEmulator;System;")),
            QStringLiteral("terminal"));
  EXPECT_EQ(activeWindowCategoryForDesktopCategories(QStringLiteral("Utility;FileManager;")), QStringLiteral("files"));
  EXPECT_EQ(activeWindowCategoryForDesktopCategories(QStringLiteral("InstantMessaging;Network;")),
            QStringLiteral("chat"));
  EXPECT_EQ(activeWindowCategoryForDesktopCategories(QStringLiteral("Audio;Player;")), QStringLiteral("music"));
  EXPECT_EQ(activeWindowCategoryForDesktopCategories(QStringLiteral("Video;Player;")), QStringLiteral("video"));
  EXPECT_EQ(activeWindowCategoryForDesktopCategories(QStringLiteral("System;Settings;")), QStringLiteral("settings"));
  EXPECT_TRUE(activeWindowCategoryForDesktopCategories(QStringLiteral("Graphics;Viewer;")).isEmpty());
}

TEST(ActiveWindowService, ParsesDesktopEntryFirstSectionAndMatchesNameOrExec) {
  const QString desktop_entry = QStringLiteral(
      "[Desktop Entry]\n"
      "Name=Visual Studio Code\n"
      "Exec=/usr/bin/code --reuse-window=enabled\n"
      "Categories=Development;IDE;\n"
      "[Ignored]\n"
      "Categories=WebBrowser;\n");

  EXPECT_EQ(activeWindowCategoriesFromDesktopEntry(desktop_entry, QStringLiteral("code")),
            QStringLiteral("Development;IDE;"));
  EXPECT_EQ(activeWindowCategoriesFromDesktopEntry(desktop_entry, QStringLiteral("visual studio")),
            QStringLiteral("Development;IDE;"));
  EXPECT_TRUE(activeWindowCategoriesFromDesktopEntry(desktop_entry, QStringLiteral("firefox")).isEmpty());
}
