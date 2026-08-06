# Phase 44 — Shared QML Presentation Cleanup

## Goal

Bundle the five remaining actionable U-10 findings from the Phase 7 backlog.
Preserve launcher, tray, notification, widget, and weather behavior while
reusing the shared icon fallback path and removing misleading QML debris.

## Scope

| Source | Phase 44 item | Required outcome |
|---|---|---|
| U-10 I-001 | Reuse icon fallback handling | Launcher side panels and tray menu items render the established fallback when a named icon cannot load. |
| U-10 I-004 | Declare plain digit glyphs | Frequently updated stable-digit delegates bypass automatic rich-text detection. |
| U-10 I-005 | Correct launcher width documentation | The right-panel comment agrees with its 256-pixel preferred width. |
| U-10 I-006 | Remove dead QML ids | Unreferenced ids no longer imply nonexistent imperative control points. |
| U-10 I-007 | Document the weather compositor API | Project guidance describes the component's current optional inputs and bridge entry point. |

## Acceptance Criteria

1. Recent and selected launcher application icons use the shared executable
   fallback without changing their size or exact icon rendering mode.
2. Tray menu named icons use the same error-aware fallback while separators and
   iconless menu entries remain unchanged.
3. Stable digit glyphs explicitly render as plain text.
4. The stale launcher width comment and five unreferenced ids are removed or
   corrected without changing layout or animation behavior.
5. Project guidance matches `WeatherIconCompositor` and `WeatherIconBridge`.
6. QML lint, QML type metadata, focused QML tests, architecture checks, and the
   project test suite pass apart from documented pre-existing failures.
7. Manual verification confirms launcher recent/selected icons, a tray menu,
   stable clock digits, notifications, and tray hover/badge visuals remain
   correct.

## Non-goals

- Changing icon sizing, semantic tint policy, launcher layout, or tray behavior.
- Changing weather mapping, moon-phase calculation, or compositor wiring.
- Addressing U-07, U-04, or other non-U-10 backlog findings.

## Backlog Accounting

Phase 43 left eight queued Low-severity candidates. Acceptance of this five-item
tranche will reduce the queued Phase 7 backlog from eight to three.
