#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
build=build/dev
prefix="$PWD/build/deps/prefix"
stage="$PWD/build/installed"
mkdir -p /work/output
package_acceptance() {
  cmake -S libs/holonight-shell-config -B build/config-package -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_PREFIX_PATH="$PWD/build/deps/prefix" \
    -DCMAKE_INSTALL_PREFIX=/work/config-prefix -DCMAKE_INSTALL_LIBDIR=lib
  cmake --build build/config-package --parallel "$JOBS"
  cmake --install build/config-package
  cmake -S tests/shell-config-package-consumer -B build/package-consumer -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_PREFIX_PATH="/work/config-prefix;$PWD/build/deps/prefix"
  cmake --build build/package-consumer --parallel "$JOBS"
  build/package-consumer/holonight-shell-config-consumer
}
package_acceptance
python3 scripts/quality.py tidy --build build/package-consumer
