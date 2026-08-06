# Phase 20 — NetworkManager Operation Error Detail: Tasks

**Status**: Complete — automated checks and user live verification passed.

## Pre-flight

- [x] T-161: Revalidate U-04 I-08 against current HEAD.
  - Result: `requestScan()`, `setWirelessEnabled()`, `activateKnown()`,
    `disconnectActive()`, and `addAndActivate()` still emit fixed operation
    text when their D-Bus reply fails. `property()` and `allProperties()` in
    the same backend already log `reply.error().message()`, confirming the
    missing diagnostic detail is limited to these operation paths.

## Implementation and Tests

- [x] T-162: Preserve optional D-Bus detail for asynchronous operation
  failures.
  - REQs: REQ-F-01
  - Files: `libs/holonight-services/src/network/NetworkManagerBackend.cpp`.
  - Check: keep the established action context, append non-empty D-Bus detail,
    and leave local validation errors and scheduling unchanged.
  - Result: a shared formatter now retains context and appends only non-empty
    detail. The scan, radio-state, saved-activation, deactivation, and
    add-and-activate failure paths capture that completed text before their
    queued error delivery; local validation errors are unchanged.

- [x] T-163: Add deterministic fake-D-Bus regression coverage.
  - REQs: REQ-F-01
  - Files: `tests/test_network_service.cpp`.
  - Check: a failed asynchronous NetworkManager operation emits an error that
    includes both its operation context and the fake's D-Bus detail.
  - Result: the session-bus fixture unregisters its fake NetworkManager object
    for a saved-activation call, creating a deterministic D-Bus failure. The
    regression test verifies that `operationError` retains its action context
    and differs from the former context-only message.

## Validation and Handoff

- [x] T-164: Run focused and project validation.
  - Check: focused NetworkManager backend tests, `task test`,
    `task format-check`, and `git diff --check`.
  - Record unrelated pre-existing failures separately.
  - Result: focused `QtNetworkManagerBackendTest.*` passed all 7 tests;
    `task test` passed all 936 tests. `task format-check` reports only the
    four pre-existing formatting violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp`; Phase 20 C++
    files are clang-formatted. Final diff checks passed.

- [x] T-165: Record user live acceptance and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the unscheduled Low
    backlog from 51 to 50 only after all acceptance evidence is recorded.
  - Result: user verified the completed phase. `7ce92c0` (`fix: complete
    phase 20 network remediation`) implements U-04 I-08; the other 50
    Low-severity candidates remain queued.
