# HoloNight Shell

For current build, test, container acceptance and provider workflows, see [DEVELOPMENT.md](DEVELOPMENT.md).

A dark, holographic Wayland shell for Hyprland. Built with C++23 and Qt6/QML.

HoloNight is designed around TokyoNight Storm colors, thin HUD geometry, and restrained neon. Dark structure first — glow communicates state, not decoration.

## Implemented

### Top bar

| Section | Description |
|---|---|
| Distro logo | Reads `/etc/os-release`, resolves icon from XDG theme |
| Workspaces | `ext-workspace-v1` protocol + Hyprland IPC sync; per-state glow (active, urgent, occupied) |
| Active window | Per-monitor tracking via Hyprland IPC; category icon from XDG desktop files; gradient fade on truncated titles |
| Media (MPRIS) | Now-playing pill for any `org.mpris.MediaPlayer2.*` player — app icon, artist—title (gradient fade on truncation), and play/pause/next/previous controls gated by the active player's reported capabilities. Auto-selects the most recently active Playing player, falling back to Paused; collapses to zero width when no player is present. Commands are fire-and-forget over D-Bus — controls reflect the real player state only after it reports back, never optimistically |
| Network | WiFi strength + SSID via NetworkManager D-Bus |
| Audio | Volume level + muted state via PipeWire/PulseAudio-compatible control (libpulse) |
| Battery | Charge percent + charging state via UPower D-Bus |
| Keyboard layout | Live layout code (e.g. `EN`, `UA`) via Hyprland IPC |
| System tray | StatusNotifierItem (SNI) protocol; attention glow on urgent items |
| Weather | Open-Meteo or OpenWeatherMap current/details/hourly/daily forecasts and graphs. Saved/external settings update live. Manual city/coordinates or explicit IP geolocation; provider/location-specific offline cache |
| Clock | Time (`HH:mm`) and date (`ddd d MMM`) with Rajdhani font; click opens the right sidebar |
| Session | Canvas-drawn power button (top bar); session actions (lock, log out, sleep, reboot, shut down) are in the right sidebar |
| Status popups | Click Network / Audio / Battery / Keyboard to open a positioned popup below the widget (notch aligned to widget center, per-id sizing) via `StatusPopupSurface`; dismiss by re-clicking, switching widgets, clicking outside, or Esc. The Audio popup is a mini-pavucontrol (Output / Input / Applications tabs with per-row mute + volume slider, click-row-to-set-default, and a Master Volume bar). The Battery popup is a compact panel: charge percent + fill bar, charge state with time-to-empty/full, conditional Health and Cycle-count rows, and a power-profile selector (Power Saver / Balanced / Performance circle buttons) backed by power-profiles-daemon — hidden when the daemon is absent |
| Tooltips | 450 ms hover-delay tooltips on every widget via `TooltipSurface` |
| Notifications | In-process freedesktop notification daemon (owns `org.freedesktop.Notifications`); HUD-styled toasts on the focused monitor with actions, body markup (`<b>/<i>/<u>/<a>`), per-urgency/category accent (critical red, IM/call/presence violet, else cyan), hover-pause timeouts, replace semantics, and a per-monitor critical-priority overflow queue (`max_visible`, default 3). Critical notifications (urgency=2) are persistent until dismissed and always bypass DND. Configurable via `[notifications]`. A topbar bell icon shows a pulsing violet badge when unread history entries exist; click opens the sidebar. History is persisted to `~/.local/state/holonight/notifications/history.json` (XDG_STATE_HOME). Dismissed notifications are never stored. Configurable via `[notifications.history]`. If another notification daemon already owns the bus name at startup, a diagnostic appears in the sidebar Notifications tab |

### Wallpaper

A full-screen surface on the `wlr-layer-shell` background layer, behind the bar. Wallpapers are configured per monitor via `[background] images` — the first image maps to the first monitor, the second to the second, and so on (extra images are ignored; if there are more monitors than images, the last image repeats). Images are cropped to fill (`PreserveAspectCrop`), and runtime config changes crossfade to the new wallpaper. With no image (or a path that fails to load) the surface falls back to the `surfaceVariant` theme color.

### Desktop widgets

Non-interactive widgets on the `wlr-layer-shell` `bottom` layer (above the wallpaper, below windows), configured as `[[widget]]` entries. Each widget is anchored to one of nine positions (`left-top`, `center-top`, `right-top`, `left-center`, `center-center`, `right-center`, `left-bottom`, `center-bottom`, `right-bottom`) with a uniform `[widgets] margin`; top-anchored widgets clear the bar automatically. Target specific monitors by output name (`monitors = ["DP-1"]`) or leave the list empty for all monitors — unknown names are honored on hotplug. With a connected compositor that supports occupancy (including Hyprland and Sway), widget content is shown only while its monitor's visible workspace is empty. On labwc and other backends without occupancy, or while compositor integration is disconnected or absent, content stays visible on the bottom layer so application windows naturally cover it. Surfaces are cached across visibility changes and content refreshes before reveal. If two widgets claim the same position on the same monitor, the earlier one in config order wins (the loser is dropped on that monitor with a single warning). Any widget can be turned off in place with `enabled = false` (it creates no surface and no longer blocks a position). Three widget types exist today:

- `time-to-event` — a title, a glowing adaptive countdown (`12d 04h 37m`, or with `show_seconds`, `…37m 12s`; `Now` once reached), and the event date below.
- `clock` — a large 24-hour `HH:mm` with smaller inline glowing seconds (`HH:mm:ss`, the seconds at half size) over a localized date row (`Sunday, 8 June 2026` by default). Optional `date_format` (Qt date pattern) and `locale` (BCP-47) keys; `show_seconds = false` drops the seconds and ticks per minute.
- `mpris` — a large "now playing" panel (artwork, title/artist/album, progress bar, elapsed/duration, player icon) mirroring whichever player the topbar Media pill shows. Artwork uses a bundled media placeholder when empty or unresolvable (`file://`, `http(s)://`, and inline base64 `data:` artwork URLs are supported); the player identity remains a separate icon badge. Dims when the player pauses and fades out entirely after `pause_hide_minutes` (default 10) of continuous pause; resuming playback or switching the active player resets that timer.

Occupancy loss enables the visible fallback; recovery restores occupancy-based visibility. Covered widgets may continue updating: window coverage does not suspend timers. On labwc, the existing native `ToggleShowDesktop` action (Super+D) reveals the widgets without any shortcut changes.

### On-screen display (OSD)

A transient overlay on the `wlr-layer-shell` `overlay` layer that reports volume, brightness, and keyboard-layout changes. Level channels (volume, brightness) render an icon, a channel label, a progress bar, and a percentage; the keyboard-layout channel renders the short layout code large (`EN`) with the full name below it and no bar. Muted audio dims the bar and reads `Muted` while the fill stays at the real volume, so unmuting does not look like a jump from zero.

The overlay is purely event-driven — no surface exists until the first change, so nothing appears at login, and a two-second startup grace period keeps services settling into their initial state from flashing an overlay. It opens on the focused monitor (resolved per event, falling back to the primary output) and sets an empty input region, so clicks pass through to the window underneath. Repeat changes on the same channel update the value in place and restart the hide timer; a change on a *different* channel replaces the content immediately rather than hiding first.

Redundant overlays are suppressed: volume changes show no OSD while the topbar audio popup is open, and brightness changes show none while the sidebar's Quick Settings tab is open, since both already display the value. Keyboard-layout changes are never suppressed.

Configurable via `[osd]` — master switch, timeout (default 1500 ms after the last change), one of the nine widget anchors (default `center-bottom`), and a per-channel opt-out for each of volume, brightness, and keyboard layout.

### AI chat integration

HoloNight Shell can toggle the standalone `holonight-chat` quick panel through
its session D-Bus service without loading chat code or QML into the shell
process. The shell forwards the focused monitor name, so a Hyprland keybind can
open the panel on the output where the user is currently working:

```ini
bind = SUPER, A, exec, printf 'chat:toggle:' | nc -N -U "$XDG_RUNTIME_DIR/holonight-shell/control.sock"
```

The `holonight-chat` D-Bus activation service must be installed. The first
toggle starts the application when necessary; later toggles collapse the
workspace into the panel or restore it. See
[`docs/config.md`](docs/config.md#ai-chat-keybind) for fixed-output routing.

### Right sidebar

A persistent per-monitor layer-shell panel anchored to the top-right of each output, opened by clicking the clock widget or notification bell. Six tabs — Overview, Calendar, Notifications, System, Quick Settings, Media — with a 64px Canvas-drawn icon tab bar. Three-phase animated open/close (grow down → expand content → content fade-in); tab switching animates width only. Click outside or Esc to dismiss; mutual exclusion ensures only one monitor's sidebar is open at a time.

**System tab** (implemented): default application role pickers and KDE environment diagnostics.
- *Default Applications* — six inline `ComboBox` rows (Browser, Terminal, File Manager, Image Viewer, Text Editor, Video Player). Each picker lists only installed apps that declare the relevant MIME type in their `.desktop` file; reads and writes via `xdg-mime` (async, non-blocking). Both the candidate list and the selected default update live (within ~500ms) when apps are installed/removed or when `mimeapps.list` / `mimeinfo.cache` changes — no shell restart required.
- *KDE cache diagnostics* — detects `kbuildsycoca6` present in PATH but `XDG_MENU_PREFIX` unset (the root cause of Dolphin not opening files on Arch). Shows a warning row with a "Rebuild caches" button that runs `update-desktop-database` then `kbuildsycoca6 --noincremental`; hidden entirely on non-KDE systems.

**Quick Settings tab** (implemented):
- *Keep Awake* — toggle that acquires a logind idle inhibitor; see the Idle management section below.
- *Brightness* — slider (0–100%) backed by `SysfsBackend`; hidden automatically when no backlight device is present (`/sys/class/backlight/` is empty). Writes via `org.freedesktop.login1.Session.SetBrightness`; tracks external Fn-key changes via inotify with a drag-guard so in-flight drags are not interrupted.
- *Charge limit* — read-only row showing the hardware charge-limit percentage (e.g. "80%"); hidden when the device does not expose `ChargeEndThreshold` via UPower or sysfs (`charge_control_end_threshold`).
- *Sleep inhibitors* — list of processes currently holding a logind sleep inhibitor (who + why), polled every 5 seconds; hidden when there are none. Useful for diagnosing why the system refuses to suspend.

**Background power behaviors** (automatic, no UI toggle needed):
- *Low battery notifications* — fires a desktop notification at 20 % (warning) and 10 % (critical) while discharging; each threshold fires at most once per discharge cycle and resets on charging, full charge, or battery re-insertion. Thresholds are configurable via `holonight/power/warningThreshold` and `holonight/power/criticalThreshold` in QSettings.
- *Lid-close activity gating* — pauses background activity (e.g. suspend-inhibitor polling) when the laptop lid is closed; resumes when opened. Desktop systems without a lid are detected and skipped automatically.

**Notifications tab** (implemented): DND mode and per-app notification rules.
- *Do Not Disturb* — runtime-only manual toggle (disc button, red glow when active). While DND is on, all non-critical notifications are suppressed; critical urgency always breaks through.
- *Per-app rules* — auto-populated from apps seen this session. Each row shows a toggle (enable/disable all) and a urgency filter (All / Block Low / Block Normal / Block Low+Normal). Rules are session-scoped and cleared on restart.
- *Daemon conflict diagnostic* — if another process owns `org.freedesktop.Notifications` at startup, a ⚠ warning row shows the owner name and explains why HoloNight notifications are inactive.

**Overview tab** (implemented): a single-scroll panel with four sections:
- *Calendar* — month view with Mon/Sun week-start (configurable via `[calendar]`), today highlighted in cyan with violet glow, weekend days in violet, adjacent-month days muted. Prev/next month navigation.
- *Upcoming* — next 3 events from the coming 7 days, pulled from configured CalDAV accounts and ICS feeds. Shows event title, time label (cyan "Now", yellow "in Xm", subdued for later), a description excerpt (first 100 characters), and a "View all ›" link. States: loading spinner, connection error (with error detail), offline indicator, "no events" placeholder. Triggers a sync on sidebar open if the last sync is overdue. CalDAV syncs every 15 minutes; ICS feeds sync every 60 minutes; both are paused when battery is below 10 %. Requires at least one `[calendar.caldav.<name>]` or `[calendar.ics.<name>]` entry (see Configuration).
- *Notifications* — last 3 notification groups (by app), each with icon, app name, latest summary, unread count, and relative time. "+N notifications / View all" link switches to the Notifications tab.
- *Session bar* — four icon buttons pinned to the bottom: Log out, Sleep, Reboot, Shut down (via `loginctl`). A failed dispatch (e.g. the target binary vanished from `PATH`) surfaces as a notification toast instead of failing silently.

## Planned

- Remaining sidebar tabs — Calendar, Media (full content)
- App launcher — keyboard-first with search, pinned apps, recent files
- Dynamic accent extraction — wallpaper-driven accent colors blended with HoloNight defaults

### Idle management

The shell owns `org.freedesktop.ScreenSaver` on the session bus and implements `GetSessionIdleTime()` / `Inhibit()` / `UnInhibit()` / `ActiveChanged` — the interface Teams, Zoom, and Slack use to detect user idle and transition to "Away". A Keep Awake toggle in the sidebar acquires a logind idle inhibitor while enabled. Calendar and weather polling are paused automatically while idle.

Requires **hypridle ≥ 0.2.0** (or swayidle) to track idle time via `ext-idle-notify-v1`. If neither is running, a persistent notification appears at startup. With hypridle < 0.2.0, `GetSessionIdleTime` / `ActiveChanged` are unavailable because hypridle 0.1.x claims the bus name without `AllowReplacement`.

## Requirements

- Qt ≥6.11 (`Core`, `Gui`, `Quick`, `Qml`, `QuickControls2`, `WaylandClient`, matching `WaylandClientPrivate`, `DBus`, `Network`, `Concurrent`, `Sql`)
- CMake 3.25+
- **hypridle ≥ 0.2.0** or swayidle — recommended; shell runs without them but idle detection and Teams/Zoom "Away" status are disabled
- Ninja
- `wayland-client`, `wayland-protocols`
- `tomlplusplus`
- `libpulse`
- `libsecret-1` (for CalDAV password lookup from the system keyring)
- Polkit agent development files and polkit-qt6 0.200.0+
- Installed GTest ≥1.14 with GMock and Qt Test/QuickTest (for tests)
- Python 3, bubblewrap, D-Bus and Sway for isolated test acceptance (no live session required)

See [`docs/dev-setup.md`](docs/dev-setup.md) for the qualified Arch CI environment.

## Building

All workflows go through [`task`](https://taskfile.dev):

See [DEVELOPMENT.md](DEVELOPMENT.md) for current native and container commands.

**Without `task`:**

See [DEVELOPMENT.md](DEVELOPMENT.md) for current native and container commands.

```bash
task install:system  # Release build, install to /usr using sudo
task install:local   # Release build, install to ~/.local; task install is an alias
```

These tasks use separate `build/system-install/` and `build/local-install/`
directories, with tests and coverage disabled. They reload the current user's
systemd unit files after installation. Existing services are not restarted.

To inspect a system installation without changing `/usr`:

```bash
task install:stage   # builds in build-install-stage/, stages under build-stage/usr/
```

For a direct CMake installation:

```bash
cmake -S . -B build-install-system -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-install-system -j$(nproc)
sudo cmake --install build-install-system
systemctl --user daemon-reload
```

Integration files are generated at installation time, so `cmake --install
--prefix <prefix>` also updates their paths. `DESTDIR` stages files without
embedding the staging directory in launchers or units. Askpass aliases resolve
to the installed helper executable; the test automation executable is never
installed.

Uninstall uses the corresponding dedicated install manifest and asks for
confirmation:

```bash
task uninstall         # local installation
task uninstall:system  # system installation
```

To preview the files without removing them:

```bash
python3 scripts/uninstall.py --manifest build-install-system/install_manifest.txt --prefix /usr
```

Reinstall once with the updated task before using these uninstall commands for
an older installation. Manifests are validated against the selected prefix
before any file is removed; user configuration and dependency packages are
outside the shell's install manifest.

The standalone `xdg-desktop-portal-holonight` package owns the Settings and FileChooser
descriptor and activation service. The shell installs routing configurations:

```text
share/xdg-desktop-portal/holonight-portals.conf
share/holonight/xdg/xdg-desktop-portal/hyprland-portals.conf
share/holonight/xdg/xdg-desktop-portal/sway-portals.conf
```

After installing or updating these files, log out and back in, or restart the
user portal broker after `holonight-shell` is running:

```bash
systemctl --user restart xdg-desktop-portal
```

Check the installed version:

```bash
holonight-shell --version    # -> holonight-shell 0.1.0
```

### Desktop session entry

Installing also provides `HoloNight (Hyprland)`, `HoloNight (Sway)`, and `HoloNight (labwc)` Wayland
login entries. Use `task install:system` to make them available to the display
manager; display managers generally do not discover entries under `~/.local`.

Select the matching entry in the display manager. `holonight-session` requires
UWSM and a working systemd user manager in both `auto` (default) and `uwsm` mode.
There is no automatic fallback to unmanaged startup. Install the chosen compositor
(Hyprland with `start-hyprland`, Sway, or labwc) and UWSM before logging in.
One graphical session per user manager is supported.

Managed identities are `HoloNight-Hyprland:HoloNight:Hyprland`,
`HoloNight-Sway:HoloNight:sway`, and `HoloNight-Labwc:HoloNight:labwc`.
The installed `share/uwsm/env-holonight` hook prepares Qt, cursor, askpass and
installation paths. It prepends `~/.local/bin` if absent, even before that directory
exists; existing PATH entries retain their order. Interactive shell configuration
is not sourced. Put environment overrides in `~/.config/uwsm/env` or
`~/.config/uwsm/env-holonight` (or the equivalent under `$XDG_CONFIG_HOME`). User
files run after installed defaults. UWSM propagates and cleans up activation
environments; the shell service inherits its manager's PATH.

Managed compositor defaults live in `share/holonight/session`. Each login composes
a private configuration under `$XDG_RUNTIME_DIR/holonight/`. Optional overrides:

- `$XDG_CONFIG_HOME/holonight/session/hyprland.lua`
- `$XDG_CONFIG_HOME/holonight/session/sway.conf`
- `$XDG_CONFIG_HOME/holonight/session/labwc/rc.xml`, `menu.xml`, and `themerc-override`

`XDG_CONFIG_HOME` defaults to `~/.config`. Hyprland automatically loads
`$XDG_CONFIG_HOME/hypr/hyprland.lua` as its base when present, preserving personal
monitor scales, shortcuts and startup callbacks. Its directory is prepended to
Lua's module search path (`?.lua` and `?/init.lua`); the entrypoint and required
modules are tracked for automatic reload. Errors appear in Hyprland's diagnostics
without replacing the personal configuration with defaults. When the entrypoint
is absent, Hyprland uses the shipped monitor and keybinding defaults.
Sway and labwc continue to use shipped defaults.

The managed Hyprland default `misc.disable_xdg_env_checks = true` acknowledges
HoloNight's desktop identity before user settings load. Overrides follow the base
configuration, then HoloNight installs its readiness callback. Shipped
defaults provide native input/output behavior, basic focus/close/workspace bindings,
and Super+E to launch HoloNight Files through `uwsm app`. labwc retains native
keyboard/mouse defaults. Configure your preferred terminal and styling explicitly.

The compositor calls `holonight-session-finalize` with its display and compositor
marker. UWSM waits for `HOLONIGHT_SESSION_READY` before graphical-session activation;
its bounded timeout handles incomplete startup. Finalization then queues the shell
and session-specific Polkit agent. Both stop with the graphical session. Restarting
the shell leaves applications running; logging out through UWSM stops managed apps.
No services are globally enabled at login.

Portal defaults live in `share/xdg-desktop-portal/holonight-<compositor>-portals.conf`.
Settings and FileChooser prefer `holonight;gtk`. Both activate the standalone
backend independently of the shell. Hyprland or wlr supplies compositor capabilities. Install
`xdg-desktop-portal`, `xdg-desktop-portal-gtk`, and the selected compositor backend
(`xdg-desktop-portal-hyprland` or `xdg-desktop-portal-wlr`); missing components warn
without blocking login. User and administrator portal overrides retain standard
[search precedence](https://flatpak.github.io/xdg-desktop-portal/docs/portals.conf.html).
Managed mode does not inject the legacy portal directory into `XDG_CONFIG_DIRS`.
Legacy configuration files remain installed for direct integration.

For manual integration, set `HOLONIGHT_SESSION_MODE=direct`. This explicitly imports
PATH and the desktop environment, uses the compositor's native configuration, and
leaves shell startup to you. Standalone `holonight-shell` inherits its host session.

Hyprland uses your existing native Lua configuration automatically; personal files
are never edited. For Sway and labwc, move desired settings into the optional
HoloNight overrides. Remove duplicate shell/finalization startup commands from
personal startup settings when using managed sessions.
Install changes, then log out and choose a HoloNight entry on the next login. Do not
restart the current desktop to apply this change. See the
[managed session design](docs/sdd/managed-wayland-sessions.md) for acceptance steps.

#### Developing in labwc

Install with `task install:system`, then log out yourself and select **HoloNight
(labwc)** in the display manager. The managed session requires `labwc` and `uwsm`
(including the UWSM labwc plugin). labwc 0.20.2
exports `WAYLAND_DISPLAY` and `LABWC_PID` on DRM startup; the UWSM plugin waits
for that marker before declaring the graphical session ready. The shell wrapper
also waits for both variables. No user labwc configuration or autostart file is
replaced; output scaling, theme, key bindings and terminal commands remain yours.

The bar uses labwc's actual named `ext-workspace-v1` workspaces and shows the focused
window title and application icon on each output where that window is visible. Other
outputs show Desktop. Window updates use `wlr-foreign-toplevel-management-unstable-v1`
without polling. Windows excluded by labwc's `skipTaskbar` rule are absent from this
protocol (see the [configuration manual](https://labwc.github.io/labwc-config.5.html)). This
integration does not provide numbered workspace slots, keyboard layout controls,
private widgets, or compositor-specific window activation.

Open a terminal with your existing labwc binding or the shell launcher. Inspect
startup and restart the installed shell with:

```bash
journalctl --user -u holonight-shell.service -b
journalctl --user -u wayland-wm@labwc.service -b
systemctl --user restart holonight-shell.service
```

For a development build, stop the installed shell and run the checkout:

```bash
systemctl --user stop holonight-shell.service
task run
# After stopping the development shell, restore the installed one:
systemctl --user start holonight-shell.service
```

Rebuilding or exiting the shell leaves labwc running. In explicit `direct` mode,
start `holonight-shell` or `task run` yourself from a terminal inside labwc.
Logout uses `uwsm stop` for a managed session and `labwc --exit` with a validated
inherited `LABWC_PID` for a direct session, as described in the
[labwc manual](https://labwc.github.io/labwc.1.html).

`task labwc-runtime-smoke` checks startup, real workspace listing and activation,
plugin isolation, and shell restart using temporary headless labwc configuration
and a private D-Bus. It disables labwc activation-environment updates and hides
the active desktop's runtime directory from the test.

After relogin, check automatic shell startup, terminal and launcher access,
popups, named workspace switching, locking, logout, and shell restart. To return
to Sway, log out and select **HoloNight (Sway)** at the display manager. Installation
and automated checks do not log out your current session.

After login, verify desktop integration:

```bash
holonight-session-doctor --check-executable hn-files
task compositor-smoke-check
```

The doctor compares selected caller, manager, shell, compositor, and portal broker
environments, executable resolution, readiness, effective portal preferences, and
running providers. It never imports variables, restarts services, or activates
providers. Exit codes are 0 for healthy checks, 1 for problems or incomplete checks,
and 2 for invalid arguments. Restricted process access is reported explicitly.
The source-tree `scripts/check-desktop-integration.sh` includes these checks alongside
its broader MIME, menu and cache audit.

The session-keyed Polkit startup wrapper supports UWSM compositors outside their
logind session scope. It verifies the Wayland socket's peer process against the
requested session before starting the agent. This discovery requires Python 3
and its standard library. Conflicting session/display state or multiple matching
compositors cause startup to fail rather than route a prompt to an uncertain
session. Existing authentication agents are never replaced automatically.

After choosing HoloNight as the Polkit agent and stopping the previous agent,
start the instance for the active session:

```bash
auth_session="${XDG_SESSION_ID:-$(loginctl show-seat seat0 -p ActiveSession --value)}"
systemctl --user start "holonight-polkit-agent@${auth_session}.service"
systemctl --user status "holonight-polkit-agent@${auth_session}.service"
```

The Polkit template is started by the HoloNight session launcher in UWSM mode;
it has no global `enable` step. Direct compositor sessions must start their
session's instance from their own startup configuration.

## Releases

Versions follow [Semantic Versioning](https://semver.org/); the version is the
single source of truth in `CMakeLists.txt` (`project(... VERSION ...)`) and is
plumbed into the binary's `--version` output. Notable changes are recorded in
[`CHANGELOG.md`](CHANGELOG.md). Maintainers cut releases with
`scripts/release.sh` — see [`RELEASING.md`](RELEASING.md) for the full process.
Each release publishes a source tarball; build from source as above.

## Architecture

This repository builds `holonight-shell` and owns the independently installable `HoloNightShellConfig::Config`
product-configuration package. The standalone Settings GUI consumes that package rather than defining Shell's
schema. Global appearance is a separate contract supplied by `HoloNight::Config`.

`holonight_platform` owns low-level Wayland, Hyprland IPC, and D-Bus helpers; `holonight_core` owns shared models,
the product-configuration service, and workspace/keyboard integration; `holonight_services` owns service adapters
exposed to QML; `holonight_surfaces` owns layer-shell surfaces, tray UI plumbing, and narrow presentation
orchestration over service state; `holonight_app` wires the shell runtime together from `apps/shell/app/`.
`apps/shell/main.cpp` remains the shell executable entry point, and tests link the smallest target that covers each
behavior.

QML is registered as the `HolonightShell` module with resource paths under `qrc:/HolonightShell/`. Shell QML files live under `apps/shell/qml/` and are registered by `apps/shell/CMakeLists.txt`. Compatibility QML components live under `qml/HoloNight/` and are registered as the `Holonight.Components` module — import with `import Holonight.Components`.

Qt Quick standard controls use `import QtQuick.Controls as Controls`, including enums and attached properties.
Use `Holonight.Core` for palette/primitives and `Holonight.Controls` for composites. Graphical executables
embed an overridable Holonight default; do not import a concrete style or select one imperatively.

Wayland client code is generated from XML files in `protocols/` plus the system `xdg-shell` protocol. Runtime integration expects Hyprland IPC, NetworkManager D-Bus, UPower D-Bus, PipeWire or PulseAudio (via libpulse), and StatusNotifierItem-compatible tray clients. The standalone `holonight-chat` application is optional; when installed, its D-Bus activation service backs the shell's AI quick-panel command. power-profiles-daemon D-Bus is optional — its profile selector in the battery popup is hidden when the daemon is not running. xdg-desktop-portal is optional — `PortalService` probes for it at startup and exposes availability diagnostics and system color-scheme/accent-color settings to QML; the shell runs normally without it.

## Configuration

Shell product behavior lives in `$XDG_CONFIG_HOME/holonight/config.toml`; global appearance lives independently in
`$XDG_CONFIG_HOME/holonight/appearance.toml`. Both fall back below `~/.config` when `XDG_CONFIG_HOME` is unset and
reload live without coupling their failure or save domains. Missing overrides use typed defaults; reads never create
configuration files or insert defaults. Invalid known values reject the document and preserve the running values;
startup errors use defaults with diagnostics. Remove an assignment to reset it. See [`docs/config.md`](docs/config.md) for both schemas.

Abbreviated example:

```toml
[bar.workspaces]
count = 5  # accepted: 3-10

[bar.systemtray]
max_items = 3  # accepted: 2-5

[background]
images = []  # per-monitor wallpapers, in monitor order; empty = solid surfaceVariant fill

[logo]
# file = "~/my-logo.svg"  # overrides the topbar logo; ignored with a warning if unreadable
generic = false            # true = always show the generic tux logo instead of the distro logo

[calendar]
week_start_day = "Mon"  # first day of each week column; accepted: "Mon" (default), "Sun"

# CalDAV accounts — one [calendar.caldav.<name>] section per account
[calendar.caldav.work]
url = "https://dav.example.com/calendars/user/default/"
username = "user@example.com"
password_keyring_key = "caldav-work"  # key name in the system keyring (libsecret)
# include = ["Work", "Team"]   # if set, only sync these calendar names (exact match)
# exclude = ["Birthdays"]      # calendar names to skip (applied after include)

# ICS feed accounts — one [calendar.ics.<name>] section per feed
[calendar.ics.holidays]
url = "https://calendar.google.com/calendar/ical/en.ukrainian%23holiday%40group.v.calendar.google.com/public/basic.ics"
# label = "Holidays"  # optional display label (defaults to section name)

[notifications]
default_timeout_ms = 5000  # auto-dismiss delay for normal/low urgency; must be > 0
max_visible = 3            # concurrent on-screen toasts per monitor; accepted: 1-10

[notifications.history]
enabled = true
max_items = 100            # oldest entries evicted when this count is exceeded
max_age_days = 14          # entries older than this are dropped on startup and after each write
persist_body = true        # set false to omit notification body from the JSON file (privacy)

[osd]
enabled = true             # master switch for the on-screen display
timeout = 1500             # ms visible after the last change; accepted: 300-10000
position = "center-bottom" # same nine anchors as desktop widgets

[osd.volume]
enabled = true             # false = volume changes show no OSD

[osd.brightness]
enabled = true

[osd.keyboard_layout]
enabled = true

[widgets]
margin = 32  # logical px between widgets and screen edges (top adds the bar height)

[[widget]]
type = "time-to-event"
title = "New Year"
deadline = "2027-01-01T00:00:00"  # ISO 8601 date or datetime (local time)
position = "center-bottom"        # one of the nine anchors; default center-center
monitors = []                     # output names, e.g. ["DP-1"]; empty = all monitors
show_seconds = true               # tick every second; false = per-minute, no seconds field
enabled = true                    # false = defined but not shown (any widget type)

[[widget]]
type = "clock"
position = "center-center"
show_seconds = true               # false = HH:mm only, ticks per minute
date_format = ""                  # Qt date pattern; empty = "dddd, d MMMM yyyy"
locale = ""                       # BCP-47 (e.g. "de_DE"); empty = system locale

[[widget]]
type = "mpris"
position = "center-bottom"
pause_hide_minutes = 10           # fades out after this many minutes continuously paused (1-60)

[weather]
provider = "open-meteo" # or openweathermap, which requires api_key
location_source = "manual" # or auto, which requires geo_api_key
city = "Lviv"
country = "Ukraine"
api_key = ""          # required only for OpenWeatherMap
geo_api_key = ""      # ipgeolocation.io key for automatic mode
# latitude = 49.83968  # uncomment to pin location and skip IP geolocation
# longitude = 24.02972
units = "metric"      # metric | imperial | standard
lang = "en"
refresh_interval = 600  # seconds; must be > 0
```

Paths may be absolute or start with `~/`. Any format `QImageReader` supports is accepted.

`[logo]` is read once at startup only — unlike every other section, editing it while the shell is running has no
effect until restart. Resolution order: `logo.file` (if readable) > `logo.generic` > known distro logo > the
pre-existing `/usr/share/pixmaps` / icon-theme fallback.

## Development

### Debug workspace state

Set `HOLONIGHT_DEBUG=1` to print live workspace state to stdout on every `applyBatchUpdate` call:

```bash
HOLONIGHT_DEBUG=1 task run
```

Output format (one line per workspace entry):

```
[ws rev=1] id=1 state=Active on_monitor=true
[ws rev=1] id=2 state=Occupied on_monitor=false
```

### Pixel-level visual testing

`scripts/check-pill.sh` samples a single screen pixel and compares it to an expected color:

```bash
scripts/check-pill.sh --x 42 --y 8 --color 7dcfff --tolerance 5
```

Coordinates are compositor-space (use `hyprctl monitors` to find monitor offsets). Requires `grim` and `imagemagick`.

## License

GPL-3.0-or-later — see [LICENSE](LICENSE).

## Storage devices

Storage uses the independently instantiated `HoloNightSystem::Storage` component and UDisks2 on the system bus.
Install the `udisks2` runtime package and retain an existing polkit agent for authorization prompts.
Operations are manual: no automatic mounting, encrypted-volume unlocking or disk administration is provided.
Eject and power-off are distinct actions, both immediate (no confirmation step); power-off also powers off
sibling drives sharing the same physical device. Busy devices and authorization failures are reported without
forced unmounts or automatic retries.

The Storage topbar icon is hidden with zero devices attached, dimmed with devices attached but none mounted, and shows the system accent color plus a device-count badge once at least one volume is mounted. The popup opens with a "Removable Storage" header (device count, plus a currently-inert settings affordance) followed by one collapsible card per physical drive — icon (optical/SSD/HDD/flash-media/generic, by connection bus and media type), name, connection/media subtitle, total capacity, and Eject/Power Off — expanding to a row per volume with a used/free capacity bar (accent above 10% free, amber 5–10%, red below 5%) and Mount/Unmount. Clicking a volume row's body opens it in `holonight-files`, mounting first if needed; footer buttons open Files directly or at the first mounted volume. In-flight operations show inline "Mounting…"/"Ejecting…"/etc. text with conflicting actions disabled; failures show an inline error with a "Try Again" retry. A drive newly connecting posts a "connected" desktop notification (suppressed for devices already attached at shell startup); a successful eject or power-off posts a "safe to remove" notification — both standalone, outside the shell's own notification history. Mount/unmount do not notify. If the last device disappears while the popup is open, it shows an empty state instead of closing. Empty readers and fixed volumes are hidden.

### Sharing Sway finalization with regular UWSM sessions

If the same native Sway configuration serves regular UWSM and HoloNight sessions,
replace its unconditional `exec uwsm finalize` with
`exec sh ~/.config/sway/finalize-regular.sh`. Save this exact helper recipe there:

```sh
#!/bin/sh
case ":${XDG_CURRENT_DESKTOP:-}:" in
    *:HoloNight:*) exit 0 ;;
esac
exec uwsm finalize
```

Preflight recognizes this complete helper (blank lines and comments are allowed).
The colon-delimited identity must contain the exact `HoloNight` component. Other
helper bodies are inspected normally; merely mentioning HoloNight is not an exemption.
HoloNight supplies its own finalization after including the native configuration.

Preflight inspects literal commands, environment assignments, native includes, and
identifiable shell scripts in command position without executing personal code.
Shell launcher/overview control commands, window rules, socket arguments, and
command-valued settings are not service startup. Native executables and referenced
assets are not decoded as configuration. Dynamic constructs are reported as unchecked;
a successful check is not a guarantee about arbitrary Lua or shell code. Inspection
is limited to 128 referenced files, 1 MiB per file, and 16 nested command wrappers.

Bluetooth controls use the installed `HoloNightSystem::Bluetooth` client and BlueZ.
The bar widget opens connection and pairing controls; Bluetooth Settings opens the
independent Settings page. Opening the popup scans; closing releases this client's
scan after two seconds. Stop scan persists until Start or reopening. Dismissal
cancels pairing. Default-agent conflicts appear as diagnostics; Blueman may coexist.

Connected and paired Bluetooth rows expand into a bordered card containing their
connection action; nearby devices keep an inline Pair button. Discovery grows the
Available section to five rows before scrolling, preserving existing row order.
Full device names appear on hover only when the row label is truncated.
