#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

allowed_surface_service_includes=(
  "NotificationService.h"
  # MprisWidgetManager (docs/sdd/mpris-desktop-widget/) drives its surfaces from MprisService's
  # centralized position/pause tracking and shares one MprisArtworkCache instance across monitors
  # (DESIGN.md §2.3/§2.4) — the same "narrow presentation orchestration" dependency shape already
  # established for NotificationService.
  "MprisService.h"
  "MprisArtworkCache.h"
)

mapfile -t service_headers < <(
  find "${repo_root}/libs/holonight-services/src" -type f -name '*.h' -printf '%f\n' | sort -u
)

declare -A service_header_set=()
for header in "${service_headers[@]}"; do
  service_header_set["${header}"]=1
done

declare -A allowed_surface_service_include_set=()
for header in "${allowed_surface_service_includes[@]}"; do
  allowed_surface_service_include_set["${header}"]=1
done

violations=()
while IFS= read -r include_line; do
  file="${include_line%%:*}"
  rest="${include_line#*:}"
  line="${rest%%:*}"
  include_target="${include_line##*#include \"}"
  include_target="${include_target%%\"*}"
  include_basename="${include_target##*/}"

  if [[ -n "${service_header_set[${include_basename}]:-}" && -z "${allowed_surface_service_include_set[${include_basename}]:-}" ]]; then
    violations+=("${file}:${line}: ${include_target}")
  fi
done < <(grep -RIn --include='*.h' --include='*.cpp' '#include "' "${repo_root}/libs/holonight-surfaces/src")

if (( ${#violations[@]} > 0 )); then
  {
    echo "Unexpected service header includes from libs/holonight-surfaces/src:"
    printf '  %s\n' "${violations[@]}"
    echo
    echo "Either move the orchestration out of surfaces or add the header to"
    echo "allowed_surface_service_includes in scripts/check-architecture-boundaries.sh with a reviewed rationale."
  } >&2
  exit 1
fi

local_layer_shell_violations=()
while IFS= read -r violation; do
  local_layer_shell_violations+=("${violation}")
done < <(grep -RInE --include='*.h' --include='*.cpp' --include='CMakeLists.txt' \
  '(qwayland-wlr-layer-shell|wayland-wlr-layer-shell|zwlr_layer_(shell|surface)_v1|wl_surface_commit|class LayerShell;|class LayerSurface;|#include "LayerShell.h"|#include "LayerSurface.h")' \
  "${repo_root}/apps" "${repo_root}/libs" "${repo_root}/tests" || true)

if (( ${#local_layer_shell_violations[@]} > 0 )); then
  {
    echo "Local layer-shell ownership or direct protocol use is forbidden:"
    printf '  %s\n' "${local_layer_shell_violations[@]}"
    echo
    echo "Describe surfaces with Holonight::Wayland::LayerSurfaceSpec and own them with LayerSurfaceHost."
  } >&2
  exit 1
fi

authentication_violations=()
while IFS= read -r violation; do authentication_violations+=("${violation}"); done < <(
  grep -RInE --include='*.h' --include='*.cpp' --include='*.qml' --include='CMakeLists.txt' \
    '(HolonightShell|holonight_(app|services|surfaces|compositor)|LayerSurface|layer-shell|ControlServer)' \
    "${repo_root}/apps/authentication" "${repo_root}/libs/holonight-authentication" \
    "${repo_root}/qml/Authentication" || true
)
if (( ${#authentication_violations[@]} > 0 )); then
  {
    echo "Authentication targets must remain independent of shell-owned code:"
    printf '  %s\n' "${authentication_violations[@]}"
  } >&2
  exit 1
fi

authentication_install_violations=()
while IFS= read -r violation; do authentication_install_violations+=("${violation}"); done < <(
  grep -RInE --exclude='check-architecture-boundaries.sh' --include='CMakeLists.txt' --include='*.cmake' --include='*.sh' \
    '(/etc/sudo\.conf|systemctl[^#]*(enable|disable|mask|stop).*(polkit|policykit)|pkill[^#]*(polkit|policykit))' \
    "${repo_root}/CMakeLists.txt" "${repo_root}/apps" "${repo_root}/cmake" "${repo_root}/scripts" || true
)
if (( ${#authentication_install_violations[@]} > 0 )); then
  {
    echo "Authentication packaging must not modify sudo policy or competing agents:"
    printf '  %s\n' "${authentication_install_violations[@]}"
  } >&2
  exit 1
fi

echo "Architecture boundary check passed."
