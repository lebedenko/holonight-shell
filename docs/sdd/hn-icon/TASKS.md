# SDD Tasks — hn-icon

- [x] T-001: Create `qml/HoloNight/CMakeLists.txt` module definition
  - REQs: REQ-NF-006, REQ-NF-007, REQ-C-002
  - Check: File exists at `qml/HoloNight/CMakeLists.txt` and defines `holonight_components` STATIC library and `holonight_componentsplugin` target with `URI Holonight.Components` and `QML_FILES HnIcon.qml`.

- [x] T-002: Add `add_subdirectory(qml/HoloNight)` to root `CMakeLists.txt` before app subdirs
  - REQs: REQ-NF-006
  - Check: Root `CMakeLists.txt` line 181 (before `add_subdirectory(apps/shell)`) contains `add_subdirectory(qml/HoloNight)`.

- [x] T-003: Extend `ALL_QML_FILES` glob and qml-lint `-I` paths in root `CMakeLists.txt`
  - REQs: REQ-NF-006
  - Check: Root `CMakeLists.txt` ALL_QML_FILES glob includes `qml/HoloNight/*.qml` and qml-lint target has `-I "${CMAKE_CURRENT_BINARY_DIR}/qml/HoloNight"` in its command.

- [x] T-004: Link `holonight_componentsplugin` into `holonight-shell` target
  - REQs: REQ-NF-008, REQ-F-001
  - Check: `apps/shell/CMakeLists.txt` target_link_libraries for `holonight-shell` includes `holonight_componentsplugin` in PRIVATE scope.

- [x] T-005: Link `holonight_componentsplugin` into `holonight-settings` target
  - REQs: REQ-NF-008, REQ-F-001
  - Check: `apps/settings/CMakeLists.txt` target_link_libraries for `holonight-settings` includes `holonight_componentsplugin` in PRIVATE scope.

- [x] T-006: Create `qml/HoloNight/HnIcon.qml` component
  - REQs: REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019, REQ-F-020, REQ-F-021, REQ-F-022, REQ-F-023, REQ-F-024, REQ-C-001, REQ-C-004, REQ-C-005, REQ-C-006, REQ-C-007, REQ-C-008, REQ-C-009, REQ-C-010, REQ-C-011, REQ-C-012
  - Check: File exists at `qml/HoloNight/HnIcon.qml`, contains `required property url source`, `property int size: 24`, `property int iconState: HnIcon.Normal`, `property bool tinted: true`, four color properties with HoloniightPalette defaults, enum IconState with Normal/Muted/Disabled/Active, Image with `visible: !root.tinted`, MultiEffect with `colorization: 1.0` and `visible: root.tinted`, `implicitWidth/Height: root.size`, no Qt5Compat imports, and handles empty/invalid sources silently.

- [x] T-007: Verify `task build` completes successfully with no new errors
  - REQs: REQ-NF-006, REQ-NF-004
  - Check: Running `task build` from project root exits with code 0 and CMake/build output contains no new compilation errors for HnIcon or module registration.

- [x] T-008: Write QML unit test `tests/qml/tst_HnIcon.qml`
  - REQs: REQ-NF-001, REQ-NF-002, REQ-NF-003, REQ-NF-005
  - Check: File exists at `tests/qml/tst_HnIcon.qml` and contains test cases for: default instantiation, all four states with correct resolvedColor, color overrides, cross-state isolation, tinted/untinted mode toggling, size changes, empty/invalid sources, out-of-range iconState, and implicit dimensions.

- [x] T-009: Verify `task qml-lint` passes for all QML files including `HnIcon.qml`
  - REQs: All functional and non-functional requirements
  - Check: Running `task qml-lint` from project root exits with code 0 and produces no errors for HnIcon.qml or any other QML file.
