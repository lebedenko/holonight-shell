# DESIGN: Bar Frame Extraction and Sidebar Shape Alignment

**Spec:** `docs/sdd/bar-frame-and-sidebar-shape/SPEC.md`
**Date:** 2026-07-23
**Status:** Implemented

## Component boundary

`HudFrame` is the semantic surface adapter. It maps shell variants to shared roles and delegates geometry to
`HnSurfaceFrame`. `BarFrame` is a shell-specific decorative primitive: its offsets and whole-side cuts describe
the composition of the system bar rather than a theme corner style.

`HudFrame` retains its shell-owned gradient, highlight, shadow, and glow overlay for resolved rounded shapes.
Those effects are independent of asymmetric bar layout and remain suppressed for chamfered/hybrid shapes where
the rounded Canvas path would disagree with the shared outline.

The extraction intentionally preserves the existing Canvas renderer for the static bar frames. This avoids a
visual redesign while preventing bar geometry from becoming a compatibility branch in every semantic surface.

## Sidebar tint

The sidebar `HudFrame` exposes the resolved `pathData`. A decorative `Shape` redraws that exact path using a
translucent fill, while a narrow parent clips it at the tab/content divider. Only this leaf decoration is clipped;
the sidebar content is not sent through an item-sized mask or effect pass.

The decoration is positioned inside `HudFrame.frameInset`, matching the coordinate system in which
`HnSurfaceFrame.pathData` is generated.

Because both layers consume one resolved path, live radius/chamfer/profile changes cannot leave rectangular
corner pixels behind.

## Verification

QML tests exercise the public component contracts. Automated checks cover loading and static correctness; final
pixel appearance remains a live-compositor check in both dark and light themes.
