# System Appearance Portal Design

## Components

- `SettingsPortalBackend` implements `org.freedesktop.impl.portal.Settings`.
- `ThemeService` owns the backend and remains responsible for watching `theme.conf`.
- `holonight.portal` advertises only the Settings backend.
- `holonight-portals.conf` routes Settings to `holonight` and leaves all other portals on the default backend chain.
- `scripts/check-desktop-integration.sh` verifies that the portal descriptor, routing config, desktop name, and D-Bus owner are visible in the active session.

## Data Flow

1. Shell startup constructs `ThemeService`.
2. `ThemeService` constructs `SettingsPortalBackend`.
3. The backend reads `theme.conf`, registers the D-Bus service/object, and serves `Read`/`ReadAll`.
4. File changes trigger `ThemeService::onThemeConfigPathChanged`.
5. The backend reloads values and emits changed portal settings before the shell requests a palette reload.

## Non-Goals

- No D-Bus activation file is installed; a duplicate shell must not be started by portal activation.
- No file chooser, screencast, inhibit, or URI portal backend is implemented here.
- No attempt is made to own `org.freedesktop.portal.Desktop`; `xdg-desktop-portal` remains the broker.
- No CMake install hook restarts `xdg-desktop-portal`; users should relogin or restart it manually after the shell is running.
