#!/bin/sh
set -eu
lane=$1
mkdir /work/source
cp -a /input/. /work/source/
cd /work/source
export HOME=/work/build/home LC_ALL=C.UTF-8 TZ=UTC
mkdir -p "$HOME"
if [ "$lane" = licensing ]; then
  reuse --version
  reuse lint
  exit
fi
python3 --version
cmake --version
ninja --version
c++ --version
clang-format --version
clang-tidy --version
pkg-config --modversion Qt6Core Qt6Quick
bwrap --version
sway --version
python3 scripts/ci/test_launcher.py
python3 scripts/ci/test_tooling_tidy.py
mkdir -p /work/providers
fetch_provider() {
  name=$1
  revision=$2
  git init -q "/work/providers/$name"
  git -C "/work/providers/$name" fetch --depth 1 "https://github.com/lebedenko/$name.git" "$revision"
  git -C "/work/providers/$name" checkout --detach FETCH_HEAD
  [ "$(git -C "/work/providers/$name" rev-parse HEAD)" = "$revision" ]
}
fetch_provider holonight-config fe69a59e6b73167fd5349223a4d265d75386c139
fetch_provider holonight-qt 8d11e3e91fea5ad0d20a34f2ed27e5e5f485124a
fetch_provider holonight-system-services 3e2928eb55bbc3de2b1e877e29aa57d47077c05d
prefix=/work/providers/prefix
cmake -S /work/providers/holonight-config -B /work/providers/config-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build /work/providers/config-build --parallel 2
cmake --install /work/providers/config-build --prefix "$prefix"
cmake -S /work/providers/holonight-qt -B /work/providers/qt-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$prefix" \
  -DBUILD_TESTS=OFF -DBUILD_DEMO=OFF -DBUILD_CONTROLS_GALLERY=OFF -DBUILD_WAYLAND=ON
cmake --build /work/providers/qt-build --parallel 2
cmake --install /work/providers/qt-build --prefix "$prefix"
cmake -S /work/providers/holonight-system-services -B /work/providers/services-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
cmake --build /work/providers/services-build --parallel 2
cmake --install /work/providers/services-build --prefix "$prefix"
# Match existing acceptance: provider source trees must be unavailable to consumers.
python3 - <<'PY_CLEAN'
from pathlib import Path
import shutil
for name in ('holonight-config', 'holonight-qt', 'holonight-system-services'):
    shutil.rmtree(Path('/work/providers') / name)
PY_CLEAN
build=build/verification
trap 'status=$?; for name in Testing uqc-launch-logs; do if [ -d "$build/$name" ]; then cp -a "$build/$name" /output/; fi; done; exit "$status"' 0
scripts/check-architecture-boundaries.sh
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPython3_EXECUTABLE=/usr/bin/python3 -DBUILD_TESTS=ON -DCMAKE_PREFIX_PATH="$prefix"
cmake --build "$build" --parallel 2
export LD_LIBRARY_PATH="$prefix/lib"
if [ "$lane" = static-checks ]; then
  cmake --build "$build" --target format-check
  cmake --build "$build" --target tidy
  # Analyze the standalone installed-package consumer in its actual compiler context.
  stage=/work/static-install
  cmake --install "$build" --prefix "$stage"
  consumer=build/tidy-package-consumer
  cmake -S tests/shell-config-package-consumer -B "$consumer" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_PREFIX_PATH="$stage;$prefix"
  cmake --build "$consumer" --parallel 2
  python3 - "$consumer" <<'PY_DB'
from pathlib import Path
import json
import sys
build = Path(sys.argv[1])
entries = json.loads((build / 'compile_commands.json').read_text())
for entry in entries:
    entry['command'] = entry['command'].replace('-mno-direct-extern-access', '').replace('-Wno-template-id-cdtor', '')
(build / 'clang').mkdir()
(build / 'clang/compile_commands.json').write_text(json.dumps(entries))
PY_DB
  run-clang-tidy -quiet -j 2 -p "$consumer/clang" \
    -header-filter "^$PWD/(apps|libs|integrations|tests)/.*\\.(h|hpp)$" \
    "$PWD/tests/shell-config-package-consumer/main.cpp"
else
  scripts/check-qmltypes.sh "$build"
  bwrap --unshare-net --ro-bind / / -- true
  python3 scripts/run-isolated-test.py --hide-host ctest --test-dir "$build" --output-on-failure --no-tests=error
  cmake --build "$build" --target qml-lint
fi
