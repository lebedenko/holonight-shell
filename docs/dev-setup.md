# Development Setup

This project is developed against a Linux Wayland/Hyprland environment with C++23, Qt 6, CMake, Ninja, and Task.

## Qualified Arch CI environment

CI runs in the full Arch image pinned in `scripts/ci/images.json`; the Ubuntu host only launches containers.
The qualified image supplies Qt ≥6.11 (public/private versions must match), installed GTest ≥1.14 with
GMock, Qt QuickTest, Wayland/scanner/protocols, libsecret, tomlplusplus, polkit/polkit-qt6, PulseAudio,
CMake/Ninja, Task, compiler, clang-format/tidy, bubblewrap, D-Bus and Sway.
Debian package mappings remain reference data; Debian is not a qualified release gate.

Run `task deps PRESET=test` explicitly before local configure/build/test. Provider source overrides accept
paths with spaces. Internal providers use `HOLONIGHT_DEPENDENCY_PREFIX`; external packages use
`HOLONIGHT_EXTERNAL_PREFIX` or normal system discovery. Missing prerequisites fail without downloading.
Run `task ci:prepare` before `task ci` for offline acceptance and Release staged installation.
See `tooling/README.md` for provider identities, separate prefixes and incompatible cache archival.

## Configure Presets

`CMakePresets.json` mirrors the main local workflows:

```bash
cmake --preset debug
cmake --build --preset debug

cmake --preset debug-tests
cmake --build --preset debug-tests
ctest --preset debug-tests

cmake --preset coverage
cmake --build --preset coverage
```

`task` remains the preferred day-to-day interface, while presets give CI and contributors a reproducible CMake entry point.

## QML Type Metadata

`holonight-shell` exposes QML singletons from several static-library targets: `holonight_core`, `holonight_services`, and
`holonight_surfaces`. Qt's regular `qt6_add_qml_module()` path only sees metatypes attached to the executable target, so the
build extracts metatypes from those internal targets, combines them with `scripts/combine-metatypes.cmake`, assigns the
combined file to the executable with `_qt_internal_assign_build_metatypes_files_and_properties()`, and then runs
`_qt_internal_qml_type_registration()`.

Those `_qt_internal_*` calls are private Qt CMake APIs. They are used here to keep the project split into small CMake
targets while still producing one `HolonightShell` QML module. A Qt upgrade can break this path in a way that still
configures and builds but leaves `build/apps/shell/HolonightShell/holonight-shell.qmltypes` nearly empty, often just `Module {}`. That
breaks qmllint and QML tooling visibility for singletons such as `AudioService`, `NotificationService`, and
`WorkspaceModel`.

Run this after CMake/QML registration changes:

```bash
task qmltypes-check
```

CI runs the same check after building the executable.

## Runtime Notes

`task run` requires a live Wayland compositor, and the shell is currently designed for Hyprland. Unit tests run offscreen and do not require a compositor session.
