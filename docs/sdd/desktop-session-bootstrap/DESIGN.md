# desktop-session-bootstrap - Architecture Design

## 1. Overview

This feature closes the gap between "HoloNight is running in Hyprland" and
"applications launched in this session see a complete desktop environment".

The design has three layers:

1. **Bootstrap artifacts** - installable scripts/session files that establish
   the environment before Hyprland, portals, D-Bus activation, and HoloNight
   clients start.
2. **Diagnostics service** - a QML singleton that reports live session health
   and exposes safe repair actions.
3. **Smoke tooling** - a script that prints the same checks in a terminal for
   live-session debugging and bug reports.

The Dolphin "Open with..." issue is treated as a KDE framework cache health
problem. `xdg-mime` defaults can be correct while Dolphin's KDE sycoca database
is missing, stale, or built with an invalid XDG menu prefix. Therefore the
diagnostic must look beyond `XDG_MENU_PREFIX` being non-empty.

## 2. Components

### 2.1 SessionIntegrationService

**Files**

```text
src/services/session-integration/
    SessionIntegrationService.h
    SessionIntegrationService.cpp
    SessionIntegrationTypes.h
```

The service lives in `holonight_services`, matching `PortalService`,
`MimeService`, and `KdeCompatService`.

The public QML API:

```cpp
class SessionIntegrationService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(QString overallStatus READ overallStatus NOTIFY diagnosticsChanged FINAL)
  Q_PROPERTY(QVariantList diagnostics READ diagnostics NOTIFY diagnosticsChanged FINAL)
  Q_PROPERTY(bool refreshInProgress READ refreshInProgress NOTIFY refreshInProgressChanged FINAL)
  Q_PROPERTY(bool rebuildInProgress READ rebuildInProgress NOTIFY rebuildInProgressChanged FINAL)

 public:
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void rebuildApplicationCaches();

 Q_SIGNALS:
  void diagnosticsChanged();
  void refreshInProgressChanged();
  void rebuildInProgressChanged();
  void rebuildFinished(bool success);
};
```

`diagnostics` is a `QVariantList` of maps to keep QML simple:

```cpp
{
  "id": "xdg-menu-prefix",
  "title": "XDG menu prefix",
  "status": "warning",
  "observed": "hyprland-",
  "expected": "hyprland-applications.menu exists",
  "detail": "No matching menu file was found in XDG menu paths.",
  "command": "find ~/.config/menus /etc/xdg/menus -name '*applications.menu'"
}
```

`overallStatus` is derived from the worst diagnostic status:

- `ok` when all rows are `ok` or `info`
- `warning` when at least one row is `warning`
- `error` when at least one row is `error`

### 2.2 Diagnostic runner

The service should use small helper functions instead of embedding every check
inside the QObject:

```text
src/services/session-integration/
    EnvironmentDiagnostics.h/.cpp
    KdeCacheDiagnostics.h/.cpp
    PortalDiagnostics.h/.cpp
    ApplicationCacheRebuilder.h/.cpp
```

The helpers return value types and accept injected seams for tests:

```cpp
class CommandExecutor {
 public:
  virtual ~CommandExecutor() = default;
  virtual void run(QString program, QStringList args,
                   std::function<void(CommandResult)> callback) = 0;
};
```

Production uses `QProcess` with timeouts. Tests use a fake executor.

### 2.3 Environment checks

The service reads process environment directly through `qgetenv()` for the shell
process values.

For the systemd user manager environment, it runs:

```bash
systemctl --user show-environment
```

The parser handles `KEY=value` lines and ignores malformed lines. If systemd user
is unavailable or inaccessible, the diagnostic is informational because the shell
can still run in non-systemd sessions.

The required variables are:

```text
WAYLAND_DISPLAY
XDG_CURRENT_DESKTOP
XDG_SESSION_DESKTOP
XDG_SESSION_TYPE
XDG_MENU_PREFIX
XDG_DATA_DIRS
XDG_CONFIG_DIRS
DBUS_SESSION_BUS_ADDRESS
```

Recommended Hyprland values:

```text
XDG_CURRENT_DESKTOP=Hyprland
XDG_SESSION_DESKTOP=Hyprland
XDG_SESSION_TYPE=wayland
XDG_MENU_PREFIX=hyprland-
```

`XDG_MENU_PREFIX=hyprland-` is not automatically considered healthy; it must
match an existing `hyprland-applications.menu` file.

### 2.4 XDG menu validation

Menu search paths are built from:

1. `$XDG_CONFIG_HOME/menus` or `~/.config/menus`
2. Each entry in `$XDG_CONFIG_DIRS`, plus `/menus`
3. `/etc/xdg/menus` as a fallback if not already present

The selected menu file is:

```text
${XDG_MENU_PREFIX}applications.menu
```

Examples:

```text
hyprland-applications.menu
arch-applications.menu
plasma-applications.menu
```

If the selected file is missing but another `*-applications.menu` file exists,
the diagnostic should recommend one of:

- create a matching menu file or symlink in `~/.config/menus`
- change the bootstrap `XDG_MENU_PREFIX` to the installed menu prefix
- leave the prefix empty only if the distribution's unprefixed menu exists

The service must not create symlinks automatically.

### 2.5 KDE sycoca checks

KDE sycoca is user-cache state and is safe to inspect. Paths:

```text
$XDG_CACHE_HOME/ksycoca6*
~/.cache/ksycoca6*
```

The diagnostic compares mtimes:

- newest `.desktop` file in application dirs
- newest `mimeinfo.cache` in application dirs
- newest `ksycoca6*` file

If a desktop file or `mimeinfo.cache` is newer than sycoca, report stale KDE
cache. If no sycoca file exists while `kbuildsycoca6` exists, report missing KDE
cache.

The existing `KdeCompatService` should either be replaced by this service or
kept as a thin wrapper during migration. The new service owns the broader
diagnostic; the old "prefix is empty" condition is insufficient.

### 2.6 Application cache rebuild

`ApplicationCacheRebuilder` runs sequentially:

1. Find user-writable app dirs:
   - `$XDG_DATA_HOME/applications`
   - `~/.local/share/applications`
   - user Flatpak export dir if present
2. For each dir containing `.desktop` files, run:

```bash
update-desktop-database <dir>
```

3. If `kbuildsycoca6` exists, run:

```bash
kbuildsycoca6 --noincremental
```

The rebuilder records each step:

```cpp
struct RebuildStep {
  QString command;
  int exit_code;
  QString stderr_text;
  bool success;
};
```

System dirs such as `/usr/share/applications` are inspected but not passed to
`update-desktop-database` unless they are writable by the current user. This
avoids hidden privilege assumptions.

After the rebuild:

- call `SessionIntegrationService::refresh()`
- call `MimeService::refreshAllRoles()`
- trigger launcher validation or refresh through an existing public slot/signal
  if available; otherwise add a narrow public slot to `LauncherService`

### 2.7 Portal and D-Bus checks

This feature does not replace `PortalService`. It adds coarse health checks that
are useful in the same diagnostic panel and smoke script:

- `org.freedesktop.portal.Desktop` owner exists
- `org.freedesktop.impl.portal.desktop.hyprland` is active or activatable
- at least one toolkit backend, usually GTK or KDE, is active or activatable
- `org.freedesktop.ScreenSaver` owner and PID/process name
- `org.kde.StatusNotifierWatcher` owner, to detect tray ownership conflicts
- `org.freedesktop.Notifications` owner, to detect notification daemon conflicts

The service should prefer Qt D-Bus APIs for in-process checks. The smoke script
can use `busctl --user`.

### 2.8 Bootstrap script

**File**

```text
scripts/holonight-hyprland-session
```

Responsibilities:

1. Export default desktop variables when the login manager did not provide them.
2. Import those variables into D-Bus activation and systemd user manager.
3. Start Hyprland through UWSM when requested/available, otherwise start Hyprland
   directly.

The script should be conservative:

```bash
export XDG_CURRENT_DESKTOP="${XDG_CURRENT_DESKTOP:-Hyprland}"
export XDG_SESSION_DESKTOP="${XDG_SESSION_DESKTOP:-Hyprland}"
export XDG_SESSION_TYPE="${XDG_SESSION_TYPE:-wayland}"
export XDG_MENU_PREFIX="${XDG_MENU_PREFIX:-hyprland-}"

dbus-update-activation-environment --systemd \
  WAYLAND_DISPLAY DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP \
  XDG_SESSION_TYPE XDG_MENU_PREFIX XDG_DATA_DIRS XDG_CONFIG_DIRS \
  QT_QPA_PLATFORM QT_QPA_PLATFORMTHEME QT_STYLE_OVERRIDE

systemctl --user import-environment \
  WAYLAND_DISPLAY DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP \
  XDG_SESSION_TYPE XDG_MENU_PREFIX XDG_DATA_DIRS XDG_CONFIG_DIRS \
  QT_QPA_PLATFORM QT_QPA_PLATFORMTHEME QT_STYLE_OVERRIDE
```

Do not run cache rebuilds in the login path. Cache rebuild remains an explicit
repair action because it can be slow and noisy.

### 2.9 Wayland session desktop file

**File**

```text
data/wayland-sessions/holonight-hyprland.desktop
```

Content:

```ini
[Desktop Entry]
Name=HoloNight (Hyprland)
Comment=Hyprland session with HoloNight desktop integration
Exec=holonight-hyprland-session
TryExec=Hyprland
DesktopNames=Hyprland
Type=Application
```

CMake installs it to:

```text
${CMAKE_INSTALL_DATADIR}/wayland-sessions/
```

The script installs to:

```text
${CMAKE_INSTALL_BINDIR}/
```

Use `GNUInstallDirs` if the project does not already include it.

### 2.10 Smoke check script

**File**

```text
scripts/check-desktop-integration.sh
```

Output sections:

```text
Environment:
  process env from current shell
  systemctl --user show-environment diff

Portals:
  org.freedesktop.portal.Desktop owner
  org.freedesktop.impl.portal.* names

D-Bus desktop services:
  org.freedesktop.ScreenSaver owner
  org.freedesktop.Notifications owner
  org.kde.StatusNotifierWatcher owner

MIME/default apps:
  xdg-mime query default inode/directory
  xdg-mime query default text/plain
  xdg-settings get default-web-browser

XDG menus:
  XDG_MENU_PREFIX
  discovered *applications.menu files
  selected menu exists yes/no

KDE caches:
  kbuildsycoca6 path
  ksycoca6 cache files and mtimes
```

The script should not change state. It is a read-only diagnostic.

## 3. UI Integration

Add a compact "Session Integration" section to `SidebarSystem.qml`, below the
existing default-app rows and above or replacing the old KDE-specific warning.

The UI should show:

- one status row: Healthy / Needs attention / Broken
- a small list of failing diagnostics only
- a "Refresh" button
- a "Rebuild application caches" button when KDE/MIME cache diagnostics warn

Do not show every successful diagnostic in the sidebar by default. The full list
belongs in logs or the smoke script.

## 4. Testing Strategy

Unit tests should cover parsers and decision logic without depending on the live
desktop:

- parse `systemctl --user show-environment`
- derive XDG menu search paths
- validate selected menu file exists/missing
- classify missing/stale sycoca cache
- choose user-writable app dirs for `update-desktop-database`
- run rebuild sequence with fake command executor
- derive overall status from diagnostic rows

Live checks are manual/smoke tests because they depend on Hyprland, portals,
D-Bus activation, KDE tools, and systemd user state.

## 5. Migration Notes

`KdeCompatService` currently handles only one condition:

```text
kbuildsycoca6 present and XDG_MENU_PREFIX empty
```

This should be migrated into `SessionIntegrationService`. During migration, keep
the QML-visible `KdeCompatService` API only if needed to avoid touching
unrelated UI in the same patch. Long term, the broader service should be the only
source of session/cache diagnostics.
