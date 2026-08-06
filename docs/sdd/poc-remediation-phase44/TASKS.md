# Phase 44 — Shared QML Presentation Cleanup: Tasks

- [x] T-292: Revalidate all seven U-10 findings against current `main` and keep
  the five confirmed actionable items in scope.
- [x] T-293: Replace the three direct named-icon loaders with the shared
  `ExternalIcon` fallback path while preserving size and tint behavior.
- [x] T-294: Mark stable digit glyphs as plain text, correct the launcher width
  comment, and remove the five unreferenced QML ids.
- [x] T-295: Update project guidance for the current weather compositor inputs
  and bridge entry points.
- [x] T-296: Run focused QML tests, `task qml-lint`, `task qmltypes-check`,
  `task architecture-check`, and `task test`; document any pre-existing
  failures precisely.
  - Result: the focused QML harness, QML lint, QML type metadata and packaging,
    and architecture checks pass. The full suite runs 958 tests with only the
    established `KeyboardLayoutService.EventSocketConnectedTriggersDevicesQuery`
    failure; `SidebarManagerMonitorValidation.AcceptsAnyCurrentlyConnectedNonEmptyNamedScreen`
    remains skipped as expected.
- [x] T-297: Manually verify launcher recent/selected icons, one tray menu,
  stable clock digits, notification close behavior, and tray hover/badge
  visuals.
  - Result: accepted by the user after confirming the affected presentation
    paths work correctly.
- [x] T-298: After acceptance, record implementation/documentation commits and
  reduce the queued Phase 7 Low-severity backlog from eight to three.
  - Result: implementation commit `4b0401c` (`refactor: consolidate shared qml
    presentation paths`) completes U-10 I-001/I-004/I-005/I-006/I-007. This
    documentation closeout records the accepted five-item reduction; three
    Low-severity candidates remain queued.
