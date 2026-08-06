# SPEC - desktop-session-bootstrap

## 1. Purpose

Make a HoloNight-on-Hyprland login behave like a complete desktop session for
applications launched from the shell, Hyprland, D-Bus activation, portals, and KDE
frameworks. The immediate target is the Dolphin "Open with..." failure where the
application chooser falls back to manual executable entry until `kbuildsycoca6`
is run.

This feature adds installable session bootstrap artifacts, live session
diagnostics, and safe user-triggered repair actions. HoloNight remains the
control plane over existing system daemons; it does not replace
`xdg-desktop-portal`, KDE KIO, systemd user, or D-Bus activation.

## 2. Scope and Non-Goals

### Scope

- Add a `SessionIntegrationService` QML singleton that reports desktop-session
  health.
- Validate the shell process environment, systemd user manager environment, and
  D-Bus activation environment for required desktop variables.
- Validate XDG menu prefix health for KDE frameworks by checking whether the
  selected `${XDG_MENU_PREFIX}applications.menu` exists.
- Validate portal broker and backend availability using command/service checks
  that complement `PortalService`.
- Validate MIME/application cache health, including KDE sycoca presence and
  staleness.
- Provide a safe rebuild action for application caches:
  `update-desktop-database` on user-writable application directories, then
  `kbuildsycoca6 --noincremental` when available.
- Add installable session/bootstrap files and documentation for Hyprland/UWSM.
- Add a command-line smoke check script for live-session debugging.

### Non-Goals

- Implement an XDG desktop portal backend.
- Implement KDE KIO, KDE application services, or Plasma session components.
- Automatically mutate user session environment after startup without explicit
  install/session bootstrap.
- Run privileged commands or write to `/usr`, `/etc`, or system package-owned
  caches.
- Replace Hyprland, UWSM, systemd user, D-Bus, or portal startup policy.
- Guarantee compatibility with every desktop environment; Hyprland is the first
  target.

## 3. Functional Requirements

### 3.1 Session Environment Diagnostics

**REQ-F-001** (Ubiquitous)  
The system shall expose a `SessionIntegrationService` QML singleton registered
under the `HolonightShell` module.

*Acceptance criterion*: QML can read `SessionIntegrationService.overallStatus`
and call `SessionIntegrationService.refresh()` without runtime errors.

**REQ-F-002** (Ubiquitous)  
The service shall report the shell process values for:

- `WAYLAND_DISPLAY`
- `XDG_CURRENT_DESKTOP`
- `XDG_SESSION_DESKTOP`
- `XDG_SESSION_TYPE`
- `XDG_MENU_PREFIX`
- `XDG_DATA_DIRS`
- `XDG_CONFIG_DIRS`
- `DBUS_SESSION_BUS_ADDRESS`

*Acceptance criterion*: A diagnostic model includes one row per variable with
the observed value and a status of `ok`, `warning`, or `error`.

**REQ-F-003** (Conditional)  
When `systemctl --user show-environment` is available, the service shall compare
the systemd user manager values for the variables in REQ-F-002 against the shell
process environment.

*Acceptance criterion*: If `XDG_CURRENT_DESKTOP=Hyprland` is present in the
shell but missing from the systemd user manager, the diagnostic reports a
warning that D-Bus-activated applications may inherit an incomplete desktop
environment.

**REQ-F-004** (Conditional)  
When `dbus-update-activation-environment` is available, the diagnostic shall
report whether the current process environment appears suitable for D-Bus
activation.

*Acceptance criterion*: Missing `DBUS_SESSION_BUS_ADDRESS` is reported as an
error; missing desktop variables are reported as warnings with the suggested
bootstrap command.

### 3.2 XDG Menu and KDE Cache Diagnostics

**REQ-F-005** (Ubiquitous)  
The service shall validate `XDG_MENU_PREFIX` by searching
`${XDG_CONFIG_HOME}/menus`, each directory in `XDG_CONFIG_DIRS`, and
`/etc/xdg/menus` for `${XDG_MENU_PREFIX}applications.menu`.

*Acceptance criterion*: If `XDG_MENU_PREFIX=hyprland-` and no
`hyprland-applications.menu` exists, the diagnostic reports a warning even
though the variable is non-empty.

**REQ-F-006** (Ubiquitous)  
If `XDG_MENU_PREFIX` is unset or empty, the service shall report the condition as
a warning when `kbuildsycoca6` is present.

*Acceptance criterion*: The existing Dolphin/KDE warning remains visible on KDE
frameworks systems with no menu prefix.

**REQ-F-007** (Ubiquitous)  
The service shall report available application menu files discovered under the
XDG menu search paths.

*Acceptance criterion*: The diagnostic output includes candidates such as
`arch-applications.menu`, `lxqt-applications.menu`, or
`hyprland-applications.menu` when present.

**REQ-F-008** (Ubiquitous)  
When `kbuildsycoca6` is present, the service shall check whether at least one
`$XDG_CACHE_HOME/ksycoca6*` file exists.

*Acceptance criterion*: Missing sycoca files produce a warning with the suggested
action "Rebuild application caches".

**REQ-F-009** (Ubiquitous)  
When `kbuildsycoca6` is present, the service shall compare the newest relevant
`.desktop` file mtime against the newest `ksycoca6*` cache mtime.

*Acceptance criterion*: If an application desktop file is newer than all sycoca
caches, the diagnostic reports the KDE application cache as stale.

### 3.3 MIME/Application Cache Diagnostics and Repair

**REQ-F-010** (Ubiquitous)  
The service shall check user MIME defaults for the common roles already covered
by `MimeService`: browser, terminal, file manager, image viewer, text editor,
and video player.

*Acceptance criterion*: The diagnostic includes the desktop file returned by
`xdg-mime query default` or `xdg-settings get default-web-browser` for each role.

**REQ-F-011** (Ubiquitous)  
The service shall check for `mimeinfo.cache` files in each application directory
returned by `DesktopEntryScanner::defaultApplicationDirs()`.

*Acceptance criterion*: Missing cache files are reported as informational for
read-only system dirs and as warnings for user-writable app dirs that contain
`.desktop` files.

**REQ-F-012** (User-triggered)  
The service shall provide a `rebuildApplicationCaches()` invokable that runs
cache rebuild commands in sequence:

1. `update-desktop-database <dir>` for each existing user-writable application
   directory.
2. `kbuildsycoca6 --noincremental` when `kbuildsycoca6` is present.

*Acceptance criterion*: The method never invokes `sudo`, never writes to
system-owned directories, reports per-step success/failure, and refreshes
diagnostics after completion.

**REQ-F-013** (Event-driven)  
After a successful rebuild, the service shall notify `MimeService` and
`LauncherService` to refresh their in-memory state.

*Acceptance criterion*: Default-app UI and launcher candidates update without
restarting the shell.

### 3.4 Portal and D-Bus Service Diagnostics

**REQ-F-014** (Ubiquitous)  
The service shall report whether `org.freedesktop.portal.Desktop` is owned on
the session bus.

*Acceptance criterion*: If the portal broker is absent, the diagnostic reports an
error with suggested packages/services.

**REQ-F-015** (Ubiquitous)  
The service shall report active portal implementation names matching
`org.freedesktop.impl.portal.*`.

*Acceptance criterion*: A healthy Hyprland session lists at least
`org.freedesktop.impl.portal.desktop.hyprland` plus a toolkit backend such as
GTK or KDE where installed.

**REQ-F-016** (Ubiquitous)  
The service shall report the owner of `org.freedesktop.ScreenSaver`.

*Acceptance criterion*: If the owner is `hypridle`, the diagnostic distinguishes
between "owned by external daemon" and "HoloNight registered it"; if unavailable,
the diagnostic warns that idle-aware applications may not integrate correctly.

### 3.5 Installable Bootstrap Artifacts

**REQ-F-017** (Ubiquitous)  
The project shall install a desktop session bootstrap script that exports the
recommended HoloNight/Hyprland desktop environment and imports it into D-Bus and
systemd user activation environments.

*Acceptance criterion*: The script runs without root, exits non-zero on missing
Hyprland, and includes the equivalent of:

- `dbus-update-activation-environment --systemd ...`
- `systemctl --user import-environment ...`

**REQ-F-018** (Ubiquitous)  
The project shall install a Wayland session desktop file for a HoloNight-managed
Hyprland session.

*Acceptance criterion*: Installing to a prefix creates a session file under
`share/wayland-sessions/` whose `Exec=` invokes the bootstrap script.

**REQ-F-019** (Conditional)  
When UWSM is available, the bootstrap path shall support starting Hyprland
through UWSM without duplicating compositor lifecycle management.

*Acceptance criterion*: Documentation explains the UWSM and non-UWSM start
commands and the diagnostic script can identify which mode is active.

### 3.6 Smoke Check Tooling

**REQ-F-020** (Ubiquitous)  
The repository shall include a live-session smoke check script for desktop
integration.

*Acceptance criterion*: Running the script prints shell/systemd environment
values, portal D-Bus owners, ScreenSaver owner, MIME defaults, XDG menu files,
sycoca cache paths, and suggested next actions.

## 4. Non-Functional Requirements

**REQ-NF-001** (Performance)  
Startup diagnostics shall not block the QML thread.

*Acceptance criterion*: All subprocess and D-Bus checks execute asynchronously or
from a worker; opening the sidebar does not freeze.

**REQ-NF-002** (Safety)  
Repair actions shall be user-scoped and non-destructive.

*Acceptance criterion*: No repair path deletes files, rewrites user MIME
defaults, invokes `sudo`, or writes outside user-writable directories.

**REQ-NF-003** (Observability)  
Each diagnostic row shall include enough detail for a user or developer to run
the equivalent shell command manually.

*Acceptance criterion*: The UI or smoke script prints the failing check, observed
value, expected value, and suggested command.

**REQ-NF-004** (Graceful degradation)  
Missing optional tools shall not crash the shell.

*Acceptance criterion*: Missing `kbuildsycoca6`, `update-desktop-database`,
`systemctl`, `busctl`, or `dbus-update-activation-environment` produces an
informational diagnostic, not a fatal error.

## 5. Constraints

**REQ-C-001**  
HoloNight shall not become a session manager that supervises Hyprland itself.
Bootstrap artifacts may start Hyprland for login-session integration, but the
running shell process does not restart or control the compositor.

**REQ-C-002**  
HoloNight shall not implement portal, KIO, notification, keyring, or secret
service backends in this feature.

**REQ-C-003**  
The service shall avoid distro-specific package-manager commands. Diagnostics
may mention generic packages by name but shall not run `pacman`, `apt`, `dnf`, or
similar tools.

**REQ-C-004**  
Environment mutation is allowed only in the explicit bootstrap script or when a
user runs the documented command. The long-running shell process shall not call
`setenv()` to paper over a broken login environment.
