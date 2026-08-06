# SDD Tasks — active-window-per-monitor

**SDD Session:** active-window-per-monitor  
**Feature:** Per-monitor active window tracking  
**Owner:** Andrii L  
**Updated:** 2026-05-23

---

## Overview

Tasks implement the per-monitor active window feature (SPEC.md, DESIGN.md). Each task builds on prior work and can be independently verified before starting the next.

**Prerequisites:**
- Read SPEC.md and DESIGN.md completely
- Review current `ActiveWindowService.h/cpp` and `HyprlandIpc.h/cpp`
- Review `src/qml/Topbar/ActiveWindowSection.qml`

---

## Phase 1: Data Structure & Parsing

### T-001: Add new structs to `HyprlandIpc.h` ✓
- **REQs:** REQ-F-001 (data structure), REQ-F-013 (event parsing)
- **Check:** All four new structs compile; `isEmpty()` method on `HyprlandActiveWindow` returns correct boolean
- **Details:**
  - Add `struct HyprlandFocusedMonitor { QString monitor_name; QString workspace_name; }`
  - Add `struct HyprlandMonitorInfo { QString name; int active_workspace_id; }`
  - Add `struct HyprlandClientInfo { QString app_class; QString title; int workspace_id; int focus_history_id; }`
  - Add `struct HyprlandOpenWindow { QString address; QString workspace_name; QString app_class; QString title; }`
  - Update `HyprlandActiveWindow` struct: add `QString category` field and `[[nodiscard]] bool isEmpty() const` method
  - Build: `task build` passes

### T-002: Update `parseHyprlandFocusedMonitorEvent` signature and implementation in `HyprlandIpc.h/cpp` ✓
- **REQs:** REQ-F-004 (monitor focus event), REQ-F-013 (monitor name extraction)
- **Check:** New signature compiles; parsing of `focusedmon>>HDMI-1,5` extracts `monitor_name="HDMI-1"` and `workspace_name="5"`
- **Details:**
  - Change return type from `std::optional<int>` to `std::optional<HyprlandFocusedMonitor>` in header and implementation
  - Parse `focusedmon>>` prefix, extract monitor name (before first comma) and workspace name (after first comma)
  - Trim whitespace from both components
  - Implement in `HyprlandIpc.cpp`; add unit test if test framework in place
  - Build: `task build` passes

### T-003: Implement `parseHyprlandOpenWindowEvent` in `HyprlandIpc.h/cpp` ✓
- **REQs:** REQ-F-005 (open window event), REQ-F-013 (event parsing)
- **Check:** Parsing `openwindow>>0x1234,5,firefox,Google%20Chrome` extracts all four fields correctly; title with commas is handled
- **Details:**
  - Add function declaration in header: `[[nodiscard]] std::optional<HyprlandOpenWindow> parseHyprlandOpenWindowEvent(const QByteArray& line);`
  - Parse format: `openwindow>>ADDRESS,WORKSPACENAME,CLASS,TITLE` (title is everything after third comma)
  - Find first three commas, split payload accordingly
  - Return struct with address, workspace_name, app_class, title
  - Build: `task build` passes

### T-004: Implement `parseHyprlandMonitorsJson` in `HyprlandIpc.h/cpp` ✓
- **REQs:** REQ-F-002 (startup init), REQ-F-014 (JSON query parsing)
- **Check:** Parsing valid `j/monitors` response returns correct `QHash<QString, int>` with monitor names and active workspace IDs; invalid JSON returns `std::nullopt`
- **Details:**
  - Add function declaration in header: `[[nodiscard]] std::optional<QHash<QString, int>> parseHyprlandMonitorsJson(const QByteArray& response);`
  - Parse JSON array; for each object extract `name` (string) and `activeWorkspace.id` (int)
  - Ignore entries with missing `name` or zero `activeWorkspace.id`
  - Use `QJsonDocument::fromJson()` with error handling (log parse failures)
  - Build: `task build` passes

### T-005: Implement `parseHyprlandClientsJson` in `HyprlandIpc.h/cpp` ✓
- **REQs:** REQ-F-002 (startup init), REQ-F-014 (JSON query parsing)
- **Check:** Parsing valid `j/clients` response returns list of clients; each client has app_class, title, workspace.id, focusHistoryID; invalid JSON returns `std::nullopt`
- **Details:**
  - Add function declaration in header: `[[nodiscard]] std::optional<QList<HyprlandClientInfo>> parseHyprlandClientsJson(const QByteArray& response);`
  - Parse JSON array; for each object extract `class`, `title`, `workspace.id` (int), and `focusHistoryID` (int)
  - Ignore entries with empty `class` or `title`
  - Handle missing `focusHistoryID` gracefully (treat as invalid entry or default to max int)
  - Use `QJsonDocument::fromJson()` with error handling
  - Build: `task build` passes

### T-006: Update call sites of `parseHyprlandFocusedMonitorEvent` in `ExtWorkspaceManager.cpp` ✓
- **REQs:** REQ-C-004 (no circular dependencies), REQ-F-013 (monitor name extraction)
- **Check:** `ExtWorkspaceManager.cpp` compiles; workspace ID extraction still works correctly
- **Details:**
  - Identify all call sites via grep or code review
  - Update each to use `parsed->workspace_name.toInt()` for workspace ID extraction
  - Keep logic otherwise unchanged
  - Build: `task build` passes

---

## Phase 2: ActiveWindowService Rewrite

### T-007: Declare per-monitor state members in `ActiveWindowService.h` ✓
- **REQs:** REQ-F-001 (per-monitor storage), REQ-F-002 (startup data structures)
- **Check:** Header compiles; all new member variables are declared with correct types and initial values
- **Details:**
  - Remove old `QString title_`, `QString app_class_`, `QString category_` members
  - Remove old `Q_PROPERTY` declarations for title, appClass, category
  - Add: `QHash<QString, HyprlandActiveWindow> monitor_windows_`
  - Add: `QHash<QString, int> monitor_workspaces_`
  - Add: `QString focused_monitor_name_`
  - Add: `enum class CommandPhase { Idle, Monitors, Clients }; CommandPhase command_phase_{CommandPhase::Idle};`
  - Add: `QHash<QString, int> pending_monitor_workspaces_` (scratch state for two-phase query)
  - Add: `QList<HyprlandClientInfo> pending_clients_` (scratch state for two-phase query)
  - Build: `task build` passes (expect linker errors until Cpp updated)

### T-008: Add `Q_INVOKABLE` getters and `monitorWindowChanged` signal to `ActiveWindowService.h` ✓
- **REQs:** REQ-F-009 (QML getter methods), REQ-F-010 (QML signal emission)
- **Check:** Header compiles; three getter methods and one signal are declared
- **Details:**
  - Add signal: `Q_SIGNAL void monitorWindowChanged(const QString& monitor_name);`
  - Add `Q_INVOKABLE QString titleForMonitor(const QString& monitor_name) const;`
  - Add `Q_INVOKABLE QString appClassForMonitor(const QString& monitor_name) const;`
  - Add `Q_INVOKABLE QString categoryForMonitor(const QString& monitor_name) const;`
  - All getters return empty string if monitor_name not in hash
  - Build: `task build` passes

### T-009: Implement `Q_INVOKABLE` getter methods in `ActiveWindowService.cpp` ✓
- **REQs:** REQ-F-009 (QML getter methods)
- **Check:** Calling `titleForMonitor("HDMI-1")` on empty hash returns `""`; calling with existing monitor returns correct title
- **Details:**
  - Implement three getters (simple hash lookups)
  - All methods use `.value()` with empty-string default
  - Methods are const and marked `[[nodiscard]]`
  - Build: `task build` passes; no new warnings

### T-010: Implement helper `setMonitorWindow(monitor_name, app_class, title)` in `ActiveWindowService.cpp` ✓
- **REQs:** REQ-F-003 (active window update), REQ-F-005 (open window update)
- **Check:** Calling `setMonitorWindow("HDMI-1", "firefox", "Google")` updates hash and emits signal
- **Details:**
  - Private method that updates `monitor_windows_[monitor_name]` with new app_class and title
  - Category set to empty string (will be resolved asynchronously)
  - Only emit `monitorWindowChanged(monitor_name)` if data actually changed (compare old vs new)
  - Build: `task build` passes

### T-011: Implement `queryAllMonitorWindows()` helper in `ActiveWindowService.cpp` — Monitors phase ✓
- **REQs:** REQ-F-002 (startup initialization), REQ-F-014 (JSON parsing)
- **Check:** Calling `queryAllMonitorWindows()` at startup opens command socket and writes `j/monitors` command
- **Details:**
  - Guard against overlapping queries: if `command_phase_ != Idle`, do nothing (return early)
  - Set `command_phase_` to `CommandPhase::Monitors`
  - Create/open `command_socket_` connection to Hyprland command socket
  - Write `"j/monitors\n"` to socket
  - Do NOT read response yet — wait for `onCommandSocketReadable()` and `onCommandSocketConnected()` signals
  - Existing socket infrastructure from current code is reused
  - Build: `task build` passes

### T-012: Implement response parsing in `onCommandSocketReadable()` for Monitors phase ✓
- **REQs:** REQ-F-002 (startup initialization), REQ-F-014 (JSON parsing)
- **Check:** Response to `j/monitors` accumulates in buffer; when complete, `parseHyprlandMonitorsJson` is called and result stored in `pending_monitor_workspaces_`
- **Details:**
  - Add per-phase handling in existing `onCommandSocketReadable()` slot
  - For `CommandPhase::Monitors`: parse accumulated `command_buffer_` via `parseHyprlandMonitorsJson()`
  - If parse succeeds, store result in `pending_monitor_workspaces_`
  - Call `finishCommandSocket(true)` to proceed to Phase 2 (Clients)
  - If parse fails, log warning and call `finishCommandSocket(false)` to abort
  - Clear `command_buffer_` for next phase
  - Build: `task build` passes

### T-013: Implement response parsing for Clients phase ✓
- **REQs:** REQ-F-002 (startup initialization), REQ-F-014 (JSON parsing)
- **Check:** Response to `j/clients` accumulates in buffer; when complete, `parseHyprlandClientsJson` is called and result stored in `pending_clients_`
- **Details:**
  - For `CommandPhase::Clients`: parse accumulated `command_buffer_` via `parseHyprlandClientsJson()`
  - If parse succeeds, store result in `pending_clients_`
  - Call `finishCommandSocket(true)` to apply results and return to Idle
  - If parse fails, log warning and call `finishCommandSocket(false)` to abort
  - Build: `task build` passes

### T-014: Implement `finishCommandSocket(bool parse_buffer)` helper ✓
- **REQs:** REQ-F-002 (startup initialization)
- **Check:** After Monitors phase, command_phase becomes Clients and new socket opens; after Clients phase, phase becomes Idle and results are applied
- **Details:**
  - Destroy current `command_socket_`
  - If `command_phase_ == CommandPhase::Monitors` and `parse_buffer` true:
    - Phase transitions to `CommandPhase::Clients`
    - Open new socket and proceed to Clients query (reuse logic from `queryAllMonitorWindows`)
  - Else if `command_phase_ == CommandPhase::Clients` and `parse_buffer` true:
    - Call `applyMonitorWindowsFromPending()`
    - Phase transitions to `CommandPhase::Idle`
    - Clear scratch state (`pending_monitor_workspaces_`, `pending_clients_`)
  - Else:
    - Phase transitions to `CommandPhase::Idle`
    - Clear scratch state without applying
  - Build: `task build` passes

### T-015: Implement `applyMonitorWindowsFromPending()` helper ✓
- **REQs:** REQ-F-002 (startup initialization), REQ-F-001 (per-monitor storage)
- **Check:** After calling `applyMonitorWindowsFromPending()`, `monitor_windows_` contains one entry per monitor with correct window data; `monitorWindowChanged` signal fires once per monitor
- **Details:**
  - For each monitor in `pending_monitor_workspaces_`:
    - Get workspace ID from `pending_monitor_workspaces_[monitor]`
    - Filter `pending_clients_` to those with matching workspace_id
    - If no clients, create empty `HyprlandActiveWindow{}` entry
    - If clients exist, select the one with **lowest** `focusHistoryID` (most recent)
    - Compare new entry to old `monitor_windows_[monitor]` (by app_class and title, not category)
    - If changed, update hash and emit `monitorWindowChanged(monitor)`
    - If app_class changed, schedule category resolution
  - Update `monitor_workspaces_` from `pending_monitor_workspaces_` (atomically)
  - Clear scratch state
  - Build: `task build` passes

---

## Phase 3: Event Handling

### T-016: Update `onSocketReadable()` to handle `activewindow>>` events — per-monitor dispatch ✓
- **REQs:** REQ-F-003 (active window event)
- **Check:** When `activewindow>>firefox,Mozilla%20Firefox` arrives, focused monitor's window is updated and signal fires
- **Details:**
  - Parse line via existing `parseHyprlandActiveWindowEvent(line)`
  - Call `setMonitorWindow(focused_monitor_name_, app_class, title)`
  - If `focused_monitor_name_` is empty, skip (edge case: focusedmon not yet received)
  - Schedule category resolution via updated `scheduleResolveCategory(app_class, monitor_name)` signature
  - Existing logic structure is preserved; only the update target changes
  - Build: `task build` passes

### T-017: Update `onSocketReadable()` to handle `focusedmon>>` events ✓
- **REQs:** REQ-F-004 (monitor focus event)
- **Check:** When `focusedmon>>HDMI-1,5` arrives, `focused_monitor_name_` is set to `"HDMI-1"` and no window signal fires
- **Details:**
  - Parse line via updated `parseHyprlandFocusedMonitorEvent(line)` (returns `HyprlandFocusedMonitor`)
  - Extract `monitor_name` and store in `focused_monitor_name_`
  - Do NOT emit `monitorWindowChanged` signal (only monitor switch, no window change)
  - Build: `task build` passes

### T-018: Implement `onSocketReadable()` handling for `openwindow>>` events ✓
- **REQs:** REQ-F-005 (open window event)
- **Check:** When `openwindow>>0x1234,5,firefox,Title` arrives on non-focused monitor's active workspace, that monitor's window updates
- **Details:**
  - Parse line via `parseHyprlandOpenWindowEvent(line)`
  - Extract `workspace_name` from payload
  - Convert workspace_name to int (handle non-numeric names gracefully with log warning)
  - Reverse-lookup which monitor has this workspace ID via `monitor_workspaces_`
  - If found, call `setMonitorWindow(affected_monitor, app_class, title)`
  - If not found or workspace not active on any monitor, skip
  - Schedule category resolution for the app_class
  - Build: `task build` passes

### T-019: Implement detection of workspace-refresh events ✓
- **REQs:** REQ-F-006 (close window event), REQ-F-007 (move window event)
- **Check:** Lines starting with `closewindow>>` or `movewindow>>` or `workspace>>` trigger `queryAllMonitorWindows()`
- **Details:**
  - Add helper: `static bool isHyprlandWorkspaceRefreshEvent(const QByteArray& line)` (returns true for close/move/workspace events)
  - In `onSocketReadable()`, after parsing specific event types, check `if (isHyprlandWorkspaceRefreshEvent(line)) queryAllMonitorWindows();`
  - This re-queries monitors and clients, applying new state via `applyMonitorWindowsFromPending()`
  - Guard against overlapping queries is already in `queryAllMonitorWindows()` via `command_phase_` check
  - Build: `task build` passes

### T-020: Update socket reconnection handler ✓
- **REQs:** REQ-NF-001 (non-blocking), REQ-F-002 (startup init)
- **Check:** When event socket reconnects after a disconnect, `queryAllMonitorWindows()` is called to refresh all monitor state
- **Details:**
  - In `onEventSocketConnected()`, after existing logic, call `queryAllMonitorWindows()` to refresh state
  - This ensures monitor state is always consistent after reconnection
  - Build: `task build` passes

---

## Phase 4: Category Resolution

### T-021: Update `scheduleResolveCategory` signature and implementation for per-monitor tracking ✓
- **REQs:** REQ-F-012 (category resolution for all windows)
- **Check:** Calling `scheduleResolveCategory("firefox", "HDMI-1")` schedules async scan; watcher callback applies resolved category to all monitors showing that app_class
- **Details:**
  - Update signature: `void scheduleResolveCategory(const QString& app_class, const QString& monitor_name);`
  - Existing logic (QtConcurrent::run, watcher setup) is preserved
  - In watcher callback, iterate `monitor_windows_` hash:
    - For each monitor where `app_class` matches, apply resolved category
    - Emit `monitorWindowChanged(monitor)` for each updated entry
  - Apply stale-result guard via per-entry app_class check (not global)
  - The `monitor_name` parameter is accepted but not used in callback (callback applies to all matching monitors)
  - Build: `task build` passes

---

## Phase 5: QML Integration

### T-022: Update `ActiveWindowSection.qml` to import `QtQuick.Window` and add per-monitor local properties ✓
- **REQs:** REQ-F-011 (QML monitor-aware section)
- **Check:** QML file compiles via `task qml-lint`; no "unqualified access" warnings on local property references
- **Details:**
  - Add `import QtQuick.Window` to imports
  - Add three local properties to root BarSection:
    - `property string localTitle: ""`
    - `property string localCategory: ""`
    - `property string localAppClass: ""`
  - Build: `task qml-lint` passes (no new warnings)

### T-023: Add `Connections` block and `Component.onCompleted` to `ActiveWindowSection.qml` ✓
- **REQs:** REQ-F-011 (QML monitor-aware section), REQ-F-010 (signal emission)
- **Check:** `Connections` block listens to `monitorWindowChanged` signal; `Component.onCompleted` initializes local properties
- **Details:**
  - Add `Connections` block:
    ```qml
    Connections {
      target: ActiveWindowService
      function onMonitorWindowChanged(monitorName) {
        if (monitorName === Screen.name) {
          root.localTitle    = ActiveWindowService.titleForMonitor(Screen.name)
          root.localCategory = ActiveWindowService.categoryForMonitor(Screen.name)
          root.localAppClass = ActiveWindowService.appClassForMonitor(Screen.name)
        }
      }
    }
    ```
  - Add `Component.onCompleted` block with same three assignments (initial load)
  - Build: `task qml-lint` passes

### T-024: Replace `ActiveWindowService` property references with local properties in `ActiveWindowSection.qml` ✓
- **REQs:** REQ-F-011 (QML monitor-aware section)
- **Check:** All bindings use local properties instead of `ActiveWindowService` properties; visual appearance unchanged
- **Details:**
  - In Column visibility binding, replace `ActiveWindowService.title !== ""` with `root.localTitle !== ""`
  - Replace `AppWindowIcon { category: ActiveWindowService.category }` with `category: root.localCategory`
  - Replace `Controls.Label { text: ActiveWindowService.title }` with `text: root.localTitle`
  - In BarTooltipArea, replace title/description references:
    - `root.localTitle.length > 0 ? root.localTitle : "Active window"`
    - `root.localAppClass.length > 0 ? "Focused app: " + root.localAppClass + "." : ...`
  - Build: `task qml-lint` passes; visual testing passes (next task)

---

## Phase 6: Verification & Cleanup

### T-025: Verify `focusHistoryID` ordering empirically ✓
- **REQs:** REQ-F-002 (startup init), REQ-F-014 (JSON parsing)
- **Check:** Test confirms that lower `focusHistoryID` value = most recently focused window on a workspace
- **Details:**
  - Open two terminal windows on the same workspace (Workspace 1)
  - Focus the second terminal
  - Run `hyprctl j/clients` and extract both windows' `focusHistoryID` values
  - Verify the focused window has the **lower** value
  - If opposite is true, update `applyMonitorWindowsFromPending()` to use `max()` instead of `min()`
  - Add code comment referencing this verification: `// focusHistoryID: 0 = most recent (verified 2026-05-23)`
  - Build: `task build` passes

### T-026: Test per-monitor window display on multi-monitor setup ✓
- **REQs:** REQ-F-001 (per-monitor storage), REQ-F-011 (QML integration), all functional requirements
- **Check:** 2-monitor test: each bar shows different windows; focus/open/close/move events update correct bar only
- **Details:**
  - Launch holonight-shell on 2-monitor Hyprland setup
  - Monitor 1 workspace 1: open Firefox, focus it
  - Monitor 2 workspace 2: open Kitty, focus it
  - Verify Bar 1 shows "Firefox", Bar 2 shows "Kitty"
  - Use `ydotool` or mouse to switch focus to Monitor 2 Kitty → verify Bar 1 unchanged, Bar 2 unchanged
  - Switch focus back to Monitor 1 Firefox → verify both bars unchanged
  - Open a new window (Terminal) on Monitor 1 workspace 1 and focus it → verify Bar 1 updates to "Terminal", Bar 2 unchanged
  - Close Terminal → verify Bar 1 reverts to "Firefox" (next-most-recent on workspace 1), Bar 2 unchanged
  - Move Firefox to workspace 2 → verify Bar 1 becomes blank, Bar 2 updates to show Firefox
  - Test passes: all bars update correctly and independently

### T-027: Verify category resolution caching across multiple monitors ✓
- **REQs:** REQ-F-012 (category resolution)
- **Check:** Firefox opened on Monitor 1 resolves category to "browser"; Firefox opened on Monitor 2 uses cached category (no re-scan)
- **Details:**
  - Launch holonight-shell on 2-monitor setup
  - Open Firefox on Monitor 1 workspace 1; wait for category to resolve
  - Verify Bar 1 icon shows browser category
  - Open Firefox on Monitor 2 workspace 2; observe category appears instantly (within 50ms)
  - Verify Bar 2 icon matches Bar 1 icon (cached category)
  - Test passes: second Firefox uses cached category without re-scanning desktop files

### T-028: Run `task build` and verify no compile errors ✓
- **REQs:** All technical requirements
- **Check:** `task build` exits with 0; no linker errors or warnings
- **Details:**
  - Execute `task build` from project root
  - Verify binary compiles successfully
  - Test passes: no compilation errors

### T-029: Run `task qml-lint` and verify no QML warnings ✓
- **REQs:** All QML requirements
- **Check:** `task qml-lint` exits with 0; no lint warnings in `ActiveWindowSection.qml` or related files
- **Details:**
  - Execute `task qml-lint` from project root
  - Verify no "unqualified access", "unused imports", or other warnings
  - Test passes: all QML files lint clean

### T-030: Run `task test` (if GTest unit tests added) ✓
- **REQs:** Test coverage (optional for MVP)
- **Check:** All unit tests pass; JSON parser tests and event parser tests cover new code
- **Details:**
  - If unit tests are present: run `task test` and verify all pass
  - If no GTest framework: skip this task
  - Test passes: all tests green or no tests exist (acceptable for MVP)

### T-031: Code review: verify no clang-tidy warnings in modified files ✓
- **REQs:** Code style, REQ-NF-001 (non-blocking)
- **Check:** `task tidy` shows no new warnings in `ActiveWindowService.h/cpp`, `HyprlandIpc.h/cpp`, `ActiveWindowSection.qml`
- **Details:**
  - Run `task tidy` from project root
  - Review output for warnings in modified files
  - Address any new warnings (variable naming, cognitive complexity, etc.)
  - Verify function complexity stays within limit (25 for main handlers)
  - Test passes: no new clang-tidy warnings

### T-032: Final manual acceptance scenario test ✓
- **REQs:** All acceptance scenarios (Scenario 1–5 from SPEC)
- **Check:** Scenarios 1, 2, 3, 4, 5 all pass (Monitor hotplug and empty workspaces optional)
- **Details:**
  - **Scenario 1:** 2-monitor startup with Firefox on Mon1 WS1, Chromium on Mon2 WS2 → both bars show correct windows
  - **Scenario 2:** Focus Terminal on Mon2 WS2 → Mon2 bar updates to Terminal, Mon1 unchanged
  - **Scenario 3:** Move Firefox from Mon1 WS1 to Mon2 WS2 → Mon1 bar becomes blank (if only window), Mon2 bar shows Firefox
  - **Scenario 4:** Close only window on a workspace → bar hides; open new window → bar shows it
  - **Scenario 5:** Rapid focus changes across monitors → bars update smoothly, no lag or missed states
  - Test passes: all scenarios work as specified

---

## Sign-Off

- [x] All 32 tasks completed and verified
- [x] `task build` passes
- [x] `task qml-lint` passes
- [x] `task test` passes (if applicable)
- [x] Manual acceptance tests pass (Scenarios 1–5)
- [x] Code review approved (no clang-tidy warnings)
- [x] CLAUDE.md updated with per-monitor architecture notes
