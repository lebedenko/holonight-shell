# SDD Tasks — sidebar-overview-tab

## Milestone 1: C++ Foundation

- [x] T-001: Create CalendarService singleton with weekStartDay property
  - REQs: REQ-F-008, REQ-F-009
  - Check: `CalendarService` QML singleton exports `weekStartDay` as a Q_PROPERTY returning QString ("Mon" or "Sun").

- [x] T-002: Add CalendarConfig struct to ConfigService
  - REQs: REQ-F-008
  - Check: `ConfigService.h` declares `WeekStartDay` enum and `CalendarConfig` struct with `weekStartDay` field and equality operator.

- [x] T-003: Implement calendar config parsing in ConfigService
  - REQs: REQ-F-029
  - Check: `ConfigService::parseFile()` parses `[calendar]` TOML section, stores in `calendar_config_`, and emits `calendarConfigChanged()` signal on load.

- [x] T-004: Wire CalendarService to ConfigService
  - REQs: REQ-F-008, REQ-F-009
  - Check: `CalendarService::weekStartDay` Q_PROPERTY reads from `ConfigService::calendarConfig()` and updates when config changes.

- [x] T-005: Create SessionService singleton with four logout/sleep/reboot/shutdown methods
  - REQs: REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019, REQ-F-020, REQ-F-021
  - Check: `SessionService` QML singleton exports four Q_INVOKABLE methods (logout, sleep, reboot, shutdown) that call the correct `loginctl`/`systemctl` commands via `QProcess::startDetached`.

- [x] T-006: Fix SessionService logout to use two-argument format
  - REQs: REQ-F-017
  - Check: `SessionService::logout()` calls `QProcess::startDetached("loginctl", {"terminate-session", "self"})` (list form, not single string).

- [x] T-007: Add NotificationService::recentHistoryGrouped Q_INVOKABLE
  - REQs: REQ-F-010
  - Check: `NotificationService::recentHistoryGrouped(int maxGroups)` Q_INVOKABLE returns `QVariantList` of grouped notification QVariantMaps (one per app, ordered by latest timestamp).

- [x] T-008: Register CalendarService and SessionService in ShellApplication
  - REQs: REQ-F-008, REQ-F-021
  - Check: Both services are registered with QML engine (via `QML_ELEMENT` + `QML_SINGLETON` or explicit `qmlRegisterSingletonType`) and available in QML as `CalendarService` and `SessionService`.

## Milestone 2: Core C++ Infrastructure

- [x] T-009: Add sidebar height storage to SidebarManager
  - REQs: REQ-F-024, REQ-F-026
  - Check: `SidebarManager` declares private `QHash<QString, int> stored_heights_` and public `onContentHeightChanged(QString monitorName, int height)` Q_INVOKABLE that clamps height ≥ 336 and stores per-monitor.

- [x] T-010: Define sidebar height constants
  - REQs: REQ-F-026
  - Check: `ShellConstants.h` (or appropriate header) declares `kSidebarMinHeight = 336` and `kSidebarDefaultHeight = 600`.

- [x] T-011: Update SidebarManager::openOnMonitor height calculation
  - REQs: REQ-F-022, REQ-F-024, REQ-F-026
  - Check: `SidebarManager::openOnMonitor()` uses stored height for the given monitor, falls back to `kSidebarDefaultHeight` on first open, and enforces `kSidebarMinHeight` floor.

## Milestone 3: QML Wiring

- [x] T-012: Add preferredHeight to SidebarContent.qml
  - REQs: REQ-F-022, REQ-F-023
  - Check: `SidebarContent.qml` declares `property int preferredHeight: loader.item?.preferredHeight ?? 0` and updates when loader item changes.

- [x] T-013: Add height-change signaling from RightSidebar.qml to SidebarManager
  - REQs: REQ-F-025
  - Check: `RightSidebar.qml` connects to `contentArea.preferredHeight` changes and calls `SidebarManager.onContentHeightChanged(barMonitorName, contentArea.preferredHeight)` when height > 0.

- [x] T-014: Add switchTab signal to SidebarContent.qml
  - REQs: REQ-F-014
  - Check: `SidebarContent.qml` declares `signal switchTab(int index)` and emits it when a tab component requests a tab switch.

- [x] T-015: Add preferredHeight property to all five placeholder tab QML files
  - REQs: REQ-F-022
  - Check: Each of the five non-Overview sidebar tab QML files declares `property int preferredHeight` with a sensible value (480, 600, 480, 400, 340 per design).

## Milestone 4: Calendar Section (SidebarOverview.qml)

- [x] T-016: Implement calendar date model builder JavaScript function
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007
  - Check: `buildDayModel(year, month, weekStartDay)` returns array of {day, isCurrentMonth, isToday, isWeekend, ...} objects with correct styling flags per all calendar requirements.

- [x] T-017: Implement calendar month/year navigation and state
  - REQs: REQ-F-001, REQ-F-002, REQ-F-007
  - Check: Calendar state tracks current displayMonth/displayYear, prev/next buttons update month correctly, and weekday column order respects `CalendarService.weekStartDay`.

- [x] T-018: Implement calendar grid with styled day cells
  - REQs: REQ-F-001, REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006
  - Check: 7-column GridLayout renders date cells with correct text colors (onSurface/textSubtle/accentViolet) and today styling (cyan text + bold + violet glow only when viewing current month).

- [x] T-019: Add MultiEffect glow for today cell
  - REQs: REQ-F-005
  - Check: Today cell has violet inner glow via `MultiEffect` declared **before** the date Text element; glow only visible when viewing current month.

## Milestone 5: Notifications Section (SidebarOverview.qml)

- [x] T-020: Implement notifications grouping and display
  - REQs: REQ-F-010, REQ-F-011, REQ-F-015
  - Check: Notifications section calls `NotificationService.recentHistoryGrouped(3)`, displays up to 3 app groups with icon, name, latest summary, unread badge, and relative time label.

- [x] T-021: Implement relative time formatter and timer
  - REQs: REQ-F-011, REQ-F-015
  - Check: `formatRelativeTime(timestampMs)` function returns human-readable time labels; Timer (60s interval) updates `timeTickCounter` to refresh all time labels; signal-connected to `NotificationService.unreadCountChanged()`.

- [x] T-022: Implement notifications overflow and "View all" button
  - REQs: REQ-F-013, REQ-F-014
  - Check: When total notifications > 3, display "+N notifications" label and "View all" button; button click emits `switchTab(2)` to navigate to Notifications tab.

- [x] T-023: Implement empty state for notifications
  - REQs: REQ-F-012
  - Check: When no notifications, display "No new notifications" label in `onSurfaceVariant` color.

- [x] T-024: Handle app icon source resolution
  - REQs: REQ-F-011
  - Check: Icon source logic checks if `appIcon` starts with "/" (absolute path) or uses `image://icon/` provider; handles missing/broken icons gracefully.

## Milestone 6: Session Bar (SidebarOverview.qml)

- [x] T-025: Implement session action bar with four buttons
  - REQs: REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019, REQ-F-020
  - Check: Fixed Item at bottom (height 56) contains RowLayout with 4 ToolButtons for logout/sleep/reboot/shutdown; buttons call `SessionService` methods.

- [x] T-026: Style session buttons with correct icons and colors
  - REQs: REQ-F-016
  - Check: Each session button has a distinct symbolic icon, correct semantic colors from HoloniightPalette, and proper hover/pressed states.

## Milestone 7: SidebarOverview.qml Layout & Height

- [x] T-027: Implement SidebarOverview main layout structure
  - REQs: REQ-F-022, REQ-F-032, REQ-F-033
  - Check: `SidebarOverview.qml` is a ColumnLayout; imports QtQuick, Holonight, HolonightShell; scrollable content area above fixed session bar; declares `preferredHeight` property.

- [x] T-028: Implement preferredHeight calculation
  - REQs: REQ-F-022, REQ-F-027
  - Check: `preferredHeight = contentColumn.implicitHeight + sessionBar.height + 8` (padding); height changes are instant (no animation).

- [x] T-029: Add switchTab signal and emit on "View all" click
  - REQs: REQ-F-014
  - Check: `SidebarOverview.qml` declares `signal switchTab(int index)` at root; emitted when notifications "View all" button is clicked; propagated up via `SidebarContent.qml`.

## Milestone 8: Build & Integration

- [x] T-030: Add SidebarOverview.qml to CMakeLists.txt
  - REQs: REQ-F-032, REQ-F-033
  - Check: `src/qml/RightSidebar/SidebarOverview.qml` is listed in `HOLONIGHT_QML_FILES` in `CMakeLists.txt`; CMake configure succeeds.

- [x] T-031: Add new C++ sources to CMakeLists.txt
  - REQs: REQ-F-008, REQ-F-021
  - Check: All new `.cpp/.h` files (CalendarService, SessionService, updated ConfigService, updated NotificationService, updated SidebarManager) are listed in CMakeLists.txt source groups.

- [x] T-032: Verify build succeeds with no new warnings
  - REQs: REQ-F-032, REQ-F-033
  - Check: `task configure` and `task build` complete without errors or new clang-format/clang-tidy warnings introduced by feature code.

- [x] T-033: Verify all five placeholder tabs have preferredHeight
  - REQs: REQ-F-022
  - Check: `task configure` and `task build` complete; all five non-Overview tabs load without "property preferredHeight not found" runtime errors.

## Milestone 9: Final Integration & Validation

- [x] T-034: Test calendar display and month navigation
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007
  - Check: Open sidebar overview tab; calendar shows current month; prev/next buttons work; today has cyan text + violet glow; weekend/adjacent days styled correctly; switching `week_start_day` config changes column order.

- [x] T-035: Test notifications section and "View all" navigation
  - REQs: REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-015
  - Check: Notifications display grouped by app; empty state shows when no notifications; "View all" button switches to Notifications tab (index 2); new notifications refresh the display.

- [x] T-036: Test session action buttons
  - REQs: REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019, REQ-F-020
  - Check: Each of the four session buttons is clickable and invokes the correct system command (logout/sleep/reboot/shutdown) without blocking the UI.

- [x] T-037: Test dynamic sidebar height
  - REQs: REQ-F-022, REQ-F-023, REQ-F-024, REQ-F-025, REQ-F-026, REQ-F-027, REQ-F-028
  - Check: Sidebar height changes instantly when tab content changes; height clamped to ≥ 336px; stored per monitor and restored on re-open; width animations unaffected.
