# Phase 29 — Tray Item Membership Index: Tasks

**Status**: Complete — implementation, automated validation, and user verification passed.

## Pre-flight

- [x] T-206: Revalidate the next Phase 7 candidate against current HEAD.
  - Result: U-02 I-10 remains applicable. `TrayWatcher` uses
    `QStringList::contains()` in registration and three asynchronous D-Bus
    property-fetch completion guards; each is a linear membership check.

## Implementation and Tests

- [x] T-207: Add synchronized indexed membership while retaining the ordered
  registered-items list.
  - REQs: REQ-F-01
  - Files: `libs/holonight-surfaces/src/TrayWatcher.{h,cpp}`.

- [x] T-208: Extend focused tray watcher coverage for the synchronized list
  and key set.
  - REQs: REQ-F-01
  - Files: `tests/test_tray_watcher.cpp`.
  - Check: duplicate registration, individual removal, service removal,
    service counts, and visible list order remain correct.

## Validation and Handoff

- [x] T-209: Run project validation.
  - Check: focused tray watcher tests, `task test`, `task format-check`, and
    `git diff --check`.
  - Result: the focused TrayWatcher CTest selection passed all 11 tests and
    the full CTest run passed all 942 tests. The changed C++ files pass direct
    `clang-format --dry-run --Werror`, and `git diff --check` passes.
    `task format-check` reports only four pre-existing violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp` (lines 56, 232,
    257, and 295).

- [x] T-210: Record user verification and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the queued Low backlog
    only after acceptance evidence is recorded.
  - Result: user verification passed. `bee950b` (`perf: index tray item
    membership`) implements U-02 I-10; the other 40 Low-severity candidates
    remain queued.
