# Configuration

`holonight-shell` reads TOML configuration from `$XDG_CONFIG_HOME/holonight/config.toml`. If `XDG_CONFIG_HOME`
is unset, the default path is `~/.config/holonight/config.toml`.

Palette appearance mode is intentionally separate from shell behavior config. Settings reads and writes it in
`$XDG_CONFIG_HOME/holonight/theme.conf` (default `~/.config/holonight/theme.conf`) as:

```ini
[appearance]
mode=dark
```

Accepted mode values are `dark`, `light`, and `system`. Legacy `[theme]` sections in `config.toml` are ignored by
the shell and are dropped the next time settings saves shell config.

The file is created with defaults on first run. Missing keys are added back to the file with default values, which
allows new settings to appear after upgrades. Invalid present values are corrected in memory only: type errors and
non-positive font sizes use defaults, while bounded numeric values are clamped to the nearest accepted value. All
inconsistencies are logged through `holonight.config`; the shell keeps running.

Changes are watched and reloaded after a 200 ms debounce. If the file contains invalid TOML, the previous valid
configuration remains active.

```toml
[appearance]
ui_font = "Inter"
ui_font_size = 12
fixed_font = "JetBrains Mono"
fixed_font_size = 12
clock_font = "Rajdhani"
clock_font_size = 24
title_font = "Audiowide"
title_font_size = 8

[bar.workspaces]
count = 5 # accepted: 3-10

[bar.systemtray]
max_items = 3 # accepted: 2-5

[tray.icon_overrides]
# [tray.icon_overrides.slack]
# id = "Slack_status_icon_1"
# icon = "slack-indicator"
# attention_icon = "slack-indicator"

[background]
images = [] # per-monitor wallpapers, in monitor order; empty = solid surfaceVariant fill

[logo]
# file = "~/my-logo.svg" # overrides the topbar logo; ignored with a warning if unreadable
generic = false           # true = always show the generic tux logo instead of the distro logo

[weather]
api_key = "" # openweathermap.org key; empty = weather widget disabled
geo_api_key = "" # ipgeolocation.io key; used only when latitude/longitude are unset
# latitude = 49.83968 # uncomment to pin location and skip IP geolocation
# longitude = 24.02972
units = "metric" # metric | imperial | standard
lang = "en"
refresh_interval = 600 # seconds; must be > 0

[notifications]
default_timeout_ms = 5000 # auto-dismiss delay for normal/low urgency; must be > 0
max_visible = 3 # concurrent on-screen toasts per monitor; accepted: 1-10

[osd]
enabled = true            # master switch for the on-screen display
timeout = 1500            # ms the OSD stays visible after the last update; accepted: 300-10000
position = "center-bottom" # same nine anchors as desktop widgets

[osd.volume]
enabled = true

[osd.brightness]
enabled = true

[osd.keyboard_layout]
enabled = true

[widgets]
margin = 32 # logical px between widgets and screen edges; top adds the bar height

[[widget]]
type = "time-to-event"
title = "New Year"
deadline = "2027-01-01T00:00:00" # ISO 8601 date or datetime, interpreted in local time
position = "center-bottom"       # one of the nine anchors; default center-center
monitors = []                    # output names, e.g. ["DP-1"]; empty = all monitors
show_seconds = true              # tick every second; false = per-minute, no seconds field
enabled = true                   # false = defined but not shown (applies to any widget type)

[[widget]]
type = "clock"
position = "center-center"
show_seconds = true              # false = HH:mm only, ticks per minute
date_format = ""                 # Qt date pattern; empty = "dddd, d MMMM yyyy"
locale = ""                      # BCP-47 tag, e.g. "de_DE"; empty = system locale
```

Tray icon overrides replace D-Bus-provided tray pixmaps with theme icons. Match by stable SNI `id` when available;
`service`, `object_path`, and `title` are optional fallbacks. `icon` must resolve through the current icon theme or the
override is ignored. `attention_icon` is optional and is used when the item reports `NeedsAttention`.

`background.images` maps positionally to monitors in the order Qt enumerates them: the first path is used for
the first monitor, the second for the second, and so on. Extra paths (more images than monitors) are ignored
silently; extra monitors (more monitors than images) all reuse the last path. Paths may be absolute or start with
`~/`, and any format `QImageReader` supports is accepted. An empty list, a missing key, or a path that fails to
load falls back to the `surfaceVariant` theme color. Changes crossfade in at runtime; new monitors are only picked
up on restart.

The topbar logo is resolved once at startup, in order: `logo.file` (if it exists and is readable; otherwise a
warning is logged and this step is skipped) > `logo.generic` (shows the bundled generic tux logo) > a static
distro-ID alias table mapped against `/etc/os-release` > the pre-existing `/usr/share/pixmaps` / icon-theme
fallback. Bundled and distro-mapped logos are tinted to the active theme color; a `logo.file` override renders
as-is, untinted. Unlike every other section, `[logo]` is not watched for live reload — changes only take effect
on the next shell restart.

Weather is hidden until `weather.api_key` is set and either coordinates or a geolocation key are available.
Set both `weather.latitude` and `weather.longitude` to pin a location and skip IP geolocation; omit them to let
`weather.geo_api_key` resolve the location. `weather.city` and `weather.country` are optional display text; missing
values are resolved from geolocation. `weather.units` is passed to OpenWeather as `metric`, `imperial`, or `standard`; `weather.lang`
controls localized condition text. Fetched weather is cached on disk for offline startup, and updates run every
`weather.refresh_interval` seconds.

Notifications are served by the in-process freedesktop notification daemon. `notifications.default_timeout_ms`
sets the auto-dismiss delay for normal/low urgency notifications; critical notifications stay until dismissed.
`notifications.max_visible` limits concurrent on-screen toasts per monitor and is clamped to the accepted range.

The on-screen display is a transient overlay that reports volume, brightness, and keyboard-layout changes. It is
event-driven: no surface exists until the first displayable change, so nothing appears at login. `osd.enabled = false`
is a master switch — no surface is ever created. Each channel can be turned off independently with
`[osd.volume]`, `[osd.brightness]`, or `[osd.keyboard_layout]`; a disabled channel is not observed at all, and
disabling one does not affect the others. `osd.timeout` is the delay after the *last* update, so a rapid series of
changes keeps the OSD on screen and hides once, `osd.timeout` ms after the series ends. `osd.position` uses the same
nine anchor names as desktop widgets; an unrecognized name is ignored with a warning and the default is used.
Out-of-range `osd.timeout` values are clamped to the accepted range rather than rejected. All `[osd]` keys are
watched and applied live.

The OSD opens on the focused monitor, resolved per event — moving focus between outputs routes the *next* event
without any teardown — and falls back to the primary monitor when nothing is focused yet. It sets an empty input
region, so clicks pass through to the window underneath. Two suppression rules avoid redundant overlays: no volume
OSD while the topbar audio popup is open, and no brightness OSD while the sidebar's Quick Settings tab is open,
because both already display the value being changed. Keyboard-layout changes are never suppressed. For the first
two seconds after startup the OSD stays silent, so services settling into their initial state do not flash an
overlay.

Desktop widgets are declared as a TOML array of tables — one `[[widget]]` entry each — plus the shared
`[widgets]` table. Each widget is targeted by monitor *output name* (`monitors = ["DP-1"]`); an empty or omitted
list means all monitors, and a configured name that is not currently connected is honored on hotplug (warned once).
A widget renders only while its monitor's visible workspace has no windows; when a window occupies it the widget is
hidden and its update timer freezes, then resyncs to the wall clock on reveal. The surface is cached across these
transitions, so toggling is instant. `position` is one of nine anchors and is global across a widget's monitors;
top-anchored positions are offset below the bar by `margin`. `enabled = false` (any widget type) keeps the
definition but creates no surface, and such a widget does not block a position. If two widgets claim the same
`(monitor, position)`, the one earlier in config order wins on that monitor and the other is dropped there
(warned once). A widget is skipped (warned once) if its `type` is unknown, or — for `time-to-event` — its `title`
is empty or its `deadline` is missing/unparseable.

`time-to-event` deadlines parse as ISO 8601: a date-only value means `00:00:00`; the event-date label shows the
time only when the deadline carried one. `clock` shows a 24-hour `HH:mm` with smaller inline seconds (omitted, and
ticking per minute, when `show_seconds = false`) over a date row; `date_format` is a Qt date pattern (empty =
`dddd, d MMMM yyyy`) and `locale` is a BCP-47 tag (empty = system locale). An unrecognized `locale` or an empty
`date_format` result falls back (system locale / default pattern) with a one-time warning. The occupancy gate
requires Hyprland. Widget changes rebuild live; new monitors are picked up on hotplug.

| Key | Default | Accepted values |
|---|---:|---|
| `appearance.ui_font` | `"Inter"` | String |
| `appearance.ui_font_size` | `12` | Positive integer |
| `appearance.fixed_font` | `"JetBrains Mono"` | String |
| `appearance.fixed_font_size` | `12` | Positive integer |
| `appearance.clock_font` | `"Rajdhani"` | String |
| `appearance.clock_font_size` | `24` | Positive integer |
| `appearance.title_font` | `"Audiowide"` | String |
| `appearance.title_font_size` | `8` | Positive integer |
| `bar.workspaces.count` | `5` | Integer from `3` to `10` |
| `bar.systemtray.max_items` | `3` | Integer from `2` to `5` |
| `tray.icon_overrides.<name>.id` | unset | Stable SNI item id |
| `tray.icon_overrides.<name>.icon` | unset | Existing theme icon name |
| `tray.icon_overrides.<name>.attention_icon` | unset | Existing theme icon name for `NeedsAttention` |
| `background.images` | `[]` | Array of image paths (absolute or `~/`-prefixed) |
| `logo.file` | unset | Path to an image (absolute or `~/`-prefixed); ignored with a warning if unreadable |
| `logo.generic` | `false` | Boolean; `true` = always show the generic tux logo |
| `weather.api_key` | `""` | OpenWeather API key; empty disables weather |
| `weather.geo_api_key` | `""` | IP geolocation API key used when coordinates are unset |
| `weather.latitude` | unset | Optional floating-point latitude |
| `weather.longitude` | unset | Optional floating-point longitude |
| `weather.city` | unset | Optional display label |
| `weather.country` | unset | Optional display country |
| `weather.units` | `"metric"` | `"metric"`, `"imperial"`, or `"standard"` |
| `weather.lang` | `"en"` | OpenWeather language code |
| `weather.refresh_interval` | `600` | Positive integer seconds |
| `notifications.default_timeout_ms` | `5000` | Positive integer milliseconds |
| `notifications.max_visible` | `3` | Integer from `1` to `10` |
| `osd.enabled` | `true` | Boolean; `false` = no OSD surface is ever created |
| `osd.timeout` | `1500` | Integer milliseconds from `300` to `10000` (clamped) |
| `osd.position` | `"center-bottom"` | One of the nine anchor names |
| `osd.volume.enabled` | `true` | Boolean; `false` = volume changes show no OSD |
| `osd.brightness.enabled` | `true` | Boolean; `false` = brightness changes show no OSD |
| `osd.keyboard_layout.enabled` | `true` | Boolean; `false` = layout changes show no OSD |
| `widgets.margin` | `32` | Non-negative integer (logical px) |
| `widget[].type` | required | `"time-to-event"` or `"clock"` |
| `widget[].position` | `"center-center"` | One of the nine anchor names |
| `widget[].monitors` | `[]` | Array of output names; empty = all monitors |
| `widget[].enabled` | `true` | Boolean; `false` = defined but not shown |
| `widget[].title` | required (`time-to-event`) | String (non-empty) |
| `widget[].deadline` | required (`time-to-event`) | ISO 8601 date or datetime (local time) |
| `widget[].show_seconds` | `false` (`time-to-event`) / `true` (`clock`) | Boolean |
| `widget[].date_format` | `""` (`clock`) | Qt date pattern; empty = `dddd, d MMMM yyyy` |
| `widget[].locale` | `""` (`clock`) | BCP-47 tag; empty = system locale |

## AI chat keybind

The shell control socket can toggle the standalone `holonight-chat` quick panel.
An empty monitor argument targets the currently focused monitor:

```ini
bind = SUPER, A, exec, printf 'chat:toggle:' | nc -N -U "$XDG_RUNTIME_DIR/holonight-shell/control.sock"
```

Use an explicit output name when fixed routing is preferred:

```ini
bind = SUPER, A, exec, printf 'chat:toggle:DP-5' | nc -N -U "$XDG_RUNTIME_DIR/holonight-shell/control.sock"
```

The `holonight-chat` D-Bus service file must be installed so the first toggle can
activate the application when it is not already running.
