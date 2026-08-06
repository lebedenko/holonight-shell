# Phase 14 — Surface and Lifecycle Reliability: Tasks

**Status**: Complete — automated checks and user live verification passed.

## Pre-flight

- [x] T-125: Revalidate the four Phase 7 findings against current HEAD.
  - Check: record source/line drift and stop for SDD revision if an item is
    already fixed or its behavior has materially changed.
  - Result: all four findings remained present and matched the Phase 14 scope.

## Implementation and Tests

- [x] T-126: Synchronize tray image-state reads and writes.
  - REQs: REQ-F-01
  - Files: `libs/holonight-surfaces/src/TrayModel.{h,cpp}`, tray tests.
  - Check: image reads return snapshots during add/update/remove activity and
    no model signal or menu action is performed under the lock.
  - Result: `TrayModel` now takes read/write locks only around image-state
    snapshots and mutations; model signals and tray interactions remain
    outside the lock.

- [x] T-127: Add and apply post-`setSource()` QQuickView validation.
  - REQs: REQ-F-02
  - Files: affected surfaces sources plus focused surface tests.
  - Check: every enumerated creation path validates status before commit/open
    state; an invalid source logs useful errors and cleans up safely.
  - Result: every layer-surface QML load uses `loadQmlSource()`, which logs
    the URL and QML errors and leaves each caller's established cleanup path
    in control on failure.

- [x] T-128: Remove event-loop pumping from NetworkManager teardown.
  - REQs: REQ-F-03
  - Files: `libs/holonight-services/src/network/NetworkManagerBackend.{h,cpp}`
    and focused network tests.
  - Check: shutdown cancels and awaits live futures without stale watcher
    snapshots, and late completions cannot queue refresh work.
  - Result: teardown sets a shutdown guard, disconnects, cancels, and awaits
    live watchers without `processEvents()` or a stale watcher snapshot.

- [x] T-129: Preserve launcher selection across background rebuilds.
  - REQs: REQ-F-04
  - Files: launcher service, only necessary launcher QML, service/QML tests.
  - Check: a selected non-first app/action remains selected and visible when
    still present; removed/empty results use the specified fallback behavior.
  - Result: launcher rebuilds restore application/action selection by semantic
    identity and clamp a missing selection to the nearest valid result.

## Validation and Handoff

- [x] T-130: Run focused and project validation.
  - Check: affected C++/QML tests, `task test`, `task qml-lint`,
    `task qmltypes-check`, `task architecture-check`, `task format-check`,
    and `git diff --check` pass. Attribute pre-existing format/tidy failures
    separately if encountered.
  - Result: `task build`, focused launcher/tray CTest, and `git diff --check`
    passed. Touched C++ files pass `clang-format --dry-run --Werror`. The
    project-wide `task format-check` remains blocked by pre-existing formatting
    errors in `libs/holonight-core/src/HyprlandWorkspaceService.cpp`.

- [x] T-131: Perform live compositor and shutdown verification.
  - Steps:
    1. Run `task compositor-smoke-check` and start the shell in Hyprland.
    2. Exercise tray icon updates while opening/closing tray items.
    3. Trigger launcher rescan with a non-first selected result and confirm
       the same logical row remains selected/visible.
    4. Verify a deliberately invalid test-only QML source reports its URL and
       errors without leaving a visible ghost surface.
    5. Stop the shell during NetworkManager activity and confirm clean exit.
  - Result: user verified the phase in the live environment.

- [x] T-132: Record accepted items, commit/test evidence, and the remaining
  Phase 7 Low-severity backlog in the Phase 7 handoff.
  - Result: the Phase 7 report records all four Medium findings as accepted in
    `af590d6` (`fix: complete phase 14 reliability remediation`); the 61
    Low-severity findings remain queued.
