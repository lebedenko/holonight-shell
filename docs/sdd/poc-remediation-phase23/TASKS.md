# Phase 23 — Single-Lookup Battery Property Parsing: Tasks

**Status**: Complete — implementation, automated validation, and user verification passed.

## Pre-flight

- [x] T-176: Revalidate U-01 I-05 against current HEAD.
  - Result: `batteryStateUpdateFromProperties()` calls `contains()` and then
    `value()` for each of its eight supported UPower keys. The current
    `tests/test_battery_state.cpp` suite covers complete, missing, invalid,
    state, and threshold parsing outcomes.

## Implementation and Tests

- [x] T-177: Use a single `QVariantMap` lookup for each supported property.
  - REQs: REQ-F-01
  - Files: `libs/holonight-core/src/BatteryState.cpp`.
  - Check: retain all conversions, UPower state mappings, and absent-property
    behavior while replacing paired `contains()`/`value()` accesses.
  - Result: each of the eight supported keys now uses `constFind()` and its
    resulting iterator for both presence and value access. All conversions and
    state/threshold handling are unchanged.

- [x] T-178: Confirm parsing behavior remains covered.
  - REQs: REQ-F-01
  - Files: `tests/test_battery_state.cpp` only if a coverage gap is found.
  - Check: focused battery-state tests preserve complete, partial, invalid,
    and threshold parsing behavior.
  - Result: no coverage gap was found. The existing focused suite exercises
    each parsing branch through observable `BatteryStateUpdate` values, so no
    implementation-detail test was added.

## Validation and Handoff

- [x] T-179: Run focused and project validation.
  - Check: focused battery-state tests, `task test`, `task format-check`, and
    `git diff --check`.
  - Record unrelated pre-existing failures separately.
  - Result: rebuilt focused `BatteryState.*` coverage passed all 16 tests, and
    `task test` passed all 938 tests. `BatteryState.cpp` passes
    `clang-format --dry-run --Werror` and `git diff --check` passes.
    `task format-check` reports only the four pre-existing violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp` (lines 56, 232,
    257, and 295).

- [x] T-180: Record user verification and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the unscheduled Low
    backlog from 48 to 47 only after acceptance evidence is recorded.
  - Result: user verification passed. `c12f9dc` (`perf: avoid duplicate
    battery property lookups`) implements U-01 I-05; the other 47
    Low-severity candidates remain queued.
