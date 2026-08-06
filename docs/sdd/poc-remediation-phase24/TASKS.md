# Phase 24 — Precise Workspace Model Role Notifications: Tasks

**Status**: Complete — implementation, automated validation, and user verification passed.

## Pre-flight

- [x] T-181: Revalidate U-01 I-01 against current HEAD.
  - Result: `emitRowsChanged()` emits a full model row range with an empty
    role list for same-count snapshots, focus, occupancy, and urgency changes.
    The latter three paths change only `WorkspaceStateRole`.

## Implementation and Tests

- [x] T-182: Carry changed-role lists through `WorkspaceModel` notifications.
  - REQs: REQ-F-01
  - Files: `libs/holonight-core/src/WorkspaceModel.cpp`,
    `libs/holonight-core/src/WorkspaceModel.h`.
  - Check: state-only changes report `WorkspaceStateRole`; same-count batch
    updates explicitly report all roles while retaining their row range.

- [x] T-183: Cover the notification role contract.
  - REQs: REQ-F-01
  - Files: `tests/test_workspace_model.cpp`.
  - Check: QSignalSpy observes state-only and full-snapshot role lists.

## Validation and Handoff

- [x] T-184: Run focused and project validation.
  - Check: focused workspace-model tests, `task test`, `task format-check`,
    and `git diff --check`.
  - Result: the focused `WorkspaceModelTest.*` suite passed all 42 tests;
    `task test` passed all 939 tests; the changed C++ files pass direct
    `clang-format --dry-run --Werror`; and `git diff --check` passes.
    `task format-check` reports only four pre-existing violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp` (lines 56, 232,
    257, and 295).

- [x] T-185: Record user verification and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the unscheduled Low
    backlog from 47 to 46 only after acceptance evidence is recorded.
  - Result: user verification passed. `cbcefee` (`perf: narrow workspace
    model role notifications`) implements U-01 I-01; the other 46
    Low-severity candidates remain queued.
