# Decoupled Light/Dark Mode Switcher Design

## Data Ownership

Shell `config.toml` remains the owner of shell behavior, fonts, bar settings, widgets, weather, notifications, and calendar configuration. Appearance mode moves to `holonight-qt`'s user config file, `theme.conf`.

The shared shell config parser ignores legacy `[theme]` sections. The config writer serializes only shell-owned sections, so the next settings save naturally drops old `[theme]` data from `config.toml`.

## Settings Flow

`SettingsEditModel` tracks shell config and theme mode as separate dirty inputs. `ConfigFileService` loads `config.toml` through `holonight_config` and loads mode through `ThemeConfigFile`.

If `theme.conf` does not contain `appearance/mode`, `ConfigFileService` checks legacy `config.toml` `[theme].mode`; when present, it writes that value to `theme.conf` immediately. Later saves write shell settings through `ConfigWriter` and mode through `ThemeConfigFile`.

## Shell Reload Flow

`ThemeService` remains the QML-facing appearance/font service. It watches `~/.config/holonight/theme.conf` and its parent directory using `QFileSystemWatcher`. Any relevant change emits `paletteReloadRequested`, and `ThemeReloadBridge.qml` calls `HoloniightPalette.reload()`.

The shell does not inspect mode values during reload. `holonight-qt` resolves mode and token values when `HoloniightPalette.reload()` runs.
