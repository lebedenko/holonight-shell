# MPRIS Desktop Widget Mockup-Fidelity Polish — Design

## Overview

`MprisWidgetManager` continues to create and update a dedicated transparent layer-shell surface.
Only its internal size constants change to `368×456`; the QML property contract and presentation
state machine remain intact.

## Composition

```text
transparent surface: 368 × 456
  content column:      x 56, width 256
    artwork:           y 32, height 256
    title row:         spectrum glyph + right-elided title
    artist:            optional, right-elided
    progress/time:     full column width
```

No visual item paints the surface bounds. `album` and `desktopEntry` remain available for contract
compatibility even though neither produces a separate visual row or badge.

## Artwork and ambient wash

`MprisArtwork.qml` exposes its effective real-or-fallback source. One reusable inline `Shape`
primitive defines both the alpha mask and visible border. A single hidden image uses
`PreserveAspectCrop`, and `MultiEffect` masks it through that shape so the crop fills the complete
chamfered frame at fractional scales.

`MprisWidgetSurface.qml` decodes a cached asynchronous `96×96` duplicate of the effective source,
stretches it behind the full artwork-and-metadata footprint, and applies one bounded blur and color
adjustment pass. Playing and paused states select opacity `0.14` and `0.07`; effective surface
visibility gates the pass. A separate subtle theme-colored shadow sits behind metadata without
painting a card.

## Glyph, progress, and timestamps

`assets/media/now-playing-glyph.svg` contains three rounded static spectrum bars and remains a
semantic-color resource. It renders at `16px` inside the title row.

`MprisProgressBar.qml` uses a centered `2px` `textDisabled` track behind a `3px` horizontal
`accentCyan → accentBlue → accentViolet` gradient. `canSeek` changes fill opacity only. Elapsed and
duration use `StableDigitsText` after an `8px` gap, preventing digit updates from shifting edges.

## Compatibility and performance

No C++ property pushes or MPRIS APIs change. The wash is disabled with the widget. Artwork decode
sizing, asynchronous loading, pause-timeout behavior, and the manager's 2 Hz position sampling are
unchanged.
