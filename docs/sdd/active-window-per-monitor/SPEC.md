# SPEC: Per-Monitor Active Window Widget

**Feature**: Active Window Title and Icon shown per-monitor, not globally.

**Owner**: Andrii L  
**Created**: 2026-05-23  
**Status**: Active  

---

## Overview

`holonight-shell` is a C++23/Qt6 Wayland layer shell displaying a single bar per monitor via `LayerShellManager`. Currently, each monitor's bar shows the **globally focused window** (same title, icon, category on all monitors). This spec mandates that each bar display the **active window on its own workspace/monitor**, allowing users to see at a glance what is focused on each screen.

### Scope

- Per-monitor window tracking for all monitors reported by Hyprland's `j/monitors` endpoint
- Reactive updates to title, icon, and category on Hyprland IPC events
- Startup initialization of active windows for non-focused monitors
- Window lifecycle management (open, close, move, focus-switch events)
- Category resolution (app icon classification) applied uniformly across all windows
- QML integration via monitor-aware signal/slot pattern using `Screen.name`

### Non-Goals

- Custom (non-numeric) workspace name support
- Cross-compositor support (Hyprland-specific)
- Window-switcher history or switcher UI
- Per-workspace per-monitor window stacking or Z-order

---

## Functional Requirements

### REQ-F-001: Per-Monitor Window Storage

**Ubiquitous**: The `ActiveWindowService` C++ singleton shall maintain a collection of active windows keyed by monitor/screen name.

**Details**:
- Data structure: `QHash<QString, HyprlandActiveWindow> monitor_windows_` where `HyprlandActiveWindow` contains `title`, `appClass`, and `category` (or struct member equivalents)
- Keyed by monitor/screen name as reported by Hyprland's `j/monitors` (e.g., `"HDMI-1"`, `"DP-2"`)
- Each entry represents the currently active window on that monitor's active workspace

**Acceptance Criterion**: 
- Query `j/monitors` at startup and verify that each monitor is enumerated
- Verify that `monitor_windows_` is populated with one entry per monitor (size = active monitors count)
- Confirm that distinct monitors hold distinct window entries (not shared by reference)

---

### REQ-F-002: Startup Initialization

**Event-driven**: When `ActiveWindowService` initializes (after IPC socket connect), the service shall query Hyprland and populate active windows for all monitors.

**Details**:
- Call `queryActiveWindowsPerMonitor()` synchronously after event socket connect (same pattern as current `queryActiveWindow()`)
- Use two queries:
  1. `j/monitors` → extract `active_workspace` ID for each monitor name
  2. `j/clients` → for each monitor's active workspace, find windows and select the one with lowest `focusHistoryID` (most recently focused)
- Fall back gracefully if a workspace has no windows (blank entry in `monitor_windows_`)
- Use `waitForConnected(1000)` + `waitForReadyRead(2000)` (blocking acceptable at startup only)

**Acceptance Criterion**:
- Launch bar on a 2-monitor setup with windows on each workspace
- Verify that `monitor_windows_` contains entries for both monitors
- Confirm that a monitor with no windows in its workspace has an empty/blank entry
- Verify that the most recently focused window (lowest `focusHistoryID`) is selected, not the oldest or any arbitrary window

---

### REQ-F-003: Active Window Event - Globally Focused Window

**Event-driven**: When Hyprland sends `activewindow>>CLASS,TITLE` event, the service shall update the currently focused monitor's window and emit a signal to notify all bars.

**Details**:
- Trigger on receipt of `activewindow>>` IPC event (already implemented)
- Extract `appClass` and `title` from the payload
- Identify the **currently focused monitor** via `focused_monitor_name_` (maintained from `focusedmon>>` events)
- Update `monitor_windows_[focused_monitor_name_]` only
- All other monitor windows remain unchanged
- Emit signal `monitorWindowChanged(const QString& monitorName)` with the focused monitor name
- Schedule async category resolution via existing `scheduleResolveCategory(app_class)` mechanism

**Acceptance Criterion**:
- Set focus to a window on monitor 1; verify `monitor_windows_["HDMI-1"]` is updated and signal fires with `"HDMI-1"`
- Without switching focus, verify `monitor_windows_["DP-2"]` remains unchanged
- Confirm category resolution is scheduled (no blocking resolution in event handler)

---

### REQ-F-004: Monitor Focus Switch Event

**Event-driven**: When Hyprland sends `focusedmon>>MONNAME,WSNAME` event, the service shall update the focused monitor identifier and emit a signal.

**Details**:
- Parse `focusedmon>>MONNAME,WSNAME` event (MONNAME = monitor name like `"HDMI-1"`, WSNAME = workspace ID/name)
- Extract and store `MONNAME` as the new `focused_monitor_name_`
- Emit signal `focusedMonitorChanged(const QString& monitorName)` (optional, for UI feedback)
- Do **not** update window title/icon for the previously focused monitor (it keeps its last state)
- The next `activewindow>>` event will update the new focused monitor's window

**Acceptance Criterion**:
- Focus monitor 1 workspace with window A active
- Switch focus to monitor 2; verify `focused_monitor_name_` = `"DP-2"`
- Verify monitor 1's bar still shows window A (no change to its widget)
- Move focus back to monitor 1; verify focused monitor returns to `"HDMI-1"`

---

### REQ-F-005: Open Window Event - Non-Focused Monitor

**Event-driven**: When Hyprland sends `openwindow>>ADDRESS,WORKSPACENAME,CLASS,TITLE` event, the service shall identify the target monitor's active workspace and update that monitor's active window if applicable.

**Details**:
- Parse `openwindow>>` event payload (existing facility, may need adaptation)
- Extract `WORKSPACENAME` (workspace ID or name)
- Query `j/monitors` to find which monitor has this workspace as its active workspace
- If found, update that monitor's entry in `monitor_windows_` with the new window's `CLASS` and `TITLE`
- If the opening window's workspace is not the monitor's active workspace, do nothing (window is not on the active workspace)
- Emit signal `monitorWindowChanged(const QString& monitorName)` for the affected monitor
- Schedule category resolution for the new app class

**Acceptance Criterion**:
- Monitor 1 active workspace = 1 (no windows); Monitor 2 active workspace = 3 (one window visible)
- Open a new window on workspace 1; verify monitor 1's bar updates to show that window
- Open another window on workspace 3; verify monitor 2's window is updated (or remains if same app class)
- Open a window on workspace 2 (not active on any monitor); verify no bar updates

---

### REQ-F-006: Close Window Event - Full Re-Query

**Event-driven**: When Hyprland sends `closewindow>>ADDRESS` event, the service shall re-query all monitors' active windows and update affected entries.

**Details**:
- Parse `closewindow>>ADDRESS` event (address of closed window)
- Perform full per-monitor re-query (same as startup initialization):
  1. `j/monitors` → active workspace per monitor
  2. `j/clients` → remaining windows per workspace
- For each monitor, select the next-most-recently-focused window (lowest `focusHistoryID`) on that workspace
- Update `monitor_windows_` for all affected monitors
- Emit signal `monitorWindowChanged(const QString& monitorName)` for each monitor that changed
- If a monitor's active workspace has no remaining windows, clear/blank that entry
- Schedule category resolution for any new app classes encountered

**Acceptance Criterion**:
- Monitor 1 active workspace has windows A and B (B most recent); close B
- Verify monitor 1's bar updates to show window A
- Monitor 2's workspace is unchanged by the event; verify its bar does not refresh
- Close the only window on monitor 2's workspace; verify bar becomes blank/hidden

---

### REQ-F-007: Move Window Event - Full Re-Query

**Event-driven**: When Hyprland sends `movewindow>>ADDRESS,WORKSPACENAME` event, the service shall re-query all monitors and update active windows for source and destination monitor/workspace pairs.

**Details**:
- Parse `movewindow>>ADDRESS,WORKSPACENAME` event
- Perform same full re-query as close-window event (both `j/monitors` and `j/clients`)
- Identify the source monitor (where the window was before the move) — inferred from before-state or re-query
- Identify the destination monitor (whose workspace WORKSPACENAME belongs to)
- Update both monitors' active window entries
- Emit signal `monitorWindowChanged()` for both source and destination monitors
- Schedule category resolution for app class if moving window becomes newly active

**Acceptance Criterion**:
- Monitor 1 workspace 1 has window A (active); Monitor 2 workspace 2 has window B (active)
- Move window A to workspace 2; verify Monitor 1 bar becomes blank/hidden and Monitor 2 bar updates to show window A
- Move window B back to workspace 1; verify both monitors update appropriately

---

### REQ-F-008: Empty Workspace Display

**State-driven**: While a monitor's active workspace contains no windows, the service shall leave that monitor's entry blank or null, and `ActiveWindowSection.qml` shall hide the widget.

**Details**:
- Maintain empty entries in `monitor_windows_` as null/empty objects or sentinel values (`HyprlandActiveWindow{ title: "", appClass: "", category: "" }`)
- Do not remove key from hash (always one entry per monitor)
- `ActiveWindowSection.qml` reads `titleForMonitor(Screen.name)` and checks for blank title
- QML binding: `visible: titleForMonitor(Screen.name) !== ""`
- No title/icon/category properties are set or rendered when workspace is empty

**Acceptance Criterion**:
- Create a new empty workspace on monitor 1 and focus it
- Verify monitor 1's ActiveWindowSection widget is invisible (not taking up space)
- Move a window to that workspace; verify widget becomes visible immediately
- Move window away; verify widget hides again

---

### REQ-F-009: QML Getter Methods for Per-Monitor Data

**Ubiquitous**: The `ActiveWindowService` singleton shall expose `Q_INVOKABLE` getter methods for QML to query window data by monitor name.

**Details**:
- Add three `Q_INVOKABLE QString` methods:
  - `titleForMonitor(const QString& monitorName)` → returns title string (empty if blank/null)
  - `appClassForMonitor(const QString& monitorName)` → returns app class string (empty if blank/null)
  - `categoryForMonitor(const QString& monitorName)` → returns category string (empty if blank/null)
- All methods return empty string if monitor name is not in `monitor_windows_` hash (safe fallback)
- Methods are synchronous; no blocking or async operations

**Acceptance Criterion**:
- Call `titleForMonitor("HDMI-1")` while monitor 1 displays window A; verify return value matches A's title
- Call `titleForMonitor("HDMI-1")` while monitor 1 has blank workspace; verify return value is `""`
- Call `titleForMonitor("NONEXISTENT-MONITOR")` ; verify return value is `""`

---

### REQ-F-010: QML Per-Monitor Signal Emission

**Ubiquitous**: The `ActiveWindowService` singleton shall emit `monitorWindowChanged(const QString& monitorName)` signal whenever a specific monitor's active window changes.

**Details**:
- Define signal: `Q_SIGNAL void monitorWindowChanged(const QString& monitorName);`
- Emit whenever `monitor_windows_[monitorName]` is updated (any of: title, appClass, category changes)
- Do **not** emit for unrelated events (e.g., a `focusedmon` event that only changes focused monitor, not a window's data)
- Emit once per monitor affected; if both monitors update in one event, emit twice with different monitorName values
- QML `Connections { onMonitorWindowChanged { } }` handlers will receive the signal

**Acceptance Criterion**:
- Set up QML `Connections` listening to `monitorWindowChanged`
- Change focus to a window on monitor 1; verify signal fires once with `monitorName = "HDMI-1"`
- Switch focus to monitor 2 (no window change, only focus change); verify no `monitorWindowChanged` signal fires
- Open a window on monitor 2's workspace; verify signal fires with `monitorName = "DP-2"`

---

### REQ-F-011: QML Monitor-Aware Active Window Section

**Ubiquitous**: Each `ActiveWindowSection.qml` instance shall identify its own monitor via `Screen.name` and subscribe to monitor-specific window changes.

**Details**:
- `ActiveWindowSection.qml` root: add `id: root` and store `Screen.name` in a property (or use directly in bindings)
- Replace all `ActiveWindowService.title` bindings with `ActiveWindowService.titleForMonitor(Screen.name)`
- Replace all `ActiveWindowService.appClass` bindings with `ActiveWindowService.appClassForMonitor(Screen.name)`
- Replace all `ActiveWindowService.category` bindings with `ActiveWindowService.categoryForMonitor(Screen.name)`
- Add QML `Connections` block:
  ```qml
  Connections {
    target: ActiveWindowService
    function onMonitorWindowChanged(monitorName) {
      if (monitorName === Screen.name) {
        // Qt bindings automatically update via getter methods
        // Explicit refresh/notification not needed (property binding is reactive)
      }
    }
  }
  ```
- Visible binding: `visible: (titleForMonitor(Screen.name) !== "")`

**Acceptance Criterion**:
- Launch bar on 2-monitor setup; verify each bar shows **different** windows if workspaces differ
- Change focus on monitor 1; verify monitor 1 bar updates, monitor 2 bar unchanged
- Confirm that `Screen.name` correctly identifies each monitor's output (e.g., `"HDMI-1"` vs `"DP-2"`)

---

### REQ-F-012: Category Resolution for All Per-Monitor Windows

**Event-driven**: When any window's `appClass` is set or updated, the service shall schedule asynchronous category resolution.

**Details**:
- Reuse existing `scheduleResolveCategory(app_class)` mechanism (already implemented for global active window)
- Category resolution runs on `QtConcurrent::run` (non-blocking)
- Scans XDG desktop files in `~/.local/share/applications/` and `/usr/share/applications/` (two-pass: exact filename, then case-insensitive field scan)
- Result cached in `category_cache_` with `resolved_classes_` set to prevent re-scanning
- On resolution completion, update `monitor_windows_[affected_monitor].category` and emit `monitorWindowChanged(affected_monitor)`
- Stale-result guard: only apply result if the window's `appClass` has not changed since the scan started (identical app class)

**Acceptance Criterion**:
- Open Firefox on monitor 1; verify category resolves to `"browser"` within 500ms (async) and bar icon updates
- Open a second Firefox window on monitor 2; verify cached result is used (no second XDG scan) and category appears instantly
- Verify that cached category is returned on subsequent `titleForMonitor` calls without re-scanning

---

### REQ-F-013: Hyprland Event Parsing - Monitor Name Extraction

**Ubiquitous**: Event parsing utilities shall extract monitor names from Hyprland IPC payloads accurately.

**Details**:
- `focusedmon>>MONNAME,WSNAME` → parse monitor name as substring before first comma
- `openwindow>>ADDRESS,WORKSPACENAME,CLASS,TITLE` → workspace name is second comma-separated field
- `movewindow>>ADDRESS,WORKSPACENAME` → workspace name is second comma-separated field
- No assumptions about monitor name format (alphanumeric, hyphens allowed: `"HDMI-1"`, `"DP-2"`, etc.)
- Trim whitespace from parsed components

**Acceptance Criterion**:
- Parse `focusedmon>>HDMI-1,5` and verify extracted monitor = `"HDMI-1"`, workspace = `"5"`
- Parse `focusedmon>>DP-2,10` and verify correct extraction
- Parse `focusedmon>>HDMI-1-ext,workspace-name` and verify monitor name with hyphens is extracted correctly

---

### REQ-F-014: Hyprland JSON Query - Monitor and Client Data

**Ubiquitous**: The service shall parse Hyprland's JSON responses (`j/monitors`, `j/clients`) to extract per-monitor workspace assignments and window `focusHistoryID` values.

**Details**:
- `j/monitors` response: array of monitor objects, each with `name` (string) and `activeWorkspace` (object with `id` field, numeric)
- Extract `monitors[i].name` and `monitors[i].activeWorkspace.id` → key-value pair for each monitor
- `j/clients` response: array of window objects, each with:
  - `address` (hex string, unique window identifier)
  - `class` (app class)
  - `title` (window title)
  - `workspace` (object with `id` field, numeric)
  - `focusHistoryID` (numeric, 0 = most recent OR highest = most recent — to be verified)
- For a given workspace ID, filter `clients` to those matching that workspace ID, then select the one with the **lowest** `focusHistoryID` (assumed most recent; verify on implementation)
- Use `QJsonDocument::fromJson()` with error handling (log parse failures, don't crash)

**Acceptance Criterion**:
- Query `j/monitors` and parse response; verify that all monitor names are extracted as strings (not null/invalid)
- Query `j/clients` and verify at least one window has valid `class`, `title`, `workspace.id`, and `focusHistoryID` fields
- Filter clients by workspace 1; verify the selected client has the lowest numeric `focusHistoryID` among that workspace's windows
- Confirm that malformed JSON responses are logged as warnings and do not crash the service

---

## Non-Functional Requirements

### REQ-NF-001: Non-Blocking Event Handling

**Ubiquitous**: All Hyprland IPC event handlers shall complete in <100ms to prevent blocking the Qt main loop and UI frame rendering.

**Details**:
- Category resolution is scheduled on `QtConcurrent::run` (off-thread, does not block)
- JSON queries at startup use blocking sockets (acceptable: startup context)
- Event handler code is minimal: parse, hash lookup/update, signal emit
- No expensive operations (file I/O, network, sync JSON parsing) in event critical path

**Acceptance Criterion**:
- Measure event handler execution time via `QElapsedTimer` in code review
- Confirm that handler completes before next frame (16ms for 60fps; 100ms is safe margin)
- Verify frame rate remains stable (no jank) during rapid window focus changes

---

### REQ-NF-002: Memory Efficiency

**Ubiquitous**: The per-monitor window storage shall use minimal heap memory, with no leaks or unbounded growth.

**Details**:
- `monitor_windows_` hash: one entry per physical monitor (typically 1–4 monitors), not per workspace
- Each entry stores three strings (`title`, `appClass`, `category`); typical total < 2KB per monitor
- `category_cache_` is a file-scan result cache; grow slowly over session lifetime as new apps are resolved (bounded by unique app count)
- No circular references or shared pointer cycles
- Qt memory management (RAII via `QObject` ownership) applies to C++ objects

**Acceptance Criterion**:
- Measure heap size on a 4-monitor setup with 50+ windows total; confirm per-monitor storage < 10KB total
- Run valgrind or Qt's memory profiler; confirm no memory leaks over 1-hour session

---

### REQ-NF-003: Signal Emission Correctness

**Ubiquitous**: QML signal connections shall not miss updates or receive stale data.

**Details**:
- Emit `monitorWindowChanged(monitorName)` **after** updating `monitor_windows_[monitorName]` (not before)
- Use `Q_EMIT` macro for clarity
- No deferred/queued signal delivery; direct connection for local signals
- Verify that QML `Connections { onMonitorWindowChanged }` handlers trigger correctly (test harness)

**Acceptance Criterion**:
- Connect QML handler to `monitorWindowChanged` signal; verify it fires in response to window changes
- Verify that getter methods called from the handler return updated data (not stale data)

---

## Constraints

### REQ-C-001: Hyprland IPC Protocol Compatibility

**Ubiquitous**: The implementation shall target Hyprland version 0.55.2+ (tested on this version) and shall not assume behaviors from newer or older versions.

**Details**:
- Use event socket format: `$XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE/.socket2.sock`
- Assume `activewindow>>`, `focusedmon>>`, `openwindow>>`, `closewindow>>`, `movewindow>>` events are present
- `j/monitors`, `j/clients`, `j/activewindow` endpoints available
- `focusHistoryID` field present on window objects in `j/clients` response
- Do not assume custom workspace names; treat workspace IDs as numeric (to be verified on implementation)

**Acceptance Criterion**:
- Test on Hyprland 0.55.2; confirm all IPC calls succeed and responses parse correctly
- Confirm no errors from missing fields or unexpected JSON structures (graceful fallback to empty entries)

---

### REQ-C-002: Qt6 QML Execution Model

**Ubiquitous**: QML property bindings and signals shall follow Qt6's synchronous reactive model.

**Details**:
- Property getters (`titleForMonitor()` etc.) are invoked on every binding update
- `Connections { onSignal }` handlers execute synchronously in the main thread
- No async/Promise-style signals; all state is imperative

**Acceptance Criterion**:
- Verify that QML binding `text: ActiveWindowService.titleForMonitor(Screen.name)` updates immediately when monitor changes (no delay)
- Confirm that handler code in `Connections` block runs before next paint cycle

---

### REQ-C-003: Screen.name Reliability

**Ubiquitous**: The `Screen.name` QML property shall reliably identify each monitor's output name (e.g., `"HDMI-1"`, `"DP-2"`).

**Details**:
- `Screen.name` comes from Qt's QScreen object, which is bound to Wayland's wl_output global
- Hyprland assigns output names to match wl_output names
- Names are stable across session restarts (for the same physical monitor arrangement)
- Names may differ if monitor USB port changes or DisplayPort tunnel changes; but within a session, names are constant

**Acceptance Criterion**:
- Compare `ActiveWindowSection.qml`'s `Screen.name` with Hyprland's monitor names (via `hyprctl monitors`); verify they match exactly
- Confirm that a bar's monitor name remains constant through focus/workspace changes

---

### REQ-C-004: No Circular Dependencies Between Services

**Ubiquitous**: `ActiveWindowService` shall not depend on other Wayland protocol services (e.g., `ExtWorkspaceManager`, `LayerShellManager`) to avoid initialization ordering issues.

**Details**:
- Hyprland IPC is sufficient for window tracking; no D-Bus or other protocol required
- Existing `ActiveWindowService` has no such dependencies; change preserves this
- QML layer (`ActiveWindowSection.qml`) may depend on other services for UI (cosmetic), but C++ core does not

**Acceptance Criterion**:
- Code review: confirm no `#include` of other service headers in `ActiveWindowService.h/cpp`
- Test: launch bar with only Hyprland running (no other protocol servers); confirm window tracking works

---

## Acceptance Scenarios

### Scenario 1: Startup with Multiple Workspaces

**Setup**: 2-monitor system. Monitor 1 workspace 1 has Firefox (focused). Monitor 2 workspace 2 has Chromium.

**Expected**:
- Bar 1 shows "Firefox" with browser icon
- Bar 2 shows "Chromium" with browser icon
- Each bar shows its monitor's workspace content, not global focus

**Test Steps**:
1. Launch holonight-shell on this setup
2. Observe bars after 1 second (category resolution async)
3. Verify window titles and icons match (Firefox ≠ Chromium visual)

---

### Scenario 2: Focus Switch on Non-Focused Monitor

**Setup**: Same 2-monitor setup as above.

**Expected**:
- User clicks in a terminal window on Monitor 2 (same workspace 2 as Chromium, but different window)
- Bar 2 updates to "Terminal" (or actual app name)
- Bar 1 still shows "Firefox" (unchanged)

**Test Steps**:
1. Launch setup from Scenario 1
2. Click a terminal window on Monitor 2, or use `ydotool` to focus it
3. Verify Monitor 2 bar updates; Monitor 1 bar unchanged
4. Verify category resolves to terminal/shell icon

---

### Scenario 3: Move Window Between Workspaces

**Setup**: Monitor 1 workspace 1 has 2 windows: Firefox (most recent) and Terminal (older).

**Expected**:
- User moves Firefox to workspace 2
- Bar 1 updates to show "Terminal" (next-most-recent on workspace 1)
- Bar 2 (if workspace 2 is active) updates to show "Firefox"

**Test Steps**:
1. Set up windows and workspaces as described
2. `hyprctl dispatch movetoworkspace 2` on Firefox
3. Verify both bars update appropriately
4. Verify workspace 1 bar shows Terminal; workspace 2 bar shows Firefox

---

### Scenario 4: Close Only Window on Monitor's Workspace

**Setup**: Monitor 2 workspace 5 has single window: Slack.

**Expected**:
- User closes Slack
- Bar 2 blank/hidden (no window on workspace 5)
- Bar 2 becomes visible again when a new window opens on workspace 5

**Test Steps**:
1. Set up Monitor 2 with only Slack on its active workspace
2. Close Slack
3. Verify Bar 2 becomes invisible/blank
4. Open a new window on workspace 5 (e.g., file manager)
5. Verify Bar 2 shows that window

---

### Scenario 5: Rapid Focus Changes

**Setup**: 2 monitors with 10+ windows across workspaces.

**Expected**:
- User rapidly switches focus (Alt+Tab, mouse clicks on multiple windows)
- Bars update smoothly, no lag or missed updates
- Frame rate remains 60fps (no jank)

**Test Steps**:
1. Set up multi-window scenario
2. Rapidly focus different windows on each monitor
3. Observe bars for smooth updates (no delays, no missed states)
4. Monitor CPU/frame time (should be <100ms per update)

---

### Scenario 6: Monitor Hotplug

**Setup**: 1-monitor system; user plugs in second monitor while bar is running.

**Expected**:
- Hyprland detects new monitor and emits events
- LayerShellManager creates new bar for new monitor
- New bar shows appropriate active window for that monitor's workspace

**Test Steps**:
1. Launch bar on single monitor
2. Physically plug in (or simulate hotplug) second monitor
3. Verify Bar 2 appears and shows active window on its workspace
4. Verify both bars function independently (focus/workspace changes test separately)

---

### Scenario 7: Empty All Workspaces

**Setup**: User closes all windows, leaving all workspaces empty.

**Expected**:
- All bars become blank/invisible (no window titles shown)
- Bars remain ready to show windows when they are opened

**Test Steps**:
1. Close all windows on all monitors
2. Verify all bars become invisible
3. Open a window on monitor 1
4. Verify only bar 1 becomes visible; bar 2 remains blank

---

## Implementation Notes

### Data Structure: HyprlandActiveWindow

Suggest a simple struct or data class:

```cpp
struct HyprlandActiveWindow {
  QString title;
  QString appClass;
  QString category;
  
  bool isEmpty() const { return title.isEmpty() && appClass.isEmpty(); }
};
```

### Event Parsing: focusHistoryID Ordering

**To be verified at implementation time**: Is `focusHistoryID = 0` the most recent window, or is the highest value most recent? Check Hyprland source code or empirical testing with `j/clients` before implementation.

### Category Cache Reuse

The existing `scheduleResolveCategory()` and `category_cache_` infrastructure is reused without modification. If a window's `appClass` has been resolved before (on the globally focused window), the cached category is returned immediately.

### QML Binding Reactive Updates

Qt6 property bindings are reactive: when `ActiveWindowService.titleForMonitor(Screen.name)` is called, the engine automatically re-invokes the method whenever the underlying data changes (signal emission). Explicit `onMonitorWindowChanged` handlers are provided for UI state that cannot be expressed via property bindings (optional optimization).

---

## Open Questions for Implementation

1. **focusHistoryID ordering**: Is 0 = most recent, or highest = most recent? Verify on Hyprland 0.55.2.
2. **Custom workspace names**: Hyprland supports non-numeric names (e.g., "discord", "music"). Should per-monitor active window support them, or assume numeric IDs only? (Deferred to future scope; assume numeric for MVP.)
3. **Stale window references**: After `closewindow>>`, can an address re-appear on the client list before we re-query? (Unlikely; Hyprland uses unique addresses, but test to confirm.)
4. **Category resolution latency**: Is 500ms typical? Test on target system and adjust timeout expectations if needed.

---

## Testing Strategy

### Unit Tests (GTest)

- Test JSON parsing (j/monitors, j/clients) with mock responses
- Test event parsing (focusedmon>>, openwindow>>, etc.) with various payloads
- Test getter methods with populated/empty `monitor_windows_` hash

### Integration Tests

- Multi-monitor setup (2–4 monitors)
- QML bar functionality with actual Hyprland IPC
- Signal emission and QML binding reactivity
- Category resolution (actual XDG desktop file scanning)

### Manual Acceptance Tests

- Run all scenarios in "Acceptance Scenarios" section
- Visual inspection: bar appearance, title/icon updates
- Performance: frame time, CPU usage during rapid changes

---

## Definition of Done

- [x] SPEC.md document complete with all EARS requirements
- [ ] C++ implementation of per-monitor window storage and event handlers
- [ ] QML integration (ActiveWindowSection.qml updated with per-monitor getters and connections)
- [ ] Unit tests passing (GTest)
- [ ] Integration tests passing (multi-monitor setup)
- [ ] All acceptance scenarios passing (manual testing)
- [ ] Code review complete (no clang-tidy warnings)
- [ ] QML lint passes (`task qml-lint`)
- [ ] Documentation updated (CLAUDE.md with new architecture notes)

---

## References

- Current `ActiveWindowService`: src/ActiveWindowService.h/cpp
- Layer shell setup: src/LayerShell.h, src/LayerShellManager.h/cpp
- QML Topbar: src/qml/Topbar/TopBar.qml, src/qml/Topbar/ActiveWindowSection.qml
- Hyprland IPC docs: [Hyprland IPC Socket Documentation](https://wiki.hyprland.org/IPC/)
- Qt6 QML Connections: [Qt6 Connections Documentation](https://doc.qt.io/qt-6/qml-qtqml-connections.html)
- EARS format reference: [Easy Approach to Requirements Syntax](https://www.sstc.gatech.edu/research/ears/)

