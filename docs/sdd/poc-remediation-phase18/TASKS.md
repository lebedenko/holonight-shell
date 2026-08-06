# Phase 18 — Power Profiles Signal Lifecycle: Tasks

**Status**: Complete — automated checks and user live verification passed.

## Pre-flight

- [x] T-151: Revalidate U-03 I-07 against current HEAD.
  - Result: `PowerProfilesService::initFromService()` still calls
    `connectSignal(..., PropertiesChanged, ...)` on every initialization;
    `clearState()` erases the endpoint without calling the available
    `DbusPropertyClient::disconnectSignal()` wrapper. The current focused fake
    can be extended to record both calls.

## Implementation and Tests

- [x] T-152: Track and disconnect the active `PropertiesChanged` subscription.
  - REQs: REQ-F-01
  - Files: `PowerProfilesService.cpp`, `PowerProfilesService.h`.
  - Check: daemon loss and endpoint reinitialization each leave no stale
    subscription; failed connection remains untracked.
  - Result: `properties_changed_connected_` records only a successful signal
    connection. `disconnectPropertiesChanged()` removes that exact subscription
    before both reinitialization and endpoint-state clearing.

- [x] T-153: Add deterministic reconnect lifecycle coverage.
  - REQs: REQ-F-01
  - Files: `tests/test_power_profiles_service.cpp`.
  - Check: a simulated reinitialization balances old disconnect/new connect;
    failed connection does not produce an unmatched disconnect.
  - Result: the focused fake records subscriptions; a direct watcher-signal
    simulation verifies daemon loss disconnects the old subscription and
    registration establishes exactly one replacement. The failed-connection
    test verifies no unmatched disconnect is issued.

## Validation and Handoff

- [x] T-154: Run focused and project validation.
  - Check: focused Power Profiles tests; `task test`,
    `task architecture-check`, `task format-check`, and `git diff --check`.
  - Record unrelated pre-existing failures separately.
  - Result: the focused CTest selection passed 12 tests; `task test` passed
    all 934 tests; and `task architecture-check` passed. `task format-check`
    reports only the four pre-existing formatting violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp`; all Phase 18 C++
    files were formatted with `clang-format`. `git diff --check` passed.

- [x] T-155: Record live acceptance and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the unscheduled Low
    backlog from 53 to 52 only after all acceptance evidence is recorded.
  - Result: user verified the completed phase in a live session. `6ec9872`
    (`fix: complete phase 18 power profiles remediation`) implements U-03
    I-07; the 52 other Low-severity candidates remain queued.
