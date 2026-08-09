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

make_fake Hyprland 'printf "hyprland cursor=%s path=%s config=%s platformtheme=%s quickstyle=%s styleoverride=%s\\ndesktop=%s\\n" "${XCURSOR_THEME-}" "${HOLONIGHT_APPEARANCE_FILE-}" "${XDG_CONFIG_HOME-}" "${QT_QPA_PLATFORMTHEME-}" "${QT_QUICK_CONTROLS_STYLE-}" "${QT_STYLE_OVERRIDE-}" "${XDG_CURRENT_DESKTOP-}" >>"${TEST_LOG}"'
make_fake uwsm 'printf "uwsm %s cursor=%s platformtheme=%s quickstyle=%s styleoverride=%s\\n" "$*" "${XCURSOR_THEME-}" "${QT_QPA_PLATFORMTHEME-}" "${QT_QUICK_CONTROLS_STYLE-}" "${QT_STYLE_OVERRIDE-}" >>"${TEST_LOG}"'
make_fake dbus-update-activation-environment 'printf "dbus %s cursor=%s\\n" "$*" "${XCURSOR_THEME-}" >>"${TEST_LOG}"'
make_fake systemctl '
if [[ "$*" == "--user show-environment" ]]; then
  printf "%s\\n" "${FAKE_SYSTEMD_ENV-}"
else
  printf "systemctl %s cursor=%s\\n" "$*" "${XCURSOR_THEME-}" >>"${TEST_LOG}"
fi'
make_fake holonight-shell 'printf "shell cursor=%s path=%s config=%s platformtheme=%s quickstyle=%s styleoverride=%s\\n" "${XCURSOR_THEME-}" "${HOLONIGHT_APPEARANCE_FILE-}" "${XDG_CONFIG_HOME-}" "${QT_QPA_PLATFORMTHEME-}" "${QT_QUICK_CONTROLS_STYLE-}" "${QT_STYLE_OVERRIDE-}" >>"${TEST_LOG}"'
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
assert_contains "hyprland cursor=CanonicalCursor path=${test_root}/override.toml config=${test_root}/xdg platformtheme=holonight quickstyle=Holonight styleoverride="
assert_contains "desktop=HoloNight:Hyprland"
grep -F 'dbus --systemd' "${log_file}" | grep -Fq 'QT_QPA_PLATFORMTHEME QT_QUICK_CONTROLS_STYLE XCURSOR_THEME'
grep -F 'systemctl --user import-environment' "${log_file}" | grep -Fq 'QT_QPA_PLATFORMTHEME QT_QUICK_CONTROLS_STYLE XCURSOR_THEME'

: >"${log_file}"
env -i PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" HOME="${test_root}/home" \
  QT_QPA_PLATFORMTHEME=UserPlatform QT_QUICK_CONTROLS_STYLE=UserQuick QT_STYLE_OVERRIDE=UserWidgets \
  HOLONIGHT_HYPRLAND_SESSION_MODE=direct "${source_dir}/scripts/holonight-hyprland-session"
assert_contains "hyprland cursor=CanonicalCursor path= config= platformtheme=UserPlatform quickstyle=UserQuick styleoverride=UserWidgets"
if grep -F 'dbus --systemd' "${log_file}" | grep -Fq 'QT_STYLE_OVERRIDE'; then
  printf 'QT_STYLE_OVERRIDE must not be imported\n' >&2
  exit 1
fi

: >"${log_file}"
env -i PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" HOME="${test_root}/home" \
  XDG_CURRENT_DESKTOP=Hyprland HOLONIGHT_HYPRLAND_SESSION_MODE=direct \
  "${source_dir}/scripts/holonight-hyprland-session"
assert_contains "desktop=HoloNight:Hyprland"

: >"${log_file}"
env -i PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" HOME="${test_root}/home" \
  XDG_CURRENT_DESKTOP=HoloNight:Sway HOLONIGHT_HYPRLAND_SESSION_MODE=direct \
  "${source_dir}/scripts/holonight-hyprland-session"
assert_contains "desktop=HoloNight:Sway"

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
  grep -Fq "hyprland cursor=Inherited path= config= platformtheme=holonight quickstyle=Holonight" "${log_file}"
done

mv "${fake_bin}/holonight-appearance-adapter" "${fake_bin}/adapter-away"
: >"${log_file}"
XCURSOR_THEME=Inherited run_session direct
grep -Fq "hyprland cursor=Inherited path= config= platformtheme=holonight quickstyle=Holonight" "${log_file}"
mv "${fake_bin}/adapter-away" "${fake_bin}/holonight-appearance-adapter"

: >"${log_file}"
run_session uwsm
assert_contains "adapter query --appearance ${test_root}/home/.config/holonight/appearance.toml --field cursor-theme"
assert_contains "uwsm start -e -D Hyprland hyprland.desktop cursor=CanonicalCursor platformtheme=holonight quickstyle=Holonight styleoverride="

: >"${log_file}"
FAKE_SYSTEMD_ENV=$'WAYLAND_DISPLAY=wayland-9\nHYPRLAND_INSTANCE_SIGNATURE=sig\nXCURSOR_THEME=ManagerCursor\nXDG_CONFIG_HOME=/manager/config\nHOLONIGHT_APPEARANCE_FILE=/manager/appearance.toml\nQT_QPA_PLATFORMTHEME=holonight\nQT_QUICK_CONTROLS_STYLE=Holonight\nQT_STYLE_OVERRIDE=Unsupported' \
  env PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" HOLONIGHT_SHELL_SYSTEMD_ENV_TIMEOUT=0 \
  "${source_dir}/scripts/holonight-shell-systemd"
assert_contains "shell cursor=ManagerCursor path=/manager/appearance.toml config=/manager/config platformtheme=holonight quickstyle=Holonight styleoverride="

printf 'session script tests passed\n'
