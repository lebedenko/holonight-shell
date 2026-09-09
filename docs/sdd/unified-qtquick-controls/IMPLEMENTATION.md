# UQC-102 implementation and local acceptance

Date: 2026-09-09. Local implementation and canonical CI acceptance complete.
[Requirements](SPEC.md), [design](DESIGN.md), [ordered ledger](TASKS.md).

## Baselines and ownership

Implementation starts at shell design revision
`5324b47fb01501d4fa3e90e7865b23065efce970`, coordinated by umbrella
`e3b85b5a339207ee421cb70f53311b1120f6b2c6`. Provider remains
`00e6e208b6c9b30d89b66ef3aeb4ef8175050764` and configuration remains
`fe69a59e6b73167fd5349223a4d265d75386c139`; their canonical main revisions
were rechecked. No public provider API or configuration schema was added.
Local system-services uses the umbrella pin
`60685b9d9f050bfba42b60191a498bd861744f37`; CI/release preserve their existing
`4556dc9b22237823387110340347cd1301ed9245` dependency. Builds and installs stay
in shell-owned `build-dependencies/{config,qt,system-services,prefix}`.

## Final implementation

- `resources/qtquickcontrols2.conf`, `cmake/QuickControlsRuntime.cmake`, and private
  `libs/holonight-core/src/QuickControlsRuntime.{h,cpp}` supply embedded defaults,
  exact-build-executable discovery and executable-relative installed discovery.
  The helper is compiled into each graphical executable separately; authentication
  does not acquire the complete shell core dependency. A direct Config ELF dependency
  and executable-relative RPATH allow provider plugins to load with host libraries hidden.
- `CMakeLists.txt`, `apps/{shell,authentication}/CMakeLists.txt`, all three graphical
  entry points, `tests/main.cpp`, `tests/test_qml_harness.cpp`, and `tests/CMakeLists.txt`
  wire matching production/test defaults. Qt retains environment, external config and
  CLI precedence. Askpass validates extra positional arguments after Qt consumes options.
- `LauncherRightPanelBrowse.qml` and authentication `AuthenticationDialog.qml`,
  `AuthenticationPrompt.qml`, `MessageList.qml`, `IdentitySelector.qml` use qualified
  Controls. IdentitySelector uses the public HnIconComboBox hooks and preserves its
  avatar delegate, 82-unit row height, identity roles and input behavior. Equivalent
  point sizes avoid setting both font pixelSize and pointSize under the selected style.
- `cmake/ShellQml.cmake` shares the existing production resource registration between
  shell and compiled acceptance; no production QML is copied into a test substitute.
  `tests/uqc_qml/tst_CompiledControls.qml` exercises those resources, while the source
  harness remains independent. `tests/FakeQmlServices.h` supplies external service
  boundaries; a deterministic test image provider replaces desktop icon lookup.
- `PerMonitorLayerManager.cpp` and authentication entry points report created UI and
  implementation contexts only with `holonight.controls.runtime.debug=true` enabled.
  Selection, searched module roots and loaded implementations are distinct evidence.
  Fusion Label evidence correlates a created QQuickLabel's declaration context with
  that context's resolved Controls.Label URL. Provider plugins alone are insufficient.
  `SessionIntegrationService.cpp` labels its existing status as configured selection.
  Askpass diagnostics use stderr and leave protocol stdout untouched.
- Existing session/peer wrappers and activation paths already carry explicit style;
  their production routing and allowlists need no changes. Expanded wrapper tests
  verify Fusion propagation and rejection of peer-provided discovery paths.
- `scripts/check-qml-import-policy.py` and `tests/test_qml_import_policy.py` enforce
  runtime/Core boundaries across apps, compatibility/authentication QML and tests;
  17 independent fixtures cover permitted and forbidden patterns.
  `tests/qml/tst_LauncherActionRow.qml` adds its missing file-local Core import.
- `scripts/run-isolated-test.py`, `tests/test_quick_controls_launch.py`, Taskfile,
  CI/release workflows and `scripts/install-ci-dependencies.sh` provide private
  configuration, buses without activation, hidden host provider discovery and
  headless Sway launch acceptance. CI uses a dedicated non-root test account inside
  a privileged ephemeral container so bubblewrap can create namespaces. The disposable
  Ubuntu runner disables its AppArmor unprivileged-userns restriction for this job
  and runs a bubblewrap network-namespace preflight. This changes no developer host
  setting. Arch CI installs bubblewrap/Sway and Polkit development dependencies before
  building; the Debian release job installs the corresponding development packages.
- `README.md` publishes canonical import/default guidance. Local ignored AGENTS.md
  and CLAUDE.md advice was corrected, but these files are not repository-tracked and
  are not force-added. A narrowly documented Qt-parenting analyzer suppression in
  `AccountProfileResolver.cpp` addresses an existing ownership false positive.

## Acceptance and commands

All runtime checks use disposable HOME/XDG directories, private D-Bus without
activation directories, unavailable system bus/audio endpoints and offscreen QML.
Host HoloNight QML/native libraries are hidden by bubblewrap. The shell executable
runs only against a private headless Sway socket; no live desktop or authentication
service is involved. All child processes are bounded, terminated and reaped.

Run from the shell repository (`task` is `/usr/bin/go-task` on this workstation):

```sh
task configure-tests NPROC=6
cmake --build build --parallel 6
python3 scripts/run-isolated-test.py --hide-host ctest --test-dir build --output-on-failure -R '^uqc_(compiled|authentication)'
python3 scripts/run-isolated-test.py --hide-host /usr/bin/go-task test NPROC=6
cmake --build build --target tidy
cmake --build build --target format-check qml-lint
scripts/check-qmltypes.sh build
scripts/check-architecture-boundaries.sh
python3 scripts/check-qml-import-policy.py
python3 tests/test_qml_import_policy.py
reuse lint
git diff --check
```

Focused acceptance passed 60/60 checks before broad verification. Expanded geometry
acceptance passed 8/8 process entries, covering both styles, DPR 1/1.25 and popup
scales 0.78/1/1.25. Window resizing is rendered before account-popup measurements;
Wi-Fi popup placement uses the production centering anchors.

The launch runner passed 45 selector/entry-point combinations plus three independent
missing-module cases. Five modes (default, environment Fusion, CLI Fusion, conflicting
environment plus CLI, external configuration) cover shell, askpass and polkit from
build outputs and relocated staged installation, plus all three installed bin askpass
aliases. Assertions prove created control origins and native provider plugin paths;
installed evidence rejects the exact shell build directory, build dependency and
source-QML paths. The final tightened launch assertion was rerun successfully
through `ctest -R '^uqc_launch$'` in 124.29 seconds. Missing style,
Core and Controls cases fail with actionable diagnostics and empty askpass stdout.
CTest entry `uqc_launch` retains logs in `build/uqc-launch-logs`; CI uploads them.

Local toolchain: Qt/QtTest 6.11.2, GCC 16.2.1. Final `task test` passed **1157/1157**
CTest entries in 290.45 seconds, including `uqc_launch` in 123.99 seconds. Formatting,
QML lint, all three metadata/packaging checks, architecture checks, import scan and
17 policy fixtures pass. REUSE passes for 1220 files. Full clang-tidy found only four
style issues in the new test harness; those were corrected and both compiled/source
harness variants pass `clang-tidy --quiet -p build/tidy tests/test_qml_harness.cpp`.
Remote static analysis and licensing pass in
[CI run 34346937932](https://github.com/lebedenko/holonight-shell/actions/runs/34346937932).
That run's tests stopped before execution because the Ubuntu host denied bubblewrap's
network setup. Earlier runs exposed stale-image package omissions; the workflow
now installs the required Polkit development packages and uses Arch's package manager.
The corrected published revision `b4da0ff6d973a7020dd4a02e1377b3a10816d769` passed
all required gates in [CI run 34351602771](https://github.com/lebedenko/holonight-shell/actions/runs/34351602771):
build/test (22m51s), static checks including full clang-tidy (34m54s), and licensing.
This handoff adds the locally rerun exact-build-path assertion and the completion
record. Its own remote CI must also pass before the coordinator updates the gitlink;
the umbrella handoff records that final documentation revision and CI result.

The auxiliary [CI image run 34344665413](https://github.com/lebedenko/holonight-shell/actions/runs/34344665413)
built its image successfully but GHCR denied publication with `permission_denied:
write_package`. Registry permissions require owner maintenance; required build/test,
static and licensing verification uses the existing image with explicit job-local
package installation. No registry credentials or permissions were modified.
The Debian release package names were checked against the distribution's
[Polkit agent development package](https://packages.debian.org/trixie/libpolkit-agent-1-dev)
and [Qt 6 development package](https://packages.debian.org/trixie/libpolkit-qt6-1-dev).
No release was created or deployed.

## Limits and reserved integration work

QML lint returns success with existing unresolved AudioService inherited-metadata
warnings. The provider does not export its AudioController metatype here; this is
outside the Controls migration. Compiled sidebar tab replacement exposes an existing
Loader height binding warning in unchanged SidebarContent.qml under both styles.
The compiled suite permits that exact warning and fails other runtime QML warnings;
this does not constitute a clean-warning claim for all existing shell surfaces.

Live authentication, human-operated Hyprland/Sway, ecosystem activation, session/power
operations and final visual acceptance remain UQC-201. The initiative stays Accepted;
local completion does not mark it Integrated. Package-manager mockups are untouched.
