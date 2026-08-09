# CTV-104: Qt session activation repair

Status: Implemented

Baseline: `holonight-shell@e95f73e`.

## Session contract

The HoloNight session establishes these defaults before starting Hyprland or UWSM:

- `QT_QPA_PLATFORMTHEME=holonight`
- `QT_QUICK_CONTROLS_STYLE=Holonight`

An inherited non-empty value is an explicit user override and is preserved. Both values are imported into D-Bus
activation and the systemd user manager, then recovered by the Shell service wrapper. `QT_STYLE_OVERRIDE` is not set,
imported, or recovered because a global widget-style override bypasses the supported platform-theme contract; a
pre-existing value remains in the launching process and is reported as unsupported.

## Diagnostics

CLI and in-Shell diagnostics report missing or non-HoloNight process values, values absent or mismatched in the
systemd activation environment, and any global `QT_STYLE_OVERRIDE`. An explicit user override is preserved even
though diagnostics identify it as differing from the HoloNight default.
