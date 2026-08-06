# Decoupled Light/Dark Mode Switcher Spec

## Requirements

- The shell shall no longer own appearance mode in `config.toml`.
- `~/.config/holonight/theme.conf` shall be the canonical user file for palette appearance mode.
- `theme.conf` shall store mode as INI:

```ini
[appearance]
mode=dark
```

- Supported mode values are `dark`, `light`, and `system`; invalid values fall back to `dark`.
- Settings shall save shell behavior settings to `config.toml` and appearance mode to `theme.conf`.
- When `theme.conf` has no mode, settings shall migrate legacy `[theme].mode` from `config.toml` once.
- Shell config parsing shall tolerate legacy `[theme]` sections without publishing or rewriting them.
- Shell palette reload shall be triggered by changes to `theme.conf` or its parent directory.
- Shell code shall not parse or resolve `holonight-qt` palette tokens.

## Deferred

- Theme variants and accent colors are not user-editable in this pass.
- Resolved token editing remains owned by `holonight-qt`.
