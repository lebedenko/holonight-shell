# Phase 40 — Top-Bar QML Contract Cleanup: Tasks

**Status**: Complete — automated checks and user verification passed.

## Pre-flight

- [x] T-270: Revalidate U-08 I-02, I-06, and I-07 against current `main`.
  - Result: all 12 `BarSection` subclasses override `implicitWidth`; all 13
    `BarTooltipArea` instances provide `barMonitorName`; and no arrow behavior
    consumes the enabled hover tracking.

## Implementation and tests

- [x] T-271: Remove the dead `BarSection` implicit-width fallback.
  - REQs: REQ-F-01
  - Check: each current concrete section retains its explicit width policy.

- [x] T-272: Make tooltip monitor ownership a required QML contract.
  - REQs: REQ-F-02
  - Check: every current tooltip instance compiles with an explicit monitor
    binding.

- [x] T-273: Remove unused workspace-arrow hover tracking and pin the behavior.
  - REQs: REQ-F-03
  - Check: the pointer area has hover tracking disabled while click activation
    remains covered.

## Validation and handoff

- [x] T-274: Run focused and project validation.
  - Check: focused workspace-arrow QML tests, `task qml-lint`,
    `task qmltypes-check`, `task test`, `task architecture-check`, and
    `git diff --check`.
  - Result: all 8 focused workspace-arrow QML cases passed. `task qml-lint`,
    `task qmltypes-check`, `task architecture-check`, and `git diff --check`
    passed. `task test` built successfully and ran 955 tests; the only failure
    was the known pre-existing
    `KeyboardLayoutService.EventSocketConnectedTriggersDevicesQuery` test, and
    the compositor-screen test retained its expected skip.

- [x] T-275: Obtain user verification and close the phase.
  - Check: user confirms top-bar sizing, tooltip routing, and workspace-arrow
    clicks remain functional in a live Hyprland session; then record commits
    and reduce the queued Low-severity backlog from 24 to 21.
  - Result: user verified top-bar sizing, tooltip routing, and workspace-arrow
    clicks in the live session. `61b009a` (`refactor: tighten topbar qml
    contracts`) implements U-08 I-02, I-06, and I-07; 21 Low-severity
    candidates remain queued.
