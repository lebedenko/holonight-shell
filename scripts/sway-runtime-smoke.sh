#!/usr/bin/env bash
set -euo pipefail

shell_binary="${1:-build/holonight-shell}"
runtime_dir="$(mktemp -d /tmp/holonight-sway-smoke.XXXXXX)"
session_runtime="${XDG_RUNTIME_DIR:?XDG_RUNTIME_DIR must name the user runtime directory}"
wayland_display=""
config_file="${runtime_dir}/sway.conf"
sway_log="${runtime_dir}/sway.log"
shell_log="${runtime_dir}/shell.log"
sway_pid=""

finish() {
  local status=$?
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
printf '%s\n' \
  'output HEADLESS-1 mode 1280x720' \
  'workspace "dev:web" output HEADLESS-1' \
  'workspace "1" output HEADLESS-1' \
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

SWAYSOCK="${sway_socket}" swaymsg -t get_workspaces | grep -q '^\['
SWAYSOCK="${sway_socket}" swaymsg 'workspace "dev:web"' >/dev/null
SWAYSOCK="${sway_socket}" swaymsg -t get_workspaces | grep -q '"name": "dev:web"'

set +e
XDG_RUNTIME_DIR="${session_runtime}" WAYLAND_DISPLAY="${wayland_display}" SWAYSOCK="${sway_socket}" \
  XDG_CURRENT_DESKTOP=sway timeout 5s "${shell_binary}" --debug --no-log-file >"${shell_log}" 2>&1
shell_status=$?
set -e
[[ ${shell_status} -eq 0 || ${shell_status} -eq 124 ]]
! grep -Eiq 'fatal|failed to load|segmentation fault|assertion.*failed' "${shell_log}"

SWAYSOCK="${sway_socket}" swaymsg exit >/dev/null || true
wait "${sway_pid}" || true
sway_pid=""
echo "Headless Sway runtime smoke passed"
