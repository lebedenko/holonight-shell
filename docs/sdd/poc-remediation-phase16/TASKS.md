# Phase 16 — Power Service Bounds and Cleanup: Tasks

**Status**: Complete — automated checks and user live verification passed.

## Pre-flight

- [x] T-140: Revalidate U-03 I-04/I-06/I-09 against current HEAD.
  - Result: `BrightnessService::computePercent()` remains unclamped;
    `parseProfileNames()` retains its unreachable `QDBusArgument` tail; and
    `idle_threshold_ms_{300'000}` remains an inline default. Existing focused
    test targets cover all three services.

## Implementation and Tests

- [x] T-141: Bound the QML-facing brightness percentage.
  - REQs: REQ-F-01
  - Files: `brightness/BrightnessService.cpp`,
    `tests/test_brightness_service.cpp`.
  - Check: construction and external changes clamp negative and over-maximum
    raw values to 0 and 100 respectively.
  - Result: `computePercent()` now preserves its rounding rule and clamps the
    final UI percentage; focused tests cover both initialization and backend
    signal updates.

- [x] T-142: Remove the unreachable Power Profiles parsing branch.
  - REQs: REQ-F-02
  - Files: `PowerProfilesService.cpp`,
    `tests/test_power_profiles_service.cpp` as needed.
  - Check: supported D-Bus/list forms retain their current capability flags;
    an unsupported value produces no profiles.
  - Result: the redundant trailing `QDBusArgument` path was removed; the
    existing list-form test and a new unsupported-value regression test pass.

- [x] T-143: Name the idle threshold default.
  - REQs: REQ-F-03
  - Files: `idle/IdleService.h` and, only if useful, its existing focused test.
  - Check: the default remains 300,000 ms and setter/signal behavior is
    unchanged.
  - Result: a private `kDefaultIdleThresholdMs` now initializes the property;
    the existing default and setter tests pass unchanged.

## Validation and Handoff

- [x] T-144: Run focused and project validation.
  - Check: focused brightness, power-profile, and idle tests; `task test`,
    `task architecture-check`, `task format-check`, and `git diff --check`.
    Record unrelated pre-existing failures separately.
  - Result: focused CTest selection (37 tests), `task test`, and
    `task architecture-check` passed. `task format-check` still fails only on
    the pre-existing four formatting violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp`; Phase 16 C++ files
    were formatted with `clang-format`.

- [x] T-145: Record acceptance and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the unscheduled Low
    backlog from 58 to 55 only after all acceptance evidence is recorded.
  - Result: user verified the completed phase in a live session. `a93e26e`
    (`fix: complete phase 16 power remediation`) implements U-03 I-04/I-06/I-09;
    the 55 other Low-severity candidates remain queued.
