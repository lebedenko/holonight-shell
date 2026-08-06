# Phase 45 — Weather and Calendar Data-Path Cleanup: Tasks

- [x] T-299: Reconcile the six remaining Phase 7 findings and select the three
  cohesive U-07 data-path items for Phase 45.
- [x] T-300: Preserve structured HTTP authentication failure information from
  `WeatherProvider` through `WeatherService` and suppress automatic retry for
  rejected credentials.
- [x] T-301: Cache hourly and daily QML variant lists at forecast replacement
  boundaries.
- [x] T-302: Replace sequential VEVENT property-name comparisons with a static
  lookup table and exhaustive dispatch.
- [x] T-303: Add focused weather regression coverage and run existing complete
  calendar-field parsing coverage.
  - Result: 26 focused weather/provider/calendar tests pass.
- [x] T-304: Run formatting, architecture checks, and `task test`; document any
  pre-existing failures precisely.
  - Result: all Phase 45 files pass `clang-format --dry-run --Werror`, the
    architecture check passes, and the full suite runs 960 tests with only the
    established `KeyboardLayoutService.EventSocketConnectedTriggersDevicesQuery`
    failure; the compositor-dependent sidebar test remains skipped. The
    repository-wide format target remains blocked by four pre-existing
    violations in `HyprlandWorkspaceService.cpp`.
- [x] T-305: Manually verify ordinary weather refresh and calendar display.
  - Result: accepted by the user after confirming the affected weather and
    calendar paths work correctly.
- [x] T-306: After acceptance, record implementation/documentation commits and
  reduce the queued Phase 7 backlog from three to zero.
  - Result: implementation commit `564fba7` (`refactor: tighten weather and
    calendar data paths`) completes U-07 I-07/I-08/I-09. This documentation
    closeout records the accepted three-item reduction; no Phase 7
    Low-severity candidates remain queued.
