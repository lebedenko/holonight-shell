# Phase 27 — Tray Pixmap Decode Efficiency: Tasks

**Status**: Complete — implementation, automated validation, and user verification passed.

## Pre-flight

- [x] T-196: Revalidate the next Phase 7 candidate against current HEAD.
  - Result: U-02 I-03 was already addressed by Phase 8's `TooltipGeometry`
    extraction, so this phase advances to the next still-open ranked candidate,
    U-02 I-06. `decodePixmapList()` still constructs candidate strings with
    disabled info logging and calls `QImage::setPixel()` for every decoded
    pixel.

## Implementation and Tests

- [x] T-197: Write validated pixmap pixels directly to image scanlines and gate
  candidate diagnostics on info logging.
  - REQs: REQ-F-01
  - Files: `libs/holonight-surfaces/src/TrayItem.cpp`.

- [x] T-198: Add row-major decode regression coverage.
  - REQs: REQ-F-01
  - Files: `tests/test_tray_item.cpp`.
  - Check: a two-row ARGB fixture retains all four coordinates and channels.

## Validation and Handoff

- [x] T-199: Run focused and project validation.
  - Check: focused surfaces tests, `task test`, `task format-check`, and
    `git diff --check`.
  - Result: the focused `TrayItem` CTest selection passed all 8 tests and
    `task test` passed all 942 tests. The changed C++ files pass direct
    `clang-format --dry-run --Werror`, and `git diff --check` passes.
    `task format-check` reports only four pre-existing violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp` (lines 56, 232,
    257, and 295).

- [x] T-200: Record user verification and update the Phase 7 handoff.
  - Check: cite the implementation commit and reconcile the queued Low backlog
    only after acceptance evidence is recorded.
  - Result: user verification passed. `471209e` (`perf: streamline tray pixmap
    decoding`) implements U-02 I-06. During this closeout, U-02 I-03 was also
    reconciled as already accepted through Phase 8's tooltip-geometry
    extraction; the other 42 Low-severity candidates remain queued.
