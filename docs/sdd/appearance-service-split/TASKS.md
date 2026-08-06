# Appearance Service Split Tasks

- [x] Add `AppearanceService` with config-backed font properties and `debugOverlays`.
- [x] Slim `ThemeService` to theme config watching and `paletteReloadRequested()`.
- [x] Register both QML singletons from `ShellApplication`.
- [x] Add `AppearanceService` sources to CMake.
- [x] Move QML font and debug overlay reads to `AppearanceService`.
- [x] Split QML smoke fakes into `FakeAppearanceService` and reload-only `FakeThemeService`.
- [x] Update integration tests to target `AppearanceService` for config-driven appearance updates.
- [x] Run `task test`.
- [x] Run `task qml-lint`.
- [x] Run `task qmltypes-check`.
- [x] Run `task format-check`.
