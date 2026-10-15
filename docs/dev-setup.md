# Development Setup

This project is developed against a Linux Wayland/Hyprland environment with C++23, Qt 6, CMake, Ninja, and Task.

See [DEVELOPMENT.md](../DEVELOPMENT.md) for native CMake commands, installed provider
prefixes, the pinned shared image, and the `check` / `check:integration` acceptance paths.
There are two presets: `dev` (with tests) and `release`. Coverage remains available via
`task coverage`. Container acceptance supplies an isolated filesystem and network;
the Shell test wrapper also creates private configuration and a bus without activation.

## QML Type Metadata

`holonight-shell` exposes QML singletons from several static-library targets: `holonight_core`, `holonight_services`, and
`holonight_surfaces`. Qt's regular `qt6_add_qml_module()` path only sees metatypes attached to the executable target, so the
build extracts metatypes from those internal targets, combines them with `scripts/combine-metatypes.cmake`, assigns the
combined file to the executable with `_qt_internal_assign_build_metatypes_files_and_properties()`, and then runs
`_qt_internal_qml_type_registration()`.

Those `_qt_internal_*` calls are private Qt CMake APIs. They are used here to keep the project split into small CMake
targets while still producing one `HolonightShell` QML module. A Qt upgrade can break this path in a way that still
configures and builds but leaves `build/dev/apps/shell/HolonightShell/holonight-shell.qmltypes` nearly empty, often just `Module {}`. That
breaks qmllint and QML tooling visibility for singletons such as `AudioService`, `NotificationService`, and
`WorkspaceModel`.

Run this after CMake/QML registration changes:

```bash
task qmltypes-check
```

CI runs the same check after building the executable.

## Runtime Notes

`task run` requires a live Wayland compositor, and the shell is currently designed for Hyprland. Unit tests run offscreen and do not require a compositor session.
