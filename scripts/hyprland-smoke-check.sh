#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-build}"
binary="${repo_root}/${build_dir}/holonight-shell"
mode="${1:---checklist}"

usage() {
  cat <<'EOF'
Usage: scripts/hyprland-smoke-check.sh [--checklist|--probe]

Print a live Hyprland compositor smoke checklist for holonight-shell.

Modes:
  --checklist  Check basic prerequisites and print the manual checklist.
  --probe      Also send safe probe commands for launcher and notifications.

Run from a live Hyprland session after starting the shell with `task run`
or `./build/holonight-shell`.
EOF
}

if [[ "${mode}" == "--help" || "${mode}" == "-h" ]]; then
  usage
  exit 0
fi

if [[ "${mode}" != "--checklist" && "${mode}" != "--probe" ]]; then
  echo "ERROR: unknown mode: ${mode}" >&2
  usage >&2
  exit 1
fi

status() {
  printf '[%s] %s\n' "$1" "$2"
}

have_command() {
  command -v "$1" >/dev/null 2>&1
}

require_or_warn() {
  local command_name="$1"
  local reason="$2"

  if have_command "${command_name}"; then
    status OK "${command_name} (${reason})"
  else
    status WARN "missing ${command_name} (${reason})"
  fi
}

control_socket="${XDG_RUNTIME_DIR:-/tmp}/holonight-shell/control.sock"
monitor_name=""
if have_command hyprctl; then
  hyprctl_monitors="$(hyprctl monitors 2>/dev/null || true)"
  monitor_name="$(printf '%s\n' "${hyprctl_monitors}" | awk '/^Monitor / { print $2; exit }')"
fi

echo "Holonight live compositor smoke check"
echo

if [[ -n "${WAYLAND_DISPLAY:-}" ]]; then
  status OK "WAYLAND_DISPLAY=${WAYLAND_DISPLAY}"
else
  status FAIL "WAYLAND_DISPLAY is not set"
fi

if [[ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ]]; then
  status OK "Hyprland session detected"
else
  status WARN "HYPRLAND_INSTANCE_SIGNATURE is not set"
fi

if [[ -S "${control_socket}" ]]; then
  status OK "control socket: ${control_socket}"
else
  status WARN "control socket missing: ${control_socket}"
fi

if [[ -x "${binary}" ]]; then
  status OK "shell binary: ${binary}"
else
  status WARN "shell binary not found or not executable: ${binary}; run task build"
fi

if have_command hyprctl; then
  if [[ -n "${monitor_name}" ]]; then
    status OK "hyprctl monitor query: first monitor is ${monitor_name}"
  else
    status WARN "hyprctl is installed, but monitor query failed; run hyprctl monitors manually"
  fi
else
  status WARN "missing hyprctl (monitor/workspace setup checks)"
fi
require_or_warn notify-send "notification smoke probes"
require_or_warn busctl "notification daemon ownership checks"
require_or_warn dbus-monitor "notification signal observation"
require_or_warn socat "manual sidebar control-socket probe"
require_or_warn pactl "audio popup live-update checks"
require_or_warn grim "optional screenshots and pixel checks"
require_or_warn convert "optional scripts/check-pill.sh pixel checks"

echo
cat <<EOF
Manual checklist

Start:
  1. Stop other notification daemons such as mako, dunst, swaync, or a desktop environment daemon.
  2. Run: task run
  3. In another terminal, run this checklist: scripts/hyprland-smoke-check.sh
  4. Optional D-Bus trace: dbus-monitor "interface='org.freedesktop.Notifications'"

Popup positioning:
  1. Click Network, Audio, Battery, Keyboard, and Session topbar widgets.
  2. Each popup opens below its widget, with the notch centered on the triggering widget.
  3. Re-clicking the same widget toggles the popup closed.
  4. Opening another popup closes the previous popup.
  5. Clicking outside or pressing Esc dismisses the popup when the compositor grants focus.
  6. On multiple monitors, repeat on each bar and verify popups stay on the triggering monitor.

Sidebar:
  1. Click the clock widget and the notification bell.
  2. The sidebar opens on the matching monitor, then closes on outside click or Esc.
  3. Opening the sidebar on another monitor closes the previous sidebar.
  4. Optional socket probe for the first monitor:
     printf 'sidebar:toggle:${monitor_name:-<monitor-name>}' | socat - UNIX-CONNECT:${control_socket}

Launcher:
  1. Run: ${binary} --toggle-launcher
  2. The launcher opens on the focused monitor.
  3. Type a query, move selection with keyboard, launch an app, then reopen and verify recent-app ordering.
  4. Run the same toggle command again and verify it closes.

Tray:
  1. Start at least one StatusNotifierItem client, for example nm-applet, blueman-applet, keepassxc, or another tray app.
  2. Verify the item appears in the topbar tray, keeps its icon stable, and shows attention styling when the app requests attention.
  3. Click the tray item and verify the menu opens next to the icon, closes on outside click, and is replaced by another tray menu.
  4. Quit the tray app and verify the item disappears without leaving an empty slot.

Notifications:
  1. Run: busctl --user list | grep Notifications
  2. Run: notify-send "Holonight smoke" "normal toast"
  3. The toast appears on the focused monitor with the default accent and times out.
  4. Run: notify-send -t 0 -u critical "Holonight critical" "sticky toast"
  5. The critical toast stays until dismissed and uses the critical accent.
  6. Run several sticky notifications and verify queue overflow respects notifications.max_visible.
  7. Click the notification bell and verify the sidebar history reflects unread notifications.

Widgets:
  1. Move to an empty workspace and verify desktop widgets appear above the wallpaper and below normal windows.
  2. Open a normal window on that workspace and verify widgets hide.
  3. Close the window and verify widgets reappear without a visible rebuild flash.
  4. Edit ~/.config/holonight/config.toml widget entries and verify live rebuild behavior.

On-screen display:
  1. Change the volume (pactl set-sink-volume @DEFAULT_SINK@ +5%) and verify an OSD appears with icon, bar, and percentage.
  2. Toggle mute (pactl set-sink-mute @DEFAULT_SINK@ toggle) and verify the bar dims and the text reads Muted while the fill stays at the real volume.
  3. Change brightness and verify the brightness OSD appears with the same layout.
  4. Switch keyboard layout and verify the large short code plus full layout name appear with no bar.
  5. Change volume repeatedly and verify the value updates in place without replaying the entrance animation, and the hide timer restarts.
  6. Change volume, then immediately change the layout, and verify the content is replaced without a hide-then-show gap.
  7. Verify the OSD hides osd.timeout ms after the last change (1500 ms by default).
  8. Click through the OSD while it is visible and verify the window underneath receives the click.
  9. Move focus to another monitor and verify the next OSD opens there.
  10. Open the Audio topbar popup, change the volume, and verify no OSD appears; close it and verify the next change shows one again.
  11. Open the sidebar Quick Settings tab, change brightness, and verify no OSD appears; verify a layout change still does.
  12. Set osd.position and osd.timeout in ~/.config/holonight/config.toml and verify both apply live.
  13. Set osd.enabled = false and verify no OSD appears on any channel; set a per-channel enabled = false and verify only that channel is silent.
  14. Restart the shell and verify no OSD appears at login or during the first two seconds.

Optional visual probes:
  1. Use hyprctl monitors to find compositor-space coordinates.
  2. Use scripts/check-pill.sh --x X --y Y --color RRGGBB --tolerance 5 for targeted pixel checks.
EOF

if [[ "${mode}" == "--probe" ]]; then
  echo
  echo "Running safe probes"
  if [[ -x "${binary}" ]]; then
    "${binary}" --toggle-launcher || status WARN "launcher toggle probe failed"
  fi
  if have_command notify-send; then
    notify-send "Holonight smoke probe" "notification daemon probe from scripts/hyprland-smoke-check.sh" || \
      status WARN "notify-send probe failed"
  fi
fi
