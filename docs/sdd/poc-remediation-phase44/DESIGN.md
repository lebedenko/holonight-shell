# Phase 44 — Shared QML Presentation Cleanup: Design

## Icon fallback reuse

The three affected icon sites duplicate direct `Image` loading without handling
`Image.Error`. Replace those instances with the existing `ExternalIcon`
component, using `application-x-executable` as the fallback and disabling
semantic tint so successful named icons keep their current exact rendering.
Sizes and visibility conditions remain owned by the existing call sites.

`ExternalIcon` already resets fallback state when its inputs change and has
focused QML coverage for named, empty, pixmap, and fallback sources. This keeps
error handling in one component rather than adding three local state machines.

## Static QML cleanup

Set `Text.PlainText` on the per-glyph `StableDigitsText` delegate. Correct the
launcher right-panel comment to 256 pixels. Remove only ids that have no
reference within their QML component: three visual ids in `TrayItem`, the frame
and repeater ids in `TraySection`, and the close-button id in `ToastItem`.

## Weather documentation

Update project guidance to list `conditionDescription`, `windSpeedKmh`, and the
optional `moonPhase` override, and to distinguish `layersForWeather(...)` from
the simpler `layersFor(...)` bridge entry point. No weather source or API is
changed.

## Verification Strategy

- Run the existing `ExternalIcon` and tray-item QML tests.
- Run QML lint and QML type metadata checks for imports and compilation.
- Run architecture checks and the full project suite.
- Manually inspect launcher icon fallback, a tray menu, clock digits,
  notification close behavior, and tray hover/badge effects.

The changes are either shared-component substitutions with preserved sizing
and tint policy, or removal of statically unreferenced identifiers. The main
risk is an import or component-construction error, covered by QML compilation
and focused tests.
