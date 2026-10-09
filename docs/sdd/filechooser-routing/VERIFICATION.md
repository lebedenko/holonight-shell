# FileChooser routing verification — 2026-10-09

Baseline: 25e1556bc5ffc06015a757d48e468f7c0d9c9ccb. Backend:
af1e9a4806f1b74aaffe392588c81a76d1275b2f, published and pinned in
umbrella ae5ea86 before implementation. Local assignment SDD was committed before
routing changes; the accepted provider contract is recorded in a4b28d5.

Added the FileChooser preference to HoloNight, Hyprland, Sway and labwc. Source
routing checks verify exact preference maps and unchanged Settings bus/interface.
No Shell product C++ or QML changed. GTK remains the configured fallback.

Providers: Config d6a392b41991f70a004d58f7694c7b6115cb7280, Qt
6c7ac33004702e166b8c152dcde918296be54286 and SystemServices
39472e6dcafc93acea218a234213c346be256586, all clean source checkouts.
Reused the Config artifact from the accepted Files dependency prefix. Rebuilt Qt
from its exact pinned revision after runtime verification rejected the reused style
plugin: generated Popup/ComboBox cache symbols were unresolved. A fresh Qt build
exports those symbols correctly. Rebuilt the full SystemServices provider
(Audio/Storage/Compositor enabled) at the same revision because the Files subset
lacked Audio. No provider sources or revisions changed. Toolchain: GCC 16.2.1,
Qt 6.12.0. Installed/copy-reused artifacts in build/filechooser-providers.

The initial /tmp prefix was hidden by the existing isolated test runner. Release
CMake also selected a user-local Python executable hidden by that runner; configured
Python3_EXECUTABLE=/usr/bin/python3 for the final run. Moved
the same artifacts into the exposed repository build directory and reconfigured;
that initial test failure is not acceptance evidence. The first sandboxed task
test also required escalation to bind its private D-Bus socket.

Focused checks passed:

- python3 tests/test_portal_routing.py: 2 tests.
- python3 tests/test_installation.py: 6 tests, relocated prefixes and wrappers.
- task format-check and REUSE.

Acceptance commands:

```sh
cmake -S . -B build/filechooser-routing-acceptance -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DENABLE_COVERAGE=OFF \
  -DPython3_EXECUTABLE=/usr/bin/python3 -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH="$PWD/build/filechooser-providers" \
  -DHoloNightSystemServices_DIR="$PWD/build/filechooser-providers/lib/cmake/HoloNightSystemServices" \
  -DHOLONIGHT_RUNTIME_QML_PATH="$PWD/build/filechooser-providers/lib/qt6/qml"
cmake --build build/filechooser-routing-acceptance -j 4
python3 scripts/run-isolated-test.py --hide-host \
  ctest --test-dir build/filechooser-routing-acceptance --output-on-failure
HOLONIGHT_DEPENDENCY_PREFIX="$PWD/build/filechooser-providers" JOBS=4 task test
cmake --build build/filechooser-routing-acceptance --target qml-lint
bash scripts/check-qmltypes.sh build/filechooser-routing-acceptance
DESTDIR=/tmp/filechooser-shell-stage cmake --install build/filechooser-routing-acceptance
HOLONIGHT_PORTAL_STAGE=/tmp/filechooser-shell-stage/usr python3 tests/test_portal_routing.py
```

Clean Release build and CTest passed: 1157 tests, four existing native-user-manager
skips. Staged installation, routing and QML metadata passed. The Debug task workflow
initially retained its preset QML import cache despite using the accepted provider
CMake package. Its launch-origin check correctly rejected loading controls from the
default build/deps prefix. Reconfigured HOLONIGHT_RUNTIME_QML_PATH to match the
accepted provider prefix and rebuilt; the corrected Debug suite passed all 1157
tests with the same four existing skips.
No production or test assertion was changed to accommodate that failure. Logs remain under
/tmp/filechooser-shell-*.log. QML lint exits successfully with existing advisories
in unchanged QML (including block-scope var declarations). Configuration emits
existing Qt private ABI and policy notices; no policy/source changes were made to
suppress them. C++ tidy was not repeated because no owned C++ translation unit
changed. Real-broker and manual application/compositor acceptance belongs to I-004;
no live installation or service restart is performed by this work package.
