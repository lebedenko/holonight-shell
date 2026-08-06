# Phase 25 — D-Bus Service Registration Diagnostics: Tasks

**Status**: Complete — implementation, automated validation, and user verification passed.

## Pre-flight

- [x] T-186: Revalidate U-01 I-03 against current HEAD.
  - Result: `serviceRegistered()` silently returns `false` for both a missing
    bus interface and an invalid `isServiceRegistered()` reply, whereas the
    other D-Bus operations log these failure classes through
    `lcDbusPropertyClient`.

## Implementation and Tests

- [x] T-187: Emit service-registration diagnostics.
  - REQs: REQ-F-01
  - Files: `libs/holonight-platform/src/DbusPropertyClient.cpp`.
  - Check: log unavailable interfaces and invalid replies without warning on a
    valid negative availability result.

- [x] T-188: Cover service-registration transport diagnostics.
  - REQs: REQ-F-01
  - Files: `tests/test_dbus_property_client.cpp`.
  - Check: QtTest observes a warning naming the queried service when the
    client uses an intentionally disconnected D-Bus connection.

## Validation and Handoff

- [x] T-189: Run focused and project validation.
  - Check: focused platform tests, `task test`, `task format-check`, and
    `git diff --check`.
  - Result: the focused transport-diagnostic test passed through CTest, and
    `task test` passed all 940 tests. The changed C++ files pass direct
    `clang-format --dry-run --Werror`, and `git diff --check` passes.
    `task format-check` reports only four pre-existing violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp` (lines 56, 232,
    257, and 295).

- [x] T-190: Record user verification and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the unscheduled Low
    backlog from 46 to 45 only after acceptance evidence is recorded.
  - Result: user verification passed. `3c291a1` (`fix: log D-Bus service
    registration failures`) implements U-01 I-03; the other 45 Low-severity
    candidates remain queued.
