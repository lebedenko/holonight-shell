#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
lane=$1
mkdir /work/shell
cp -a /input/. /work/shell/
cd /work/shell
export HOME=/work/build/home LC_ALL=C.UTF-8 TZ=UTC
export JOBS=${JOBS:-2} CMAKE_BUILD_PARALLEL_LEVEL=${JOBS:-2}
export XDG_RUNTIME_DIR=/work/runtime QT_FORCE_STDERR_LOGGING=1
export QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RHI_BACKEND=software
export QT_QPA_PLATFORMTHEME= QT_STYLE_OVERRIDE=
mkdir -p "$HOME"
mkdir -m 700 "$XDG_RUNTIME_DIR"
if [ "$lane" = licensing ]; then
  reuse --version
  reuse lint
  exit
fi
mkdir -p build/ci-provenance
pacman -Q > build/ci-provenance/packages.txt
c++ --version > build/ci-provenance/compiler.txt
cmake --version > build/ci-provenance/cmake.txt
{ python3 --version; task --version; ninja --version; clang-format --version; clang-tidy --version; qmake6 --version; wayland-scanner --version; } > build/ci-provenance/versions.txt 2>&1
capture() {
  status=$?
  cp -a build/ci-provenance /output/
  if [ -d build/test/uqc-launch-logs ]; then cp -a build/test/uqc-launch-logs /output/; fi
  if [ -d build/test/Testing ]; then cp -a build/test/Testing /output/; fi
  if [ -f build/test/compile_commands.json ]; then cp build/test/compile_commands.json /output/; fi
  if [ -d .cache/tooling ]; then cp -a .cache/tooling /output/tooling; fi
  exit "$status"
}
trap capture 0
if [ "$lane" = prepare ]; then
  export HOLONIGHT_CONFIG_SOURCE=/prepared/sources/holonight-config
  export HOLONIGHT_QT_SOURCE=/prepared/sources/holonight-qt
  export HOLONIGHT_SYSTEM_SERVICES_SOURCE=/prepared/sources/holonight-system-services
  if [ -d /prepared/deps ]; then cp -a /prepared/deps build/deps; fi
  task deps PRESET=test
  mkdir -p /prepared/deps
  cp -a build/deps/. /prepared/deps/
  cp -a build/ci-provenance /prepared/environment
  exit
fi
cp -a /prepared/deps build/deps
cp /prepared/provider-revisions.json build/ci-provenance/
cp build/deps/provider-revisions.tsv build/ci-provenance/
export LD_LIBRARY_PATH="$PWD/build/deps/prefix/lib"
python3 scripts/ci/test_launcher.py
python3 scripts/ci/test_dependencies.py
python3 scripts/ci/test_tooling_tidy.py
scripts/check-architecture-boundaries.sh
package_acceptance() {
  cmake -S libs/holonight-shell-config -B build/config-package -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DHOLONIGHT_DEPENDENCY_PREFIX="$PWD/build/deps/prefix" \
    -DCMAKE_INSTALL_PREFIX=/work/config-prefix -DCMAKE_INSTALL_LIBDIR=lib
  cmake --build build/config-package --parallel "$JOBS"
  cmake --install build/config-package
  cmake -S tests/shell-config-package-consumer -B build/package-consumer -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_PREFIX_PATH="/work/config-prefix;$PWD/build/deps/prefix"
  cmake --build build/package-consumer --parallel "$JOBS"
  build/package-consumer/holonight-shell-config-consumer
}
case "$lane" in
  build-test)
    bwrap --unshare-net --ro-bind / / -- true
    task test
    cat build/test/Testing/Temporary/LastTest.log
    scripts/check-qmltypes.sh build/test
    package_acceptance
    ;;
  release)
    task build PRESET=release
    DESTDIR=/work/stage cmake --install build/release
    test -x /work/stage/usr/bin/holonight-shell
    test -f /work/stage/usr/lib/cmake/HoloNightShellConfig/HoloNightShellConfigConfig.cmake
    cp -a /work/stage /output/
    ;;
  static-checks)
    task build PRESET=test
    task format-check
    package_acceptance
    task tidy
    cmake --build build/test --target qml-lint
    python3 tooling/qml_policy.py
    scripts/check-qmltypes.sh build/test
    python3 scripts/ci/tidy-package-consumer.py
    ;;
  *) echo "Unknown lane: $lane" >&2; exit 2 ;;
esac
