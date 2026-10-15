#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
build=build/dev
prefix="$PWD/build/deps/prefix"
stage="$PWD/build/installed"
mkdir -p /work/output
    task build PRESET=release
    DESTDIR=/work/stage cmake --install build/release
    test -x /work/stage/usr/bin/holonight-shell
    test -f /work/stage/usr/lib/cmake/HoloNightShellConfig/HoloNightShellConfigConfig.cmake
    cp -a /work/stage /work/output/
