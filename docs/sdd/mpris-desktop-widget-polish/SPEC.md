# MPRIS Desktop Widget Mockup-Fidelity Polish — Specification

## Scope

This cycle refines only the presentation of the read-only MPRIS desktop widget. It supersedes the
conflicting visual requirements in `docs/sdd/mpris-desktop-widget/` while preserving that cycle's
MPRIS data flow, artwork cache, visibility and occupancy rules, pause timeout, and input behavior.

## Requirements

**REQ-F-001 — Frameless surface.** The widget shall have no panel background, outer shadow, or
visible surface boundary. The pointer-transparent layer surface shall be `368×456` logical pixels,
leaving approximately `56px` of transparent horizontal perimeter around a `256px` content column.

**REQ-F-002 — Artwork and wash.** A `256×256` artwork tile shall start at `x=56`. Real and fallback
artwork shall use `PreserveAspectCrop` through one shared vector shape used by the alpha mask and
visible chamfer border. A cached asynchronous `96×96` duplicate of the effective source shall form
one blurred wash behind the complete content footprint, with approximately `brightness=-0.32`,
`contrast=-0.12`, `saturation=0.18`, and opacity `0.14` while playing or `0.07` while paused. The
wash shall not render while the widget is hidden.

**REQ-F-003 — Alignment and glyph.** Artwork, title row, artist, progress, and timestamps shall
share one `256px` column. A smaller static spectrum-only semantic SVG shall sit inside the title
row. It shall not contain a note, animate, or use the player application icon.

**REQ-F-004 — Metadata.** Title and optional artist shall use `AppearanceService.clockFont`, remain
one line, render as plain text, and right-elide. Album shall not render. A subtle theme-colored
shadow may support metadata readability, but no card or scrim shall be added.

**REQ-F-005 — Progress and time.** A known duration shall show a centered `2px` track using
`textDisabled` at `0.28` opacity and a clamped `3px` elapsed fill with cyan, blue, and violet
gradient stops. Non-seekable media shall retain the gradient at reduced opacity. After an `8px`
gap, elapsed and duration shall use `StableDigitsText`, the same neutral color with slightly
stronger elapsed opacity, and align to the column edges. Unknown duration shall hide the bar and
duration while retaining elapsed time.

**REQ-F-006 — Preserved behavior.** Title shall fall back to player identity. Playing/Paused
metadata opacity, timeout fade, occupancy hiding, artwork fallback, and pointer transparency shall
remain unchanged. Existing pushed properties, including `album` and `desktopEntry`, remain
available even when not displayed.

## Supersession

This specification replaces v1 and earlier polish decisions for the application identity badge,
pause glyph, album row, combined ordinary timestamp, single-color progress fill, centered content,
mixed font families, artwork-local glow, `352×420` surface, and note-plus-spectrum glyph. All
non-visual v1 requirements remain active.

## Non-goals

- MPRIS backend, manager, cache, configuration-schema, or public API changes.
- Playback controls or other pointer interaction.
- Dominant-color extraction, audio visualization, tooltips, or generated raster artwork.
