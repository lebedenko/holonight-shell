# Phase 17 — Power Process and Sysfs Input Hardening: Tasks

**Status**: Complete — automated checks and user live verification passed.

## Pre-flight

- [x] T-146: Revalidate the proposed U-03 scope against current HEAD.
  - Result: U-03 I-01 is already implemented in `BatteryService::setPercent()`
    and covered by regression tests, so it is excluded. Bare two-second
    subprocess limits remain in `SuspendInhibitorService.cpp` (two sites) and
    `LogindSessionResolver.cpp` (one site); `SysfsBackend.cpp` still ignores
    extraction status for `max_brightness` and `brightness`.

## Implementation and Tests

- [x] T-147: Name the two-second subprocess timeout policies.
  - REQs: REQ-F-01
  - Files: `SuspendInhibitorService.cpp`, `LogindSessionResolver.cpp`.
  - Check: both `busctl` paths and the `loginctl` fallback retain 2,000 ms
    behavior with no bare timeout literal at the cited sites.
  - Result: `kBusctlTimeoutMs` covers the synchronous and asynchronous busctl
    paths, and `kLoginctlTimeoutMs` documents the loginctl fallback.

- [x] T-148: Reject failed sysfs numeric extraction.
  - REQs: REQ-F-02
  - Files: `brightness/SysfsBackend.cpp` and focused tests as needed.
  - Check: malformed max/current brightness inputs take the documented safe
    paths, while valid integer behavior remains unchanged.
  - Result: the brightness-local `readSysfsInteger()` helper rejects unreadable
    and malformed files before device selection or current-value use; focused
    tests cover valid, malformed, and missing files.

## Validation and Handoff

- [x] T-149: Run focused and project validation.
  - Check: focused brightness, inhibitor, and logind resolver tests; `task
    test`, `task architecture-check`, `task format-check`, and `git diff
    --check`. Record unrelated pre-existing failures separately.
  - Result: the focused CTest selection passed 24 tests; `task test` passed
    all 933 tests; and `task architecture-check` passed. `task format-check`
    still reports only the four pre-existing formatting violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp`; all Phase 17 C++
    files were formatted with `clang-format`.

- [x] T-150: Record live acceptance and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the unscheduled Low
    backlog from 55 to 53 only after all acceptance evidence is recorded.
  - Result: user verified the completed phase in a live session. `b0c5635`
    (`fix: complete phase 17 power hardening`) implements U-03 I-03/I-10;
    the 53 other Low-severity candidates remain queued.
