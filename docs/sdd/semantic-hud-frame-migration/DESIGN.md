# DESIGN: Semantic HUD Frame Migration

**Spec:** `docs/sdd/semantic-hud-frame-migration/SPEC.md`
**Date:** 2026-07-23
**Status:** Superseded in part by `bar-frame-and-sidebar-shape`

## 1. Boundary

`holonight-qt` owns role-to-shape resolution and the reusable scene-graph frame. The shell owns the meaning
of its four historical variants and its atmospheric decoration. `HudFrame` therefore becomes a compatibility
adapter rather than a second shape-token authority.

## 2. Variant mapping

| Shell variant | Shared role | Rationale |
|---|---|---|
| Popup | Popup | Top-level transient HUD surface |
| Section | Hud | Top-bar HUD grouping |
| Widget | Card | Embedded information surface |
| Sidebar | Panel | Large persistent shell panel |

The wrapper passes `HnCornerStyle.Inherit`, so profile/configuration changes flow through `HnAppearance`.
`cornerRadius: NaN` means no local override. This intentionally replaces hard-coded default radii while
keeping explicit caller overrides source compatible.

## 3. Rendering

The normal branch contains `HnSurfaceFrame`, inset by the legacy `frameInset`. Its fill and border receive
the existing shell colors and opacity. A Canvas above it draws only the legacy rounded decoration. It is
disabled for non-rounded resolved shapes to avoid disagreeing with the shared path.

Asymmetric edge offsets cannot be expressed by the role/mask model. When any offset or side cut is non-zero,
the shared frame is hidden and a preserved legacy Canvas draws the complete old frame. `usesLegacyGeometry`
makes that transition observable and testable.

This fallback is deliberately local: it does not add shell geometry to `holonight-qt`, and it can disappear
when `WorkspaceSection` adopts a semantic shape without slanted edge offsets.

## 4. Compatibility and risks

- Existing property names and variants remain intact.
- Default radii now follow appearance configuration, so visuals may change by design.
- Hybrid/chamfered frames omit the old rounded-only atmospheric overlay. This avoids incorrect clipping and
  keeps the shared geometry authoritative.
- Canvas remains only for shell decoration and explicitly legacy geometry; ordinary fill/border rendering
  moves to the Qt Quick Shapes-backed shared component.

## 5. Verification

Add `tst_HudFrame.qml` to verify variant mapping and fallback selection. Run the QML harness, qmllint,
qmltypes check, and full tests. A live compositor visual check remains appropriate for the final appearance.
