# Top Bar Implementation Plan

Full top bar for holonight-shell, implemented across multiple SDD sessions.
Each session is self-contained; later sessions build on earlier ones.

## Sessions

| # | Slug | Focus | Status |
|---|------|-------|--------|
| 1 | [topbar-skeleton](topbar-skeleton/) | wlr-layer-shell anchoring · BarBackground · BarSection · LogoSection (Canvas Arch logo) · real clock | ✅ Complete |
| 2 | [topbar-workspaces](topbar-workspaces/) | ext-workspace-v1 protocol · C++ WorkspaceModel · WorkspacePill QML (empty/occupied/active/urgent states) | ✅ Complete |
| 3 | [topbar-window-title](topbar-window-title/) | Hyprland IPC Unix socket · C++ ActiveWindowService · animated window title display | ✅ Complete |
| 4 | topbar-battery | UPower D-Bus · C++ BatteryService · battery icon + percentage widget | ✅ Complete |
| 5 | topbar-audio | PipeWire/PulseAudio D-Bus · C++ AudioService · volume icon + level widget | ✅ Complete |
| 6 | topbar-network | NetworkManager D-Bus · C++ NetworkService · connection status + SSID widget | ✅ Complete |
| 7 | topbar-tray | StatusNotifierItem D-Bus protocol · C++ TrayHost · TrayItem QML | ⏳ Planned |
| 8 | topbar-session | Session/power button · logout · reboot · shutdown · lock screen actions | ✅ Complete |
| 9 | topbar-hud-polish | Canvas angled section frames · neon border glow · full HoloNight visual polish | ⏳ Planned |

## Architecture Decisions

- **Protocol binding**: C++ only. QML is visual only — no Wayland calls from QML.
- **Multi-output**: one `QQuickView` + `wl_layer_surface` per `wl_output` from Session 1 onward.
- **Colors**: always from `HoloniightPalette` (import Holonight). No hardcoded hex values.
- **Icons**: HoloNight symbolic SVGs from `assets/dont-commit/holonight-symbolic-icons/` where available; Canvas fallback for distro logo.
- **Output hotplug**: output *removal* lifecycle deferred to after Session 2 (workspaces) so both surface and model cleanup are handled together.
- **Glow**: `Qt5Compat.GraphicalEffects.Glow` only. Requires `Qt6::Qt5Compat` in CMakeLists.txt. Glow = state, not decoration.
- **Cyan** (`#7dcfff`) = active/interaction/focus. **Violet** (`#bb9af7`) = atmosphere/ambient. Never competing borders.
- **Animation easing**: `Easing.OutCubic` for all transitions. Workspace switch: 160-220ms. Hover: 100-140ms.
- **Design reference**: `assets/dont-commit/` — always consult before any visual decision.

## Session Design Notes

### Session 2 — topbar-workspaces
- Protocol: `ext-workspace-v1` XML → `qt6_generate_wayland_protocol_client_sources`
- C++: `WorkspaceModel : QAbstractListModel`, roles: id/name/state enum (Empty/Occupied/Active/Urgent). Singleton via `qmlRegisterSingletonInstance`.
- Pill geometry: h=34, radius=12, font JetBrains Mono 14px, inactive w=42, active w=58
- States: empty=muted outline (#565f89 border, #161925 fill, no glow), occupied=muted fill (#1f2335 fill, #a9b1d6 text), active=cyan border 1.8px + Glow(radius 18, spread 0.4), urgent=red (#f7768e) border + Glow pulse
- Width transition: 180ms OutCubic Behavior
- Design refs: `assets/dont-commit/04-workspace-pill.svg`, `ws-indicators/`, `qml-glow-examples/WorkspacePill.qml`

### Session 3 — topbar-window-title
- Protocol: Hyprland IPC Unix socket (`$HYPRLAND_INSTANCE_SIGNATURE`), subscribe to `activewindow` event, raw Unix socket + JSON
- C++: `ActiveWindowService : QObject`, properties: `QString title`, `QString appClass`. QSocketNotifier or background thread.
- QML: text label with `elide: Text.ElideRight`. Label "// ACTIVE WINDOW" in blue (#7aa2f7) above, title in text primary (#c0caf5), Inter font.
- Design ref: `assets/dont-commit/03-topbar-module.svg`

### Session 4 — topbar-battery
- Protocol: UPower DBus (`org.freedesktop.UPower`)
- C++: `BatteryService : QObject`, properties: `int percent`, `bool charging`, `bool present`
- QML: body rect 76×22 px radius 5, cap nub 6×8 px, fill bar `66 * percent / 100`, levelColor = percent≤20 → #f7768e else #7dcfff, Glow on body, percent text JetBrains Mono 13px. Animated 250ms OutCubic.
- Design ref: `assets/dont-commit/10-battery-widget.svg`, `qml-glow-examples/BatteryIndicator.qml`

### Session 5 — topbar-audio
- Protocol: PipeWire/PulseAudio DBus
- C++: `AudioService : QObject`, properties: `int volume` (0-100), `bool muted`
- Icons: `assets/dont-commit/status-indicators/audio_muted.svg`, `audio_wave.svg`, `audio_slider.svg`
- Design ref: `assets/dont-commit/09-audio-visualizer.svg`

### Session 6 — topbar-network
- Protocol: NetworkManager DBus
- C++: `NetworkService : QObject`, properties: `bool online`, `QString ssid`, `int strength`
- Icons: `assets/dont-commit/status-indicators/wifi_online.svg`, `wifi_offline.svg`. Green (#9ece6a) online, red (#f7768e) offline.

### Session 7 — topbar-tray
- Protocol: StatusNotifierItem DBus. Full design grilling deferred to that session.

### Session 8 — topbar-session
- Actions: logout, reboot, shutdown, lock via Hyprland IPC `dispatch exec` or systemd DBus
- Design ref: `assets/dont-commit/08-session-power-button.svg`

### Session 9 — topbar-hud-polish
- Canvas-drawn angled frames (cut corners ~24px, not simple radius). No new C++ services.
- Border gradient: blue→cyan→violet. Glow radius 20, spread 0.18, opacity 0.55.
- Design ref: `assets/dont-commit/02-hud-panel-frame.svg`, `qml-glow-examples/HudPanel.qml`
