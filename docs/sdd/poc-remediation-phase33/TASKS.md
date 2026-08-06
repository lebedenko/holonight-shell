# Phase 33 — Portal Color-Scheme Protocol Constants: Tasks

**Status**: Complete — automated checks and user verification passed.

## Pre-flight

- [x] T-226: Revalidate U-05 I-08 against current HEAD.
  - Check: confirm `SettingsPortalBackend` still represents the freedesktop
    portal dark and light values with bare `1` and `2` literals, and determine
    whether `PortalService` has a second literal-based mapping.
  - Result: confirmed. `SettingsPortalBackend::Values::color_scheme` defaults
    to `1`, while `colorSchemeForThemeConfig()` returns bare `2 : 1` in both
    the configured-scheme and mode-fallback paths. `PortalService` only stores
    the value it receives and has no second literal mapping.

## Implementation and Tests

- [x] T-227: Define and use named Settings portal dark/light protocol values.
  - REQs: REQ-F-01
  - Files: `libs/holonight-services/src/portal/SettingsPortalBackend.{h,cpp}`
    as appropriate.
  - Check: the default and every theme mapping branch use the named values;
    D-Bus serialization remains an unsigned integer with dark=`1` and
    light=`2`.
  - Result: added private `kColorSchemeDark` and `kColorSchemeLight` constants
    to `SettingsPortalBackend`. The `Values` default and both scheme/mode
    mapping branches now use them; `variantForColorScheme()` retains the
    existing unsigned D-Bus serialization.

- [x] T-228: Add or refine focused Settings portal backend regression coverage.
  - REQs: REQ-F-01
  - Files: `tests/test_portal_service.cpp` as appropriate.
  - Check: focused tests explicitly retain both dark and light published
    values, including a mode fallback where practical.
  - Result: added
    `SettingsPortalBackendTest.ReadUsesLightModeFallbackWhenSchemeIsAbsent`.
    Focused `SettingsPortalBackendTest.*` coverage passes all 5 tests and
    retains dark=`1`, scheme-derived light=`2`, and mode-fallback light=`2`.

## Validation and Handoff

- [x] T-229: Run project validation.
  - Check: focused portal tests, `task test`, `task architecture-check`,
    `task format-check`, and `git diff --check`; record unrelated existing
    failures separately.
  - Result: focused backend coverage passes 5/5; `task architecture-check`,
    `git diff --check`, and direct changed-file
    `clang-format --dry-run --Werror` pass. `task test` ran 943 registered
    tests: 916 passed, one environment-gated monitor-validation test was
    skipped, and 26 unrelated socket/D-Bus/network integration tests failed,
    including the pre-existing keyboard-layout failure. The refreshed CTest
    registry contains 944 tests including the new case. `task format-check`
    reports only the four pre-existing formatting violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp`.

- [x] T-230: Record user acceptance and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the queued Low backlog
    only after acceptance evidence is recorded.
  - Result: user verified the Settings portal continues to publish the expected
    color-scheme behavior. `8e3ad79` (`refactor: name portal color scheme
    values`) implements U-05 I-08; the other 36 Low-severity candidates remain
    queued.
