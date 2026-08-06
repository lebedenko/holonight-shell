# Phase 40 — Top-Bar QML Contract Cleanup

**Status**: Complete — automated checks and user verification passed.

## Objective

Bundle the three remaining U-08 top-bar QML findings from the Phase 7 backlog.
This tranche removes dead base sizing, makes tooltip monitor ownership explicit,
and stops workspace arrows from requesting hover tracking they never consume.

| Source | Phase 40 item | Impact |
|---|---|---|
| U-08 I-02 | Remove dead `BarSection` implicit-width fallback | Section width has one authoritative owner in each concrete component. |
| U-08 I-06 | Require `BarTooltipArea.barMonitorName` | A tooltip cannot be instantiated without the monitor identity needed by `TooltipSurface`. |
| U-08 I-07 | Remove unused workspace-arrow hover tracking | Arrow pointer handling performs only the click tracking its behavior requires. |

## Functional Requirements

### REQ-F-01 — Concrete sections own their width

`BarSection` shall not derive an implicit width from its internal content
container. Every current concrete section shall retain its existing explicit
`implicitWidth` policy and rendered footprint.

**Acceptance**: all current `BarSection` subclasses define `implicitWidth`, and
the base component no longer supplies a competing fallback.

### REQ-F-02 — Tooltip monitor identity is mandatory

`BarTooltipArea.barMonitorName` shall be a required string property. Every
current instance shall provide the owning bar monitor name before component
completion.

**Acceptance**: QML compilation and linting succeed across all tooltip call
sites, and existing tooltip routing remains unchanged.

### REQ-F-03 — Workspace arrows track clicks without hover state

`WorkspaceEdgeArrow` shall retain its enabled-state and click behavior without
enabling `MouseArea` hover tracking. No visual state shall depend on pointer
hover.

**Acceptance**: focused QML coverage confirms hover tracking is disabled and
the activation signal still fires on click.

## Constraints and Verification

- Keep the change within the reusable top-bar QML components and focused QML
  coverage; do not redesign top-bar layout, tooltip timing, or workspace
  navigation.
- Add no new QML types, dependencies, or public service APIs.
- Run the focused QML test, `task qml-lint`, `task qmltypes-check`, `task test`,
  `task architecture-check`, and `git diff --check`.
- Perform a live Hyprland check that top-bar sizing, tooltips, and workspace
  arrows remain functional on the active monitor before closing the phase.

## Out of Scope

- Previously completed U-08 workspace-bound and section-exit findings.
- Notification/sidebar delegate contracts, icon fallback handling, settings
  QML cleanup, and every other queued Phase 7 candidate.
