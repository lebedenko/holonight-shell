# Decoration-independent Viewer

Work package: I-004. Exact upstream baseline: `e490ff73f3da4b3a671aaf0496c8dbdc94a53ff8` (origin/main).

## Requirements and design

Compositor adapter and plugin ABI 3. Follow the [umbrella contract](../../../../docs/initiatives/decoration-independent-viewer/README.md). No decoration settings or desktop heuristics. Preserve unrelated behavior.

## Implementation and verification

Removed the Hyprland adapter's detection-only refresh forwarding. Advanced the private IntegrationPlugin IID to org.holonight.Integration/3.0 and aligned all four plugin catalog templates. A real previous-IID plugin fixture and previous-IID catalog are rejected by metadata discovery without loading either; current plugins and fallback behavior remain supported. Shell's unrelated WindowPresentation/window command APIs are preserved.

## Local provider handoff

Single consistent stage: build/decoration-provider. Config baseline d6a392b41991f70a004d58f7694c7b6115cb7280; Qt 6c7ac33004702e166b8c152dcde918296be54286; SystemServices 39472e6dcafc93acea218a234213c346be256586. Qt 6.12.0, Debug, Wayland enabled. Unchanged Config artifacts were reused after reviewing their provider-revision record. This stage prevents Config's include directory from shadowing the changed compositor headers with a different provider revision.

## Verification — 2026-10-08

- Clean CMake/Ninja Debug build in build/decoration-independent, BUILD_TESTS=ON, then affected rebuilds after correcting local provider/cache configuration. Explicit CMAKE_PREFIX_PATH=build/decoration-provider and Python3_EXECUTABLE=/usr/bin/python3; cached HOLONIGHT_RUNTIME_QML_PATH was cleared so build launch discovery uses that same stage.
- Full isolated suite via `python3 scripts/run-isolated-test.py -- ctest --test-dir build/decoration-independent --output-on-failure`: 1156 checks, only uqc_launch initially failed because an old /tmp QML import cache path was hidden by isolation. Four outer native-systemd tests were skipped; the dedicated test_application_launch_private_bus passed with their required private-bus environment.
- After the local cache correction and rebuilding shell/askpass/polkit/authentication-test targets, `ctest --test-dir build/decoration-independent -R '^uqc_launch$' --output-on-failure`: passed (124.01 seconds). Together with the prior passing checks this covers the full suite. This tests build/installed relocation and styles in private headless Sway/bubblewrap sessions without using the desktop pointer or focus.
- Plugin discovery/rejection, current plugin relocation/loading, compositor regressions, architecture boundaries, QML import/lint/type/package metadata, focused changed-test clang-tidy/format, REUSE and final diff review passed. QML lint retains unrelated baseline warnings; build logs contain the existing private-Qt/protocol policy notices.
- Earlier mixed-prefix and live-bus runs were diagnostic only, superseded by the consistent-prefix isolated run. No CI result is claimed.
- Logs: /tmp/decoration-shell-final-{build,tests,qml,qmltypes,architecture}.log, /tmp/decoration-shell-runtime-build.log, /tmp/decoration-shell-package-final.log and build/decoration-independent/uqc-launch-logs/.

Existing user edits in SettingsNavigationService.cpp/.h, DesktopMenu.qml, FakeQmlServices.h, tst_DesktopMenu.qml and test_settings_navigation_service.cpp were preserved and excluded from this task's commit. Tests also compile those pre-existing edits in the shared working tree. Publication, deployment and pin updates remain separate.

## Publication preparation

CI provider fetches now match the exact provider revisions used for local acceptance above. Shell syntax and CI launcher regression checks passed before publication.
