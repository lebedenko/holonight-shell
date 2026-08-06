#!/bin/bash
#
# scripts/delayed-screenshot.sh
# Take a screenshot after a configurable delay, with optional geometry.
#
# Usage:
#   scripts/delayed-screenshot.sh [-t <timeout>] [-g <X:Y:W:H>] [output-file]
#

set -euo pipefail

# Check requirements
command -v grim >/dev/null 2>&1 || {
  echo "ERROR: grim is not installed. Install with your package manager (e.g. pacman -S grim)." >&2
  exit 1
}

# Defaults
TIMEOUT=5
GEOMETRY=""
OUTPUT_FILE=""

show_help() {
  echo "Usage: $(basename "$0") [options] [output-file]"
  echo ""
  echo "Options:"
  echo "  -t, --timeout <seconds>   Delay before taking screenshot (default: 5)"
  echo "  -g, --geometry <X:Y:W:H>  Capture a specific region (coordinates are X:Y:W:H)"
  echo "  -h, --help                Show this help message"
  echo ""
  echo "If [output-file] is not specified, it defaults to screenshot_YYYY-MM-DD_HHMMSS.png"
}

# Parse options
while [[ $# -gt 0 ]]; do
  case "$1" in
    -t|--timeout)
      if [[ -z "${2:-}" || ! "$2" =~ ^[0-9]+$ ]]; then
        echo "ERROR: --timeout requires a non-negative integer argument" >&2
        exit 1
      fi
      TIMEOUT="$2"
      shift 2
      ;;
    -g|--geometry)
      if [[ -z "${2:-}" ]]; then
        echo "ERROR: --geometry requires an argument in X:Y:W:H format" >&2
        exit 1
      fi
      GEOMETRY="$2"
      shift 2
      ;;
    -h|--help)
      show_help
      exit 0
      ;;
    -*)
      echo "ERROR: Unknown option '$1'" >&2
      show_help >&2
      exit 1
      ;;
    *)
      if [[ -n "$OUTPUT_FILE" ]]; then
        echo "ERROR: Multiple output files specified" >&2
        exit 1
      fi
      OUTPUT_FILE="$1"
      shift
      ;;
  esac
done

# Validate & format geometry
GRIM_GEOM=""
if [[ -n "$GEOMETRY" ]]; then
  # Regex to match X:Y:W:H, where X and Y can be negative (e.g. -100:200:300:400)
  # W and H must be positive integers (1 or more digits)
  if [[ "$GEOMETRY" =~ ^(-?[0-9]+):(-?[0-9]+):([0-9]+):([0-9]+)$ ]]; then
    X="${BASH_REMATCH[1]}"
    Y="${BASH_REMATCH[2]}"
    W="${BASH_REMATCH[3]}"
    H="${BASH_REMATCH[4]}"
    GRIM_GEOM="${X},${Y} ${W}x${H}"
  else
    echo "ERROR: Invalid geometry format '$GEOMETRY'. Expected X:Y:W:H (e.g. 100:200:300:400)" >&2
    exit 1
  fi
fi

# Set default output file if not provided
if [[ -z "$OUTPUT_FILE" ]]; then
  OUTPUT_FILE="screenshot_$(date +%Y-%m-%d_%H%M%S).png"
fi

# Countdown
if [ "$TIMEOUT" -gt 0 ]; then
  echo "Taking screenshot in $TIMEOUT seconds..."
  for ((i = TIMEOUT - 1; i >= 1; i--)); do
    sleep 1
    echo "$i..."
  done
  sleep 1
  echo "Capturing!"
else
  echo "Capturing immediately!"
fi

# Run grim
grim_args=()
if [[ -n "$GRIM_GEOM" ]]; then
  grim_args+=("-g" "$GRIM_GEOM")
fi
grim_args+=("$OUTPUT_FILE")

grim "${grim_args[@]}"

echo "Screenshot saved to: $OUTPUT_FILE"
