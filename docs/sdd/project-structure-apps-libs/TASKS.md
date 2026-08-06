# Tasks

- [x] T-001: Capture a baseline build with `task build`.
- [x] T-002: Move shell-owned files to `apps/shell/`.
- [x] T-003: Move reusable C++ modules to `libs/holonight-*`.
- [x] T-004: Add local CMake target definitions for shell app and reusable libraries.
- [x] T-005: Update top-level CMake wiring, format, tidy, qml-lint, and coverage paths.
- [x] T-006: Update tests and fake QML module generation for `apps/shell/qml`.
- [x] T-007: Update architecture and qmltypes scripts for the new paths.
- [x] T-008: Update contributor-facing documentation.
- [x] T-009: Run final validation commands.
- [x] T-010: Run live compositor smoke checks in a Hyprland session.

## Validation Notes

- `task build` passed before and after the migration.
- `task test` passed all 737 tests after the final CMake and formatting changes.
- `task qml-lint` passed with existing `InhibitorModel` unresolved-type warnings.
- `task qmltypes-check` passed after adding explicit static-library moc metatype collection.
- `task architecture-check` passed.
- `task format-check` passed.
- `task tidy` was run and failed on pre-existing clang-tidy diagnostics unrelated to the path migration.
- Live Hyprland compositor smoke testing was completed successfully on 2026-06-29.
