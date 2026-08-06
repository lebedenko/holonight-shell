# Phase 39 — Notification Rule Model Hardening: Tasks

**Status**: Complete — automated checks and user verification passed.

## Pre-flight

- [x] T-264: Revalidate U-06 I-05 and I-06 against current `main`.
  - Result: `ensureApp()` still appended every unseen non-empty application
    name, persisted rules were not count-bounded, and both notification role
    enums lacked explicit underlying types.

## Implementation and tests

- [x] T-265: Bound live rule discovery and oversized persisted input.
  - REQs: REQ-F-01
  - Check: at most 256 rows are retained; existing rows update at capacity;
    startup keeps the most recently seen rows and persists any reduction.

- [x] T-266: Give both notification role enums explicit 16-bit storage.
  - REQs: REQ-F-02
  - Check: role values and names remain unchanged and notification models
    compile with their existing QML registration.

- [x] T-267: Add focused deterministic regression tests.
  - REQs: REQ-F-01, REQ-F-02
  - Check: insertion and persisted-load boundaries are covered without a live
    compositor or session bus.

## Validation and handoff

- [x] T-268: Run focused and project validation.
  - Result: 18 focused notification rule/service tests passed. `task test`
    built successfully and ran 955 tests: 953 passed, one compositor-screen
    test was skipped, and only the pre-existing
    `KeyboardLayoutService.EventSocketConnectedTriggersDevicesQuery` failed.
    `task architecture-check`, changed-file clang-format, and
    `git diff --check` passed. Repository-wide `task format-check` reports only
    the four pre-existing `HyprlandWorkspaceService.cpp` violations at lines
    56, 232, 257, and 295.
- [x] T-269: Obtain user verification and close the phase.
  - Check: user confirms notification rules and toggles remain functional;
    then record commits and reduce the queued Low-severity backlog from 26 to
    24.
  - Result: user verified notification rules and toggles remain functional.
    `76df990` (`fix: bound notification rule discovery`) implements U-06 I-05
    and I-06; 24 Low-severity candidates remain queued.
