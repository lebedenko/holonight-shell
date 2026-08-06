# Phase 31 — Portal D-Bus Adapter Naming: Tasks

**Status**: Complete — automated checks and user verification passed.

## Pre-flight

- [x] T-216: Revalidate the next unresolved Phase 7 candidate against current
  HEAD.
  - Check: confirm `NullPortalBackend.{h,cpp}` still contains only
    `IPortalDBus`, `SystemPortalDBus`, and `NullPortalDBus`, while
    `SettingsPortalBackend` remains the independent server-side portal
    implementation.
  - Result: confirmed. The adapter pair contains the injected broker-client
    interface plus production and test implementations, while
    `SettingsPortalBackend` is the separate session-bus service owned by
    `ThemeService`.

## Implementation and Tests

- [x] T-217: Rename the portal broker adapter file pair and update live source
  and test includes.
  - REQs: REQ-F-01
  - Files: `libs/holonight-services/src/portal/NullPortalBackend.{h,cpp}` to
    `PortalDbus.{h,cpp}`, `PortalService.cpp`, and focused tests.
  - Check: no live source or test include to `NullPortalBackend` remains;
    the CMake source glob discovers the renamed pair, and
    `SettingsPortalBackend` is unchanged.
  - Result: renamed the pair to `PortalDbus.{h,cpp}` and updated
    `PortalService.cpp` plus `test_portal_service.cpp`. The service library's
    existing recursive source glob discovers the new files without a CMake
    list edit.

- [x] T-218: Run focused portal-service regression coverage.
  - REQs: REQ-F-01
  - Files: `tests/test_portal_service.cpp`.
  - Check: the injected `NullPortalDBus` seam and `SystemPortalDBus` timeout
    coverage compile and pass without behavioral changes.
  - Result: the focused `SystemPortalDBusTest`, `PortalServiceTest`, and
    `SettingsPortalBackendTest` selection passed all 28 tests.

## Validation and Handoff

- [x] T-219: Run project validation.
  - Check: focused portal tests, `task test`, `task architecture-check`,
    `task format-check`, and `git diff --check`. Record unrelated existing
    failures separately.
  - Result: `task architecture-check` and `git diff --check` pass. The
    changed C++ files pass direct `clang-format --dry-run --Werror`. `task
    test` ran 943 tests: 942 passed, one environment-gated monitor-validation
    test was skipped, and the unrelated
    `KeyboardLayoutService.EventSocketConnectedTriggersDevicesQuery` failed
    reproducibly. `task format-check` reports only the four pre-existing
    formatting violations in `libs/holonight-core/src/HyprlandWorkspaceService.cpp`.

- [x] T-220: Record acceptance and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the queued Low backlog
    only after acceptance evidence is recorded.
  - Result: user verified the rename preserves portal behavior. `3c76ca4`
    (`refactor: clarify portal D-Bus adapter`) implements U-05 I-05; the
    other 38 Low-severity candidates remain queued.
