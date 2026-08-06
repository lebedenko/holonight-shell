#!/usr/bin/env bash
set -euo pipefail

with_static_tools=false

usage() {
  cat <<'EOF'
Usage: scripts/install-ci-dependencies.sh [--with-static-tools]

Install Debian packages required by CI build/test jobs.

Options:
  --with-static-tools  Also install clang-format and clang-tidy for static checks.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --with-static-tools)
      with_static_tools=true
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "ERROR: unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

packages=(
  ca-certificates
  cmake
  dbus
  g++
  git
  libgmock-dev
  libgl1-mesa-dev
  libgtest-dev
  libpulse-dev
  libsecret-1-dev
  libqt6sql6-sqlite
  libtomlplusplus-dev
  libvulkan-dev
  libwayland-bin
  libwayland-dev
  ninja-build
  pkg-config
  qml6-module-qtquick-controls
  qml6-module-qtquick-effects
  qml6-module-qtquick-layouts
  qml6-module-qtquick-shapes
  qt6-base-dev
  qt6-base-private-dev
  qt6-declarative-dev
  qt6-declarative-dev-tools
  qt6-wayland-dev
  wayland-protocols
)

if [[ "${with_static_tools}" == true ]]; then
  packages+=(
    clang-format
    clang-tidy
  )
fi

apt-get update
apt-get install -y --no-install-recommends "${packages[@]}"

if [[ "${with_static_tools}" == true ]]; then
  command -v run-clang-tidy >/dev/null
fi
