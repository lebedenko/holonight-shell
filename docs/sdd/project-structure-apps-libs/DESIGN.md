# Design

## Layout

The repository uses application and library ownership boundaries:

- `apps/shell/`: shell executable entry point, shell-local app wiring target, and `HolonightShell` QML files.
- `apps/settings/`: settings executable and `HolonightSettings` QML files.
- `libs/holonight-config/`: reusable config structs, parsers, and writer.
- `libs/holonight-platform/`: low-level Wayland, Hyprland IPC, and D-Bus helpers.
- `libs/holonight-core/`: shared models, configuration service, logging, workspace, keyboard, and state types.
- `libs/holonight-services/`: service adapters and QML-facing service models.
- `libs/holonight-surfaces/`: layer-shell surfaces, tray plumbing, and presentation orchestration.

## CMake

The top-level `CMakeLists.txt` keeps global Qt/package discovery, helper interface targets, Wayland scanner fallback setup, tests, and shared tooling targets. Each app or library owns its target definition in a local `CMakeLists.txt`.

Target names stay unchanged to reduce churn in tests, CI, and developer muscle memory.

`apps/shell/CMakeLists.txt` owns:

- `holonight_app`
- `holonight-shell`
- `HolonightShell` QML registration
- shell QRC resource bundles
- metatype combining for QML tooling

## QML

Shell QML moves from `src/qml/` to `apps/shell/qml/`, but aliases are still computed relative to the QML root. For example:

`apps/shell/qml/Topbar/TopBar.qml` remains available as `qrc:/HolonightShell/Topbar/TopBar.qml`.

No shared QML module is introduced in this migration.

## Tooling

The architecture boundary script now scans `libs/holonight-surfaces/src/` against service headers in `libs/holonight-services/src/`. QML tests generate source-file `qmldir` entries for `apps/shell/qml/...`. The qmltypes check reads the generated shell module metadata from the shell app build directory.
