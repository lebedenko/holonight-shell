# SPEC: Bar Frame Extraction and Sidebar Shape Alignment

**Date:** 2026-07-23
**Status:** Implemented

## Goal

Separate the system top bar's intentional asymmetric frame geometry from semantic surface frames, and ensure
the sidebar tab-column tint never paints outside the resolved panel shape.

## Requirements

- `BarFrame` shall own the top-bar offsets, corner cuts, fill, border, gradients, highlights, shadows, and glow.
- Every top-bar and tray section shall use `BarFrame`; ordinary shell surfaces shall continue to use `HudFrame`.
- `HudFrame` shall resolve variants through `HnSurfaceFrame`, retain rounded-surface decoration, and shall not
  expose asymmetric bar geometry.
- The sidebar tab tint shall use the same resolved outline as its `HudFrame` and remain limited to the tab column.
- Focused QML tests shall cover semantic role mapping, bar-frame geometry properties, and shared sidebar path access.

## Non-goals

- Moving top-bar-specific geometry into `holonight-qt`.
- Masking or layering the complete sidebar subtree.
- Changing the designed top-bar silhouette.

## Acceptance

- Top-bar call sites contain no `HudFrame` instances.
- `HudFrame` has no offset or corner-cut fallback API.
- The sidebar tint has transparent pixels outside rounded, chamfered, and hybrid panel corners.
- Focused tests, QML lint, qmltypes checks, and the full test suite pass.
