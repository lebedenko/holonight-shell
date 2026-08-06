# SDD Tasks — clock-widget

- [x] T-001: ConfigService data model — Clock type, ClockConfig, generic `enabled`
  - REQs: REQ-F-006, REQ-F-009, REQ-F-010, REQ-C-002
  - Check: `src/core/ConfigService.h` adds `WidgetType::Clock`, a `ClockConfig { bool show_seconds{true}; QString date_format{}; QString locale{}; }` struct (all default member inits, `operator==` defaulted), and `WidgetDefinition` gains `bool enabled{true}` plus an embedded `ClockConfig clock`.

- [x] T-002: ConfigService parsing — `parseClockFields()` + `parseWidgetEntry()` wiring
  - REQs: REQ-F-007, REQ-F-008, REQ-F-022, REQ-C-002
  - Check: `ConfigService.cpp` has `parseClockFields()` reading `show_seconds`/`date_format`/`locale` (never nullopt); `parseWidgetEntry()` reads `enabled` before the type switch and assigns it on both the `time-to-event` and new `type == "clock"` branches, the clock branch assembling a full `WidgetDefinition`.

- [x] T-003: `WidgetClock.h/.cpp` pure formatters
  - REQs: REQ-F-001, REQ-F-002, REQ-F-005, REQ-F-006, REQ-F-011, REQ-F-013, REQ-F-014
  - Check: `src/surfaces/WidgetClock.{h,cpp}` implement `formatClockTime` (`HH:mm`), `formatClockSeconds` (two-digit or empty when `show_seconds` false), `formatClockDate` (custom-or-default `"dddd, d MMMM yyyy"` pattern, `QLocale`-or-system, invalid-pattern fallback + warn-once via `bool&`), and `clockTickIntervalMs` (1000 vs ms-to-next-minute).

- [x] T-004: Unit test `tests/test_widget_clock.cpp`
  - REQs: REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014
  - Check: New test (listed in `tests/CMakeLists.txt`, `task configure-tests` run) covers seconds on/off, default vs custom `date_format`, invalid `date_format` fallback, invalid/empty `locale` fallback, and tick interval 1s vs minute-aligned; `task test` shows it compiled and passed.

- [x] T-005: WidgetManager branching on `definition_.type`
  - REQs: REQ-F-005, REQ-F-015, REQ-F-016, REQ-F-020, REQ-NF-003
  - Check: `WidgetManager` gains `clock_time_text_`/`clock_seconds_text_`/`clock_date_text_`/`format_warned_` and `recomputeClockStrings()`/`widgetLabel()`; `qmlSource`, `recomputeAndPropagate`, `startTickTimer`, `updateTimerState` (no stop-on-past for clock), and `applyVisibility` (recompute-before-reveal) all branch on `WidgetType::Clock`; collision log uses `widgetLabel()`.

- [x] T-006: `WidgetSurface.qml` clock dispatch
  - REQs: REQ-F-005
  - Check: `WidgetSurface.qml` root declares plain `timeText`/`secondsText`/`dateText` (empty-string defaults) and a `Loader { active: widgetType === "clock" }` instantiating `ClockWidget` with those three strings passed through.

- [x] T-007: `ClockWidget.qml` body
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-009, REQ-NF-001, REQ-NF-002
  - Check: A centered `ColumnLayout` renders a time row (`MultiEffect` glow declared before a baseline-aligned `Row` of two `StableDigitsText`: big `HH:mm` at `clockFontSize*2`, small seconds at half size, `visible` bound to `secondsText.length > 0`) above a centered date `Text`; only `HoloniightPalette` tokens and `ThemeService` fonts, zero hardcoded colors/families.

- [x] T-008: Build wiring — CMake + `enabled` skip
  - REQs: REQ-F-005, REQ-F-010
  - Check: `CMakeLists.txt` adds `ClockWidget.qml` to `HOLONIGHT_QML_FILES` and `WidgetClock.{h,cpp}` to sources; `ShellApplication::rebuildWidgets()` skips `enabled == false` definitions before constructing a `WidgetManager`.

- [x] T-009: Build + static checks
  - REQs: REQ-F-005, REQ-NF-001, REQ-NF-002
  - Check: `task build`, `task qml-lint`, `task tidy`, and `task format-check` all pass with no new failures attributable to files touched in this cycle.

- [x] T-010: Manual visual verification — happy path + occupancy
  - REQs: REQ-F-001, REQ-F-003, REQ-F-004, REQ-F-016, REQ-F-017
  - Check: On the live Wayland session with a sample `[[widget]] type="clock"`, the clock renders on an empty workspace with glow, seconds visibly half the time height, and a centered date row; opening a window hides it; closing it reveals it showing the current (not stale) time.

- [x] T-011: Manual verification — config options
  - REQs: REQ-F-007, REQ-F-008, REQ-F-009, REQ-F-010
  - Check: `show_seconds=false` hides seconds (HH:mm only); `date_format="yyyy-MM-dd"` renders ISO date; `locale="de_DE"` renders German weekday/month names; `enabled=false` creates no surface on any monitor with no error logs.
