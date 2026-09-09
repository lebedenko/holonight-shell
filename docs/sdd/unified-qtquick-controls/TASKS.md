# UQC-102 work and verification ledger

Design checkpoint: 2026-09-09. Only D0 is complete in this iteration. All product
changes and product verification below remain pending; no local acceptance claimed.
See [SPEC.md](SPEC.md) and [DESIGN.md](DESIGN.md) for contracts and file inventory.

| ID | State | Dependency | Work and exact principal files |
|---|---|---|---|
| D0 | Done | Assigned published baseline | Inspect code/provider contracts; write SPEC.md, DESIGN.md and TASKS.md; review links, ordering, whitespace and documentation-only scope |
| D1 | Planned | Published shell SDD and linked umbrella checkpoint | Prepare exact provider/config dependencies privately; Taskfile.yml, .github/workflows/ci.yml, inspect release.yml; preserve existing system-services pin |
| D2 | Planned | D1 | Embed resources and process-local discovery; resources/qtquickcontrols2.conf, cmake/QuickControlsRuntime.cmake, libs/holonight-core/src/QuickControlsRuntime.h/.cpp, libs/holonight-core/CMakeLists.txt, CMakeLists.txt, apps/shell/CMakeLists.txt, apps/shell/main.cpp, apps/authentication/CMakeLists.txt, apps/authentication/askpass/main.cpp, apps/authentication/polkit/main.cpp, tests/CMakeLists.txt, tests/main.cpp, tests/test_qml_harness.cpp |
| D3 | Planned | D2 | Migrate six QML files identified in DESIGN inventory; preserve public IdentitySelector geometry and Core/composites; correct CLAUDE.md, AGENTS.md and README.md |
| D4 | Planned | D3 | Wire private selection/discovery diagnostics; verify scripts/holonight-session, scripts/holonight-polkit-agent-session, scripts/holonight-wayland-session-environment, cmake/InstallIntegration.cmake.in and data/systemd/user/holonight-polkit-agent@.service.in; edit only where propagation/discovery tests demonstrate a need |
| D5 | Planned | D4 | Add policy, selector/discovery and compiled acceptance tests; run focused then broad checks below, resolve failures and record evidence |
| D6 | Planned | D5 | Publish verified shell implementation and local acceptance record, require green remote CI, confirm canonical commit, hand off to umbrella coordinator |

## Test implementation inventory

Extend existing `tests/test_askpass_process.cpp`, `tests/test_polkit_agent_process.cpp`,
`tests/test_authentication_core.cpp`, `tests/test_polkit_bridge_integration.cpp`,
`tests/test_authentication_install.sh`, `tests/test_polkit_session_wrapper.sh`,
`tests/test_wayland_session_environment.py`, `tests/test_session_scripts.sh`,
`tests/test_qml_smoke.cpp`, `tests/qml/tst_AuthenticationDialog.qml`,
`tests/qml/tst_WifiPasswordDialog.qml`, `tests/qml/tst_LauncherSharedControls.qml`,
`tests/qml/tst_RightSidebarSharedControls.qml`, `tests/qml/tst_AudioSharedControls.qml`.
Reuse `tests/FakeQmlServices.h` and fake authentication sessions. The polkit process
harness already provides a private registration authority; actual requests must
never reach a live authority/helper. Askpass's fd-3 automation stays test-only.

Planned additions: `tests/test_quick_controls_runtime.cpp`,
`tests/test_qml_import_policy.py`, `scripts/check-qml-import-policy.py`,
`tests/test_quick_controls_launch.py`, `tests/test_quick_controls_compiled.cpp`,
`tests/qml/tst_QuickControlsAcceptance.qml`. Register them in tests/CMakeLists.txt.
Compiled acceptance links actual shell/authentication QML resources with fake
services; keep source-QML tests as independent regression coverage. Extend existing
CMake production resource registration for reuse rather than copying QML.
The launch runner owns disposable prefixes/configuration, private services,
filtered host discovery, timeouts, trace assertions and child termination/reaping.
It must accept build and installed executable paths, including every askpass alias.

The existing source harness uses a temporary HolonightShell qmldir and fixed /tmp
provider environment in CTest. Replace only provider discovery assumptions; do not
mistake its source paths for installed acceptance. Installed tests prohibit all
source/build dependency paths and verify native plugin origin as well as QML origin.
Provide missing-style and missing-Core/composite cases independently: Fusion cannot
mask an unavailable required composite dependency.

## Future verification commands

Run from the shell root after implementing and registering the proposed targets.
New UQC test names/runner interface below are requirements for D5, not existing
commands or executed results. Use a disposable configuration and isolated services
inside each runtime runner; never use `task run` for this acceptance.

```sh
# Private dependency preparation: task configure-tests must consume the exact
# provider/config revisions from SPEC; record private prefix and commands in handoff.
task configure-tests
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -R 'Authentication|Askpass|Polkit|test_(authentication_install|polkit_session_wrapper|wayland_session_environment|session_scripts)'
python3 scripts/check-qml-import-policy.py
python3 tests/test_qml_import_policy.py
ctest --test-dir build --output-on-failure -R '^uqc_(runtime|compiled|launch|missing_module|policy)'
# CTest entries above must create separate processes for every selector/style/DPR.
# The launch entry stages via cmake --install into its private prefix (never /usr),
# then runs build and installed matrices with host provider discovery hidden.
task test
task qml-lint
task qmltypes-check
task format-check
task architecture-check
git diff --check
```

Run focused cases first, fix failures, then full verification. `task architecture-check`
is applicable to the private core helper and resource/target linkage changes.
Run applicable static analysis in CI and locally for changed C++; record commands
and any existing toolchain limitations. Require remote build/test/static and
licensing workflows to pass for the published revision. Record final exact commands,
counts, Qt versions, origin evidence, limitations and date in a future local
IMPLEMENTATION.md before the verified handoff. A green build without control origin
or post-load observation is insufficient. UQC-201 owns live authentication,
compositor/manual interaction and actual ecosystem activation acceptance.

## Documentation checkpoint verification

2026-09-09: shell was clean at its assigned baseline; provider and configuration
matched authoritative pins. Canonical origin/main was rechecked for shell, provider,
configuration and umbrella before publication. Reviewed the QML inventory, public
HnIconComboBox hooks, all three graphical entry points, test entry points, aliases,
verified session wrappers and activation export paths. Local Markdown links and
referenced existing inventory files were checked; whitespace and commit scope were
checked. Product tests are intentionally not required for this documentation-only
checkpoint and have not been run. Implementation verification remains pending.

Publish the three shell documents first and confirm the commit on canonical origin.
Only then link the SDD in the umbrella, set UQC-102 In Progress and pin that published
documentation revision. Publish the umbrella checkpoint and stop. Initiative stays
Accepted; UQC-201 stays Planned. Preserve unrelated package-manager mockups.
