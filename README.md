# HoloNight Shell

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
| Weather | Current conditions via OpenWeather; click opens a popup with current/details/hourly/daily sections plus temperature & precipitation graphs. Hidden entirely until an API key is configured. Location from config coordinates or IP-geolocation fallback. Disk-cached for offline startup |
| Clock | Time (`HH:mm`) and date (`ddd d MMM`) with Rajdhani font; click opens the right sidebar |
| Session | Canvas-drawn power button (top bar); session actions (lock, log out, sleep, reboot, shut down) are in the right sidebar |
| Status popups | Click Network / Audio / Battery / Keyboard to open a positioned popup below the widget (notch aligned to widget center, per-id sizing) via `StatusPopupSurface`; dismiss by re-clicking, switching widgets, clicking outside, or Esc. The Audio popup is a mini-pavucontrol (Output / Input / Applications tabs with per-row mute + volume slider, click-row-to-set-default, and a Master Volume bar). The Battery popup is a compact panel: charge percent + fill bar, charge state with time-to-empty/full, conditional Health and Cycle-count rows, and a power-profile selector (Power Saver / Balanced / Performance circle buttons) backed by power-profiles-daemon — hidden when the daemon is absent |
| Tooltips | 450 ms hover-delay tooltips on every widget via `TooltipSurface` |
| Notifications | In-process freedesktop notification daemon (owns `org.freedesktop.Notifications`); HUD-styled toasts on the focused monitor with actions, body markup (`<b>/<i>/<u>/<a>`), per-urgency/category accent (critical red, IM/call/presence violet, else cyan), hover-pause timeouts, replace semantics, and a per-monitor critical-priority overflow queue (`max_visible`, default 3). Critical notifications (urgency=2) are persistent until dismissed and always bypass DND. Configurable via `[notifications]`. A topbar bell icon shows a pulsing violet badge when unread history entries exist; click opens the sidebar. History is persisted to `~/.local/state/holonight/notifications/history.json` (XDG_STATE_HOME). Dismissed notifications are never stored. Configurable via `[notifications.history]`. If another notification daemon already owns the bus name at startup, a diagnostic appears in the sidebar Notifications tab |

### Wallpaper

A full-screen surface on the `wlr-layer-shell` background layer, behind the bar. Wallpapers are configured per monitor via `[background] images` — the first image maps to the first monitor, the second to the second, and so on (extra images are ignored; if there are more monitors than images, the last image repeats). Images are cropped to fill (`PreserveAspectCrop`), and runtime config changes crossfade to the new wallpaper. With no image (or a path that fails to load) the surface falls back to the `surfaceVariant` theme color.

### Desktop widgets

Non-interactive widgets on the `wlr-layer-shell` `bottom` layer (above the wallpaper, below windows), configured as `[[widget]]` entries. Each widget is anchored to one of nine positions (`left-top`, `center-top`, `right-top`, `left-center`, `center-center`, `right-center`, `left-bottom`, `center-bottom`, `right-bottom`) with a uniform `[widgets] margin`; top-anchored widgets clear the bar automatically. Target specific monitors by output name (`monitors = ["DP-1"]`) or leave the list empty for all monitors — unknown names are honored on hotplug. To save resources, a widget is shown only while its monitor's visible workspace is empty (no windows) and hidden — with its update timer frozen — when a window occupies it; the surface is cached across these transitions, so reveals are instant. If two widgets claim the same position on the same monitor, the earlier one in config order wins (the loser is dropped on that monitor with a single warning). Any widget can be turned off in place with `enabled = false` (it creates no surface and no longer blocks a position). Three widget types exist today:

- `time-to-event` — a title, a glowing adaptive countdown (`12d 04h 37m`, or with `show_seconds`, `…37m 12s`; `Now` once reached), and the event date below.
- `clock` — a large 24-hour `HH:mm` with smaller inline glowing seconds (`HH:mm:ss`, the seconds at half size) over a localized date row (`Sunday, 8 June 2026` by default). Optional `date_format` (Qt date pattern) and `locale` (BCP-47) keys; `show_seconds = false` drops the seconds and ticks per minute.
- `mpris` — a large "now playing" panel (artwork, title/artist/album, progress bar, elapsed/duration, player icon) mirroring whichever player the topbar Media pill shows. Artwork uses a bundled media placeholder when empty or unresolvable (`file://`, `http(s)://`, and inline base64 `data:` artwork URLs are supported); the player identity remains a separate icon badge. Dims when the player pauses and fades out entirely after `pause_hide_minutes` (default 10) of continuous pause; resuming playback or switching the active player resets that timer.

Requires Hyprland (the occupancy gate uses Hyprland IPC).

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

- Qt 6 (`Core`, `Gui`, `GuiPrivate`, `Quick`, `WaylandClient`, `DBus`, `Network`, `Concurrent`, `Sql`)
- CMake 3.25+
- **hypridle ≥ 0.2.0** or swayidle — recommended; shell runs without them but idle detection and Teams/Zoom "Away" status are disabled
- Ninja
- `wayland-client`, `wayland-protocols`
- `tomlplusplus`
- `libpulse`
- `libsecret-1` (for CalDAV password lookup from the system keyring)
- GTest (optional, for tests — fetched automatically if not installed)

See [`docs/dev-setup.md`](docs/dev-setup.md) for the Ubuntu package baseline used by CI.

## Building

All workflows go through [`task`](https://taskfile.dev):

```bash
task configure    # configure CMake
task build        # build the binary
task run          # build and launch (requires a live Wayland/Hyprland session)
task test         # build and run unit tests
task format       # auto-format with clang-format
task tidy         # run clang-tidy
task qml-lint     # lint QML files
task clean        # remove build/
```

**Without `task`:**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -S .
cmake --build build -j$(nproc)
sudo cmake --install build --prefix /usr
```

Install to a local prefix (no sudo):

```bash
cmake --install build --prefix ~/.local
```

The install also writes the HoloNight Settings portal descriptor and routing
config to the selected prefix:

```text
share/xdg-desktop-portal/portals/holonight.portal
share/xdg-desktop-portal/holonight-portals.conf
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

Installing also provides a Wayland login entry:

```bash
cmake --install build --prefix ~/.local
```

Select `HoloNight (Hyprland)` in the display manager. The entry runs
`holonight-hyprland-session`, which exports the HoloNight/Hyprland desktop environment,
imports it into D-Bus activation and the systemd user manager, queues
`holonight-shell.service`, then starts Hyprland. With UWSM installed, the script
uses the distribution Hyprland desktop entry:

```bash
uwsm start -e -D Hyprland hyprland.desktop
```

The script defaults missing values to:

```bash
XDG_CURRENT_DESKTOP=HoloNight:Hyprland
XDG_SESSION_DESKTOP=Hyprland
XDG_SESSION_TYPE=wayland
XDG_MENU_PREFIX=hyprland-
```

Startup mode is controlled with `HOLONIGHT_HYPRLAND_SESSION_MODE`:

- `auto` (default) uses UWSM when `uwsm` is installed, otherwise runs
  `Hyprland` directly.
- `uwsm` requires UWSM and starts Hyprland with
  `uwsm start -e -D Hyprland hyprland.desktop`.
- `direct` runs `Hyprland` directly after importing the environment. Direct mode
  does not automatically start `holonight-shell`; use the manual Hyprland
  fallback below when running without UWSM.

The installed systemd user unit is intentionally not enabled globally during
installation. The `HoloNight (Hyprland)` session starts it for that session.
Users who keep their own Hyprland session can opt in with either:

```ini
exec-once = uwsm app -- holonight-shell
```

Without UWSM, use the binary directly:

```ini
exec-once = holonight-shell
```

or:

```bash
systemctl --user enable --now holonight-shell.service
```

If you installed from source into an already-running user manager and the unit
is not found, reload user units first:

```bash
systemctl --user daemon-reload
```

For a one-off direct launch without UWSM:

```bash
HOLONIGHT_HYPRLAND_SESSION_MODE=direct holonight-hyprland-session
```

After login, verify desktop integration:

```bash
scripts/check-desktop-integration.sh
task compositor-smoke-check
```

The integration check reports whether `XDG_CURRENT_DESKTOP` includes
`HoloNight`, whether the HoloNight portal files are discoverable, and whether
`org.freedesktop.impl.portal.desktop.holonight` is owned on the session bus.

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

QML is registered as the `HolonightShell` module with resource paths under `qrc:/HolonightShell/`. Shell QML files live under `apps/shell/qml/` and are registered by `apps/shell/CMakeLists.txt`. Shared QML components used by both binaries live under `qml/HoloNight/` and are registered as the `Holonight.Components` module — import with `import Holonight.Components`.

Wayland client code is generated from XML files in `protocols/` plus the system `xdg-shell` protocol. Runtime integration expects Hyprland IPC, NetworkManager D-Bus, UPower D-Bus, PipeWire or PulseAudio (via libpulse), and StatusNotifierItem-compatible tray clients. The standalone `holonight-chat` application is optional; when installed, its D-Bus activation service backs the shell's AI quick-panel command. power-profiles-daemon D-Bus is optional — its profile selector in the battery popup is hidden when the daemon is not running. xdg-desktop-portal is optional — `PortalService` probes for it at startup and exposes availability diagnostics and system color-scheme/accent-color settings to QML; the shell runs normally without it.

## Configuration

Shell product behavior lives in `$XDG_CONFIG_HOME/holonight/config.toml`; global appearance lives independently in
`$XDG_CONFIG_HOME/holonight/appearance.toml`. Both fall back below `~/.config` when `XDG_CONFIG_HOME` is unset and
reload live without coupling their failure or save domains. See [`docs/config.md`](docs/config.md) for both schemas.

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
api_key = ""          # openweathermap.org key; empty = weather widget disabled
geo_api_key = ""      # ipgeolocation.io key; used only when latitude/longitude are unset
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
