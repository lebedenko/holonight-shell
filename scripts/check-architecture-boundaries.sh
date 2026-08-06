#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

allowed_surface_service_includes=(
  "MonitorOccupancyService.h"
  "NotificationService.h"
  # MprisWidgetManager (docs/sdd/mpris-desktop-widget/) drives its surfaces from MprisService's
  # centralized position/pause tracking and shares one MprisArtworkCache instance across monitors
  # (DESIGN.md §2.3/§2.4) — the same "narrow presentation orchestration" dependency shape already
  # established for MonitorOccupancyService/NotificationService.
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

echo "Architecture boundary check passed."
