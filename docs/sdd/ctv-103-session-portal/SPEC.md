# CTV-103: Hyprland session and portal propagation

Status: Implemented

Baselines: `holonight-shell@93e1faf`, `holonight-appearance-adapters@e3b3efd`,
`holonight-settings@2d25b3e`, `holonight-config@5cd36ec`, and `holonight-qt@e30ff79`.

## Session contract

`holonight-hyprland-session` resolves the canonical appearance file from the first non-empty value in this order:

1. `HOLONIGHT_APPEARANCE_FILE`, used exactly as supplied;
2. `$XDG_CONFIG_HOME/holonight/appearance.toml`;
3. `$HOME/.config/holonight/appearance.toml`.

Before importing activation environments, it runs
`holonight-appearance-adapter query --appearance PATH --field cursor-theme`. Exactly one non-empty output line, at
most 4096 characters and containing no control characters, becomes `XCURSOR_THEME`. A missing adapter, unresolved
path, failed query, or malformed result produces a warning and preserves any inherited cursor theme. Startup remains
non-fatal and toolkit-native cursor behavior is the final fallback.

The bootstrap imports `XCURSOR_THEME`, `XDG_CONFIG_HOME`, and `HOLONIGHT_APPEARANCE_FILE` into D-Bus and systemd
activation environments. The systemd service wrapper recovers the same variables from the user manager so direct,
UWSM, and service launch paths share canonical state.

## Portal and restart behavior

The in-process Shell backend remains the sole HoloNight Settings portal publisher at
`org.freedesktop.impl.portal.desktop.holonight`. CTV-103 does not start another portal or XSettings owner, export
`GTK_THEME`, invoke adapter `apply`/`revert`, or modify canonical files.

Color-scheme and accent portal changes are live. Applications may need relaunch to consume toolkit settings. Cursor
environment changes require a new desktop session; diagnostics compare the active `XCURSOR_THEME` with the canonical
cursor and report that boundary explicitly.
