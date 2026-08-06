# Phase 43 — Settings Application Cleanup: Tasks

- [x] T-282: Revalidate the seven remaining U-11 findings against current
  `main` and exclude U-11 I-C1, I-C4, and I-Q3 already completed earlier.
- [x] T-283: Make parsed-config loading emit only changed-value signals and
  clear dirty state through `recomputeDirty()`.
- [x] T-284: Remove the dead string-mode `SettingsEditModel::markSaved()`
  overload and update its test caller to the production appearance contract.
- [x] T-285: Cache the font-family enumeration within each `FontListModel` and
  filter the cached list on `fixedPitchOnly` changes.
- [x] T-286: Add regression coverage for parsed-config notifications, dirty
  restoration, and cached-list filter restoration.
- [x] T-287: Give settings navigation one page-state owner and bind both the
  navigation panel and content stack to it.
- [x] T-288: Enable bound component behavior for affected settings delegates,
  replace dynamic parent-role access, cache navigation colors, and mark all
  settings `Text` items as plain text.
- [x] T-289: Run focused settings tests, build the settings executable, and run
  `task qml-lint`, `task qmltypes-check`, `task architecture-check`, and
  `task test`; document any pre-existing failures precisely.
  - Result: eight focused settings tests pass; the settings executable builds;
    QML lint, QML type metadata, and architecture checks pass. The full suite
    runs 958 tests with only the established
    `KeyboardLayoutService.EventSocketConnectedTriggersDevicesQuery` failure;
    `SidebarManagerMonitorValidation.AcceptsAnyCurrentlyConnectedNonEmptyNamedScreen`
    remains skipped as expected.
- [x] T-290: Manually verify Appearance, Bar, and at least one placeholder page;
  confirm highlight/content synchronization plus edit, discard, and save flows.
  - Result: accepted by the user after confirming the settings application
    behaves correctly.
- [x] T-291: After acceptance, record implementation/documentation commits and
  reduce the queued Phase 7 Low-severity backlog from 15 to 8.
  - Result: implementation commit `98a9426` (`refactor: tighten settings
    application contracts`) completes U-11 I-C2/I-C3/I-C5 and
    I-Q1/I-Q2/I-Q5/I-Q6. This documentation closeout records the accepted
    seven-item reduction; eight Low-severity candidates remain queued.
