# Appearance Service Split Design

## Service Responsibilities

`AppearanceService` depends on `ConfigService`. It reads `ConfigService::appearance()` during construction and connects to `ConfigService::appearanceChanged` for live updates. It exposes the same font and debug overlay values previously carried by `ThemeService`.

`ThemeService` has no `ConfigService` dependency. It resolves `$XDG_CONFIG_HOME/holonight/theme.conf`, watches the file and parent directory with `QFileSystemWatcher`, and emits `paletteReloadRequested()` for `ThemeReloadBridge.qml`.

## QML Registration

`ShellApplication` owns both services:

- `AppearanceService(config_service_, this)`
- `ThemeService(this)`

`registerQmlTypes()` registers both as `HolonightShell` singletons. QML appearance consumers bind to `AppearanceService.*`; `ThemeReloadBridge.qml` remains connected to `ThemeService.paletteReloadRequested`.

## Test Fakes

QML smoke tests register both singleton names:

- `FakeAppearanceService` exposes constant font and debug properties.
- `FakeThemeService` exposes only `paletteReloadRequested()`.

Integration tests assert config-backed font behavior through `AppearanceService` and keep palette reload coverage on `ThemeService`.
