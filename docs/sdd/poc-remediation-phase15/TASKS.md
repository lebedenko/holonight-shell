# Phase 15 — Active Window State Hygiene: Tasks

**Status**: Complete — automated checks and user live verification passed.

## Pre-flight

- [x] T-133: Revalidate U-04 I-02/I-03/I-06 against current HEAD and grep
  all `focusedMonitor()` callers.
  - Result: all three candidates remained; `NotificationService` was the one
    checked-in `focusedMonitor()` caller and was migrated to the canonical API.

## Implementation and Tests

- [x] T-134: Reconcile removed monitors after complete snapshots.
  - REQs: REQ-F-01
  - Files: `ActiveWindowService.{h,cpp}` and focused tests.
  - Check: removal clears state and emits existing monitor/workspace changes;
    incomplete snapshots do not alter state.

- [x] T-135: Retire the duplicate focused-monitor accessor.
  - REQs: REQ-F-02
  - Files: `ActiveWindowService.{h,cpp}` and affected callers/tests.
  - Check: property and invokable behavior use `focusedMonitorName()`.

- [x] T-136: Share desktop-entry parsing between category and matching paths.
  - REQs: REQ-F-03
  - Files: `ActiveWindowService.cpp` and focused tests.
  - Check: matching and categories preserve current semantics for valid and
    malformed desktop files.

## Validation and Handoff

- [x] T-137: Run focused and project validation.
  - Automated results: `task test`, `task architecture-check`, and
    `git diff --check` passed. `task format-check` still fails only on the
    pre-existing formatting drift in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp`; all files changed
    by this phase were formatted with `clang-format`.
  - Check: focused service tests, `task test`, `task architecture-check`,
    `task format-check`, and `git diff --check` pass. Record unrelated
    pre-existing failures separately.

- [x] T-138: Perform live Hyprland monitor lifecycle verification.
  - Steps:
    1. Start the shell with two monitors and open an application on each.
    2. Disconnect one monitor or otherwise remove it from Hyprland.
    3. Confirm no remaining widget shows its title/category/workspace.
    4. Reconnect it and confirm the new snapshot repopulates correctly.
  - Result: user verified the phase in a live Hyprland session.

- [x] T-139: Record acceptance and update the Phase 7 handoff.
  - Check: cite implementation commit and reduce the unscheduled Low backlog
    from 61 to 58 once all acceptance evidence is recorded.
  - Result: `e7c81cd` (`fix: complete phase 15 active window remediation`)
    implements the three accepted U-04 findings; the 58 other Low-severity
    candidates remain queued.
