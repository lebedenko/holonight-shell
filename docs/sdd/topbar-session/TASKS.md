# SDD Tasks — topbar-session

- [x] T-001: Add bar-icons SVG resources to CMakeLists.txt
  - REQs: REQ-F-401, REQ-F-402
  - Check: `qt6_add_resources` block for four icons exists in CMakeLists.txt with PREFIX "/HolonightShell" and BASE assets.

- [x] T-002: Add SessionService.h and SessionService.cpp to CMakeLists.txt executable sources
  - REQs: REQ-F-001, REQ-F-002
  - Check: Both files are listed in `qt6_add_executable` sources.

- [x] T-003: Add SessionSection.qml and SessionPopup.qml to CMakeLists.txt with QT_RESOURCE_ALIAS
  - REQs: REQ-NF-601
  - Check: Both QML files have `set_source_files_properties` with `QT_RESOURCE_ALIAS` stripping `src/qml/`, and both are in `QML_FILES` list.

- [x] T-004: Create SessionService.h with QML singleton registration macros
  - REQs: REQ-F-001, REQ-F-002
  - Check: Header declares four Q_INVOKABLE methods (lockScreen, logout, reboot, shutdown) and includes QML_ELEMENT + QML_SINGLETON macros.

- [x] T-005: Create SessionService.cpp with method implementations
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-NF-001
  - Check: logout() calls `QProcess::startDetached("hyprctl", {"dispatch", "exit"})`, reboot() calls systemctl reboot, shutdown() calls systemctl poweroff, lockScreen() is a no-op comment, all use QProcess::startDetached.

- [x] T-006: Register SessionService singleton in main.cpp
  - REQs: REQ-F-001, REQ-F-002
  - Check: main.cpp instantiates SessionService, sets CppOwnership, and calls qmlRegisterSingletonType with lambda returning the instance.

- [x] T-007: Create SessionPopup.qml with popup structure and animation
  - REQs: REQ-F-201, REQ-F-202, REQ-F-203, REQ-F-501, REQ-F-502, REQ-NF-501, REQ-NF-202
  - Check: Rectangle with #1f2335 fill and #bb9af7 border; Column containing four Row delegates; opacity and y Behaviors with 160ms OutCubic easing; visible property gates hit-testing.

- [x] T-008: Create SessionSection.qml with Canvas power symbol and popup toggle
  - REQs: REQ-F-101, REQ-F-102, REQ-C-101, REQ-NF-101
  - Check: Canvas draws power symbol (arc + vertical line) using `context.arc` and `context.lineTo`; MouseArea toggles popupOpen boolean; symbol color is HoloniightPalette.onSurface at rest and primary when open/hovered.

- [x] T-009: Wire SessionSection into TopBar.qml after StatusSection
  - REQs: REQ-F-601
  - Check: SessionSection component appears in TopBar.qml RowLayout after StatusSection and does not overlap existing sections.

- [x] T-010: Verify project builds without errors
  - REQs: (all)
  - Check: `task build` completes successfully with no CMake, C++, or linker errors.

- [x] T-011: Verify QML linting passes
  - REQs: (all QML tasks)
  - Check: `task qml-lint` reports no errors in SessionSection.qml or SessionPopup.qml.
