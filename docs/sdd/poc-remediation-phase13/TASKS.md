# Phase 13 — Adaptive Status Popup Sizing: Tasks

**Status**: Accepted

## Pre-flight

- [x] T-118: Revalidate the fixed status-popup sizing path.
  - Check: trace `statusPopupContentSize`, `statusPopupGeometry`, and
    `StatusPopupSurface::ensureSurface`; record current Weather content height
    and the selected monitor's available geometry in a live session.
  - Result: the fixed `statusPopupContentSize` lookup fed geometry before QML
    loaded, while Weather's content area was 37 px shorter than its declared
    content height. The implementation keeps the pre-show sizing point and
    replaces the lookup with a monitor-aware policy.

## Policy and Geometry

- [x] T-119: Replace the fixed per-ID size lookup with a central popup sizing
  policy.
  - REQs: REQ-F-01, REQ-F-05
  - Files: `StatusPopupGeometry.h/.cpp` and focused geometry tests.
  - Check: policies define min/preferred/max dimensions and intended overflow
    mode for Weather, Audio, Network, Battery, and fallback.
  - Result: `StatusPopupSizePolicy` now declares minimum, preferred, and
    maximum content bounds plus the intended overflow mode for each status
    popup kind.

- [x] T-120: Make status-popup geometry monitor-aware in both dimensions.
  - REQs: REQ-F-02, REQ-F-04
  - Files: `StatusPopupGeometry.h/.cpp`, `StatusPopupSurface.cpp`, geometry
    tests.
  - Check: resolved surface includes visual padding, remains inside the target
    safe area, and preserves horizontal pointer constraints at screen edges.
  - Result: geometry now receives the selected screen and available rectangles,
    reserves bar/gap/edge space vertically, and keeps the complete glow-padded
    surface inside horizontal screen edges.

- [x] T-121: Select Weather's preferred frame so its existing composition fits
  on a roomy monitor.
  - REQs: REQ-F-03, REQ-F-04
  - Files: central policy and only necessary Weather QML/test adjustments.
  - Check: no Weather scroll interaction at preferred height; capped sizes keep
    the Phase 12 viewport reachable.
  - Result: Weather's preferred content height is 960 px. Its existing
    Flickable becomes non-interactive when that room is available and remains
    the overflow path after a monitor-height cap.

## Verification and Handoff

- [x] T-122: Add deterministic geometry and Weather overflow coverage.
  - REQs: REQ-F-01 through REQ-F-05
  - Check: tests cover preferred and capped frames, pointer clamping, and both
    Weather viewport states.
  - Result: geometry tests cover policy selection, roomy and constrained
    monitors, horizontal edge placement, and a non-primary screen. QML tests
    cover both scrollable and non-scrollable Weather viewport states.

- [x] T-123: Run focused and project validation.
  - Check: affected C++/QML tests, `task test`, `task qml-lint`,
    `task qmltypes-check`, `task architecture-check`, and `git diff --check`
    pass.
  - Result: `task test` passed 924 tests; `task qml-lint`,
    `task qmltypes-check`, `task architecture-check`, and `git diff --check`
    passed. `task format-check` still reports four pre-existing violations in
    `HyprlandWorkspaceService.cpp`; all Phase 13 C++ files pass a targeted
    `clang-format --dry-run --Werror` check.

- [x] T-124: Perform live multi-monitor popup verification and record
  acceptance.
  - Steps:
    1. Open Weather on a roomy display and confirm all fixed content fits
       without scrolling.
    2. Open Weather on a vertically constrained display (or equivalent test
       setup), scroll to the final row, and confirm the frame stays on-screen.
    3. Open Audio and Network while their lists change; confirm their outer
       frames remain stable and list overflow remains usable.
  - Result: user verified the implementation works as expected in the live
    session. Weather uses its preferred frame without unnecessary scrolling,
    and the adaptive popup behavior is accepted.
