# Phase 8 — Correctness Remediation: Tasks

**Status**: Complete

## Pre-flight

- [x] T-080: Revalidate the six Phase 7 citations against implementation HEAD.
  - Check: record any line/path drift in the implementing commit; stop for SDD
    update if the behavior or scope has materially changed.

## Implementation and Tests

- [x] T-081: Implement bounded EOF-delimited control-command collection.
  - REQs: REQ-F-01
  - Files: `apps/shell/app/ControlServer.{h,cpp}`, `tests/test_control_server.cpp`
  - Check: fragmented valid command dispatches once after disconnect; oversized
    command dispatches zero times.

- [x] T-082: Clamp battery percentage at `BatteryService::setPercent()`.
  - REQs: REQ-F-02
  - Files: `libs/holonight-services/src/BatteryService.cpp`, battery/low-battery tests
  - Check: values below 0 and above 100 cannot enter the service or notification state machine.

- [x] T-083: Namespace calendar transient state by provider type and account name.
  - REQs: REQ-F-03
  - Files: `libs/holonight-services/src/calendar/CalendarSyncManager.{h,cpp}`, `tests/test_calendar_integration.cpp`
  - Check: same-named CalDAV and ICS accounts do not suppress, reset, or back off one another.

- [x] T-084: Correct tooltip screen-local anchoring and add pure geometry coverage.
  - REQs: REQ-F-04
  - Files: `libs/holonight-surfaces/src/TooltipSurface.cpp`, helper/test files as designed, surfaces CMake list if a new helper is added
  - Check: offset-screen anchors are translated before centering; primary-screen output is unchanged.

- [x] T-085: Enforce settings numeric ranges at model setters.
  - REQs: REQ-F-05
  - Files: `apps/settings/src/SettingsEditModel.cpp`, `tests/test_settings_app.cpp`
  - Check: below/above bounds clamp to the table in SPEC.md and in-range values stay unchanged.

- [x] T-086: Gate both settings save actions on dirty state.
  - REQs: REQ-F-06
  - Files: `apps/settings/qml/FooterBar.qml`, focused QML test if required
  - Check: both controls are disabled when clean or saving and retain the shared `fileService.save()` action.

## Validation and Handoff

- [x] T-087: Run targeted test executables while each area is changed.
  - Check: `test_holonight_app`, `test_holonight_services`, `test_holonight_surfaces`, and `test_holonight_settings` pass for their affected tests.

- [x] T-088: Run project validation.
  - Check: `task format-check`, `task tidy`, `task test`, `task qml-lint`, `task qmltypes-check`, and `task architecture-check` pass or pre-existing failures are documented with evidence.

- [x] T-089: Perform manual multi-monitor tooltip verification in a Hyprland session.
  - Prerequisite: two horizontally arranged monitors with different x origins and a tray item/tooltips available on the non-primary monitor.
  - Steps:
    1. Run the shell and open a tooltip near the left, center, and right of the secondary monitor.
    2. Confirm each tooltip is centered on its triggering anchor unless constrained by the screen edge.
    3. Confirm no tooltip appears displaced by the primary monitor's width.
  - Failure modes: a constant horizontal offset, wrong-monitor placement, or clipping at the secondary monitor's edges.
  - Result: Passed in a live Hyprland session on both monitors; tooltip placement was correct on the primary and secondary monitor.

- [x] T-090: Update the Phase 7 handoff status after all six requirements pass.
  - Check: report each source item as fixed with commit/test evidence; leave unimplemented Phase 7 items untouched and queued.
  - Result: U-02 I-07/I-08, U-03 I-01, U-07 I-02, and U-11 I-C1/I-Q3 were fixed in `daef305` (`fix: remediate phase 8 correctness findings`). T-087 and T-088 completed the targeted and project validation; all other Phase 7 candidates remain queued.
