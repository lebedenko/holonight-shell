# SDD Tasks — hnseparator-migration

## Consumer File Migrations (7 tasks)

- [x] T-001: Migrate Launcher.qml (2 instances)
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-005B, REQ-F-007
  - Check: Both instances (~L326, ~L357) replaced with HnSeparator; direction→orientation (V/H); no type→fadeMode: FadeBoth added (implicit Center default); color removed; size→height/width; opacity: 0.5 preserved.

- [x] T-002: Migrate LauncherRightPanelBrowse.qml (1 instance)
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-005B, REQ-F-007
  - Check: Instance ~L90 replaced with HnSeparator; direction: Horizontal→orientation: Qt.Horizontal; type: End→fadeMode: HnSeparator.FadeEnd; color removed; size→width: parent.width; opacity: 0.5 preserved.

- [x] T-003: Migrate NetworkPopupContent.qml (2 instances)
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004A, REQ-F-005A, REQ-F-006
  - Check: Both instances (~L26, ~L134) replaced with HnSeparator; direction removed; type: None→fadeMode: HnSeparator.Solid; color removed; size removed (Layout attached properties intact); thickness: root.sectionSeparatorThickness preserved; centerOpacity preserved; Layout.fillWidth, Layout.minimumHeight, Layout.preferredHeight intact.

- [x] T-004: Migrate WeatherCurrentSection.qml (1 instance)
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-005B, REQ-F-007
  - Check: Instance ~L96 replaced with HnSeparator; direction: Horizontal→orientation: Qt.Horizontal; type: End→fadeMode: HnSeparator.FadeEnd; color removed; size→width: parent.width; opacity: 0.5 preserved.

- [x] T-005: Migrate WeatherDailyCards.qml (1 instance)
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-005B, REQ-F-007
  - Check: Instance ~L61 replaced with HnSeparator; direction: Vertical→orientation: Qt.Vertical; no type→fadeMode: HnSeparator.FadeBoth added; color removed; size→height: parent.height; opacity: 0.5 and visible: dayCard.index > 0 binding preserved.

- [x] T-006: Migrate WeatherPopupContent.qml (8 instances)
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-005B, REQ-F-007
  - Check: All 8 instances (L86, L100, L117, L145, L232, L283, L331, L358) replaced with HnSeparator; direction→orientation applied to all; type→fadeMode mapping applied (L232/L283 explicitly End→FadeEnd; L86/100/117/145/331→FadeBoth; L358→FadeBoth with opacity: 0.32 unchanged); color removed from all; size→width/height applied correctly per orientation; opacity preserved exactly per instance (0.5 for most, 0.32 for L358).

- [x] T-007: Migrate WorkspaceSection.qml (1 instance inside Component)
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-005B, REQ-F-007
  - Check: Instance ~L121 inside Component { id: separatorComponent } replaced with HnSeparator; objectName: "workspaceSpecialSeparator" preserved; direction: Vertical→orientation: Qt.Vertical; type: Center→fadeMode: HnSeparator.FadeBoth; color removed; size: 48→height: 48 (literal, not binding); opacity: 0, Component.onCompleted: opacity = 1, and Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } } all preserved verbatim.

## Import Fixups (1 task)

- [x] T-008: Update imports across all 7 consumer files
  - REQs: REQ-F-008
  - Check: Launcher.qml: remove "import Holonight.Components" (Controls already present). LauncherRightPanelBrowse.qml: keep "import Holonight.Components" (ExternalIcon still used), verify Holonight.Controls present. NetworkPopupContent.qml, WeatherCurrentSection.qml, WeatherDailyCards.qml, WeatherPopupContent.qml, WorkspaceSection.qml: each swaps "import Holonight.Components"→"import Holonight.Controls". All 7 files have Holonight.Controls present after this task.

## Pre-Deletion Verification (1 task)

- [x] T-009: Verify zero ContentSeparator references in consumer code post-migration
  - REQs: REQ-F-013
  - Check: grep -r "ContentSeparator" apps/shell/qml/ tests/qml/ --include="*.qml" (excluding the component's own definition at qml/HoloNight/Components/ContentSeparator.qml and its test at tests/qml/tst_ContentSeparator.qml) returns zero matches.

## Source File & CMake Removal (1 task)

- [x] T-010: Delete ContentSeparator.qml source file and update CMakeLists.txt registration
  - REQs: REQ-F-009, REQ-F-010
  - Check: File qml/HoloNight/Components/ContentSeparator.qml deleted; CMakeLists.txt at qml/HoloNight/ has the line "Components/ContentSeparator.qml" removed from HOLONIGHT_COMPONENT_QML_FILES list; Components/ExternalIcon.qml and module definition remain; task build succeeds without CMake errors.

## Unit Test File Deletion (1 task)

- [x] T-011: Delete ContentSeparator unit test file tst_ContentSeparator.qml
  - REQs: REQ-F-011
  - Check: File tests/qml/tst_ContentSeparator.qml deleted; task test runs without errors related to missing tst_ContentSeparator.qml.

## QML Smoke Test Update (1 task)

- [x] T-012: Update test_qml_smoke.cpp to remove ContentSeparator qmldir entry
  - REQs: REQ-F-012
  - Check: tests/test_qml_smoke.cpp no longer contains "ContentSeparator 1.0 %1ContentSeparator.qml\n" line in componentsQmldir() function; module Holonight.Components line remains; ExternalIcon registration remains; task test passes.

## Post-Deletion Verification (1 task)

- [x] T-013: Full grep verification for complete ContentSeparator removal from live codebase
  - REQs: REQ-F-013
  - Check: grep -r "ContentSeparator" apps/ qml/ tests/ --include="*.qml" --include="*.cpp" --include="*.h" --include="CMakeLists.txt" --exclude-dir="docs" returns zero matches; docs/sdd/ historical references explicitly excluded.

## Build & Quality Gates (3 tasks)

- [x] T-014: Build gate — task build completes successfully
  - REQs: REQ-NF-004
  - Check: task build completes with zero compiler errors or warnings; holonight-shell binary generated in build/.

- [x] T-015: QML lint gate — task qml-lint passes without errors
  - REQs: REQ-NF-003
  - Check: task qml-lint completes with zero errors on all 7 migrated files; HnSeparator type resolved correctly from Holonight.Controls module.

- [x] T-016: Test gate — task test passes with 100% pass rate
  - REQs: REQ-NF-005, REQ-NF-001
  - Check: task test (including task configure-tests prerequisite) completes with all C++ and QML tests passing; no regressions in existing consumer tests for Launcher, Weather popup, Network popup, WorkspaceSection.

## Sequencing Note

Execute tasks in order: T-001 through T-007 (migrations first), T-008 (imports), T-009 (pre-deletion verification), T-010 through T-012 (deletions), T-013 (post-deletion verification), T-014 through T-016 (build/lint/test gates).
This order mirrors DESIGN.md Known Risk #5 sequencing: migrate all call sites → verify zero remaining references → delete component + CMake entry → delete test file + smoke-test qmldir line → full grep verification → build/test/lint gates.
