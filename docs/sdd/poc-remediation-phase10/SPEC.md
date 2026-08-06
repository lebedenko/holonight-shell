# Phase 10 — Bounded Workspace Pill Delegates

**Status**: Complete — automated validation and live Hyprland acceptance passed.

## Objective

Fix Phase 7 U-08 I-01: `WorkspacePillStrip` currently gives its `Repeater`
every workspace ID from 1 through the greatest ID reported by the compositor.
Because `Repeater` eagerly creates every `WorkspacePill`, a sparse or very
large workspace ID creates expensive, off-screen delegates.

This phase makes the strip window-local. It keeps the visible workspace window
and one real neighbor on either side, while retaining the current 200 ms slide,
absolute workspace IDs, and click behavior.

| Source | Phase 10 item | Rationale |
|---|---|---|
| U-08 I-01 | Bound `WorkspacePillStrip` delegates | Prevent arbitrary compositor workspace IDs from determining QML item count. |

## Functional Requirements

### REQ-F-01 — Fixed delegate bound

`WorkspacePillStrip` shall instantiate at most
`WorkspaceModel.displayCount + 2 * stripPad` workspace-pill delegates.

- The count shall not depend on `WorkspaceModel.maxWorkspaceId()`.
- The count shall not grow when `windowStart` is a large workspace ID.
- At the left boundary, the missing predecessor shall not be replaced by an
  unrelated delegate; the count may be smaller than the maximum.

**Acceptance**: QML coverage seeds a sparse high workspace ID, moves the
window to it, and observes the fixed bound rather than a count proportional to
the ID.

### REQ-F-02 — Preserve visible-window behavior

Every absolute workspace ID in the current visible window shall retain a real
`WorkspacePill` delegate with the same state, tooltip, and activation target as
before.

- One adjacent pill on either side shall exist when that absolute ID is valid.
- Normal windows near workspace 1 retain the existing right-pad behavior.
- The strip's viewport footprint remains determined solely by `displayCount`.

**Acceptance**: focused QML coverage verifies the visible IDs, both pads, and
the fixed implicit-width footprint for a high `windowStart`.

### REQ-F-03 — Preserve continuous navigation animation

When `windowStart` changes by one, the visible pills shall continue to slide
using the existing 200 ms `Easing.OutCubic` x behavior. A newly needed neighbor
may be created as the model window shifts, but the viewport shall not reveal a
blank gap or relabel an existing pill in place.

**Acceptance**: existing and extended QML coverage observes the correct final
x position and real absolute IDs after a one-step shift in either direction.

## Constraints and Verification

- Keep the production change confined to `WorkspacePillStrip.qml`; no service
  or `WorkspaceModel` API change is needed.
- Reuse the existing pill geometry, animation duration, easing, clipping, and
  `barMonitorName` propagation.
- Do not replace the strip with a `ListView` or add dependencies.
- Run the focused QML harness, `task qml-lint`, `task qmltypes-check`, and
  `task test`.
- Perform a live Hyprland check by navigating around a high numbered workspace
  after automated validation passes.

## Out of Scope

- The separate critical-battery pulse transition (U-08 I-05).
- Changes to workspace-model occupancy, urgency, or activation semantics.
- Broader top-bar layout or workspace-indicator redesign work.
