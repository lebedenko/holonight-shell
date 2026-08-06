# Appearance Service Split Specification

## Goal

Split shell presentation settings from theme palette reload handling.

`AppearanceService` owns appearance values loaded from `config.toml`. `ThemeService` owns only the palette reload bridge for `~/.config/holonight/theme.conf`.

## Public QML API

### AppearanceService

`AppearanceService` is a `HolonightShell` QML singleton exposing:

- `uiFont`
- `fixedFont`
- `clockFont`
- `titleFont`
- `uiFontSize`
- `fixedFontSize`
- `clockFontSize`
- `titleFontSize`
- `debugOverlays`

Font properties use `NOTIFY` signals and update when `ConfigService::appearanceChanged` fires. `debugOverlays` remains startup-only and is derived from the `--debug-overlays` command-line flag.

### ThemeService

`ThemeService` is a `HolonightShell` QML singleton exposing:

- `paletteReloadRequested()`

It watches the Holonight theme config path and emits the signal when `theme.conf` or its parent directory changes.

## Non-Goals

- No `config.toml` schema changes.
- No settings UI changes.
- No `holonight-qt` changes.
- No compatibility aliases for moved `ThemeService` font or debug properties.
