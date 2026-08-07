# SDD Tasks — appearance-configuration-foundation

Umbrella ACF-006 is `Ready`. Implementation started from Shell
`4fe75168632c43476ed862c70c63b95f63e4f292` against umbrella
`623129188f7deeebf943857f0c4b3691c92d63c0`, `holonight-config`
`81b01d3ae8433f3a4b017db2feb588a1ee62b714`, and `holonight-qt`
`6f591cbdfb8c8e887e87e43a1c4e0c48c1f5f39d`.

- [x] ACF6-01: Establish standalone `libs/holonight-shell-config`, package exports, tests, and external consumer smoke
  build.
- [x] ACF6-02: Move retained Shell product schema/parser/writer/test helpers into the Shell-owned package with new
  public names.
- [x] ACF6-03: Remove appearance/theme fields, missing-default flags, serialization, documentation, and fixtures from
  the Shell product contract.
- [x] ACF6-04: Adopt the Shell-owned product package in `ConfigService` and remove its appearance API/signals.
- [x] ACF6-05: Add `HoloNight::Config` and the pinned Qt projection dependency for canonical read-only appearance.
- [x] ACF6-06: Refactor `AppearanceService` into the single validated appearance state/watcher with last-known-good
  rollback and precise signals.
- [x] ACF6-07: Remove ThemeService configuration loading and make portal/palette reload consume in-memory appearance.
- [ ] ACF6-08: Update Shell QML, tests, fakes, CMake, documentation, and install/package paths for the canonical role
  names and separate configuration domains.
- [ ] ACF6-09: Add redacted security-boundary tests and record the linked credential-storage initiative without
  placing secrets in ACF artifacts.
- [ ] ACF6-10: Run standalone package, format, tidy, architecture, QML lint/types, full CTest, and manual Hyprland
  verification; publish the provider commit for Settings handoff.

## Completion evidence

Record exact commands, versions, automated results, manual observations, redaction checks, and the published commit
before requesting ACF-006 `Done`. A local or unpublished commit is not a handoff.

### 2026-08-07 — ACF6-01 through ACF6-03

- `task test:shell-config-package`: passed standalone configure/build/install and installed consumer configure/build/run.
- `cmake --build build --target test_holonight_shell_config -j32`: passed.
- `QT_QPA_PLATFORM=offscreen build/tests/test_holonight_shell_config`: 2/2 tests passed; legacy appearance/theme
  sections are inert and absent from canonical writes.
- `rg -n "AppearanceConfig|\\[appearance\\]|\\[theme\\]|ui_font|transparency|blur_strength"
  libs/holonight-shell-config --glob '!**/CMakeLists.txt'`: no matches.

### 2026-08-07 — ACF6-04 and ACF6-05

- `cmake --build build -j32`: passed for the complete Shell and test suite.
- `QT_QPA_PLATFORM=offscreen build/tests/test_holonight_core
  --gtest_filter='ConfigServiceTest.*:BackgroundConfigTest.*:LogoConfigTest.*'`: 48/48 tests passed.
- `QT_QPA_PLATFORM=offscreen build/tests/test_holonight_services
  --gtest_filter='AppearanceIntegrationTest.*'`: 3/3 tests passed for startup projection, narrow live notification,
  and invalid-replacement last-known-good retention.
- Shell no longer finds, stages, includes, or links the Settings-owned `HolonightConfig` package.

### 2026-08-07 — ACF6-06

- `AppearanceService` projects the complete resolved appearance contract, retains temporary compatibility aliases,
  and publishes precise property signals before one accepted revision notification.
- `QT_QPA_PLATFORM=offscreen build/tests/test_holonight_services
  --gtest_filter='AppearanceIntegrationTest.*'`: 4/4 tests passed for complete startup projection, precise signal and
  revision ordering, semantically unchanged replacement, and invalid-replacement last-known-good retention.
- `task format-check`: passed.
- `task architecture-check`: passed.
- `task qmltypes-check`: passed after a complete Shell build; QML type metadata and module packaging checks passed.

### 2026-08-07 — ACF6-07

- Removed the `theme.conf` path resolver, watcher, parser, and their obsolete tests. `ThemeService` is now a thin
  projection subscribed to `AppearanceService`.
- `SettingsPortalBackend` accepts only injected resolved appearance values. Portal color scheme and catalog-resolved
  accent changes are published before the related appearance revision; palette reload follows that revision.
- `QT_QPA_PLATFORM=offscreen build/tests/test_holonight_services
  --gtest_filter='AppearanceIntegrationTest.*:SettingsPortalBackendTest.*'`: 10/10 tests passed for in-memory portal
  projection, precise system-facing changes, notification ordering, revision-driven palette reload, and appearance
  watcher behavior.
- `rg -n "theme\\.conf|ThemeConfigPath|reloadFromThemeConfig" libs apps tests CMakeLists.txt Taskfile.yml`: no
  matches.
- `task format-check`, `task architecture-check`, and `task qmltypes-check`: passed; the latter completed a full Shell
  build and verified QML type metadata and module packaging.
