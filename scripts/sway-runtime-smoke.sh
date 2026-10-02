#!/usr/bin/env bash
set -euo pipefail

shell_binary="${1:-build/debug/holonight-shell}"
runtime_dir="$(mktemp -d /tmp/holonight-sway-smoke.XXXXXX)"
session_runtime="${runtime_dir}/runtime"
wayland_display=""
config_file="${runtime_dir}/sway.conf"
sway_log="${runtime_dir}/sway.log"
shell_log="${runtime_dir}/shell.log"
sway_pid=""
shell_pid=""

finish() {
  local status=$?
  if [[ -n "${shell_pid}" ]]; then kill "${shell_pid}" 2>/dev/null || true; fi
  if [[ -n "${sway_pid}" ]]; then kill "${sway_pid}" 2>/dev/null || true; fi
  if [[ ${status} -eq 0 ]]; then
    rm -rf -- "${runtime_dir}"
  else
    echo "Sway smoke logs retained in ${runtime_dir}" >&2
  fi
  exit "${status}"
}
trap finish EXIT

chmod 700 "${runtime_dir}"
mkdir "${session_runtime}"
chmod 700 "${session_runtime}"
printf '%s\n' \
  'output HEADLESS-1 mode 1280x720' \
  'workspace "1" output HEADLESS-1' \
  'workspace_auto_back_and_forth yes' \
  >"${config_file}"

env -u WAYLAND_DISPLAY XDG_RUNTIME_DIR="${session_runtime}" WLR_BACKENDS=headless WLR_RENDERER=pixman \
  sway --unsupported-gpu --config "${config_file}" >"${sway_log}" 2>&1 &
sway_pid=$!

sway_socket=""
for _ in $(seq 1 100); do
  sway_socket="$(find "${session_runtime}" -maxdepth 1 -type s -name "sway-ipc.*.${sway_pid}.sock" -print -quit)"
  [[ -n "${sway_socket}" ]] && break
  sleep 0.05
done
[[ -n "${sway_socket}" ]]
wayland_display="$(find "${session_runtime}" -maxdepth 1 -type s -name 'wayland-*' -printf '%T@ %f\n' \
  | sort -n | tail -n 1 | cut -d' ' -f2-)"
[[ -n "${wayland_display}" ]]

# Numeric slots create workspaces only on activation and reuse renamed numbered ones.
sway_command() { SWAYSOCK="${sway_socket}" swaymsg "$1" >/dev/null; }
assert_workspace() {
  SWAYSOCK="${sway_socket}" swaymsg -r -t get_workspaces | python3 -c '
import json, sys
rows = json.load(sys.stdin)
assert len(rows) == 1, rows
assert rows[0]["name"] == sys.argv[1] and rows[0]["focused"], rows
' "$1"
}
assert_workspace 1
sway_command 'workspace --no-auto-back-and-forth number 5'
assert_workspace 5
sway_command 'workspace --no-auto-back-and-forth number 5'
assert_workspace 5
sway_command 'rename workspace to "5:web"'
sway_command 'workspace --no-auto-back-and-forth number 5'
assert_workspace '5:web'
sway_command 'workspace "dev:web"'
assert_workspace 'dev:web'
sway_command 'workspace --no-auto-back-and-forth number 1'
assert_workspace 1

XDG_RUNTIME_DIR="${session_runtime}" WAYLAND_DISPLAY="${wayland_display}" SWAYSOCK="${sway_socket}" \
  XDG_CURRENT_DESKTOP=sway timeout 12s "${shell_binary}" --debug --no-log-file >"${shell_log}" 2>&1 &
shell_pid=$!
capture_bar() {
  sleep 2
  if [[ -n "${SWAY_SMOKE_SCREENSHOT_DIR:-}" ]]; then
    mkdir -p "${SWAY_SMOKE_SCREENSHOT_DIR}"
    XDG_RUNTIME_DIR="${session_runtime}" WAYLAND_DISPLAY="${wayland_display}" \
      grim "${SWAY_SMOKE_SCREENSHOT_DIR}/$1.png"
  fi
}
capture_bar startup
shell_process="$(pgrep -P "${shell_pid}" -x holonight-shell)"
grep -q 'libholonight_backend_sway' "/proc/${shell_process}/maps"
! grep -Eq 'libholonight_backend_(hyprland|wayland)' "/proc/${shell_process}/maps"
assert_workspace 1
sway_command 'workspace --no-auto-back-and-forth number 5'
capture_bar switched
assert_workspace 5
sway_command 'workspace --no-auto-back-and-forth number 1'
capture_bar removed-empty
assert_workspace 1
sway_command 'workspace "dev:web"'
capture_bar named
assert_workspace 'dev:web'
set +e
wait "${shell_pid}"
shell_status=$?
shell_pid=""
set -e
[[ ${shell_status} -eq 0 || ${shell_status} -eq 124 ]]
! grep -Eiq 'fatal|failed to load|segmentation fault|assertion.*failed' "${shell_log}"

SWAYSOCK="${sway_socket}" swaymsg exit >/dev/null || true
wait "${sway_pid}" || true
sway_pid=""
echo "Headless Sway runtime smoke passed"
