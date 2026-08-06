# SDD Tasks — topbar-window-title

- [x] T-001: Add ActiveWindowService C++ sources and ActiveWindowSection QML file to CMakeLists.txt
  - REQs: REQ-C-001, REQ-C-002, REQ-C-005
  - Check: `qt6_add_executable` includes `ActiveWindowService.h/.cpp`; `set_source_files_properties` sets `QT_RESOURCE_ALIAS` stripping `src/qml/`; `ActiveWindowSection.qml` is listed in `QML_FILES`.

- [x] T-002: Create ActiveWindowService.h interface
  - REQs: SPEC-ActiveWindowService-Singleton, SPEC-Title-Property, SPEC-AppClass-Property
  - Check: Header compiles and exposes `title` and `appClass` as readable Q_PROPERTY with NOTIFY signals.

- [x] T-003: Implement ActiveWindowService.cpp IPC socket and event loop integration
  - REQs: SPEC-Hyprland-IPC-Socket, SPEC-QLocalSocket-QSocketNotifier, SPEC-LineBuffer-Parsing
  - Check: Socket connects to `/tmp/hypr/$HYPRLAND_INSTANCE_SIGNATURE/.socket2.sock` without crashing; QSocketNotifier processes data on Qt event loop.

- [x] T-004: Implement activewindow event parsing in ActiveWindowService.cpp
  - REQs: SPEC-Event-Format-Parse, SPEC-Comma-Split-FirstOnly
  - Check: `activewindow>>appClass,title` lines parse correctly with appClass and title separated by first comma only.

- [x] T-005: Add missing-HYPRLAND_INSTANCE_SIGNATURE guard and diagnostics in ActiveWindowService.cpp
  - REQs: SPEC-No-Op-Guard, SPEC-Diagnostic-Log
  - Check: Service logs diagnostic message and does not crash when `HYPRLAND_INSTANCE_SIGNATURE` is unset.

- [x] T-006: Register ActiveWindowService singleton in main.cpp
  - REQs: SPEC-QmlRegisterSingletonInstance, SPEC-ActiveWindowService-Singleton
  - Check: `qmlRegisterSingletonInstance<ActiveWindowService>()` call in main.cpp does not produce compile errors.

- [x] T-007: Create ActiveWindowSection.qml component extending BarSection
  - REQs: SPEC-ActiveWindowSection-QML, SPEC-Visibility-Condition, SPEC-FillWidth-Layout
  - Check: Component extends BarSection, sets `visible: ActiveWindowService.title !== ""`, and `Layout.fillWidth: true`.

- [x] T-008: Add label and title text to ActiveWindowSection.qml
  - REQs: SPEC-Label-Text, SPEC-Title-Text-Styling, SPEC-Color-Tokens-Only, SPEC-ElideRight
  - Check: QML renders "// ACTIVE WINDOW" label (14px Inter, HoloniightPalette.textMuted) and title text (20px Inter, HoloniightPalette.textPrimary, elide: Text.ElideRight) without hardcoded hex colors.

- [x] T-009: Replace center spacer in TopBar.qml with ActiveWindowSection
  - REQs: SPEC-TopBar-Integration, SPEC-Alignment-VCenter
  - Check: `Item { Layout.fillWidth: true }` in TopBar.qml center is replaced with `ActiveWindowSection { Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter }`.

- [x] T-010: Build project and verify no compilation errors
  - REQs: SPEC-ActiveWindowService-Singleton, SPEC-ActiveWindowSection-QML, SPEC-TopBar-Integration
  - Check: `task build` completes successfully with zero errors.

- [x] T-011: Run qmllint and verify no QML style violations
  - REQs: SPEC-ActiveWindowSection-QML, SPEC-Label-Text, SPEC-Title-Text-Styling
  - Check: `task qml-lint` reports no errors or warnings for ActiveWindowSection.qml and TopBar.qml.

- [x] T-012: Manual test — verify section visibility on window focus change
  - REQs: SPEC-Visibility-Condition, SPEC-Title-Property
  - Check: Run in live Wayland session; ActiveWindowSection appears when window is focused and disappears when no window has focus.

- [x] T-013: Manual test — verify Unicode and special characters in window titles render correctly
  - REQs: SPEC-Title-Property, SPEC-Title-Text-Styling
  - Check: Open windows with Unicode titles (e.g., Chinese, emoji, accented characters) and observe correct rendering in topbar without corruption.
  - Note: Verified with "テスト 🎉 Привет" (CJK + emoji + Cyrillic) — rendered correctly.

- [x] T-014: Manual test — verify graceful no-op behavior on non-Hyprland session
  - REQs: SPEC-No-Op-Guard, SPEC-Diagnostic-Log
  - Check: Run on X11 or non-Hyprland Wayland session; application starts without crashing, socket connection silently fails with diagnostic log message.
  - Note: Warning logged to systemd journal (Qt default on Linux): "ActiveWindowService: HYPRLAND_INSTANCE_SIGNATURE not set, disabling". No crash.

- [x] T-015 (discovered): Fix startup backfill — query active window via command socket on init
  - Note: Added queryActiveWindow() using .socket.sock + j/activewindow JSON query. Title populates immediately on startup without requiring a focus change.

- [x] T-016 (discovered): Fix layout collapse when no active window — move visible binding to Column
  - Note: Moving visible:false from BarSection root to Column keeps the fillWidth spacer in the RowLayout. Logo+pills stay left, clock stays right.

- [x] T-017 (discovered): Fix WorkspaceSection implicitWidth — add pillRow id and implicitWidth override
  - Note: WorkspaceSection inherited BarSection's implicitWidth=16 (container.implicitWidth=0). RowLayout gave it 16px; pills overflowed unclipped into ActiveWindowSection space. Fixed by overriding implicitWidth: pillRow.implicitWidth + 16. Also set ActiveWindowSection implicitWidth: 0 to break circular dependency.

- [x] T-018 (discovered): Fix socket path — use XDG_RUNTIME_DIR instead of /tmp/hypr
  - Note: Hyprland stores sockets in $XDG_RUNTIME_DIR/hypr/$SIG/ not /tmp/hypr/$SIG/.
