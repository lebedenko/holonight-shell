# Decoupled Light/Dark Mode Switcher Tasks

- [x] Remove shell `[theme]` parsing, defaults, writing, and `ConfigService` signals.
- [x] Add settings-side `theme.conf` mode load/save support.
- [x] Migrate legacy `config.toml` `[theme].mode` when `theme.conf` has no mode.
- [x] Watch `theme.conf` from the shell and request palette reloads.
- [x] Simplify settings Appearance UI to mode only with `dark`, `light`, and `system`.
- [x] Add shell/settings tests for the new ownership boundary.
- [x] Use `theme.conf` as the sole `holonight-qt` theme source.
