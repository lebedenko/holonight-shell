# SPEC: Semantic HUD Frame Migration

**Date:** 2026-07-23
**Status:** Superseded in part by `bar-frame-and-sidebar-shape`

## 1. Goal

Migrate the shell-owned `HudFrame` to the semantic frame API supplied by `holonight-qt` while preserving
the existing `HudFrame.Variant` API and the one asymmetric top-bar use case that the shared renderer does
not model.

## 2. Requirements

### REQ-F-001: Resolve shell variants to semantic roles

`HudFrame.Popup`, `Section`, `Widget`, and `Sidebar` shall map to `HnSurfaceRole.Popup`, `Hud`, `Card`, and
`Panel`, respectively. Normal frames shall be rendered by `HnSurfaceFrame` and shall follow live
`HnAppearance` shape changes.

### REQ-F-002: Preserve source compatibility

Existing variant, color, stroke, radius, inset, offset, cut, gradient, highlight, shadow, and glow
properties shall remain available. `cornerRadius` shall default to `NaN`, allowing semantic role sizing;
a finite non-negative value shall act as a local radius override.

### REQ-F-003: Bound the legacy fallback

The old Canvas path shall be used only when a caller sets an asymmetric edge offset or whole-side corner
cut. Those properties are shell layout geometry rather than theme geometry and remain supported until the
workspace frame is redesigned. The fallback shall retain the existing visual treatment.

### REQ-F-004: Keep shell decoration shell-owned

For semantic rounded frames, gradients, inner highlight/shadow, and glow remain an overlay owned by the
shell. For chamfered and hybrid frames, the overlay shall be suppressed rather than draw a mismatched path;
the shared fill and border remain visible.

### REQ-T-001: Test observable mapping and fallback behavior

QML tests shall cover every variant mapping, semantic radius resolution, explicit radius overrides, and
activation of the legacy fallback by each legacy geometry family.

## 3. Non-goals

- Extending `HnSurfaceFrame` with shell-specific offsets or effects.
- Migrating unrelated hard-coded radii in shell controls.
- Removing compatibility properties in this cycle.

## 4. Acceptance

- `HudFrame` delegates ordinary geometry to `HnSurfaceFrame`.
- Existing call sites load unchanged.
- Focused QML tests, qmllint, qmltypes checks, and the project test suite pass.
