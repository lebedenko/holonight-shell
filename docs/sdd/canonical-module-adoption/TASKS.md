# Canonical HoloNight Module Adoption Tasks

**Project:** holonight-shell
**Date:** 2026-07-27
**Status:** Completed
**Requirements:** `docs/sdd/canonical-module-adoption/SPEC.md`
**Design:** `docs/sdd/canonical-module-adoption/DESIGN.md`
**Upstream baseline:** `holonight-qt` commit `25939dd`

## Implementation Tasks

- [x] **T-001 — Reconfirm the affected-file inventory**
  - Requirements: REQ-F-001, REQ-F-012, REQ-NF-001, REQ-NF-005
  - Inspect application QML, test QML, C++-constructed QML, CMake metadata, import paths, and
    runtime loaders against the migrated Core and Controls type lists.
  - Record the final set of files that need Core, Controls, retained style, or no HoloNight import.
  - Check: the implementation-time inventory agrees with the design or any difference is
    documented before editing; no pre-existing user change is overwritten.

- [x] **T-002 — Add a deterministic QML import-ownership guard**
  - Requirements: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-009, REQ-F-012,
    REQ-NF-003
  - Add a focused repository test or script that scans application QML using explicit Core,
    Controls, and style-owned type lists.
  - Require `Holonight.Core` for migrated Core names and `Holonight.Controls` for
    `HnSurfaceFrame`/`HnApplicationWindow`; forbid lowercase canonical imports.
  - Permit `import Holonight` only for a directly used style-owned type or a documented
    compatibility exception.
  - Check: the guard reports the exact offending file and ownership rule, fails against a
    representative invalid fixture or pre-migration state, and passes after migration.

- [x] **T-003 — Move focused Core tests to the canonical module**
  - Requirements: REQ-F-002, REQ-F-005, REQ-F-006, REQ-F-009, REQ-NF-002, REQ-NF-003
  - Update or add focused QML coverage that imports `Holonight.Core` directly.
  - Exercise `HoloniightPalette`, `HolonightTheme`, `HnAppearance`, representative shape/enums,
    `HnIconProvider`, and `HnIcon`, including existing observable binding and state behavior.
  - Check: focused headless tests resolve these APIs without relying on `import Holonight`, and
    existing icon, theme, palette, appearance, and shape assertions still pass.

- [x] **T-004 — Move focused Controls tests to the canonical module**
  - Requirements: REQ-F-003, REQ-F-006, REQ-F-009, REQ-NF-002, REQ-NF-003
  - Update or add focused QML coverage that imports `Holonight.Controls` and instantiates
    `HnSurfaceFrame`.
  - Preserve assertions for content ownership, surface role, fill, border, corner style, and
    resolved shape.
  - Check: the test loads headlessly through the installed canonical plugin and no compatibility
    import owns `HnSurfaceFrame`.

- [x] **T-005 — Remove conflicting legacy singleton registrations from the test harness**
  - Requirements: REQ-F-005, REQ-F-009, REQ-NF-001, REQ-NF-003
  - Stop `FakeQmlServices` from registering fake palette/theme instances under `Holonight` when
    canonical tests should load the real upstream Core singletons.
  - Retain shell-domain service mocks and the existing temporary XDG configuration isolation.
  - Remove fake-only code only if it becomes unused as a direct result of this task.
  - Check: canonical Core loads exactly one singleton instance per type, no duplicate-registration
    diagnostic appears, and shell component tests retain deterministic service behavior.

- [x] **T-006 — Migrate shell application imports by ownership**
  - Requirements: REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-006, REQ-F-012, REQ-NF-001,
    REQ-NF-002
  - Replace compatibility imports with `Holonight.Core` in shell QML that consumes Core-owned
    types.
  - Add `Holonight.Controls` to the four known `HnSurfaceFrame` consumers and any additional
    implementation-time discoveries.
  - Retain `Holonight` where shell QML directly instantiates a style-owned standard control;
    preserve `Holonight.Components` and existing aliases.
  - Remove unused `Holonight` imports only after checking implicit type ownership.
  - Check: component bodies and constants are unchanged, the ownership guard passes for
    `apps/shell/qml/`, and representative shell components instantiate.

- [x] **T-007 — Migrate Settings application imports by ownership**
  - Requirements: REQ-F-002, REQ-F-004, REQ-F-006, REQ-F-012, REQ-NF-001, REQ-NF-002
  - Add `Holonight.Core` wherever Settings consumes migrated types.
  - Retain `Holonight` in files that directly use styled controls, including the known
    `AppearancePage.qml`, `BarPage.qml`, and `FooterBar.qml` consumers.
  - Check: the ownership guard passes for `apps/settings/qml/`, important Settings pages
    instantiate, and no visual or behavioral code changes accompany the imports.

- [x] **T-008 — Migrate remaining QML tests and runtime-created sources**
  - Requirements: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-009, REQ-NF-003
  - Classify every remaining test QML import using the same ownership rules as application QML.
  - Update dynamically constructed QML source if the final inventory discovers any canonical type
    use.
  - Retain the style module in tests that intentionally validate standard styled controls.
  - Check: all affected QML tests use their canonical owner, no accidental compatibility
    dependency remains, and the focused test selection passes offscreen.

- [x] **T-009 — Verify dependency discovery and add only evidence-based build changes**
  - Requirements: REQ-F-007, REQ-F-008, REQ-F-010, REQ-NF-001, REQ-NF-002, REQ-NF-004
  - Configure and build against a `holonight-qt` prefix containing commit `25939dd` or later.
  - Confirm canonical plugins are dynamically discoverable through existing import paths.
  - Add `HolonightQt::Core` or `HolonightQt::Controls` target usage only if configure/build
    evidence shows a consumer needs their transitive requirements.
  - Preserve package names, target names, executable names, install destinations, and
    `QT_SKIP_AUTO_QML_PLUGIN_INCLUSION`.
  - Check: configure/build succeeds and any CMake edit has a demonstrated functional purpose.

- [x] **T-010 — Add dependency-prefix and installed-context verification**
  - Requirements: REQ-F-008, REQ-F-010, REQ-NF-002, REQ-NF-003
  - Add or refine a prefix-aware check for Core and Controls `qmldir`, qmltypes, plugin, and owned
    QML artifacts.
  - Exercise direct canonical imports in a clean installed/runtime context that does not rely on
    shell source-tree paths.
  - Make missing artifacts fail with an actionable path and module diagnostic.
  - Check: both artifact presence and actual canonical module loading are proven for the
    configured dependency prefix.

- [x] **T-011 — Run focused and repository-wide headless verification**
  - Requirements: REQ-F-005, REQ-F-006, REQ-F-008, REQ-F-009, REQ-F-010, REQ-NF-003,
    REQ-NF-004
  - Run the import guard, focused canonical Core/Controls tests, affected component tests,
    `task qml-lint`, and `task qmltypes-check` when QML/CMake registration is touched.
  - Run `task test` after the narrow checks pass.
  - Check: all commands pass, or the completion report separates exact pre-existing failures from
    regressions and includes reproducible command output.

- [x] **T-012 — Validate live launches at scale 1.0 and 1.25**
  - Requirements: REQ-F-006, REQ-F-011, REQ-NF-002
  - In an available Wayland/Hyprland session, launch HoloNight Shell and HoloNight Settings at
    `QT_SCALE_FACTOR=1.0` and `QT_SCALE_FACTOR=1.25`.
  - Capture diagnostics through graceful termination and inspect for import, binding, singleton,
    and component-creation errors.
  - Use the compositor smoke checklist for the affected shell surfaces; this is a regression
    check, not a visual redesign.
  - Check: all supported launches are clean, or the completion report states the exact
    environmental blocker and closest completed verification.

- [x] **T-013 — Audit retained compatibility imports and finalize the pipeline record**
  - Requirements: REQ-F-012, REQ-NF-001, REQ-NF-005
  - Re-run the complete import inventory and list every retained `Holonight` import with its
    style-owned type or explicit compatibility rationale.
  - Confirm there are no lowercase imports, unrelated edits, generated Wayland changes, secrets,
    lockfile edits, or upstream compatibility removals.
  - Run `git diff --check` and review the final diff.
  - Check: the SDD status and task checkboxes reflect actual results, changed files and
    verification are summarized, and no commit is created.

## Requirement Traceability

| Requirement | Tasks |
| --- | --- |
| REQ-F-001 | T-001, T-002, T-008 |
| REQ-F-002 | T-002, T-003, T-006, T-007, T-008 |
| REQ-F-003 | T-002, T-004, T-006, T-008 |
| REQ-F-004 | T-002, T-006, T-007, T-008 |
| REQ-F-005 | T-003, T-005, T-011 |
| REQ-F-006 | T-003, T-004, T-006, T-007, T-011, T-012 |
| REQ-F-007 | T-009 |
| REQ-F-008 | T-009, T-010, T-011 |
| REQ-F-009 | T-002, T-003, T-004, T-005, T-008, T-011 |
| REQ-F-010 | T-010, T-011 |
| REQ-F-011 | T-012 |
| REQ-F-012 | T-001, T-002, T-006, T-007, T-013 |
| REQ-NF-001 | T-001, T-005, T-006, T-007, T-009, T-013 |
| REQ-NF-002 | T-003, T-004, T-006, T-007, T-009, T-010, T-012 |
| REQ-NF-003 | T-002, T-003, T-004, T-005, T-008, T-010, T-011 |
| REQ-NF-004 | T-009, T-011 |
| REQ-NF-005 | T-001, T-013 |

## Pipeline Status

- Requirements: approved
- Design: approved
- Task breakdown: approved
- Implementation: completed
- Pipeline closure: approved on 2026-07-28

## Implementation Record

- 115 application/test QML files import `Holonight.Core`.
- 6 application/test QML files import `Holonight.Controls`.
- No lowercase `holonight.core` or `holonight.controls` import remains.
- The only retained application `import Holonight` uses are:
  - `apps/settings/qml/AppearancePage.qml` for `ComboBox`, `Slider`, and `Switch`;
  - `apps/settings/qml/BarPage.qml` for `Slider`;
  - `apps/settings/qml/FooterBar.qml` for `Button`;
  - `apps/shell/qml/Launcher/LauncherRightPanelBrowse.qml` for `ScrollView`.
- No CMake target linkage change was needed: direct runtime loading from the installed dependency
  prefix succeeded for both canonical modules.
- Focused smoke tests, 216 QtQuickTest cases, QML lint, qmltypes/package checks, and all 1,070
  CTest entries passed. One existing environment-dependent CTest case was skipped.
- Shell and Settings remained running during five-second live Wayland smoke launches at scale
  factors 1.0 and 1.25. Settings logged no diagnostics; Shell logged only existing portal-backend
  and notification-daemon ownership conflicts, with no QML load or singleton errors.
