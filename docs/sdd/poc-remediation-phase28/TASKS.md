# Phase 28 — Per-Monitor View Lookup: Tasks

**Status**: Complete — implementation, automated validation, and user verification passed.

## Pre-flight

- [x] T-201: Revalidate the next Phase 7 candidate against current HEAD.
  - Result: U-02 I-09 remains applicable. `WidgetManager::applyVisibility()`
    runs for every occupancy change and its lookup linearly scans live surfaces
    comparing monitor names.

## Implementation and Tests

- [x] T-202: Add a lifecycle-owned monitor-name index and shared direct view
  lookup.
  - REQs: REQ-F-01
  - Files: `libs/holonight-surfaces/src/PerMonitorLayerManager.{h,cpp}` and
    `libs/holonight-surfaces/src/WidgetManager.{h,cpp}`.

- [x] T-203: Run focused widget/surfaces regression coverage.
  - REQs: REQ-F-01
  - Check: existing widget-surface policy and widget-clock/countdown tests
    continue to pass; missing-monitor handling remains a nullable lookup.
  - Result: the focused WidgetSurfacePolicy, WidgetCountdown, and WidgetClock
    CTest selection passed all 30 tests.

## Validation and Handoff

- [x] T-204: Run project validation.
  - Check: focused surfaces tests, `task test`, `task format-check`, and
    `git diff --check`.
  - Result: `task test` passed all 942 tests. The changed C++ files pass direct
    `clang-format --dry-run --Werror`, and `git diff --check` passes.
    `task format-check` reports only four pre-existing violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp` (lines 56, 232,
    257, and 295).

- [x] T-205: Record user verification and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the queued Low backlog
    only after acceptance evidence is recorded.
  - Result: user verification passed. `e230597` (`perf: index per-monitor
    views`) implements U-02 I-09; the other 41 Low-severity candidates remain
    queued.
