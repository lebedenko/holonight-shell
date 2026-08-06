# Theme Variant Catalog Specification

## Scheme IDs

The canonical appearance selector is `appearance/scheme` in `theme.conf`.

- `holonight-dark`: default dark HoloNight scheme.
- `holonight-light`: default light HoloNight scheme.
- `tokyonight-storm`: TokyoNight-compatible dark scheme name.
- `tokyonight-day`: TokyoNight-compatible light scheme name.

## Compatibility

`appearance/mode` is derived metadata for older consumers.

- Dark scheme IDs write `mode=dark`.
- Light scheme IDs write `mode=light`.
- If `scheme` is missing or invalid, consumers may fall back from legacy `mode`.
- If both are missing or invalid, consumers use `holonight-dark`.

## Accent

`appearance/accent` selects the active accent family. Supported IDs are:

- `cyan`
- `blue`
- `violet`
- `yellow`

The selected accent affects resolved primary/focus/glow tokens in the theme resolver and is published by the shell Settings portal backend as a normalized RGB `(ddd)` value.
