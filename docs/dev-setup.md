# Development Setup

This project is developed against a Linux Wayland/Hyprland environment with C++23, Qt 6, CMake, Ninja, and Task.

## Ubuntu 24.04 CI Baseline

GitHub Actions uses `ubuntu-24.04` and installs these packages:

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  clang-format \
  clang-tidy \
  cmake \
  g++ \
  libgl1-mesa-dev \
  libgtest-dev \
  libpulse-dev \
  libtomlplusplus-dev \
  libwayland-bin \
  libwayland-dev \
  ninja-build \
  pkg-config \
  wayland-protocols
```

CI installs Qt 6.8.3 with `jurplel/install-qt-action`, including the `qtwayland` module, because the project uses
`Qt6::GuiPrivate` and `QtQuick.Effects`.

Install [Task](https://taskfile.dev) separately for the local workflow documented in `README.md`.

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
