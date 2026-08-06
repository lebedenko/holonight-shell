# SDD Tasks — topbar-logo-section

- [x] T-001: Define LogoConfig struct
  - REQs: REQ-F-008
  - Check: `LogoConfig` struct with fields `file` (QString), `generic` (bool, default false), and defaulted `operator==` compiles in `libs/holonight-config/include/holonight_config/config_structs.h`.

- [x] T-002: Implement parseLogo() function
  - REQs: REQ-F-009
  - Check: `parseLogo()` function in `libs/holonight-config/src/ConfigParsers.cpp` parses `[logo]` section with tilde-expansion for `file` field, wired into `parseConfigTable()`, and handles missing section gracefully.

- [x] T-003: Add ConfigService::logo() accessor
  - REQs: REQ-F-009
  - Check: `ConfigService` (`libs/holonight-core/src/ConfigService.h`/`.cpp`) exposes `[[nodiscard]] const LogoConfig& logo() const` method with corresponding member `logo_` populated in `applyParsedConfig()`.

- [x] T-004: Implement distro alias table and mapDistroIdToLogoName()
  - REQs: REQ-F-012, REQ-C-004
  - Check: Static `distroLogoAliasTable()` and `mapDistroIdToLogoName()` function in `libs/holonight-core/src/SystemInfo.cpp` map all Appendix C aliases correctly, including at least opensuse-leap→opensuse, pop→popos, and fedora-asahi-remix→asahilinux, returning empty string for unmapped IDs.

- [x] T-005: Declare and wire lcConfigParsers logging category
  - REQs: DESIGN §6 risk #5
  - Check: `lcConfigParsers` logging category is declared in a header reachable from `SystemInfoService.cpp` and defined in `libs/holonight-config/src/ConfigParsers.cpp`; `SystemInfoService.cpp` includes the declaration and can invoke `qCWarning(lcConfigParsers)`.

- [x] T-006: Update SystemInfoService constructor signature
  - REQs: REQ-F-010
  - Check: `SystemInfoService` constructor in `libs/holonight-services/src/SystemInfoService.h` signature changes to `explicit SystemInfoService(ConfigService* config = nullptr, QObject* parent = nullptr)` with member `ConfigService* config_{nullptr}` stored.

- [x] T-007: Implement applyLogoConfigOverride() and precedence chain
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-NF-001, REQ-NF-002, REQ-C-001, REQ-C-002, REQ-F-011
  - Check: `readOsRelease()` in `SystemInfoService.cpp` calls `applyLogoConfigOverride()` after parsing os-release; the 5-step precedence (file override → generic → distro alias → pixmaps → icon-theme) is enforced, invalid file paths log a warning via `lcConfigParsers` and fall through, and pixmaps/icon-theme code paths remain unchanged.

- [x] T-008: Add logoTinted Q_PROPERTY to SystemInfoService
  - REQs: DESIGN §2.6, REQ-F-006, REQ-F-007
  - Check: `SystemInfoService` declares `Q_PROPERTY(bool logoTinted READ logoTinted CONSTANT)` with private member `bool logo_tinted_{false}`, set to `true` for bundled/distro-mapped SVGs and `false` for file overrides and pixmaps/icon-theme paths.

- [x] T-009: Update ShellApplication to pass ConfigService to SystemInfoService
  - REQs: DESIGN §2.4
  - Check: `ShellApplication::ShellApplication()` in `apps/shell/app/ShellApplication.cpp` updates `new SystemInfoService(this)` call to `new SystemInfoService(config_service_, this)`, wiring the config dependency.

- [x] T-010: Bundle logo SVGs into QRC
  - REQs: REQ-F-013
  - Check: `apps/shell/CMakeLists.txt` adds `qt6_add_resources()` block for `assets/linux-logo/*.svg` with `PREFIX "/HolonightShell"` and `BASE` set to assets directory, producing `qrc:/HolonightShell/linux-logo/<basename>.svg` paths.

- [x] T-011: Replace Image with HnIcon in LogoSection.qml
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-C-003, DESIGN §6 risk #4
  - Check: `apps/shell/qml/Topbar/LogoSection.qml` replaces `Image { source: SystemInfoService.logoSource }` with `HnIcon { source: SystemInfoService.logoSource; tinted: SystemInfoService.logoTinted }` wrapped in qmllint disable/enable comments; layout, sizing, and label text remain unchanged.

- [x] T-012: Test distro alias table
  - REQs: REQ-F-012
  - Check: `tests/test_system_info.cpp` contains test cases for `mapDistroIdToLogoName()` covering all Appendix C rows plus sles/suse, an unmapped-ID case returning empty string, and existing `findSystemLogoPath()` tests still pass unchanged.

- [x] T-013: Test config parsing and LogoConfig struct
  - REQs: REQ-F-008, REQ-F-009
  - Check: `tests/test_config_service.cpp` contains test cases asserting `parseLogo()` handles missing section (default empty file, false generic), tilde-expansion on `file`, `generic = true`, and both set together; `LogoConfig::operator==` equality-comparison works.

- [x] T-014: Test SystemInfoService precedence chain
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-006, REQ-F-007, REQ-F-010, REQ-F-011, REQ-NF-001, REQ-NF-002
  - Check: `tests/test_system_info_service.cpp` verifies file override wins over generic and the host distro; generic wins when no valid file; invalid file paths fall through without crashing and emit a warning containing the invalid filename and "not readable"; mapped host distros are tinted while host fallback paths are untinted; and a null `ConfigService*` resolves a non-empty source without crashing. Pure alias-table coverage, including unmapped IDs, is provided by T-012. Pixmaps and icon-theme selection remain covered by the existing `SystemInfo` tests rather than by environment-specific service assertions.

- [x] T-015: QML smoke test for LogoSection.qml
  - REQs: DESIGN §7 QML smoke test section
  - Check: `tests/qml/tst_*.qml` (or existing QML test harness) contains test case for `LogoSection.qml` using mock `SystemInfoService` with `logoSource` and `logoTinted` properties faked; test asserts `HnIcon.tinted` binding reflects `SystemInfoService.logoTinted` correctly for bundled, file-override, and pixmaps sources.

- [x] T-016: Live manual verification in Hyprland session
  - REQs: SPEC.md Appendix B (tests 1-10)
  - Check: Record the date, environment, result, and any available screenshot paths for the applicable visual checks from Appendix B. Automated config-parsing and precedence checks should be referenced by their test names rather than claimed as visual verification.
  - Result: User verified the completed feature in the live environment on 2026-07-22. Automated config parsing and precedence coverage is recorded under T-012 through T-015.

- [x] T-017: End-of-implementation gate — build, test, lint, qmltypes
  - REQs: all
  - Check: Verified on 2026-07-22 with `task build`, `task test` (973 passed and one environment-dependent test skipped), `task qml-lint`, and `task qmltypes-check`. This automated gate does not complete the separate live-compositor task T-016.

- [x] T-018: User approval — mark SDD pipeline finished
  - REQs: SDD process
  - Check: Implementation, automated validation, and live behavior were reviewed and verified by the user on 2026-07-22; the topbar-logo-section SDD pipeline is complete.
