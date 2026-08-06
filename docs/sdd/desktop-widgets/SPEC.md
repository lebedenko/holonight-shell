## Overview

The Desktop Widgets feature introduces configurable, time-aware overlay surfaces to the holonight-shell Wayland compositor. Widgets are lightweight, non-interactive layer-shell surfaces positioned at nine anchor points on each monitor. They render on the `bottom` layer (above wallpaper, below normal windows) and become visible only when the currently-visible workspace on their target monitor contains no windows—providing an "empty workspace" indicator and countdown clock for upcoming events.

### Key Goals

1. **Occupancy-gated rendering**: Widgets appear only on empty workspaces, hidden when the workspace becomes occupied.
2. **Efficient caching**: All surfaces are created at startup and reused across visibility transitions; no cold-start QML compilation on show/hide.
3. **Per-monitor targeting**: Widgets can be scoped to specific monitor outputs, with global positioning but per-monitor collision avoidance.
4. **Event countdown**: Time-to-event widgets display adaptive countdowns (dropping leading zero units) with optional seconds precision.
5. **Configuration-driven**: TOML-based configuration in the existing ConfigService, with live-reload support and comprehensive validation.

---

## Functional Requirements

### Widget Lifecycle & Caching

**REQ-F-001: Eager Surface Creation**

The system shall create all widget layer-shell surfaces (one QQuickView + layer-shell surface per (monitor, widget) tuple) during application startup before any workspace or occupancy events are processed.

Acceptance criteria: At startup, before any user interaction, `WlrLayerSurfaceManager::hasSurfaceOnMonitor(widgetId, monitorName)` returns true for every widget-monitor pair that the configuration specifies, regardless of current workspace occupancy.

---

**REQ-F-002: Surface Caching Across Occupancy Transitions**

The system shall preserve the QML component instance (QQuickView + full object tree) when a widget surface transitions between visible and hidden due to occupancy changes; the surface shall not be destroyed and recompiled.

Acceptance criteria: When a workspace transitions from empty to occupied, `QQuickView::hide()` is called, but the QML root object's properties (e.g., custom countdown value) retain their previous state after `QQuickView::show()` is later called. Verify via `QObject::property()` inspection or a debug log of property state before/after hide/show.

---

**REQ-F-003: Surface Destruction on Monitor Removal or Config Reload**

The system shall destroy a widget's layer-shell surface only when: (a) the monitor is unplugged, or (b) a config reload removes or changes that widget's configuration entry.

Acceptance criteria: Disconnect a monitor with an active widget surface. The surface is destroyed and no Wayland protocol errors occur. Then reconnect the monitor. The widget surface is recreated, and if the countdown timer was running, it is re-initialized to the current wall-clock time.

---

### Occupancy Rendering Gate

**REQ-F-004: Render Gate Signal Definition**

The system shall provide a computed signal that answers: "Is the currently-visible workspace on monitor X empty?" by combining two existing data sources: `HyprlandWorkspaceService::occupied_workspace_ids` (set of workspace IDs with any window) and `ActiveWindowService::monitor_workspaces` (map of (monitor, workspace) pairs indicating what workspace is currently visible per monitor).

Acceptance criteria: Implement a new signal (e.g., `WidgetManager::monitorWorkspaceOccupancyChanged(const QString& monitorName, bool isEmpty)`). Verify the signal fires when `ActiveWindowService` detects a new visible workspace on the monitor, or when `HyprlandWorkspaceService` detects a window being added/removed from the visible workspace. The signal must not fire for unrelated workspace occupancy changes (e.g., a different monitor's workspace becoming occupied).

---

**REQ-F-005: Surface Unmapping on Workspace Occupied**

When the currently-visible workspace on a widget's monitor becomes occupied (transitions from zero to at least one window), the system shall call `QQuickView::hide()` on the widget's surface and stop (freeze) any countdown timer associated with that widget on that monitor.

Acceptance criteria: Start the application with an empty workspace. Verify the widget is visible. Open any window on that monitor in that workspace. The widget disappears within 100 ms (Wayland unmapping latency). Inspect the countdown timer via logging or property access—it is paused, and if inspected again 5 seconds later, its value has not incremented.

---

**REQ-F-006: Surface Remapping and Timer Resumption on Workspace Empty**

When the currently-visible workspace on a widget's monitor becomes empty (all windows closed), the system shall call `QQuickView::show()` on the widget's surface, resume any frozen countdown timer, and resynchronize the timer to the current wall-clock time (preventing drift if the timer was frozen for an extended period).

Acceptance criteria: After REQ-F-005, close all windows in the workspace. The widget reappears within 100 ms. The countdown timer resumes and increments correctly (compare its displayed value against the system wall-clock time, accounting for the configured deadline). Run the timer frozen for 30 seconds, then show the workspace empty—the countdown should match the current wall-clock, not resume from where it was paused.

---

**REQ-F-007: Hyprland-Specific Implementation**

The system shall treat the occupancy gate as Hyprland-specific, leveraging Hyprland's IPC to determine the visible workspace per monitor and its window count, and shall not attempt to implement generic occupancy detection across other compositors (even if Qt or Wayland provide workspace abstraction).

Acceptance criteria: The occupancy gate code references `HyprlandWorkspaceService::occupied_workspace_ids` and `ActiveWindowService::monitor_workspaces` by name. No attempt is made to query occupancy from generic Wayland atoms or a fallback compositor interface. Running the widget on a non-Hyprland compositor is unsupported and may be documented in the shell's README.

---

### Monitor Targeting

**REQ-F-008: Monitor Array Configuration**

Each widget configuration entry shall include an optional `monitors` array field (TOML array of strings) containing monitor OUTPUT NAMES (e.g., "DP-1", "eDP-1", "HDMI-A-1"), matched against `QScreen::name()` values.

Acceptance criteria: Parse a TOML configuration with entries like `monitors = ["DP-1", "eDP-1"]`. The parsed widget object has a `monitors` property (QStringList or equivalent) containing exactly those two strings, case-preserved.

---

**REQ-F-009: Empty or Omitted Monitors Array Means All Monitors**

If the `monitors` array is absent or empty in a widget configuration, the system shall display the widget on all currently-connected monitors.

Acceptance criteria: Define a widget without a `monitors` field (or with `monitors = []`). At startup with three monitors connected, verify that surfaces are created for all three monitors. If a fourth monitor is hot-plugged, a surface is created for it as well.

---

**REQ-F-010: Non-Empty Monitors Array Restricts Display**

If the `monitors` array is non-empty, the system shall display the widget only on monitors whose `QScreen::name()` is listed in that array.

Acceptance criteria: Define a widget with `monitors = ["DP-1"]`. Connect three monitors named "DP-1", "DP-2", "eDP-1". Verify surfaces are created only for "DP-1". No surfaces exist for "DP-2" or "eDP-1" even though they are connected.

---

**REQ-F-011: Forward-Compatible Monitor Name Handling**

If a monitor name in the `monitors` array does not match any currently-connected monitor at config parse time, the system shall log a warning EXACTLY ONCE at config parse time. The name shall remain honored: if that monitor is later hot-plugged during runtime, a widget surface shall be created for it.

Acceptance criteria: Define a widget with `monitors = ["HDMI-A-1"]`. Start the application with only "DP-1" and "eDP-1" connected. A warning is logged once (not repeatedly). Then hot-plug "HDMI-A-1". A widget surface is created for "HDMI-A-1" without warnings, using the name that was pre-configured.

---

### Positioning & Collision

**REQ-F-012: Nine Anchor Positions**

The system shall support nine distinct positioning anchors: `left-top`, `center-top`, `right-top`, `left-center`, `center-center`, `right-center`, `left-bottom`, `center-bottom`, `right-bottom`.

Acceptance criteria: A widget's `position` field accepts any of these nine strings. Invalid position strings are rejected during config validation (REQ-F-028). Verify visually that a widget placed at each position appears at the correct corner/edge/center of the monitor.

---

**REQ-F-013: Global Configurable Margin**

The system shall apply a single global margin (top-level `[widgets]` table in TOML, key `margin`, unit: logical pixels, default: 32) to all four directions (left, top, right, bottom) for all widgets and positions.

Acceptance criteria: With `margin = 32`, place a widget at `left-top`. The widget's left edge is 32 pixels from the screen's left edge, and its top edge is 32 pixels below the top bar (see REQ-F-014). Change the config to `margin = 64`, trigger a reload. The widget moves to maintain 64 pixels clearance from edges. Verify the margin is applied uniformly in all directions.

---

**REQ-F-014: Top-Bar Height Offset for Top-Anchored Positions**

For widgets positioned at `left-top`, `center-top`, or `right-top`, the system shall calculate the effective top inset as `barHeight + margin`. Top-anchored widgets shall be positioned downward by this amount, ensuring they never overlap the top bar.

Acceptance criteria: The top bar is measured (e.g., 32 pixels). With `margin = 32`, top-anchored widgets are positioned 64 pixels from the screen's top. Change the margin to 16; top-anchored widgets shift to 48 pixels from the screen's top. If the bar height changes (via theme or dynamic resize), top-anchored widgets reposition accordingly.

---

**REQ-F-015: Per-Monitor Per-Position Collision Detection**

When two or more widgets target the same (monitor, position) tuple, the system shall permit the widget that appears earlier in the configuration (TOML `[[widget]]` order) to occupy that position on that monitor. Later widgets targeting the same (monitor, position) shall be dropped on that monitor only; they may still occupy other positions or appear on other monitors.

Acceptance criteria: Configure two widgets, both targeting `position = "center-top"` on monitor "DP-1". Widget A is listed first; Widget B is listed second. At startup, only Widget A renders on DP-1 at center-top. Widget B is dropped on DP-1 but may still render on other monitors at center-top, or on DP-1 at a different position if configured elsewhere (this tests per-monitor per-position scoping, not global per-position scoping).

---

**REQ-F-016: Single Collision Warning Per Widget-Monitor**

When a widget is dropped due to collision on a specific monitor, the system shall log a warning EXACTLY ONCE at widget creation time for that (widget, monitor) collision. No further warnings shall be logged for that collision during the widget's lifetime, even if the configuration is reloaded or the widget is hidden/shown due to occupancy.

Acceptance criteria: Trigger a collision as in REQ-F-015. Exactly one warning is logged at startup. Then simulate occupancy transitions (hide/show the workspace multiple times). No additional warnings are logged. Reload the configuration (without changing it); a new warning is logged (because widgets are recreated), but after that, repeated reloads do not log additional warnings for the same collision.

---

**REQ-F-017: Global Position per Widget, Per-Monitor Collision**

Each widget configuration entry has a single `position` value that applies uniformly across all of that widget's targeted monitors. Collision detection is scoped per (monitor, position) pair: a widget can win position P on monitor M1 but lose it on monitor M2 (if a higher-priority widget claims it there).

Acceptance criteria: Configure widgets A and B with `position = "center-top"`. A targets `monitors = ["DP-1"]`, B targets `monitors = ["DP-1", "DP-2"]`. A wins on DP-1, B is dropped there. On DP-2, B renders at center-top (no collision, as A does not target DP-2). Verify A is not shown on DP-2 at all.

---

### Time-to-Event Widget (Widget Type: "time-to-event")

**REQ-F-018: Time-to-Event Widget Configuration Fields**

The system shall recognize widget type `"time-to-event"` and require configuration fields: `title` (free-form string, required), `deadline` (ISO 8601 date or datetime string, required), `show_seconds` (boolean, optional, default false).

Acceptance criteria: Parse a TOML `[[widget]]` entry with `type = "time-to-event"`, `title = "Flight departs"`, `deadline = "2026-07-15T14:30:00"`, `show_seconds = true`. All fields are extracted and validated. Missing `title` or `deadline` triggers validation errors (REQ-F-026, REQ-F-027).

---

**REQ-F-019: Deadline String Parsing**

The system shall parse `deadline` as either: (a) a date-only string in ISO 8601 format (e.g., "2026-07-01"), interpreted as 00:00:00 local time, or (b) a full datetime string (e.g., "2026-07-01T09:00:00"), interpreted as the specified instant in local time. The system shall track whether the deadline string included a time component (presence of "T" + time portion) for use in the display label (REQ-F-020).

Acceptance criteria: Parse `deadline = "2026-07-15"`. The parsed deadline is midnight (00:00:00) on that date in the system's local timezone. Parse `deadline = "2026-07-15T14:30:00"`. The deadline is 2:30 PM on that date. Unparseable strings (e.g., "July 15") trigger a validation error (REQ-F-027). Verify the parser preserves the "had-time" flag: for date-only, the flag is false; for datetime, it is true.

---

**REQ-F-020: Time-to-Event Display Layout**

The system shall render time-to-event widgets with three text elements: a Title (styled as the widget's `title` field), a large Countdown (the primary visual element), and a small event-date label below the countdown.

Acceptance criteria: A time-to-event widget is rendered with three visually distinct text regions: title at the top (small font), countdown in the middle (large font, prominent color), and date label at the bottom (small font). Verify the layout is centered at the widget's position.

---

**REQ-F-021: Event Date Label Format and Time Component**

The event-date label shall use a fixed, locale-independent format. If the deadline string contained only a date (no "T" + time), the label shall display the date only (e.g., "2026-07-15"). If the deadline string included a time component, the label shall append the time (e.g., "2026-07-15 14:30").

Acceptance criteria: Configure Widget A with `deadline = "2026-07-15"` (no time). The label shows "2026-07-15". Configure Widget B with `deadline = "2026-07-15T14:30:00"` (with time). The label shows "2026-07-15 14:30". The format is constant regardless of system locale (no locale-dependent month names or day names).

---

**REQ-F-022: Adaptive Countdown Format**

The system shall display the countdown in an adaptive format: starting from the largest non-zero unit (days, hours, minutes, or seconds) and showing all units down to the smallest non-zero unit. Leading zero units are omitted. No leading "0d" is ever shown.

Acceptance criteria:
- Deadline 12 days, 4 hours, 37 minutes away: "12d 04h 37m"
- Deadline 4 hours, 37 minutes, 12 seconds away: "04h 37m 12s"
- Deadline 37 minutes, 12 seconds away: "37m 12s"
- Deadline 12 seconds away: "12s"
- Never display "0d 04h" or leading zero days.

---

**REQ-F-023: Seconds Field Suppression**

When `show_seconds` is false, the system shall never display a seconds field in the countdown, even if the deadline is fewer than 60 seconds away.

Acceptance criteria: With `show_seconds = false` and deadline 45 seconds away, the countdown displays only minutes and seconds dropped or rounded (implementation-chosen). Example: "00m" or "01m" (rounded up). No seconds field is shown. Compare against `show_seconds = true`, which would display "45s".

---

**REQ-F-024: Countdown Update Frequency and Synchronization**

When `show_seconds` is true, the system shall update the countdown every 1 second. When `show_seconds` is false, the system shall update the countdown once per minute, synchronized to the minute boundary (e.g., on the transition to a new minute in wall-clock time), and no more frequently.

Acceptance criteria: Start with `show_seconds = true` and deadline 2 minutes away. Observe the countdown decrements every 1 second: "02m 00s" → "01m 59s" → "01m 58s" etc. Switch to `show_seconds = false`. Observe the countdown updates once per minute (e.g., "02m" at 2:59, then "01m" at 3:00). Verify no sub-minute updates occur with `show_seconds = false`.

---

**REQ-F-025: Deadline Reached / "Now" State**

When the deadline is reached or has passed, the system shall display a static "Now" message and stop counting. No count-up ("ago") mode is implemented.

Acceptance criteria: Set deadline to 10 seconds in the past. The countdown displays "Now" and does not change. Wait 30 seconds. The countdown still displays "Now".

---

### Configuration & Validation

**REQ-F-026: Missing Title Validation**

If a widget configuration entry lacks a `title` field, the system shall log a warning and skip (not instantiate) that widget.

Acceptance criteria: Configure a `[[widget]]` entry of `type = "time-to-event"` with `deadline = "2026-07-15"` but no `title`. A warning is logged during config parse. The widget is not created; no surface appears for it.

---

**REQ-F-027: Missing or Unparseable Deadline Validation**

If a widget configuration entry lacks a `deadline` field or contains an unparseable deadline string, the system shall log a warning and skip that widget.

Acceptance criteria: Configure a widget with `title = "Event"` but missing `deadline`. A warning is logged; the widget is skipped. Configure another widget with `deadline = "not-a-date"`. A warning is logged; the widget is skipped.

---

**REQ-F-028: Unknown Type Validation (Forward Compat)**

If a widget configuration entry specifies an unknown `type` (not "time-to-event"), the system shall log a warning and skip that widget. This allows future widget types to be added without breaking older configurations.

Acceptance criteria: Configure a widget with `type = "unknown-future-type"`. A warning is logged; the widget is skipped. A widget with `type = "time-to-event"` is processed normally. No errors or crashes occur.

---

**REQ-F-029: Default Position**

If a widget configuration entry lacks a `position` field, the system shall default to `"center-center"` without logging a warning.

Acceptance criteria: Configure a widget without specifying `position`. The widget is created and appears at the center-center position. No warning is logged.

---

**REQ-F-030: Default show_seconds**

If a widget configuration entry lacks a `show_seconds` field, the system shall default to `false`.

Acceptance criteria: Configure a time-to-event widget without `show_seconds`. The countdown updates once per minute and never displays a seconds field, as if `show_seconds = false` was explicitly set.

---

**REQ-F-031: Configuration File Location and Format**

Widget configuration shall reside in the existing TOML configuration file at `$XDG_CONFIG_HOME/holonight/config.toml`. Widgets are configured as a TOML array-of-tables: one or more `[[widget]]` entries, each with a `type` field and type-specific fields.

Acceptance criteria: The configuration file contains entries like:
```toml
[widgets]
margin = 32

[[widget]]
type = "time-to-event"
title = "Flight departs"
deadline = "2026-07-15T14:30:00"
position = "center-top"
```
The parser reads this structure, extracts the `margin` and all `[[widget]]` entries, and processes them according to the validation rules.

---

**REQ-F-032: Live Configuration Reload**

The system shall monitor the configuration file for changes. When the file is modified, the system shall rebuild all widget surfaces (destroy old surfaces, parse the new config, create new surfaces) with debouncing (~200 ms) to avoid rebuilds during rapid edits.

Acceptance criteria: Edit the config file to add a new widget. Within 200 ms, the new widget's surface appears. Edit the config file to change a widget's `position`. The widget shifts position within 200 ms. Edit the config file to remove a widget. The widget's surface is destroyed within 200 ms. The debounce prevents multiple rebuilds if the file is edited twice in quick succession (e.g., two edits within 100 ms result in one rebuild, not two).

---

**REQ-F-033: widgetsChanged Signal**

The system shall provide a `widgetsChanged` signal in the widget service/manager to notify the rest of the application (or UI) of a config reload. This signal is emitted after all widget surfaces have been recreated.

Acceptance criteria: Connect a test slot to the `widgetsChanged` signal. Trigger a config reload (by editing the config file). The slot is called once, after the new widget surfaces are created and ready. Subsequent config reloads emit the signal again.

---

## Non-Functional Requirements

**REQ-NF-001: Theme Color Compliance**

All widget colors and fonts shall be sourced from the HoloNight theme (HoloniightPalette tokens and ThemeService font properties). No hardcoded hex values, CSS color names, or literal font family strings shall appear in QML or C++ code for visual styling.

Acceptance criteria: Run clang-tidy and qmllint on the widget code with rules configured to flag hardcoded color literals. No violations appear in widget-specific code. Use ThemeService::fontFamily() or HoloniightPalette::widgetText (or equivalent tokens) for all visual properties.

---

**REQ-NF-002: PerMonitorLayerManager Base Class**

The widget manager/service shall follow the existing PerMonitorLayerManager pattern used by BackgroundManager, ensuring consistency with the codebase architecture and enabling reuse of layer-shell management infrastructure.

Acceptance criteria: The widget manager class either inherits from PerMonitorLayerManager or implements the same interface (e.g., `QMap<QString, QQuickView*> views(); void setGeometry(const QString& monitorName, const QRect& rect)`). Code review confirms the pattern matches BackgroundManager.

---

**REQ-NF-003: C++ Language and Qt Version**

The implementation shall use C++23 and Qt 6, consistent with the rest of the holonight-shell codebase.

Acceptance criteria: All C++ code compiles without warnings with C++23 enabled (-std=c++23 or equivalent). No Qt 5 compatibility code or conditional compilation is needed.

---

**REQ-NF-004: QML Module Registration**

QML components shall be registered under the HolonightShell module URI (e.g., `import HolonightShell` in QML). QRC assets shall use the prefix `/HolonightShell/`.

Acceptance criteria: A widget QML component can be imported as `import HolonightShell; WidgetTypeName { … }`. QRC files for widget assets resolve correctly via `qrc:/HolonightShell/widgets/…`.

---

**REQ-NF-005: No New External Dependencies**

The implementation shall not introduce new third-party dependencies beyond those already present in the holonight-shell project (Qt, Wayland protocols, compiler runtime).

Acceptance criteria: Run `cmake` in configure mode and verify no new find_package() or FetchContent() calls are required. Dependencies are limited to Qt6 modules (Core, Gui, Qml, Quick, etc.), system libraries, and existing project infrastructure.

---

**REQ-NF-006: Layer-Shell Configuration**

Widget surfaces shall be created as layer-shell surfaces on the `bottom` layer with `exclusive_zone = -1`. Widget windows shall be transparent to input (Qt::WindowTransparentForInput), so all input events pass through to the underlying windows and desktop.

Acceptance criteria: Inspect the layer-shell surface attributes via Wayland protocol logs or a Wayland debugger tool (e.g., wl-info, wayland-monitor). Verify `layer = LAYER_BOTTOM` and `exclusive_zone = -1`. Test input: click on a widget surface; verify the click is received by a window behind the widget, not the widget itself.

---

**REQ-NF-007: Startup Performance**

Widget surface creation and QML compilation shall complete within the existing application startup budget (no additional blocking delay beyond the current startup sequence). Surfaces created at startup shall use the existing lazy-load or streaming pattern if any.

Acceptance criteria: Measure application startup time (from binary execution to "ready" state) with and without widgets. The difference is negligible (< 50 ms for a typical widget set of 2–4 widgets per monitor on a 3-monitor setup). Startup time is deterministic (multiple runs vary by < 10%).

---

## Constraints

**REQ-C-001: Hyprland-Specific Implementation**

The widget occupancy gate is Hyprland-specific and relies on Hyprland's IPC protocol to determine the visible workspace and its window count per monitor. The feature is not portable to other Wayland compositors (e.g., Sway, KDE Plasma) without substantial additional work. This is an accepted trade-off.

Acceptance criteria: The CLAUDE.md or README documents that desktop widgets require Hyprland. Code that queries occupancy references HyprlandWorkspaceService and ActiveWindowService by name. No fallback compositor support is attempted.

---

**REQ-C-002: Layer-Shell Wayland Protocol**

The implementation shall use the wlr-layer-shell Wayland protocol (existing in the codebase as `protocols/wlr-layer-shell-unstable-v1.xml` or equivalent). No custom protocols are added.

Acceptance criteria: Widget surfaces are created via the same QWaylandClientExtensionTemplate pattern used by existing layer-shell surfaces (BackgroundManager, TopBar). No new protocol files are added to `protocols/`.

---

**REQ-C-003: Single Margin Value**

The widget margin is a single global value applying uniformly to all four directions and all widgets. No per-direction (e.g., `margin_top`, `margin_left`) or per-widget margin override is supported in v1.

Acceptance criteria: The TOML configuration allows only `[widgets] margin = N`, not `margin_top`, `margin_bottom`, etc. The code applies this single value to all positions and directions.

---

**REQ-C-004: Nine Positions Only**

The positioning system supports exactly nine discrete anchors (corners, edges, center). No free-form x/y coordinate positioning or custom anchors are supported.

Acceptance criteria: The `position` field accepts only the nine defined strings. Any other position value is rejected during validation. No support for positions like "10% from left, 20% from top" or similar.

---

**REQ-C-005: Non-Interactive v1 Widgets**

In v1, all widgets are non-interactive: `Qt::WindowTransparentForInput` is applied, and no mouse or keyboard event handling is implemented for widgets. Future versions may add interactivity.

Acceptance criteria: Clicking on a widget does not trigger any widget behavior or event handling. The mouse event passes through to windows or the desktop below. No hover effects, tooltips, or other input-driven interactions are visible.

---

**REQ-C-006: Per-Monitor Surface Multiplexing**

For widgets with `monitors = []` (all monitors), one QQuickView + layer-shell surface is created per monitor. Surfaces are independent; no cross-monitor sharing or synchronization of content (e.g., countdown values) beyond the shared timer/deadline.

Acceptance criteria: A widget with `monitors = []` on a 3-monitor setup creates exactly 3 surfaces. Each surface has its own QQuickView instance and is positioned at the specified anchor on that monitor. The countdown timer is global (shared by all surfaces of a single widget), so all instances show the same countdown.

---

## Glossary

- **Occupancy**: The presence of one or more windows (any window type, any layer) in a given workspace on a given monitor.
- **Empty workspace**: A workspace with zero windows (occupancy = 0).
- **Layer-shell surface**: A Wayland surface created via the wlr-layer-shell protocol, positioned in a fixed layer (background, bottom, top, overlay) and not subject to normal window management.
- **Monitor output**: A physical or virtual display connected to the Wayland server, identified by a name (e.g., "DP-1") via `QScreen::name()`.
- **Visible workspace**: The workspace currently active / displayed on a given monitor (as reported by Hyprland's `j/monitors` IPC endpoint or equivalent).
- **Position anchor**: One of nine named positions (left-top, center-top, right-top, left-center, center-center, right-center, left-bottom, center-bottom, right-bottom) that define a widget's placement relative to a monitor.
- **Collision**: Two or more widgets targeting the same (monitor, position) tuple; only the highest-priority widget (earliest in config order) occupies that position on that monitor.
- **Countdown**: A text field displaying the time remaining until a deadline, updated at a frequency determined by `show_seconds` (1 second if true, 1 minute if false).
- **Adaptive countdown format**: A countdown display that omits leading zero units (e.g., "12d 04h 37m" rather than "00y 00mo 12d 04h 37m 00s").
- **Deadline**: An ISO 8601 date or datetime string specifying the target instant for a time-to-event widget.
- **Margin**: The minimum clearance (in logical pixels) between a widget and the edges of the monitor, applied uniformly to all directions and all widgets.
- **Configuration reload**: The process of parsing the updated config file, validating all entries, destroying old widget surfaces, and creating new surfaces for the current configuration.
- **TOML array-of-tables**: The `[[widget]]` syntax in TOML, which defines a list of table entries, each with the same structure.
- **Logical pixels**: Screen coordinates after scaling by the monitor's DPI scale factor (e.g., a 2.0 scale on a 4K monitor means 1 logical pixel = 2 physical pixels).
- **Exclusive zone**: A layer-shell property indicating how much screen space is reserved for a surface; `-1` means the surface does not reserve any space and other windows may overlap it.
- **Input transparency**: The property of a window that allows input events (mouse, keyboard) to pass through it to windows and surfaces beneath it (Qt::WindowTransparentForInput).
