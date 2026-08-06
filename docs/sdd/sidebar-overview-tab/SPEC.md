# Sidebar Overview Tab Feature Specification

**Document Version**: 1.0  
**Feature ID**: sidebar-overview-tab  
**Status**: Specification-Ready  
**Date**: 2026-06-14  
**Author**: Andrii L

## Executive Summary

This specification defines the implementation of the Overview tab content in the holonight-shell right sidebar. The Overview tab displays three sections: a calendar showing the current month with navigation, a notifications summary listing the three most recent notification groups, and session control buttons (logout, sleep, reboot, shutdown). Additionally, this feature fixes the sidebar's dynamic height behavior so it adapts to content height rather than always consuming full screen height.

---

## Table of Contents

1. [Calendar Section](#calendar-section)
2. [Notifications Section](#notifications-section)
3. [Session Section](#session-section)
4. [Sidebar Dynamic Height](#sidebar-dynamic-height)
5. [Configuration & Services](#configuration--services)
6. [Build & Registration](#build--registration)
7. [Non-Functional Requirements](#non-functional-requirements)
8. [Constraints](#constraints)

---

## Calendar Section

### REQ-F-001: Calendar Month View Display

**Ubiquitous**: The Overview tab shall display a month view calendar showing the current calendar month at first open.

**Acceptance Criterion**: Calendar renders with the current month name and year in a header, grid-layout with days of the week, and all dates of the current month positioned correctly. No calendar is shown if the SidebarOverview component is not active.

---

### REQ-F-002: Month Navigation Controls

**Event-driven**: When the user clicks the previous-month button, the calendar shall decrement the displayed month by one; when the user clicks the next-month button, the calendar shall increment by one.

**Acceptance Criterion**: Clicking prev/next buttons updates the month/year header and day grid to the correct month. Navigation wraps correctly (December → January of next year, January → December of previous year). Days are repositioned on the grid for the new month.

---

### REQ-F-003: Adjacent-Month Day Styling

**State-driven**: While the calendar displays a month that includes days from the previous or next month (to fill the week grid), those adjacent-month days shall render in `textSubtle` palette color.

**Acceptance Criterion**: Days from the previous month (leading days) and days from the next month (trailing days) are visually distinct from the current month's days and match the `textSubtle` color token. No interactive behavior on adjacent-month days.

---

### REQ-F-004: Weekend Day Styling

**Ubiquitous**: The calendar shall render all Saturday and Sunday days in `accentViolet` palette color to distinguish weekends.

**Acceptance Criterion**: Every Saturday and Sunday cell in the calendar displays in the `accentViolet` color. Color applies regardless of which month is being displayed or whether the day is from the current month or an adjacent month.

---

### REQ-F-005: Today Highlight with Conditional Rendering

**Conditional**: Where the calendar is displaying the current month, the calendar shall highlight today's date with `accentCyan` text color, bold font weight, and a violet inner-glow frame (using `MultiEffect` shadow).

**Acceptance Criterion**: The today cell shows cyan text, bold weight, and a visible glow effect only when viewing the current month. When navigating away from the current month, today's highlight disappears. When navigating back to the current month, the highlight reappears.

---

### REQ-F-006: Default Day Styling

**Ubiquitous**: Non-weekend, non-adjacent, non-today days of the current month shall render in `onSurface` palette color.

**Acceptance Criterion**: Regular weekday dates in the displayed month appear in the `onSurface` color token. Color is visually distinct from `textSubtle` (adjacent-month) and `accentViolet` (weekend) colors.

---

### REQ-F-007: Calendar Configuration - Week Start Day

**Conditional**: Where the configuration file contains a `[calendar]` section with a `week_start_day` setting, the calendar shall use the configured week start (either "Sun" for Sunday or "Mon" for Monday) as the first column.

**Acceptance Criterion**: Setting `week_start_day = "Sun"` in holonight.toml `[calendar]` section places Sunday in the first column. Setting `week_start_day = "Mon"` places Monday in the first column. If not specified or invalid, default to "Mon". Configuration change takes effect on sidebar re-open without restart.

---

### REQ-F-008: CalendarConfig C++ Struct

**Ubiquitous**: The ConfigService shall define a `CalendarConfig` struct with a `week_start_day` field (enum `WeekStartDay::Sunday` or `WeekStartDay::Monday`, default `Monday`).

**Acceptance Criterion**: `CalendarConfig` is defined in `ConfigService.h` with an equality operator and a `week_start_day` enum field. The struct is parsed from the `[calendar]` TOML section. A getter method `calendarConfig()` is exposed on `ConfigService` and returns a const reference. A signal `calendarConfigChanged()` is emitted when the config is reloaded.

---

### REQ-F-009: CalendarService QML Singleton

**Ubiquitous**: The shell shall provide a `CalendarService` QML singleton (`QML_SINGLETON`) that exposes the `week_start_day` setting as a `Q_PROPERTY` and reacts to `ConfigService::calendarConfigChanged()`.

**Acceptance Criterion**: QML code imports `HolonightShell` and reads `CalendarService.weekStartDay` (returns `"Sun"` or `"Mon"` as a `QString`). When the config file changes, `CalendarService` emits `weekStartDayChanged()` and QML bindings update automatically. `CalendarService` is registered in `ShellApplication` alongside other singletons.

---

## Notifications Section

### REQ-F-010: Notifications Section Data Source

**Ubiquitous**: The NotificationService shall implement a `Q_INVOKABLE QVariantList recentHistoryGrouped(int maxGroups)` method that returns a list of the most recent notification groups, grouped by app name.

**Acceptance Criterion**: Calling `NotificationService.recentHistoryGrouped(3)` returns a list of at most 3 maps. Each map contains keys: `appName`, `appIcon`, `unreadCount`, `latestSummary`, `latestTimestampMs`, `totalCount`. For each app, `unreadCount` counts the number of history items with `read == false`. Groups are ordered by `latestTimestampMs` (most recent first). The method refreshes data when called.

---

### REQ-F-011: Notifications Section Display

**Ubiquitous**: The Overview tab shall display a "Notifications" section showing up to 3 recent notification groups, each on one row with app icon, app name, latest summary, unread count badge, and time-since-latest label.

**Acceptance Criterion**: The Notifications section shows a row per group with: (1) app icon image, (2) app name in `onSurface` color, (3) latest summary text in `onSurface` color, (4) unread count in a badge (e.g., circular with `accentCyan` background), (5) relative time label (e.g., "2 minutes ago") in `onSurfaceVariant` color. Layout uses palette colors only; no hardcoded hex values.

---

### REQ-F-012: Notifications Empty State

**State-driven**: While the NotificationService history is empty, the Notifications section shall display "No new notifications" in muted text (`onSurfaceVariant` color).

**Acceptance Criterion**: When `NotificationService.history_` contains no items, the Notifications section shows a single centered label with text "No new notifications" in the `onSurfaceVariant` palette color. Rows are not rendered.

---

### REQ-F-013: Notifications Overflow Indicator

**Conditional**: Where the total number of history items exceeds the number of groups shown (maxGroups = 3), the Notifications section shall display a "+N notifications" overflow label and a "View all" button.

**Acceptance Criterion**: If total unread history items > 3 shown, a label "+3 notifications" (or "+N" where N = total - shown) appears below the list. A "View all" button is clickable and clearly labeled. The button and label are visible; non-shown rows are not rendered.

---

### REQ-F-014: View All Button Switches to Notifications Tab

**Event-driven**: When the user clicks the "View all" button in the Notifications overflow area, the sidebar shall switch to the Notifications tab (tab index 2).

**Acceptance Criterion**: Clicking the button triggers a `currentTab = 2` change on the `RightSidebar` root object, which loads and displays the SidebarNotifications tab component. Sidebar remains open; animations and styling match standard tab switches.

---

### REQ-F-015: Notifications Section Refresh on Unread Signal

**Event-driven**: When `NotificationService.unreadCountChanged()` signal fires, the Overview tab shall refresh the results of `recentHistoryGrouped(3)` and re-render the Notifications section.

**Acceptance Criterion**: Adding a new notification, marking a notification as read, or closing a notification triggers `unreadCountChanged()`. The Notifications section re-evaluates `recentHistoryGrouped(3)` and updates the row count, text, badge counts, and overflow label without requiring a manual refresh or tab switch.

---

## Session Section

### REQ-F-016: Session Control Buttons

**Ubiquitous**: The Overview tab shall display four icon buttons pinned to the bottom of the sidebar panel, outside the scrollable content area, for logout, sleep, reboot, and shutdown operations.

**Acceptance Criterion**: Four buttons appear in a horizontal row at the bottom of the sidebar panel, above the layer-shell margin. Each button has a distinct icon (from system theme or custom assets) and is clickable. The buttons do not scroll with the content area above.

---

### REQ-F-017: Logout Button Command

**Event-driven**: When the user clicks the logout button, the shell shall execute `loginctl terminate-session self` via `QProcess::startDetached`.

**Acceptance Criterion**: Clicking the logout button immediately executes the loginctl command. The current session terminates without confirmation. The action completes or fails silently (no error dialog).

---

### REQ-F-018: Sleep Button Command

**Event-driven**: When the user clicks the sleep button, the shell shall execute `loginctl suspend` via `QProcess::startDetached`.

**Acceptance Criterion**: Clicking the sleep button immediately executes the loginctl command. The system enters sleep state without confirmation.

---

### REQ-F-019: Reboot Button Command

**Event-driven**: When the user clicks the reboot button, the shell shall execute `loginctl reboot` via `QProcess::startDetached`.

**Acceptance Criterion**: Clicking the reboot button immediately executes the loginctl command. The system reboots without confirmation.

---

### REQ-F-020: Shutdown Button Command

**Event-driven**: When the user clicks the shutdown button, the shell shall execute `loginctl poweroff` via `QProcess::startDetached`.

**Acceptance Criterion**: Clicking the shutdown button immediately executes the loginctl command. The system powers off without confirmation.

---

### REQ-F-021: Session Service or Singleton for Command Execution

**Ubiquitous**: The shell shall expose session control commands via a `SessionService` QML singleton or via existing infrastructure (`Q_INVOKABLE` methods callable from QML).

**Acceptance Criterion**: QML code in SidebarOverview.qml can call methods like `SessionService.logout()`, `SessionService.sleep()`, `SessionService.reboot()`, `SessionService.shutdown()`, or equivalent invokables without requiring a C++ wrapper class in the QML file. Each method executes the corresponding loginctl command.

---

## Sidebar Dynamic Height

### REQ-F-022: Content Preferred Height Property

**Ubiquitous**: Each sidebar tab component (SidebarOverview, SidebarCalendar, SidebarNotifications, etc.) shall declare a `property int preferredHeight` indicating the minimum height required to display its content.

**Acceptance Criterion**: Each tab QML file declares `property int preferredHeight: <value>` at the root level. For SidebarOverview, the value is the sum of all sections (calendar, notifications, session buttons) plus margins and spacing. Value is set in the component's `Component.onCompleted` or derived from child layout heights.

---

### REQ-F-023: SidebarContent Height Propagation

**Ubiquitous**: The SidebarContent component shall expose the current tab's `preferredHeight` via a `preferredHeight` property that reads from `loader.item?.preferredHeight`.

**Acceptance Criterion**: `SidebarContent.qml` declares `property int preferredHeight: loader.item?.preferredHeight ?? 0` or equivalent. When the active tab changes, `preferredHeight` updates to reflect the new tab's value. The property is readable from C++.

---

### REQ-F-024: SidebarManager Height Update Invokable

**Ubiquitous**: The SidebarManager shall implement a `Q_INVOKABLE void onContentHeightChanged(const QString& monitorName, int height)` method that records and applies the content's preferred height.

**Acceptance Criterion**: The C++ method accepts a monitor name and height value. The height is stored per-monitor. Subsequent calls to `openOnMonitor()` use the stored preferred height instead of the hard-coded screen-height calculation.

---

### REQ-F-025: RightSidebar Invokes Height Update

**Event-driven**: When the RightSidebar's `contentArea.preferredHeight` changes, the sidebar shall call `SidebarManager.onContentHeightChanged(barMonitorName, newHeight)`.

**Acceptance Criterion**: `RightSidebar.qml` defines a `Connections` to `contentArea.preferredHeightChanged` (or equivalent signal) that invokes the SidebarManager method. The monitor name is passed from `root.barMonitorName`.

---

### REQ-F-026: Minimum Height Floor

**Ubiquitous**: The sidebar's minimum height shall be the total height of all six tab bar buttons without scrolling, calculated as approximately 336 pixels (6 buttons × ~56 pixels each plus spacing).

**Acceptance Criterion**: `SidebarManager.openOnMonitor()` clamps the preferred height to a minimum of 336 pixels. Even if a tab reports a smaller preferred height, the sidebar never renders shorter than this minimum. All six tab buttons remain visible without vertical scrolling.

---

### REQ-F-027: Height Snap Behavior

**Ubiquitous**: Height changes shall apply instantly without animation; width animation remains unchanged.

**Acceptance Criterion**: When switching tabs, the sidebar's height changes immediately to the new tab's preferred height. Width still animates per the existing `tabSwitchAnimation` logic. No height-animation is performed.

---

### REQ-F-028: SidebarManager Height Calculation Update

**Ubiquitous**: The SidebarManager line 134 height calculation (`const int height = screen->geometry().height() - kSidebarTopMargin - kSidebarBottomMargin;`) shall be replaced with logic that uses the content's preferred height (clamped to the minimum).

**Acceptance Criterion**: `SidebarManager.cpp` line ~134 is updated to read a per-monitor stored preferred height (set via `onContentHeightChanged()`). If no preferred height has been recorded, it falls back to the old calculation. The height used in `set_size()` is the preferred height (or fallback), clamped to at least 336 pixels.

---

## Configuration & Services

### REQ-F-029: ConfigService TOML Parsing

**Ubiquitous**: The ConfigService shall parse a `[calendar]` section from the holonight.toml configuration file, with a `week_start_day` string field.

**Acceptance Criterion**: `ConfigService::loadConfig()` reads `[calendar] week_start_day = "Sun"` or `"Mon"` from the TOML file. If the section is absent, default to `CalendarConfig{ .week_start_day = WeekStartDay::Monday }`. Invalid values (e.g., "Tue") are treated as the default.

---

### REQ-F-030: NotificationService History Field

**Ubiquitous**: The NotificationService shall maintain a `QList<NotificationHistoryItem> history_` private member containing all persisted notification history items.

**Acceptance Criterion**: The `history_` field is already implemented (per codebase review). Each `NotificationHistoryItem` has fields: `appName`, `appIcon`, `summary`, `body`, `timestamp_ms`, `read` (bool), and `closed_reason`. The list is populated from `NotificationStore` on service startup.

---

### REQ-F-031: NotificationService Unread Count Signal

**Ubiquitous**: The NotificationService shall emit an `unreadCountChanged()` signal whenever a notification's read status changes, a notification is closed, or a new notification is persisted to history.

**Acceptance Criterion**: Calling `NotificationService.dismiss()`, `invokeAction()`, or adding a new notification triggers the signal. QML listeners can connect and refresh the Overview Notifications section.

---

## Build & Registration

### REQ-F-032: CMakeLists QML File Registration

**Ubiquitous**: The file `src/qml/RightSidebar/SidebarOverview.qml` shall be added to the `HOLONIGHT_QML_FILES` list in `CMakeLists.txt`.

**Acceptance Criterion**: After editing CMakeLists.txt to add the file, `task configure` succeeds without "HOLONIGHT_QML_FILES must match all src/qml/*.qml files" errors. The QML file is bundled into the resource file and accessible via `qrc:/HolonightShell/RightSidebar/SidebarOverview.qml`.

---

### REQ-F-033: SidebarOverview QML Module Declaration

**Ubiquitous**: The SidebarOverview.qml file shall declare `import HolonightShell` to ensure the module is available at QML parse time.

**Acceptance Criterion**: The first lines of the file include `import QtQuick`, `import Holonight`, and `import HolonightShell`. The file compiles without unresolved import errors.

---

## Non-Functional Requirements

### REQ-NF-001: Color Token Compliance

**Ubiquitous**: All colors used in the Overview tab QML components shall come from the `HoloniightPalette` (via `import Holonight`) and shall not include hardcoded hexadecimal color values.

**Acceptance Criterion**: A code review of `SidebarOverview.qml` and any new child components shows zero hardcoded hex color values (e.g., `#RRGGBB`). All color references use `HoloniightPalette.<token>` (e.g., `HoloniightPalette.accentCyan`).

---

### REQ-NF-002: C++ Standard Compliance

**Ubiquitous**: All C++ code additions for calendar, session, and sidebar height features shall compile with C++23 and follow the project's `.clang-tidy` and `.clang-format` rules.

**Acceptance Criterion**: Running `task format-check` and `task tidy` on modified C++ files reports no errors or warnings in the new code. Existing warnings in committed files are not required to be fixed.

---

### REQ-NF-003: Qt6 / QML Version Compatibility

**Ubiquitous**: All QML code shall be compatible with Qt 6 and shall use only officially supported APIs (no private headers, no Qt 5 compatibility modules unless documented in CLAUDE.md).

**Acceptance Criterion**: All QML files import `QtQuick` (not `QtQuick.Controls 2.15` or other version pins). The project builds with its pinned Qt6 version without errors. No use of `Qt5Compat.GraphicalEffects` for glow (use `QtQuick.Effects.MultiEffect` per CLAUDE.md).

---

### REQ-NF-004: Wayland Layer-Shell Compliance

**Ubiquitous**: Sidebar height changes shall not introduce issues with Wayland layer-shell surface management, including surface commits, margin updates, and re-mapping.

**Acceptance Criterion**: The sidebar opens and closes correctly on Hyprland and other Wayland compositors. Switching tabs updates sidebar height without broken rendering, missed surface commits, or "surface not mapped" errors in logs. Dismiss overlay follows height changes correctly.

---

### REQ-NF-005: Performance - Notification Grouping

**Ubiquitous**: The `recentHistoryGrouped(int maxGroups)` invokable shall complete in under 10 milliseconds for typical history sizes (up to 100 items), executed on the QML thread without blocking.

**Acceptance Criterion**: Profiling shows the invokable takes < 10ms on an average system with 100 persisted notifications. The implementation uses efficient list iteration and string comparison (not regex or other expensive operations).

---

### REQ-NF-006: Calendar Rendering Performance

**Ubiquitous**: Calendar month switching shall not cause visual frame drops or lag, completing in under 16 milliseconds per frame.

**Acceptance Criterion**: Clicking prev/next buttons on the calendar responds immediately with no visible delay. Frame rate remains 60 FPS during month transitions. No Qt warnings about slow property binding evaluation.

---

## Constraints

### REQ-C-001: Existing Sidebar Architecture

**Ubiquitous**: The implementation shall reuse the existing RightSidebar, SidebarContent, SidebarTabBar, and SidebarManager components and shall not restructure or rename them.

**Acceptance Criterion**: No breaking changes to existing sidebar layer-shell surface management, tab switching animations, or parent-child relationships. The Overview tab is a new SidebarOverview.qml component loaded by SidebarContent.

---

### REQ-C-002: Existing Tab Layout

**Ubiquitous**: The sidebar shall continue to display exactly six tabs (Overview, Calendar, Notifications, System, QuickSettings, Media) in the existing order.

**Acceptance Criterion**: The tab bar displays all six buttons; Overview is tab index 0. No tabs are removed or reordered.

---

### REQ-C-003: Palette Token Set

**Ubiquitous**: Only the following HoloniightPalette color tokens shall be used in this feature: `accentCyan`, `accentViolet`, `onSurface`, `textSubtle`, `onSurfaceVariant`, `surface`, `surfaceVariant`.

**Acceptance Criterion**: Code review confirms no other palette tokens are used. All rendering uses tokens from this approved list.

---

### REQ-C-004: No Breaking Changes to NotificationHistoryItem

**Ubiquitous**: The `NotificationHistoryItem` struct shall not be modified; only new invokables on NotificationService that use it shall be added.

**Acceptance Criterion**: `NotificationTypes.h` `NotificationHistoryItem` definition remains unchanged. A new `Q_INVOKABLE` method on `NotificationService` reads the existing fields and returns a `QVariantList`.

---

### REQ-C-005: Session Commands via loginctl

**Ubiquitous**: Session control commands shall use the `loginctl` CLI tool only; no direct D-Bus or systemd API calls are permitted.

**Acceptance Criterion**: C++ code executes commands via `QProcess::startDetached("loginctl", QStringList{...})`. No D-Bus org.freedesktop.login1.Manager introspection or method calls are made. Commands are: `terminate-session self`, `suspend`, `reboot`, `poweroff`.

---

### REQ-C-006: No Configuration UI

**Ubiquitous**: The `week_start_day` calendar configuration shall be set only via the holonight.toml file; no UI controls or settings panel is provided in this feature.

**Acceptance Criterion**: Users must manually edit the configuration file to change `week_start_day`. No settings button, dropdown, or dialog is added to the Overview tab.

---

### REQ-C-007: Minimum QML Loader Pausing

**Ubiquitous**: The SidebarContent Loader pausing behavior (already implemented) shall remain unchanged: when `active` becomes false, the tab component is destroyed, timers stopped, and signal connections severed.

**Acceptance Criterion**: Switching away from the Overview tab pauses its Loader, stopping any timers (e.g., relative time updates). Switching back creates a fresh instance. No state is persisted between tab switches.

---

### REQ-C-008: Desktop Entry Standard

**Ubiquitous**: Application icons in the Notifications section shall be resolved via the standard XDG icon theme or desktop-entry hints; custom icon mapping is not in scope.

**Acceptance Criterion**: App icons are fetched from `NotificationHistoryItem.app_icon` (a freedesktop hint string) and rendered via QML's standard icon provider. No custom icon mapping table is added.

---

## Verification Checklist

- [ ] Calendar displays current month on sidebar open; prev/next buttons navigate correctly.
- [ ] Today's date shows cyan + bold + glow only in the current month.
- [ ] Weekend days render in `accentViolet`; adjacent-month days in `textSubtle`.
- [ ] `week_start_day` config changes the calendar grid without restart.
- [ ] Notifications section shows up to 3 groups; empty state shows "No new notifications".
- [ ] "View all" button switches to Notifications tab (index 2).
- [ ] Session buttons execute loginctl commands without confirmation dialog.
- [ ] Sidebar height adapts to content; minimum height = 336 pixels (all 6 tab buttons visible).
- [ ] Height changes apply instantly; width animates per tab switch.
- [ ] All colors use `HoloniightPalette` tokens; no hardcoded hex values.
- [ ] C++ compiles with C++23; `task format-check` and `task tidy` pass.
- [ ] CMakeLists.txt includes `SidebarOverview.qml` in `HOLONIGHT_QML_FILES`.
- [ ] Sidebar renders correctly on Hyprland and other Wayland compositors.
- [ ] Notification grouping completes < 10 ms; calendar month switch is frame-rate smooth.

---

## Appendix A: Color Token Reference

| Token | Usage | Hex Ref (Informational) |
|-------|-------|----------------------|
| `accentCyan` | Today highlight, unread badges | #00BCD4 (approx.) |
| `accentViolet` | Weekend days, today glow frame | #9C27B0 (approx.) |
| `onSurface` | Default day text, app names, summaries | #FFFBFE (approx.) |
| `textSubtle` | Adjacent-month days | #7F7F7F (approx.) |
| `onSurfaceVariant` | Time labels, empty state | #CAC4D0 (approx.) |
| `surface` | Panel background | #1C1B1F (approx.) |
| `surfaceVariant` | Tab bar overlay | #49454E (approx.) |

**Note**: Actual hex values are defined in the HoloNight theme system and may vary. Always use `HoloniightPalette.<token>` in QML.

---

## Appendix B: File Structure

```
holonight-shell/
├── src/
│   ├── qml/
│   │   └── RightSidebar/
│   │       ├── SidebarOverview.qml           ← Main feature file (placeholder → implementation)
│   │       ├── RightSidebar.qml             ← Height propagation integration
│   │       ├── SidebarContent.qml           ← Height exposure
│   │       └── SidebarTabBar.qml            ← Existing (no changes)
│   ├── services/
│   │   ├── notifications/
│   │   │   └── NotificationService.h        ← Add recentHistoryGrouped() invokable
│   │   └── CalendarService.h/.cpp           ← New QML_SINGLETON wrapping CalendarConfig
│   ├── surfaces/
│   │   └── SidebarManager.cpp               ← Update height calculation (line ~134)
│   └── core/
│       └── ConfigService.h                  ← Add CalendarConfig struct
├── CMakeLists.txt                            ← Add SidebarOverview.qml to HOLONIGHT_QML_FILES
├── docs/
│   └── sdd/
│       └── sidebar-overview-tab/
│           └── SPEC.md                       ← This file
└── holonight.toml                            ← Add [calendar] section documentation
```

---

**End of Specification**
