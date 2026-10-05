# External window title presentation

Work package: I-002. Baseline: `ea947cd8c9e64ff653d2b38c73a83e4a8ba2d6c5`.

See the umbrella initiative for settled contracts and scope. Implementation remains local; publication and integration are pending.

## Requirements

Retain session/keyboard providers, plugin metadata, QML contributions and application models. Construct backends with shared factories. Keep special-workspace properties and invokable commands in the shell adapter.

## Implementation

Plugins select shared factories explicitly. Integration contract headers forward to provider contracts; special-workspace QML properties and invokables remain in HyprlandShellAdapter. Session providers, application models, keyboard parsing and its private socket transport remain shell-owned. Shared backend/parser/protocol tests moved to system-services; shell retains adapters, plugin loading, workspace and activation tests. Dependency bootstrap now builds SystemServices before Qt.

The labwc runtime smoke script accepts `HOLONIGHT_LABWC_PROTOCOL_PROBE` pointing to the provider-built `labwc_protocol_smoke`; its generated protocol implementation is no longer built by shell. The transferred protocol license text and install entry are removed from shell.

## Verification — 2026-10-05

- Clean acceptance build passed. Serial `task check` test phase passed all 1,128 tests; focused adapter/workspace/activation/plugin checks passed.
- Architecture, format, QML lint/import/type metadata checks passed. Focused adapter clang-tidy passed.
- Full lint stops on four pre-existing readability-math-missing-parentheses diagnostics in unchanged `StatusPopupGeometry.cpp`.
- A parallel isolated CTest invocation collided on shared D-Bus service names; the successful serial task test run is the acceptance evidence.
- Native ecosystem checks, publication and pin updates remain pending.
