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

make_compositor() {
  local name="$1"
  make_fake "${name}" 'printf "compositor=%s desktop=%s session=%s type=%s menu=%s cursor=%s configdirs=%s qt=%s quick=%s style=%s hypr=%s sway=%s i3=%s\n" "${0##*/}" "${XDG_CURRENT_DESKTOP-}" "${XDG_SESSION_DESKTOP-}" "${XDG_SESSION_TYPE-}" "${XDG_MENU_PREFIX-}" "${XCURSOR_THEME-}" "${XDG_CONFIG_DIRS-}" "${QT_QPA_PLATFORMTHEME-}" "${QT_QUICK_CONTROLS_STYLE-}" "${QT_STYLE_OVERRIDE-}" "${HYPRLAND_INSTANCE_SIGNATURE-}" "${SWAYSOCK-}" "${I3SOCK-}" >>"${TEST_LOG}"'
}

make_compositor Hyprland
make_compositor sway
make_fake uwsm 'printf "uwsm %s desktop=%s cursor=%s\n" "$*" "${XDG_CURRENT_DESKTOP-}" "${XCURSOR_THEME-}" >>"${TEST_LOG}"'
make_fake dbus-update-activation-environment 'printf "dbus %s\n" "$*" >>"${TEST_LOG}"'
make_fake systemctl '
if [[ "$*" == "--user show-environment" ]]; then
  printf "%s\n" "${FAKE_SYSTEMD_ENV-}"
else
  printf "systemctl %s\n" "$*" >>"${TEST_LOG}"
fi'
make_fake holonight-shell 'printf "shell desktop=%s hypr=%s hyprcmd=%s sway=%s i3=%s style=%s\n" "${XDG_CURRENT_DESKTOP-}" "${HYPRLAND_INSTANCE_SIGNATURE-}" "${HYPRLAND_CMD-}" "${SWAYSOCK-}" "${I3SOCK-}" "${QT_STYLE_OVERRIDE-}" >>"${TEST_LOG}"'
make_fake sleep ':'
ln -s /usr/bin/bash "${fake_bin}/bash"

make_adapter() {
  make_fake holonight-appearance-adapter '
printf "adapter %s\n" "$*" >>"${TEST_LOG}"
case "${ADAPTER_MODE:-ok}" in
  ok) printf "%s\n" "${ADAPTER_VALUE:-CanonicalCursor}" ;;
  fail) exit 4 ;;
  empty) : ;;
  multiline) printf "one\ntwo\n" ;;
  control) printf "bad\tvalue\n" ;;
  oversized) printf "%05000d\n" 0 ;;
esac'
}

assert_line() {
  grep -Fqx "$1" "${log_file}" || { printf 'missing log line: %s\n' "$1" >&2; exit 1; }
}

assert_match() {
  grep -F "$1" "${log_file}" >/dev/null || { printf 'missing log text: %s\n' "$1" >&2; exit 1; }
}

run_session() {
  local compositor="$1"
  local mode="$2"
  shift 2
  env -i PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" HOME="${test_root}/home" \
    HOLONIGHT_SESSION_MODE="${mode}" "$@" "${source_dir}/scripts/holonight-session" "${compositor}"
}

make_adapter

for compositor in hyprland sway; do
  : >"${log_file}"
  run_session "${compositor}" direct XDG_CONFIG_HOME="${test_root}/xdg" \
    HOLONIGHT_APPEARANCE_FILE="${test_root}/override.toml" QT_QPA_PLATFORMTHEME=UserQt \
    QT_QUICK_CONTROLS_STYLE=UserQuick QT_STYLE_OVERRIDE=UserWidgets XDG_SESSION_TYPE=custom \
    XDG_SESSION_DESKTOP=UserDesktop XDG_MENU_PREFIX=user- XDG_CONFIG_DIRS=/user/xdg \
    HYPRLAND_INSTANCE_SIGNATURE=stale-hypr HYPRLAND_CMD=stale-command SWAYSOCK=stale-sway I3SOCK=stale-i3
  assert_line "adapter query --appearance ${test_root}/override.toml --field cursor-theme"
  if [[ "${compositor}" == hyprland ]]; then
    assert_match "compositor=Hyprland desktop=HoloNight:Hyprland session=UserDesktop type=custom menu=user- cursor=CanonicalCursor configdirs=/usr/share/holonight/xdg:/user/xdg qt=UserQt quick=UserQuick style=UserWidgets hypr=stale-hypr sway= i3="
    assert_match "dbus --systemd SWAYSOCK= I3SOCK="
    assert_match "systemctl --user unset-environment SWAYSOCK I3SOCK"
  else
    assert_match "compositor=sway desktop=HoloNight:sway session=UserDesktop type=custom menu=user- cursor=CanonicalCursor configdirs=/usr/share/holonight/xdg:/user/xdg qt=UserQt quick=UserQuick style=UserWidgets hypr= sway=stale-sway i3=stale-i3"
    assert_match "dbus --systemd HYPRLAND_INSTANCE_SIGNATURE= HYPRLAND_CMD="
    assert_match "systemctl --user unset-environment HYPRLAND_INSTANCE_SIGNATURE HYPRLAND_CMD"
  fi
  if grep -F 'dbus --systemd WAYLAND_DISPLAY' "${log_file}" | grep -Fq QT_STYLE_OVERRIDE; then
    printf 'QT_STYLE_OVERRIDE must not be imported\n' >&2
    exit 1
  fi
done

: >"${log_file}"
run_session sway direct XDG_CONFIG_HOME="${test_root}/xdg"
assert_line "adapter query --appearance ${test_root}/xdg/holonight/appearance.toml --field cursor-theme"

for mode in fail empty multiline control oversized; do
  : >"${log_file}"
  run_session hyprland direct ADAPTER_MODE="${mode}" XCURSOR_THEME=Inherited
  assert_match "compositor=Hyprland desktop=HoloNight:Hyprland session=Hyprland type=wayland menu=hyprland- cursor=Inherited"
done

mv "${fake_bin}/holonight-appearance-adapter" "${fake_bin}/adapter-away"
: >"${log_file}"
env -i PATH="${fake_bin}" TEST_LOG="${log_file}" HOME="${test_root}/home" XCURSOR_THEME=Inherited \
  HOLONIGHT_SESSION_MODE=direct /usr/bin/bash "${source_dir}/scripts/holonight-session" sway
assert_match "compositor=sway desktop=HoloNight:sway session=sway type=wayland menu=sway- cursor=Inherited"
mv "${fake_bin}/adapter-away" "${fake_bin}/holonight-appearance-adapter"

for compositor in hyprland sway; do
  for mode in auto uwsm; do
    : >"${log_file}"
    run_session "${compositor}" "${mode}"
    assert_match "systemctl --user daemon-reload"
    assert_match "systemctl --user start holonight-shell.service"
    if [[ "${compositor}" == hyprland ]]; then
      assert_match "uwsm start -e -D Hyprland hyprland.desktop"
    else
      assert_match "uwsm start -e -D sway sway.desktop"
    fi
  done
done

mv "${fake_bin}/uwsm" "${fake_bin}/uwsm-away"
: >"${log_file}"
env -i PATH="${fake_bin}" TEST_LOG="${log_file}" HOME="${test_root}/home" HOLONIGHT_SESSION_MODE=auto \
  /usr/bin/bash "${source_dir}/scripts/holonight-session" sway
assert_match "compositor=sway"
if grep -Fq 'start holonight-shell.service' "${log_file}"; then
  printf 'direct auto fallback must not start the shell service\n' >&2
  exit 1
fi
if env -i PATH="${fake_bin}" TEST_LOG="${log_file}" HOME="${test_root}/home" HOLONIGHT_SESSION_MODE=uwsm \
  /usr/bin/bash "${source_dir}/scripts/holonight-session" sway >"${test_root}/stdout" 2>"${test_root}/stderr"; then
  printf 'uwsm mode unexpectedly succeeded without uwsm\n' >&2
  exit 1
fi
grep -Fq 'HOLONIGHT_SESSION_MODE=uwsm was requested' "${test_root}/stderr"
mv "${fake_bin}/uwsm-away" "${fake_bin}/uwsm"

for args in '' 'river' 'hyprland extra'; do
  read -r -a argv <<<"${args}"
  if env -i PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" \
    "${source_dir}/scripts/holonight-session" "${argv[@]}" >"${test_root}/stdout" 2>"${test_root}/stderr"; then
    printf 'invalid compositor arguments unexpectedly succeeded: %s\n' "${args}" >&2
    exit 1
  fi
done

if run_session hyprland invalid >"${test_root}/stdout" 2>"${test_root}/stderr"; then
  printf 'invalid session mode unexpectedly succeeded\n' >&2
  exit 1
fi
grep -Fq 'unknown HOLONIGHT_SESSION_MODE=invalid' "${test_root}/stderr"

mv "${fake_bin}/sway" "${fake_bin}/sway-away"
if env -i PATH="${fake_bin}" TEST_LOG="${log_file}" HOME="${test_root}/home" HOLONIGHT_SESSION_MODE=direct \
  /usr/bin/bash "${source_dir}/scripts/holonight-session" sway >"${test_root}/stdout" 2>"${test_root}/stderr"; then
  printf 'missing compositor unexpectedly succeeded\n' >&2
  exit 1
fi
grep -Fq 'sway was not found in PATH' "${test_root}/stderr"
mv "${fake_bin}/sway-away" "${fake_bin}/sway"

for desktop in 'HoloNight:Hyprland' 'HoloNight:sway'; do
  : >"${log_file}"
  if [[ "${desktop}" == *Hyprland ]]; then
    marker_env=$'HYPRLAND_INSTANCE_SIGNATURE=sig\nHYPRLAND_CMD=hyprctl\nSWAYSOCK=stale-sway\nI3SOCK=stale-i3'
    expected='shell desktop=HoloNight:Hyprland hypr=sig hyprcmd=hyprctl sway= i3= style='
  else
    marker_env=$'HYPRLAND_INSTANCE_SIGNATURE=stale-hypr\nHYPRLAND_CMD=stale-command\nSWAYSOCK=/run/sway.sock\nI3SOCK=/run/i3.sock'
    expected='shell desktop=HoloNight:sway hypr= hyprcmd= sway=/run/sway.sock i3=/run/i3.sock style='
  fi
  FAKE_SYSTEMD_ENV="WAYLAND_DISPLAY=wayland-9
XDG_CURRENT_DESKTOP=${desktop}
${marker_env}
QT_STYLE_OVERRIDE=Unsupported" env PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" \
    HOLONIGHT_SHELL_SYSTEMD_ENV_TIMEOUT=0 "${source_dir}/scripts/holonight-shell-systemd"
  assert_line "${expected}"
done

# A stale marker from the non-selected compositor is insufficient.
: >"${log_file}"
FAKE_SYSTEMD_ENV=$'WAYLAND_DISPLAY=wayland-9\nXDG_CURRENT_DESKTOP=HoloNight:sway\nHYPRLAND_INSTANCE_SIGNATURE=stale' \
  env PATH="${fake_bin}:/usr/bin:/bin" TEST_LOG="${log_file}" HOLONIGHT_SHELL_SYSTEMD_ENV_TIMEOUT=0 \
  "${source_dir}/scripts/holonight-shell-systemd" >/dev/null 2>&1 || true
if grep -Fq 'shell ' "${log_file}"; then
  printf 'wrapper accepted the wrong compositor marker\n' >&2
  exit 1
fi

printf 'session script tests passed\n'
