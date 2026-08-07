# SDD Tasks — appearance-configuration-foundation

All tasks remain unchecked until umbrella ACF-006 is `Ready`. Implement against the exact published provider
revisions pinned by the accepted initiative.

- [ ] ACF6-01: Establish standalone `libs/holonight-shell-config`, package exports, tests, and external consumer smoke
  build.
- [ ] ACF6-02: Move retained Shell product schema/parser/writer/test helpers into the Shell-owned package with new
  public names.
- [ ] ACF6-03: Remove appearance/theme fields, missing-default flags, serialization, documentation, and fixtures from
  the Shell product contract.
- [ ] ACF6-04: Adopt the Shell-owned product package in `ConfigService` and remove its appearance API/signals.
- [ ] ACF6-05: Add `HoloNight::Config` and the pinned Qt projection dependency for canonical read-only appearance.
- [ ] ACF6-06: Refactor `AppearanceService` into the single validated appearance state/watcher with last-known-good
  rollback and precise signals.
- [ ] ACF6-07: Remove ThemeService configuration loading and make portal/palette reload consume in-memory appearance.
- [ ] ACF6-08: Update Shell QML, tests, fakes, CMake, documentation, and install/package paths for the canonical role
  names and separate configuration domains.
- [ ] ACF6-09: Add redacted security-boundary tests and record the linked credential-storage initiative without
  placing secrets in ACF artifacts.
- [ ] ACF6-10: Run standalone package, format, tidy, architecture, QML lint/types, full CTest, and manual Hyprland
  verification; publish the provider commit for Settings handoff.

## Completion evidence

Record exact commands, versions, automated results, manual observations, redaction checks, and the published commit
before requesting ACF-006 `Done`. A local or unpublished commit is not a handoff.
