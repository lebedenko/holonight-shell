#!/usr/bin/env bash
set -euo pipefail

status() {
  printf '[%s] %s\n' "$1" "$2"
}

section() {
  printf '\n%s\n' "$1"
  printf '%s\n' "----------------------------------------"
}

have_command() {
  command -v "$1" >/dev/null 2>&1
}

print_var() {
  local name="$1"
  local value="${!name-}"

  if [[ -n "${value}" ]]; then
    status OK "${name}=${value}"
  else
    status WARN "${name} is unset"
  fi
}

print_systemd_var() {
  local name="$1"
  local output="$2"
  local value

  value="$(printf '%s\n' "${output}" | sed -n "s/^${name}=//p" | head -n 1)"
  if [[ -n "${value}" ]]; then
    status OK "systemd user ${name}=${value}"
  else
    status WARN "systemd user ${name} is missing"
  fi
}

print_bus_owner() {
  local name="$1"

  if ! have_command busctl; then
    status INFO "busctl is not installed; cannot inspect ${name}"
    return
  fi

  if busctl --user status "${name}" >/dev/null 2>&1; then
    local owner_pid=""
    local owner_cmd=""
    owner_pid="$(busctl --user status "${name}" 2>/dev/null | sed -n 's/^[[:space:]]*ProcessID=//p' | head -n 1)"
    owner_cmd="$(busctl --user status "${name}" 2>/dev/null | sed -n 's/^[[:space:]]*CommandLine=//p' | head -n 1)"
    if [[ -n "${owner_pid}" || -n "${owner_cmd}" ]]; then
      status OK "${name} owner pid=${owner_pid:-unknown} command=${owner_cmd:-unknown}"
    else
      status OK "${name} has an owner"
    fi
  else
    status WARN "${name} has no current owner"
  fi
}

xdg_config_home() {
  printf '%s\n' "${XDG_CONFIG_HOME:-${HOME}/.config}"
}

appearance_file() {
  if [[ -n "${HOLONIGHT_APPEARANCE_FILE:-}" ]]; then
    printf '%s\n' "${HOLONIGHT_APPEARANCE_FILE}"
  else
    printf '%s/holonight/appearance.toml\n' "$(xdg_config_home)"
  fi
}

xdg_config_dirs() {
  local dirs="${XDG_CONFIG_DIRS:-/etc/xdg}"
  printf '%s' "${dirs}"
}

xdg_data_home() {
  printf '%s\n' "${XDG_DATA_HOME:-${HOME}/.local/share}"
}

xdg_data_dirs() {
  local dirs="${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
  printf '%s' "${dirs}"
}

portal_config_search_dirs() {
  printf '%s/xdg-desktop-portal\n' "$(xdg_config_home)"

  IFS=':' read -r -a config_dirs <<<"$(xdg_config_dirs)"
  for dir in "${config_dirs[@]}"; do
    [[ -n "${dir}" ]] && printf '%s/xdg-desktop-portal\n' "${dir}"
  done

  printf '/etc/xdg-desktop-portal\n'
  printf '%s/xdg-desktop-portal\n' "$(xdg_data_home)"

  IFS=':' read -r -a data_dirs <<<"$(xdg_data_dirs)"
  for dir in "${data_dirs[@]}"; do
    [[ -n "${dir}" ]] && printf '%s/xdg-desktop-portal\n' "${dir}"
  done
}

menu_search_dirs() {
  printf '%s/menus\n' "$(xdg_config_home)"

  IFS=':' read -r -a config_dirs <<<"$(xdg_config_dirs)"
  for dir in "${config_dirs[@]}"; do
    [[ -n "${dir}" ]] && printf '%s/menus\n' "${dir}"
  done

  if [[ "$(xdg_config_dirs)" != *"/etc/xdg"* ]]; then
    printf '/etc/xdg/menus\n'
  fi
}

list_application_menus() {
  while IFS= read -r dir; do
    [[ -d "${dir}" ]] || continue
    find "${dir}" -maxdepth 1 -type f -name '*applications.menu' -print 2>/dev/null
  done < <(menu_search_dirs)
}

newest_file() {
  local description="$1"
  shift

  local newest=""
  newest="$(find "$@" -type f -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n 1 || true)"
  if [[ -n "${newest}" ]]; then
    local timestamp="${newest%% *}"
    local path="${newest#* }"
    local human_time
    human_time="$(date -d "@${timestamp%.*}" '+%Y-%m-%d %H:%M:%S %z' 2>/dev/null || printf 'unknown')"
    status OK "newest ${description}: ${path} (${human_time})"
    NEWEST_FILE_TIMESTAMP="${timestamp}"
  else
    status WARN "no ${description} found"
    NEWEST_FILE_TIMESTAMP="0"
  fi
}

echo "HoloNight desktop integration smoke check"

section "Environment"
for name in \
  WAYLAND_DISPLAY \
  DISPLAY \
  XDG_CURRENT_DESKTOP \
  XDG_SESSION_DESKTOP \
  XDG_SESSION_TYPE \
  XDG_MENU_PREFIX \
  XDG_DATA_DIRS \
  XDG_CONFIG_DIRS \
  XDG_CONFIG_HOME \
  HOLONIGHT_APPEARANCE_FILE \
  QT_QPA_PLATFORMTHEME \
  QT_QUICK_CONTROLS_STYLE \
  QT_STYLE_OVERRIDE \
  XCURSOR_THEME \
  DBUS_SESSION_BUS_ADDRESS; do
  print_var "${name}"
done

if have_command systemctl; then
  systemd_environment="$(systemctl --user show-environment 2>/dev/null || true)"
  for name in XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP XDG_SESSION_TYPE XDG_MENU_PREFIX DBUS_SESSION_BUS_ADDRESS \
    XDG_CONFIG_HOME HOLONIGHT_APPEARANCE_FILE QT_QPA_PLATFORMTHEME QT_QUICK_CONTROLS_STYLE XCURSOR_THEME; do
    print_systemd_var "${name}" "${systemd_environment}"
  done
else
  status INFO "systemctl is not installed; cannot inspect systemd user environment"
fi

section "Qt activation"
if [[ "${QT_QPA_PLATFORMTHEME:-}" == "holonight" ]]; then
  status OK "QT_QPA_PLATFORMTHEME=holonight"
else
  status WARN "QT_QPA_PLATFORMTHEME=${QT_QPA_PLATFORMTHEME:-missing}; expected holonight or an explicit user override"
fi
if [[ "${QT_QUICK_CONTROLS_STYLE:-}" == "Holonight" ]]; then
  status OK "QT_QUICK_CONTROLS_STYLE=Holonight"
else
  status WARN "QT_QUICK_CONTROLS_STYLE=${QT_QUICK_CONTROLS_STYLE:-missing}; expected Holonight or an explicit user override"
fi
if [[ -n "${QT_STYLE_OVERRIDE:-}" ]]; then
  status WARN "QT_STYLE_OVERRIDE=${QT_STYLE_OVERRIDE} is unsupported as a global HoloNight session setting"
else
  status OK "QT_STYLE_OVERRIDE is unset"
fi

section "Canonical cursor"
canonical_appearance="$(appearance_file)"
status INFO "canonical appearance file: ${canonical_appearance}"
if have_command holonight-appearance-adapter; then
  canonical_cursor="$(holonight-appearance-adapter query --appearance "${canonical_appearance}" --field cursor-theme 2>/dev/null || true)"
  if [[ -z "${canonical_cursor}" || "${canonical_cursor}" == *$'\n'* || ${#canonical_cursor} -gt 4096 || "${canonical_cursor}" =~ [[:cntrl:]] ]]; then
    status WARN "canonical cursor query failed; native/inherited cursor fallback remains active"
  elif [[ "${XCURSOR_THEME:-}" == "${canonical_cursor}" ]]; then
    status OK "active XCURSOR_THEME matches canonical cursor ${canonical_cursor}"
  else
    status WARN "active XCURSOR_THEME=${XCURSOR_THEME:-native fallback}; canonical=${canonical_cursor}; session restart required"
  fi
else
  status INFO "holonight-appearance-adapter is unavailable; native/inherited cursor fallback remains active"
fi

if [[ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ]]; then
  status OK "D-Bus activation import source is available in the process environment"
else
  status WARN "D-Bus activation import source is unavailable"
fi

section "Portals"
print_bus_owner org.freedesktop.portal.Desktop
print_bus_owner org.freedesktop.impl.portal.desktop.holonight
print_bus_owner org.freedesktop.impl.portal.desktop.hyprland
print_bus_owner org.freedesktop.impl.portal.desktop.gtk
print_bus_owner org.freedesktop.impl.portal.desktop.kde

if have_command busctl && busctl --user status org.freedesktop.impl.portal.desktop.holonight >/dev/null 2>&1; then
  portal_scheme="$(busctl --user call org.freedesktop.impl.portal.desktop.holonight /org/freedesktop/portal/desktop \
    org.freedesktop.impl.portal.Settings Read ss org.freedesktop.appearance color-scheme 2>/dev/null || true)"
  portal_accent="$(busctl --user call org.freedesktop.impl.portal.desktop.holonight /org/freedesktop/portal/desktop \
    org.freedesktop.impl.portal.Settings Read ss org.freedesktop.appearance accent-color 2>/dev/null || true)"
  status INFO "HoloNight portal color-scheme: ${portal_scheme:-unavailable}"
  status INFO "HoloNight portal accent-color: ${portal_accent:-unavailable}"
fi

hyprland_desktop_configured=false
IFS=':' read -r -a current_desktops <<<"${XDG_CURRENT_DESKTOP:-}"
for desktop in "${current_desktops[@]}"; do
  if [[ "${desktop,,}" == "hyprland" ]]; then
    hyprland_desktop_configured=true
    break
  fi
done

if [[ "${hyprland_desktop_configured}" == true ]]; then
  status OK "XDG_CURRENT_DESKTOP identifies Hyprland"
else
  status WARN "XDG_CURRENT_DESKTOP does not identify Hyprland"
fi

case ":${XDG_CONFIG_DIRS:-/etc/xdg}:" in
  *:/usr/share/holonight/xdg:*) status OK "HoloNight portal routing layer is active" ;;
  *) status WARN "HoloNight portal routing layer is absent from XDG_CONFIG_DIRS" ;;
esac

holonight_portal_file=""
holonight_portals_conf=""
while IFS= read -r dir; do
  [[ -n "${holonight_portal_file}" ]] || [[ ! -f "${dir}/portals/holonight.portal" ]] || \
    holonight_portal_file="${dir}/portals/holonight.portal"
  [[ -n "${holonight_portals_conf}" ]] || [[ ! -f "${dir}/holonight-portals.conf" ]] || \
    holonight_portals_conf="${dir}/holonight-portals.conf"
done < <(portal_config_search_dirs)

if [[ -n "${holonight_portal_file}" ]]; then
  status OK "HoloNight portal descriptor: ${holonight_portal_file}"
else
  status WARN "HoloNight portal descriptor was not found in XDG data portal search paths"
fi

if [[ -n "${holonight_portals_conf}" ]]; then
  status OK "HoloNight portal routing config: ${holonight_portals_conf}"
else
  status WARN "holonight-portals.conf was not found in XDG portal config/data search paths"
fi

section "D-Bus desktop services"
print_bus_owner org.freedesktop.ScreenSaver
print_bus_owner org.freedesktop.Notifications
print_bus_owner org.kde.StatusNotifierWatcher

section "HoloNight shell service"
if have_command systemctl; then
  if systemctl --user --quiet is-active holonight-shell.service; then
    status OK "holonight-shell.service is active"
  else
    service_state="$(systemctl --user is-active holonight-shell.service 2>/dev/null || true)"
    status WARN "holonight-shell.service is ${service_state:-unknown}"
  fi

  if systemctl --user cat holonight-shell.service >/dev/null 2>&1; then
    status OK "holonight-shell.service is installed"
  else
    status WARN "holonight-shell.service is not installed in the systemd user unit search path"
  fi
else
  status INFO "systemctl is not installed; cannot inspect holonight-shell.service"
fi

section "MIME/default apps"
if have_command xdg-mime; then
  for mime in x-scheme-handler/http x-scheme-handler/https inode/directory text/plain image/png video/mp4; do
    default_app="$(xdg-mime query default "${mime}" 2>/dev/null || true)"
    if [[ -n "${default_app}" ]]; then
      status OK "${mime}: ${default_app}"
    else
      status WARN "${mime}: no default application"
    fi
  done
else
  status INFO "xdg-mime is not installed"
fi

if have_command xdg-settings; then
  default_browser="$(xdg-settings get default-web-browser 2>/dev/null || true)"
  if [[ -n "${default_browser}" ]]; then
    status OK "default-web-browser: ${default_browser}"
  else
    status WARN "default-web-browser is unset"
  fi
else
  status INFO "xdg-settings is not installed"
fi

section "XDG menus"
menu_prefix="${XDG_MENU_PREFIX:-}"
if [[ -n "${menu_prefix}" ]]; then
  selected_menu="${menu_prefix}applications.menu"
  found_selected=""
  while IFS= read -r dir; do
    candidate="${dir}/${selected_menu}"
    if [[ -f "${candidate}" ]]; then
      found_selected="${candidate}"
      break
    fi
  done < <(menu_search_dirs)

  if [[ -n "${found_selected}" ]]; then
    status OK "selected menu exists: ${found_selected}"
  else
    status WARN "selected menu ${selected_menu} was not found"
  fi
else
  status WARN "XDG_MENU_PREFIX is unset; KDE/KIO may select the wrong applications menu"
fi

available_menus="$(list_application_menus || true)"
if [[ -n "${available_menus}" ]]; then
  printf '%s\n' "${available_menus}" | sed 's/^/[INFO] available menu: /'
else
  status WARN "no *applications.menu files found in XDG menu search dirs"
fi

section "KDE caches"
newest_file '.desktop file' "${HOME}/.local/share/applications" /usr/local/share/applications /usr/share/applications \
  -name '*.desktop'
desktop_timestamp="${NEWEST_FILE_TIMESTAMP}"
newest_file 'mimeinfo.cache' "${HOME}/.local/share/applications" /usr/local/share/applications /usr/share/applications -name mimeinfo.cache
mime_timestamp="${NEWEST_FILE_TIMESTAMP}"
cache_home="${XDG_CACHE_HOME:-${HOME}/.cache}"
newest_file 'ksycoca6 cache' "${cache_home}" -name 'ksycoca6*'
sycoca_timestamp="${NEWEST_FILE_TIMESTAMP}"

if [[ "${sycoca_timestamp%.*}" == "0" ]]; then
  status WARN "KDE sycoca cache is missing; run kbuildsycoca6 --noincremental inside the fixed session"
elif awk "BEGIN { exit !(${sycoca_timestamp} < ${desktop_timestamp} || ${sycoca_timestamp} < ${mime_timestamp}) }"; then
  status WARN "KDE sycoca cache is older than desktop or MIME cache data"
else
  status OK "KDE sycoca cache is at least as fresh as desktop and MIME cache data"
fi

section "Suggested next actions"
status INFO "portal color/accent changes are live for consumers that observe Settings signals"
status INFO "GTK settings and application-native preferences may require an application relaunch"
status INFO "cursor environment changes require a full session restart"
status INFO "login entry: select HoloNight (Hyprland)"
status INFO "bootstrap script: holonight-hyprland-session"
status INFO "force direct mode: HOLONIGHT_HYPRLAND_SESSION_MODE=direct holonight-hyprland-session"
status INFO "force UWSM mode: HOLONIGHT_HYPRLAND_SESSION_MODE=uwsm holonight-hyprland-session"
status INFO "manual Hyprland config fallback: exec-once = uwsm app -- holonight-shell"
status INFO "manual systemd fallback: systemctl --user enable --now holonight-shell.service"
