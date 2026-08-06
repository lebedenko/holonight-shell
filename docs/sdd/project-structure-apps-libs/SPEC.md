# Project Structure: Apps and Libs

## Goal

Physically move the project out of the root `src/` layout into an `apps/` and `libs/` structure while preserving current runtime behavior, binary names, QML module names, QRC paths, CMake target names, and test entry points.

## Requirements

- `holonight-shell` shall keep the executable name and continue to build at `build/holonight-shell`.
- `holonight-settings` shall keep the executable and QML module names already used by the settings app.
- Shell-owned code shall live under `apps/shell/`, including `main.cpp`, app wiring, and the `HolonightShell` QML module.
- Reusable shell libraries shall live under `libs/holonight-platform/`, `libs/holonight-core/`, `libs/holonight-services/`, and `libs/holonight-surfaces/`.
- Existing CMake target names shall remain stable: `holonight_platform`, `holonight_core`, `holonight_services`, `holonight_surfaces`, `holonight_app`, `holonight_config`, `holonight-shell`, and `holonight-settings`.
- QML imports shall remain stable: `HolonightShell` and `HolonightSettings`.
- Shell QML resource aliases shall remain stable under `qrc:/HolonightShell/`.
- `assets/`, `protocols/`, `tests/`, `docs/`, and `data/` shall remain at the repository root.
- Generated Wayland files shall not be edited manually.

## Non-Goals

- Extracting a shared `qml/HoloNight` module is out of scope.
- Renaming CMake targets is out of scope.
- Changing runtime service behavior is out of scope.

## Acceptance

- The root `src/` directory is absent.
- `task build`, `task test`, `task qml-lint`, `task qmltypes-check`, `task architecture-check`, and `task format-check` pass.
- Manual compositor smoke testing remains required for Wayland-shell behavior.
