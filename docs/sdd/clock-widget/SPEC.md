# Clock Widget Specification

## Overview

The clock widget is a new desktop widget type for `holonight-shell` that displays the current time and date on the bottom layer-shell surface of each monitor. It slots into the existing desktop-widget framework, inheriting the occupancy-gated visibility model, position configuration, and timer management pattern from the existing `time-to-event` widget.

The widget renders two centered rows: a 24-hour time display (HH:mm with optional seconds) above a localized date display. All display strings are formatted in C++ by the WidgetManager and pushed to QML; the display is styled using HoloNight palette tokens and ThemeService fonts.

---

## Display & Rendering

### REQ-F-001: Time display row
**Template:** The system shall render a time display row with hours, minutes, and optional seconds.
**Statement:** The clock widget shall display the current time in 24-hour format as HH:mm, with seconds displayed inline at half the pixel height of hours/minutes and baseline-aligned to the minute digits.

*Acceptance:*
- A widget configured with `show_seconds = true` displays seconds at visibly smaller size than hours/minutes.
- The seconds baseline aligns with the baseline of the minutes, not the top or center.
- Time values always use 24-hour notation (never AM/PM); values range 00:00–23:59.

### REQ-F-002: Date display row
**Template:** The system shall render a date display row with weekday, day, month, and year.
**Statement:** The clock widget shall display the current date using a configurable format pattern; the default pattern is `dddd, d MMMM yyyy`, producing output like "Sunday, 8 June 2026".

*Acceptance:*
- Weekday name, day number, full month name, and year are all visible in the date row.
- When no `date_format` config is specified, the output exactly matches the Qt format `dddd, d MMMM yyyy`.
- Weekday and month names reflect the configured locale (or system locale if no locale is configured).

### REQ-F-003: Glow effect on time row
**Template:** The system shall apply a visual effect consistent with the existing countdown widget.
**Statement:** The time row shall display with a glow effect using `QtQuick.Effects.MultiEffect` with shadow enabled, tuned to match the visual appearance of the existing time-to-event countdown widget.

*Acceptance:*
- A glow is visibly present around the time text when the widget is displayed.
- The glow effect uses the same MultiEffect configuration as the countdown widget (shadow enabled, not Qt5Compat Glow).
- The glow color derives from a HoloNight palette token (not hardcoded hex).

### REQ-F-004: Centered layout
**Template:** The system shall position content within the layer-shell surface using centered alignment.
**Statement:** Both the time row and date row shall be centered within the widget's layer-shell surface.

*Acceptance:*
- When viewed on a centered-positioned widget, both rows appear visually centered horizontally and vertically within the surface bounds.
- No asymmetric padding or left-alignment is visible.

### REQ-NF-001: Color palette compliance
**Template:** The system shall enforce color consistency with the design system.
**Statement:** The clock widget shall not use hardcoded color values in QML; all colors shall derive from HoloNight palette tokens via `import Holonight` and `HoloniightPalette.<token>`.

*Acceptance:*
- No hex color literals appear in the QML source of the clock widget.
- The widget respects theme changes that update HoloniightPalette tokens at runtime.

### REQ-NF-002: Font sourcing
**Template:** The system shall use centralized font configuration.
**Statement:** The clock widget shall source font family and size from ThemeService properties (`clockFont`, `clockFontSize`, `uiFont`, `uiFontSize`); the time row shall use the clock font at roughly 2× the clock font size; the date row shall use the UI font at the UI font size.

*Acceptance:*
- The time row visibly uses a larger font than the date row.
- Changing ThemeService font properties updates the displayed text size.
- No `font.family` strings are hardcoded in QML.

---

## Time & Date Formatting

### REQ-F-005: C++ formatting and QML binding
**Template:** The system shall separate formatting logic from presentation logic.
**Statement:** All time and date strings shall be formatted in C++ by the WidgetManager using `QDateTime` and `QLocale`, then pushed to QML via `setInitialProperties` or property updates; QML shall not perform date/time calculations or formatting.

*Acceptance:*
- The C++ WidgetManager contains all logic to format the time and date strings.
- QML components receive pre-formatted strings (e.g., "14:23:45" or "Sunday, 8 June 2026") and display them without modification.
- No `Date` object or `toLocaleString()` call appears in the clock widget QML.

### REQ-F-006: 24-hour time format
**Template:** The system shall enforce a fixed time format.
**Statement:** The clock widget shall always display time in 24-hour format; there is no configuration option to switch to 12-hour (AM/PM) format.

*Acceptance:*
- A time of 14:30 is displayed as "14:30", never "2:30 PM".
- No config key like `time_format` or `use_12_hour` exists for the clock widget.

### REQ-F-007: Date format customization
**Template:** The system shall support optional override of the default format pattern.
**Statement:** Where a `date_format` configuration key is present in the widget definition, the system shall use that Qt date format pattern instead of the default `dddd, d MMMM yyyy`.

*Acceptance:*
- A widget with `date_format = "yyyy-MM-dd"` displays dates in ISO format (e.g., "2026-06-08").
- A widget with `date_format = "d/M/yy"` displays dates in short format (e.g., "8/6/26").
- The format is applied by calling `QDateTime::toString(dateFormatString)` with the user's specified pattern.

### REQ-F-008: Locale configuration
**Template:** The system shall support localization of date text.
**Statement:** Where a `locale` configuration key is present in the widget definition, the system shall construct a `QLocale` from that BCP-47 language tag and use it to format weekday and month names; if no locale is configured, the system shall use the system default locale.

*Acceptance:*
- A widget with `locale = "de_DE"` displays weekday/month names in German (e.g., "Sonntag, 8 Juni 2026").
- A widget with `locale = "fr_FR"` displays weekday/month names in French (e.g., "Dimanche, 8 Juin 2026").
- A widget with no `locale` key uses the system locale, matching `QLocale::system()` output.

---

## Configuration & Initialization

### REQ-F-009: show_seconds configuration
**Template:** The system shall conditionally include an optional display element.
**Statement:** Where a `show_seconds` configuration key is present and set to `true`, the clock widget shall display seconds inline with the time; where `show_seconds` is `false` or absent and defaults to `true`, the behavior follows the true case; where `show_seconds` is explicitly set to `false`, the widget shall not display seconds.

*Acceptance:*
- A widget with `show_seconds = true` displays "HH:mm:ss" format.
- A widget with `show_seconds = false` displays only "HH:mm" (no seconds shown).
- The default configuration (no `show_seconds` key) behaves as if `show_seconds = true`.

### REQ-F-010: enabled configuration
**Template:** The system shall respect a feature-gating configuration key.
**Statement:** Where an `enabled` configuration key is present on a widget definition and set to `false`, the system shall not create any layer-shell surfaces for that widget definition on any monitor; where `enabled` is `true` or absent (defaulting to `true`), the system shall proceed with normal surface creation and management.

*Acceptance:*
- A widget definition with `enabled = false` creates no surfaces; the widget is invisible and uses no resources.
- A widget definition with `enabled = true` or no `enabled` key creates surfaces per the normal position/monitor configuration.
- No log output or error occurs when a widget is disabled; it is silently skipped.

### REQ-F-011: Invalid date_format fallback
**Template:** If an error condition is detected during parsing, then the system shall recover gracefully.
**Statement:** If the `date_format` configuration value cannot be used to format the current date (e.g., due to invalid Qt format syntax or an empty result), then the system shall fall back to the default pattern `dddd, d MMMM yyyy`, log a warning once per widget definition, and continue operation.

*Acceptance:*
- A widget with `date_format = "invalid%syntax"` falls back to default and logs a single warning (not repeated on each update).
- After fallback, the date displays correctly using the default pattern.
- The widget does not show a blank or error message; it recovers to a usable state.

### REQ-F-012: Invalid locale fallback
**Template:** If a configuration value is invalid or unrecognized, then the system shall use a safe default.
**Statement:** If the `locale` configuration value is not recognized as a valid BCP-47 language tag or cannot be constructed as a `QLocale`, then the system shall fall back to `QLocale::system()`, log a warning, and continue operation.

*Acceptance:*
- A widget with `locale = "xyz_ABC"` (invalid tag) falls back to system locale and logs a warning.
- After fallback, weekday and month names reflect the system locale.
- The widget does not crash or show blank text; it recovers to a usable state.

---

## Timer & Update Behavior

### REQ-F-013: Tick interval with seconds
**Template:** The system shall update the display at regular intervals.
**Statement:** When `show_seconds` is `true`, the WidgetManager shall update the displayed time and date every 1 second, aligned to the wall-clock second boundary (not arbitrary ~1s intervals).

*Acceptance:*
- Seconds advance by exactly 1 with each update (never skip or repeat).
- Updates occur near the instant the wall-clock second changes (within ±100ms).
- No drift accumulates over a period of 10 minutes.

### REQ-F-014: Tick interval without seconds
**Template:** When a configuration option is disabled, the system shall adjust behavior accordingly.
**Statement:** When `show_seconds` is `false`, the WidgetManager shall update the displayed time only when the minute boundary is reached (wall-clock minute changes), not on every second.

*Acceptance:*
- With `show_seconds = false`, the displayed time does not change for ~60 seconds at a time.
- The update occurs within ±100ms of the wall-clock minute boundary.
- CPU and timer resource usage is lower than the 1-second update mode for the same widget.

### REQ-F-015: Timer frozen while hidden
**Template:** While the system is in a particular state, it shall conserve resources.
**Statement:** While all layer-shell surfaces for a clock widget definition are hidden (due to the monitor's visible workspace being occupied by windows), the WidgetManager's timer for that widget shall be stopped and shall not invoke any updates.

*Acceptance:*
- When a monitor's workspace contains windows, CPU usage attributable to that widget's timer drops to zero (no periodic callbacks).
- The timer resumes on the next reveal cycle.
- No queued or deferred updates occur while the timer is frozen.

### REQ-F-016: Time resynchronization on reveal
**Template:** When a system transitions from one state to another, it shall re-establish consistency.
**Statement:** When a clock widget's surfaces are revealed (the monitor's visible workspace becomes empty), the WidgetManager shall immediately fetch the current wall-clock time, reformat and push the updated strings to QML, and resync the timer to the current wall-clock instant (not continue from a stale frozen value).

*Acceptance:*
- If the widget was hidden for 5 minutes and then revealed, the displayed time matches the current wall-clock time within ±1 second (not showing time from 5 minutes ago).
- The resync occurs during the occupy-to-empty transition, before the QML rendering completes.
- If `show_seconds = false`, the resync aligns the timer to the next minute boundary, not the next second.

---

## Surface Lifecycle & Integration

### REQ-F-017: Occupancy gate inheritance
**Template:** The system shall reuse existing infrastructure.
**Statement:** The clock widget shall use the same MonitorOccupancyService gate as the existing time-to-event widget: surfaces are shown only while the monitor's currently-visible workspace is empty and hidden (but kept mapped) otherwise.

*Acceptance:*
- The widget is visible only when the focused workspace on that monitor contains zero windows.
- Moving a window into the previously-empty workspace hides the widget.
- Moving the last window out of the workspace reveals the widget.
- No new visibility or occupancy logic is introduced.

### REQ-F-018: Position and monitor configuration
**Template:** The system shall reuse existing configuration structures.
**Statement:** The clock widget shall support the same `position` (nine fixed anchors: `center-center`, `left-top`, etc.) and `monitors` (list of monitor names, empty = all monitors) configuration keys as existing widget types.

*Acceptance:*
- A widget with `position = "left-top"` appears in the top-left corner of each target monitor.
- A widget with `monitors = ["HDMI-1", "DP-2"]` appears only on those monitors, not on others.
- A widget with no `monitors` key appears on all connected monitors.

### REQ-F-019: Layer-shell surface visibility toggle
**Template:** The system shall enforce a specific implementation pattern for layer-shell surfaces.
**Statement:** The clock widget's layer-shell surface visibility shall be toggled by setting the QML root's `visible` property; the surface shall not be hidden or shown via `QQuickView::hide()` or `QQuickView::show()`.

*Acceptance:*
- The WidgetManager sets `surfaceRootItem->setProperty("visible", false)` to hide (not `view->hide()`).
- The surface remains mapped and retains its layer-shell role during visibility toggles.
- The widget reappears correctly on the next reveal without layer-shell role reattachment.

### REQ-C-001: No new IPC sockets
**Template:** The system shall respect architectural boundaries.
**Statement:** The clock widget implementation shall not open new IPC sockets (D-Bus, Unix sockets, etc.); all required data (current time, locale, system configuration) shall come from in-process sources or existing service connections.

*Acceptance:*
- No new socket creation in the WidgetManager or related classes.
- Time is obtained from `QDateTime::currentDateTime()`, not from a separate service.
- System locale and configuration come from existing ConfigService and ThemeService connections.

### REQ-C-002: ConfigService parser pattern
**Template:** The system shall maintain separation of concerns.
**Statement:** The ConfigService shall parse the clock widget's TOML configuration (extracting `show_seconds`, `date_format`, `locale`, `enabled`, `position`, `monitors`) but shall not inspect or validate monitor names, occupancy state, or coordinate values; validation and application of position/monitor filtering shall occur in the WidgetManager or coordinator.

*Acceptance:*
- ConfigService returns a parsed `WidgetDefinition` struct with all clock-widget fields populated.
- No monitor validation occurs in ConfigService (unknown monitors are passed through).
- The WidgetManager or coordinator filters monitors and validates anchor positions.

### REQ-C-003: QML root visibility pattern
**Template:** The system shall enforce a specific implementation technique.
**Statement:** The clock widget's QML root item (`WidgetSurface.qml`) shall inherit the existing layer-shell hide/remap gotcha constraint: layer-shell surfaces must not be hidden via `QQuickView::hide()`/`show()`; instead, visibility must toggle the root QML item's `visible` property while the `QQuickView` remains mapped.

*Acceptance:*
- No call to `view->hide()` or `view->show()` occurs for clock widget surfaces.
- The C++ WidgetManager calls `surfaceRootItem->setProperty("visible", …)` to control visibility.
- The surface remains on the layer-shell bottom layer during all visibility changes.

---

## Collision & Error Handling

### REQ-F-020: Position collision handling
**Template:** The system shall reuse existing error-recovery patterns.
**Statement:** If a clock widget definition specifies the same `position` value as an earlier widget definition on the same monitor (creating a collision), the system shall apply the existing collision-blocker mechanism: log a warning once per collision pair, and drop the later widget's surface on that monitor (no second surface is created).

*Acceptance:*
- Two widgets with `position = "center-center"` on the same monitor result in a single warning log message.
- Only the first widget's surface is created; the second widget creates no surface on that monitor.
- Other monitors unaffected by the collision create both surfaces normally.
- No crash or user-visible error occurs; the collision is handled gracefully.

### REQ-F-021: Widget initialization error handling
**Template:** If an error occurs during initialization, the system shall fail safely.
**Statement:** If a clock widget definition cannot be initialized (e.g., due to a missing Wayland protocol, unavailable output, or internal error), the system shall log the error with sufficient detail for debugging and continue without crashing; the widget is skipped on that monitor.

*Acceptance:*
- A log message includes the widget name, monitor name, and reason for failure.
- The shell continues to run and other widgets/monitors are unaffected.
- No exception escapes to the event loop.

---

## Performance & Resource Management

### REQ-NF-003: Timer resource efficiency
**Template:** The system shall minimize resource consumption in idle states.
**Statement:** Clock widget timers shall be stopped (not fired) while all surfaces are hidden, ensuring no CPU or timer resource is consumed during the occluded state.

*Acceptance:*
- With a monitor's workspace occupied (all widgets hidden), the WidgetManager's timer callback is not invoked.
- CPU time attributable to the clock widget timer is zero while hidden (measurable via profiling).

### REQ-NF-004: Memory footprint
**Template:** The system shall reuse common infrastructure.
**Statement:** The clock widget shall reuse the existing WidgetManager, layer-shell surface lifecycle, occupancy gate, and timer infrastructure; no duplicate data structures or services are created per widget instance.

*Acceptance:*
- A single WidgetManager instance serves one clock widget definition, independent of the number of monitors.
- No per-monitor duplicate of the WidgetManager or timer is created.
- Memory overhead per clock widget definition is <1 MB.

---

## Acceptance & Testing

### REQ-F-022: Configuration parsing
**Template:** The system shall handle configuration input correctly.
**Statement:** The clock widget's TOML configuration shall be parsed by ConfigService, with all keys (`show_seconds`, `date_format`, `locale`, `enabled`, `position`, `monitors`) populated or defaulted according to the specification; parsing shall not crash on missing optional keys.

*Acceptance:*
- A minimal config `[[widget]]` `type = "clock"` `position = "center-center"` parses successfully with all defaults applied.
- A full config with all keys specified parses successfully and applies all values.
- An invalid key (e.g., `typo_format = "dd/MM"`) is ignored without error.

### REQ-F-023: Multi-monitor independence
**Template:** The system shall handle multiple independent instances.
**Statement:** When a clock widget is configured for multiple monitors via the `monitors` list (or implicitly for all monitors), each monitor's surface shall be independent: visibility, updates, and collision detection shall be per-monitor.

*Acceptance:*
- Hiding the widget on monitor A (due to occupancy) does not affect the widget's visibility on monitor B.
- A collision on one monitor does not prevent the widget from appearing on other monitors.
- Each monitor's timer and displayed time are independent (time is not synchronized across monitors).

### REQ-F-024: Startup and hot-reload
**Template:** The system shall handle configuration changes at runtime.
**Statement:** The clock widget shall support hot-reload: when ConfigService detects a configuration change and emits a config-changed signal, the WidgetManager shall re-parse the widget definition, update timers and display strings, and recreate surfaces if needed (e.g., if `enabled` changed or `position` changed).

*Acceptance:*
- Changing `show_seconds` from `true` to `false` in the config file updates the timer interval and hides seconds on the next update.
- Changing `locale` updates the displayed weekday and month names on the next update.
- Changing `enabled` from `true` to `false` destroys all surfaces for that widget; changing back to `true` recreates them.
- No crash or stale state occurs during hot-reload.
