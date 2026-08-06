#!/bin/bash
#
# check-pill.sh — sample a single pixel and compare it to an expected color.
#
# Coordinate system: compositor-space (global), not monitor-relative.
# Use `hyprctl monitors` to find monitor offsets if needed.
# On a single-monitor setup, compositor-space = monitor-space.
#
# Usage: check-pill.sh --x X --y Y --color RRGGBB [--tolerance N]
#   --x X          pixel X coordinate (compositor-space)
#   --y Y          pixel Y coordinate (compositor-space)
#   --color RRGGBB expected color as 6 hex digits, no '#'
#   --tolerance N  max per-channel delta (default: 5)
#
# Exit 0 = PASS, exit 1 = FAIL or error.

set -euo pipefail

# --- Startup checks ----------------------------------------------------------

command -v grim >/dev/null 2>&1 || {
  echo "ERROR: grim not installed. Install with: pacman -S grim"
  exit 1
}

command -v convert >/dev/null 2>&1 || {
  echo "ERROR: convert not installed. Install with: pacman -S imagemagick"
  exit 1
}

[ -n "${WAYLAND_DISPLAY:-}" ] || {
  echo "ERROR: WAYLAND_DISPLAY not set"
  exit 1
}

# --- Argument parsing --------------------------------------------------------

x=""
y=""
color=""
tolerance=5

while [[ $# -gt 0 ]]; do
  case "$1" in
    --x)         x="$2";         shift 2 ;;
    --y)         y="$2";         shift 2 ;;
    --color)     color="$2";     shift 2 ;;
    --tolerance) tolerance="$2"; shift 2 ;;
    *)
      echo "ERROR: unknown argument: $1"
      echo "Usage: check-pill.sh --x X --y Y --color RRGGBB [--tolerance N]"
      exit 1
      ;;
  esac
done

[ -n "$x" ] || { echo "ERROR: --x is required"; echo "Usage: check-pill.sh --x X --y Y --color RRGGBB [--tolerance N]"; exit 1; }
[ -n "$y" ] || { echo "ERROR: --y is required"; echo "Usage: check-pill.sh --x X --y Y --color RRGGBB [--tolerance N]"; exit 1; }
[ -n "$color" ] || { echo "ERROR: --color is required"; echo "Usage: check-pill.sh --x X --y Y --color RRGGBB [--tolerance N]"; exit 1; }

# --- Screenshot --------------------------------------------------------------

grim -g "${x},${y} 1x1" /tmp/pill-sample.png

# --- Color extraction --------------------------------------------------------

# convert returns 8-char RRGGBBAA; slice first 6 to strip alpha.
actual=$(convert /tmp/pill-sample.png -format "%[hex:u]" info:)
actual="${actual:0:6}"

# --- Tolerance comparison (pure bash, no bc/python) --------------------------

r_actual=$((16#${actual:0:2}))
g_actual=$((16#${actual:2:2}))
b_actual=$((16#${actual:4:2}))
r_expected=$((16#${color:0:2}))
g_expected=$((16#${color:2:2}))
b_expected=$((16#${color:4:2}))

abs_diff() { local d=$(( $1 - $2 )); echo $(( d < 0 ? -d : d )); }

r_diff=$(abs_diff "$r_actual" "$r_expected")
g_diff=$(abs_diff "$g_actual" "$g_expected")
b_diff=$(abs_diff "$b_actual" "$b_expected")

# --- Report ------------------------------------------------------------------

if (( r_diff <= tolerance && g_diff <= tolerance && b_diff <= tolerance )); then
  echo "PASS: pixel at (${x},${y}) = #${actual} (expected #${color}, tolerance ${tolerance})"
  exit 0
else
  echo "FAIL: pixel at (${x},${y}) = #${actual} (expected #${color}, tolerance ${tolerance})"
  exit 1
fi
