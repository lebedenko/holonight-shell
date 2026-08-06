# System Appearance Portal Specification

## Overview

`holonight-shell` owns the HoloNight XDG desktop portal Settings backend in-process. The shell does not start a separate activation daemon; the running shell exports the backend on the user session bus.

## D-Bus Contract

- Service: `org.freedesktop.impl.portal.desktop.holonight`
- Object path: `/org/freedesktop/portal/desktop`
- Interface: `org.freedesktop.impl.portal.Settings`
- Methods:
  - `Read(namespace, key) -> variant`
  - `ReadAll(namespaces) -> namespace/value map`
- Signal:
  - `SettingChanged(namespace, key, value)`

## Theme Mapping

The backend reads `$XDG_CONFIG_HOME/holonight/theme.conf`, falling back to `~/.config/holonight/theme.conf`.

- `holonight-dark` and `tokyonight-storm` publish `org.freedesktop.appearance/color-scheme = 1`.
- `holonight-light` and `tokyonight-day` publish `org.freedesktop.appearance/color-scheme = 2`.
- Missing or invalid `scheme` falls back from legacy `mode`; otherwise dark is used.
- `accent` publishes `org.freedesktop.appearance/accent-color` as a `(ddd)` RGB struct with normalized `0.0..1.0` channels.

`mode` remains compatibility metadata written by Settings and derived from `scheme`; it is not the primary setting.

## Runtime Behavior

`ThemeService` watches `theme.conf`. On relevant file or directory changes it:

- reloads portal-facing values,
- emits `SettingChanged` only for changed color-scheme or accent-color values,
- emits the existing `paletteReloadRequested()` signal.

Portal routing files are installed for the `HoloNight` desktop so `xdg-desktop-portal` can route Settings requests to the in-process backend.

## Installation

The build installs:

- `${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal/portals/holonight.portal`
- `${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal/holonight-portals.conf`

`XDG_CURRENT_DESKTOP` must include `HoloNight` so the broker selects `holonight-portals.conf`. The session bootstrap exports `HoloNight:Hyprland` by default.

After installing or updating the portal files, the user must start a new session or restart `xdg-desktop-portal` after `holonight-shell` is running. Installation does not restart user services because this project supports local prefixes and should not mutate the active desktop session as a side effect of `cmake --install`.
