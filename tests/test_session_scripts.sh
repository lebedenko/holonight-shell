#!/usr/bin/env bash
set -euo pipefail

source_dir="${1:?source directory required}"
test_root="$(mktemp -d)"
trap 'rm -rf -- "${test_root}"' EXIT
fake_bin="${test_root}/bin"
log_file="${test_root}/calls"
mkdir -p "${fake_bin}"

make_fake() {
  local name="$1"
  local body="$2"
  printf '#!/usr/bin/env bash\nset -euo pipefail\n%s\n' "${body}" >"${fake_bin}/${name}"
  chmod +x "${fake_bin}/${name}"
}

make_fake Hyprland 'printf "hyprland cursor=%s path=%s config=%s\\n" "${XCURSOR_THEME-}" "${HOLONIGHT_APPEARANCE_FILE-}" "${XDG_CONFIG_HOME-}" >>"${TEST_LOG}"'
make_fake uwsm 'printf "uwsm %s cursor=%s\\n" "$*" "${XCURSOR_THEME-}" >>"${TEST_LOG}"'
make_fake dbus-update-activation-environment 'printf "dbus %s cursor=%s\\n" "$*" "${XCURSOR_THEME-}" >>"${TEST_LOG}"'
make_fake systemctl '
if [[ "$*" == "--user show-environment" ]]; then
  printf "%s\\n" "${FAKE_SYSTEMD_ENV-}"
else
  printf "systemctl %s cursor=%s\\n" "$*" "${XCURSOR_THEME-}" >>"${TEST_LOG}"
fi'
make_fake holonight-shell 'printf "shell cursor=%s path=%s config=%s\\n" "${XCURSOR_THEME-}" "${HOLONIGHT_APPEARANCE_FILE-}" "${XDG_CONFIG_HOME-}" >>"${TEST_LOG}"'
make_fake sleep ':'

make_adapter() {
  make_fake holonight-appearance-adapter '
printf "adapter %s\\n" "$*" >>"${TEST_LOG}"
case "${ADAPTER_MODE:-ok}" in
  ok) printf "%s\\n" "${ADAPTER_VALUE:-CanonicalCursor}" ;;
  fail) exit 4 ;;
  empty) : ;;
  multiline) printf "one\\ntwo\\n" ;;
  control) printf "bad\\tvalue\\n" ;;
  oversized) printf "%05000d\\n" 0 ;;
esac'
}

run_session() {
  env -i PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" HOME="${test_root}/home" \
    XCURSOR_THEME="${XCURSOR_THEME-}" \
    HOLONIGHT_HYPRLAND_SESSION_MODE="${1:-direct}" "${source_dir}/scripts/holonight-hyprland-session"
}

assert_contains() {
  grep -Fqx "$1" "${log_file}" || { printf 'missing log line: %s\n' "$1" >&2; exit 1; }
}

make_adapter
: >"${log_file}"
env -i PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" HOME="${test_root}/home" \
  XDG_CONFIG_HOME="${test_root}/xdg" HOLONIGHT_APPEARANCE_FILE="${test_root}/override.toml" \
  HOLONIGHT_HYPRLAND_SESSION_MODE=direct "${source_dir}/scripts/holonight-hyprland-session"
assert_contains "adapter query --appearance ${test_root}/override.toml --field cursor-theme"
assert_contains "hyprland cursor=CanonicalCursor path=${test_root}/override.toml config=${test_root}/xdg"
grep -F 'dbus --systemd' "${log_file}" | grep -Fq 'XCURSOR_THEME XDG_CONFIG_HOME HOLONIGHT_APPEARANCE_FILE'
grep -F 'systemctl --user import-environment' "${log_file}" | grep -Fq 'XCURSOR_THEME XDG_CONFIG_HOME HOLONIGHT_APPEARANCE_FILE'

: >"${log_file}"
env -i PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" HOME="${test_root}/home" \
  XDG_CONFIG_HOME="${test_root}/xdg" HOLONIGHT_HYPRLAND_SESSION_MODE=direct \
  "${source_dir}/scripts/holonight-hyprland-session"
assert_contains "adapter query --appearance ${test_root}/xdg/holonight/appearance.toml --field cursor-theme"

for mode in fail empty multiline control oversized; do
  : >"${log_file}"
  env -i PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" HOME="${test_root}/home" ADAPTER_MODE="${mode}" \
    XCURSOR_THEME=Inherited HOLONIGHT_HYPRLAND_SESSION_MODE=direct \
    "${source_dir}/scripts/holonight-hyprland-session"
  assert_contains "hyprland cursor=Inherited path= config="
done

mv "${fake_bin}/holonight-appearance-adapter" "${fake_bin}/adapter-away"
: >"${log_file}"
XCURSOR_THEME=Inherited run_session direct
assert_contains "hyprland cursor=Inherited path= config="
mv "${fake_bin}/adapter-away" "${fake_bin}/holonight-appearance-adapter"

: >"${log_file}"
run_session uwsm
assert_contains "adapter query --appearance ${test_root}/home/.config/holonight/appearance.toml --field cursor-theme"
assert_contains "uwsm start -e -D Hyprland hyprland.desktop cursor=CanonicalCursor"

: >"${log_file}"
FAKE_SYSTEMD_ENV=$'WAYLAND_DISPLAY=wayland-9\nHYPRLAND_INSTANCE_SIGNATURE=sig\nXCURSOR_THEME=ManagerCursor\nXDG_CONFIG_HOME=/manager/config\nHOLONIGHT_APPEARANCE_FILE=/manager/appearance.toml' \
  env PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" HOLONIGHT_SHELL_SYSTEMD_ENV_TIMEOUT=0 \
  "${source_dir}/scripts/holonight-shell-systemd"
assert_contains "shell cursor=ManagerCursor path=/manager/appearance.toml config=/manager/config"

printf 'session script tests passed\n'
