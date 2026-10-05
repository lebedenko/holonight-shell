#!/usr/bin/env bash
set -euo pipefail

# run-isolated-test.py supplies a disposable HOME, runtime directory and private
# D-Bus without activation services. Never touch the desktop's user manager.
[[ -n "${DBUS_SESSION_BUS_ADDRESS:-}" && "${HOME}" == /tmp/uqc102-tests-* ]] || {
  echo 'Run via task labwc-runtime-smoke (requires an isolated test environment)' >&2
  exit 1
}
for executable in labwc timeout; do
  command -v "${executable}" >/dev/null || { echo "Missing dependency: ${executable}" >&2; exit 1; }
done
shell_binary="$(realpath "${1:?shell binary required}")"
probe_binary="$(realpath "${2:?workspace probe required}")"
labwc_pid=""
shell_pid=""
holder_pid=""
finish() {
  local status=$?
  if [[ -n "${shell_pid}" ]]; then kill "${shell_pid}" 2>/dev/null || true; wait "${shell_pid}" 2>/dev/null || true; fi
  if [[ -n "${holder_pid}" ]]; then kill "${holder_pid}" 2>/dev/null || true; wait "${holder_pid}" 2>/dev/null || true; fi
  if [[ -n "${labwc_pid}" ]]; then kill "${labwc_pid}" 2>/dev/null || true; wait "${labwc_pid}" 2>/dev/null || true; fi
  if (( status != 0 )); then cat "${HOME}/labwc.log" "${HOME}/shell.log" 2>/dev/null || true; fi
  exit "${status}"
}
trap finish EXIT
mkdir "${HOME}/labwc"
cat >"${HOME}/labwc/rc.xml" <<'XML'
<?xml version="1.0"?>
<labwc_config>
  <desktops><names><name>code</name><name>web</name></names></desktops>
</labwc_config>
XML
# Explicitly suppress activation imports even if the compositor's defaults change.
export LABWC_UPDATE_ACTIVATION_ENV=0 WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_HEADLESS_OUTPUTS="${3:-1}"
export XDG_CURRENT_DESKTOP=labwc QT_QPA_PLATFORM=wayland
unset WAYLAND_DISPLAY HYPRLAND_INSTANCE_SIGNATURE HYPRLAND_CMD SWAYSOCK I3SOCK LABWC_PID
# Capture the compositor's own startup environment without importing it anywhere.
cat >"${HOME}/startup" <<'STARTUP'
printf '%s\n' "$LABWC_PID" "$WAYLAND_DISPLAY" >"$HOME/labwc-ready"
STARTUP
labwc -C "${HOME}/labwc" -s "sh ${HOME}/startup" >"${HOME}/labwc.log" 2>&1 &
labwc_pid=$!
for _ in {1..100}; do
  kill -0 "${labwc_pid}"
  if [[ -s "${HOME}/labwc-ready" ]]; then
    mapfile -t ready <"${HOME}/labwc-ready"
    if [[ ${#ready[@]} -eq 2 && -n "${ready[1]}" ]]; then
      [[ "${ready[0]}" == "${labwc_pid}" ]]
      export LABWC_PID="${ready[0]}" WAYLAND_DISPLAY="${ready[1]}"
      break
    fi
  fi
  sleep 0.05
done
[[ -n "${WAYLAND_DISPLAY:-}" ]]
"${probe_binary}"
protocol_probe="${HOLONIGHT_LABWC_PROTOCOL_PROBE:-${probe_binary%/*}/labwc_protocol_smoke}"
for version in 1 2 3; do "${protocol_probe}" "${version}"; done
"${probe_binary}" --hold >"${HOME}/holder.log" 2>&1 &
holder_pid=$!
"${shell_binary}" --debug --no-log-file >"${HOME}/shell.log" 2>&1 &
shell_pid=$!
sleep 3
kill -0 "${shell_pid}"
grep -q 'libholonight_backend_labwc' "/proc/${shell_pid}/maps"
! grep -Eq 'libholonight_backend_(hyprland|sway|wayland)' "/proc/${shell_pid}/maps"
! grep -Eiq 'fatal|failed to load|segmentation fault|assertion.*failed' "${HOME}/shell.log"
# Exercise the new transient surface using the public control command.
"${shell_binary}" --toggle-window-overview
sleep 0.3
if command -v grim >/dev/null; then
  artifact_dir="${shell_binary%/*}/smoke-artifacts"
  mkdir -p "${artifact_dir}"
  grim "${artifact_dir}/labwc-overview.png"
fi
if command -v wtype >/dev/null; then
  # Headless labwc has no physical keyboard. Allow Qt to bind wl_keyboard
  # after the virtual device announces seat capabilities before sending keys.
  wtype -s 300 -k Down -s 300 -k Return -s 200 &
  keyboard_pid=$!
  sleep 0.45
  if command -v grim >/dev/null; then grim "${artifact_dir}/labwc-overview-selected.png"; fi
  wait "${keyboard_pid}"
  for _ in {1..100}; do
    [[ "$(cat "${XDG_RUNTIME_DIR}/labwc-smoke-activated" 2>/dev/null)" == labwc-taskbar-first ]] && break
    sleep 0.02
  done
  activation="$(cat "${XDG_RUNTIME_DIR}/labwc-smoke-activated")"
  [[ "${activation}" == labwc-taskbar-first ]] || { echo "Overview activated ${activation}, expected first window" >&2; exit 1; }
  "${shell_binary}" --toggle-window-overview
  sleep 0.2
  wtype -s 300 -k Escape -s 100
  [[ "$(cat "${XDG_RUNTIME_DIR}/labwc-smoke-activated")" == labwc-taskbar-first ]]
  echo 'Window overview keyboard activation and cancellation passed'
  "${shell_binary}" --toggle-window-overview
  sleep 0.2
fi
kill -0 "${shell_pid}"
"${shell_binary}" --toggle-window-overview
sleep 0.2
! grep -Eq '(WindowOverlay|WindowTaskbar).*Error|ReferenceError|TypeError' "${HOME}/shell.log"
if [[ "${WLR_HEADLESS_OUTPUTS}" == 2 ]] && command -v wtype >/dev/null; then
  primary_output="$(cat "${XDG_RUNTIME_DIR}/labwc-smoke-primary-output")"
  [[ "${primary_output}" =~ ^HEADLESS-[0-9]+$ ]]
  python3 - "${HOME}/labwc/rc.xml" "${primary_output}" <<'PYXML'
import sys
import xml.etree.ElementTree as ET
tree = ET.parse(sys.argv[1])
keyboard = ET.SubElement(tree.getroot(), 'keyboard')
binding = ET.SubElement(keyboard, 'keybind', key='W-r')
ET.SubElement(binding, 'action', name='VirtualOutputRemove', output_name=sys.argv[2])
tree.write(sys.argv[1])
PYXML
  labwc --reconfigure
  sleep 0.2
  "${shell_binary}" --toggle-window-overview
  sleep 0.3
  wtype -s 300 -M logo -k r -m logo -s 200
  for _ in {1..100}; do
    [[ "$(cat "${XDG_RUNTIME_DIR}/labwc-smoke-screen-count")" == 1 ]] && break
    sleep 0.02
  done
  [[ "$(cat "${XDG_RUNTIME_DIR}/labwc-smoke-screen-count")" == 1 ]]
  sleep 0.3
  previous="$(cat "${XDG_RUNTIME_DIR}/labwc-smoke-activated")"
  expected=labwc-taskbar-first
  [[ "${previous}" == labwc-taskbar-first ]] && expected=labwc-taskbar-second
  "${shell_binary}" --toggle-window-overview
  sleep 0.3
  if command -v grim >/dev/null; then grim "${artifact_dir}/labwc-overview-after-output-removal.png"; fi
  wtype -s 300 -k Down -s 200 -k Return -s 200
  for _ in {1..100}; do
    [[ "$(cat "${XDG_RUNTIME_DIR}/labwc-smoke-activated")" == "${expected}" ]] && break
    sleep 0.02
  done
  [[ "$(cat "${XDG_RUNTIME_DIR}/labwc-smoke-activated")" == "${expected}" ]]
  echo 'Output removal dismissed the overview; reopening and activation on the remaining output passed'
fi
# The compositor must survive a development shell restart.
kill "${shell_pid}"
wait "${shell_pid}" || true
shell_pid=""
kill -0 "${labwc_pid}"
"${shell_binary}" --debug --no-log-file >>"${HOME}/shell.log" 2>&1 &
shell_pid=$!
sleep 2
kill -0 "${shell_pid}"
kill -0 "${labwc_pid}"
echo 'Headless labwc runtime smoke passed (including shell restart)'
