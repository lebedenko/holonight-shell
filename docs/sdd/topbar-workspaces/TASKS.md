# SDD Tasks — topbar-workspaces

- [x] T-001: Update CMakeLists.txt to add Qt5Compat find_package and link library
  - REQs: REQ-C-003
  - Check: `find_package` includes `Qt5Compat` and `target_link_libraries` includes `Qt6::Qt5Compat` with no build errors.

- [x] T-002: Add WorkspaceModel and ExtWorkspaceManager C++ source files to CMakeLists.txt executable
  - REQs: REQ-C-001
  - Check: `qt6_add_executable` includes all four new C++ source paths (WorkspaceModel.h, WorkspaceModel.cpp, ExtWorkspaceManager.h, ExtWorkspaceManager.cpp) with no undefined reference errors.

- [x] T-003: Add Qt5Compat.GraphicalEffects glow protocol generation to CMakeLists.txt
  - REQs: REQ-F-001, REQ-C-001
  - Check: `qt6_generate_wayland_protocol_client_sources()` is declared for `protocols/ext-workspace-v1.xml` and generated headers compile without errors.

- [x] T-004: Add WorkspacePill and WorkspaceSection QML files to CMakeLists.txt with QT_RESOURCE_ALIAS
  - REQs: REQ-C-002
  - Check: Both `set_source_files_properties` entries set `QT_RESOURCE_ALIAS` to strip `src/qml/` prefix, and both files are listed in `qt6_add_qml_module QML_FILES`.

- [x] T-005: Create WorkspaceModel.h with WorkspaceState enum and QAbstractListModel interface
  - REQs: REQ-F-002, REQ-F-003, REQ-F-013
  - Check: Header defines `WorkspaceState` enum (Empty/Occupied/Active/Urgent) with `Q_ENUM`, declares `QAbstractListModel` subclass with `rowCount()`, `data()`, `roleNames()`, and `applyBatchUpdate()` method.

- [x] T-006: Create WorkspaceModel.cpp with role mapping and model implementation
  - REQs: REQ-F-002, REQ-F-003, REQ-F-015
  - Check: `roleNames()` maps `WorkspaceIdRole`, `WorkspaceNameRole`, `WorkspaceStateRole`, `WorkspaceOnMonitorRole` to "wsId", "wsName", "wsState", "wsOnMonitor"; `applyBatchUpdate()` calls `beginResetModel()` on first population and `dataChanged()` on subsequent updates.

- [x] T-007: Create ExtWorkspaceManager.h with QWaylandClientExtensionTemplate binding
  - REQs: REQ-F-001
  - Check: Header subclasses `QWaylandClientExtensionTemplate<ExtWorkspaceManager>`, declares `ExtWorkspaceManager(WorkspaceModel*)` constructor with version 1, and declares protocol event handler virtual methods.

- [x] T-008: Create ExtWorkspaceManager.cpp with protocol event handlers and state mapping
  - REQs: REQ-F-001, REQ-F-004, REQ-F-005
  - Check: Implementation overrides `workspace_manager_done()` to flush `staged_` into model, overrides `workspace_state()` to map protocol bits (active 0x1, urgent 0x2) to `WorkspaceState`, and implements `mapProtocolState()` returning Urgent > Active > Empty.

- [x] T-009: Register WorkspaceModel as QML singleton in main.cpp
  - REQs: REQ-F-013, REQ-F-015
  - Check: `main.cpp` constructs `WorkspaceModel*`, constructs `ExtWorkspaceManager*` passing model pointer, calls `qmlRegisterSingletonInstance()` with "HolonightShell" module and "WorkspaceModel" type name before `LayerShellManager` construction.

- [x] T-010: Create WorkspacePill.qml with state-driven styling and width animation
  - REQs: REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-F-010, REQ-F-011, REQ-F-014, REQ-NF-001
  - Check: Component has `required wsId` and `wsState` properties, height 34, animates width to 58 on Active else 42 with 180ms OutCubic, applies Glow only when Active/Urgent, and all colors come from `HoloniightPalette` with no hex values.

- [x] T-011: Create WorkspaceSection.qml with BarSection, six-pill Repeater, and overflow logic
  - REQs: REQ-F-004, REQ-F-005, REQ-F-012, REQ-F-014
  - Check: Component extends `BarSection`, has Row with 6px spacing, Repeater displays pills 1–6 calling `_stateForId()`, overflow pill visible only when `_overflowId > 0` (Active + id>6 + onMonitor), and `_stateForId()` scans model returning Empty if not found.

- [x] T-012: Insert WorkspaceSection into TopBar.qml between LogoSection and center spacer
  - REQs: REQ-F-012, REQ-C-004
  - Check: TopBar layout has WorkspaceSection with `Layout.alignment: Qt.AlignVCenter` positioned after LogoSection and before the `Item { Layout.fillWidth: true }` spacer; no other changes to TopBar.qml.

- [x] T-013: Run task build to verify all C++, CMake, and Wayland protocol generation compiles
  - REQs: REQ-C-001, REQ-C-003
  - Check: `task build` completes with exit code 0, all generated wayland-ext-workspace headers are in build/, and no undefined references or link errors.

- [x] T-014: Run task qml-lint to verify QML imports, HoloniightPalette tokens, and module URI
  - REQs: REQ-C-002, REQ-F-014
  - Check: `task qml-lint` runs with -I flags for Holonight and HolonightShell modules, reports no unresolved imports or undefined properties in WorkspacePill.qml and WorkspaceSection.qml.

- [x] T-015: Run holonight-shell on Hyprland and verify six workspace pills render with Empty state
  - REQs: REQ-F-004, REQ-F-006
  - Check: `task run` launches shell on Hyprland, TopBar appears, WorkspaceSection displays exactly six pills numbered 1–6, all pills show Empty state (42px wide, #161925 fill, #565f89 border, #565f89 text).

- [x] T-016: Switch workspaces on Hyprland and verify pill state transitions and width animations (re-verified: multi-engine singleton fix + MultiEffect glow)
  - REQs: REQ-F-008, REQ-F-011, REQ-NF-003
  - Check: Focusing workspace 1 changes pill 1 width to 58px with 180ms animation, border color to cyan, text to light, and glow visible; switching to workspace 2 shrinks pill 1 back to 42px while expanding pill 2; frame rate remains 60 FPS.

- [x] T-017: Create a window with urgency hint and verify urgent workspace pill glows
  - REQs: REQ-F-009, REQ-NF-003
  - Check: Running `notify-send` or `dbus-send` to trigger urgency on workspace 3 changes pill 3 to 42px wide, #f7768e border/text, and glow with radius 12 and spread 0.30; glow is static (no pulse).
  - Note: Hyprland v0.55.2 does not propagate urgency to ext-workspace-v1 urgent bit — runtime trigger is impossible. Verified via GTest: WorkspaceModelTest.StateForIdReturnsUrgent, UrgentAndActiveAreDistinct, UrgentStateCleared (tests/test_workspace_model.cpp).

- [x] T-018: Test overflow pill by creating workspace 7+ and activating it
  - REQs: REQ-F-005
  - Check: On Hyprland, create workspace 7 via `hyprctl dispatch workspace 7`, verify an additional overflow pill appears showing "7" with Active state (58px, cyan border, glow); switch back to workspace 1, verify overflow pill disappears.

- [x] T-019: Verify no overflow pill appears when active workspace > 6 is not assigned to monitor
  - REQs: REQ-F-005
  - Check: Create workspace 8 on a disconnected monitor (if available) or manually unassign; verify overflow pill does not appear even though workspace 8 is active; reconnect/reassign and verify overflow pill reappears.
  - Note: Hardware disconnect not available. Verified via GTest: WorkspaceModelTest.OverflowZeroWhenOffMonitor, OverflowReturnedWhenOnMonitor, OverflowZeroForInRangeActive, OverflowZeroWhenOutOfRangeNotActive (tests/test_workspace_model.cpp).

- [x] T-020: Verify workspace pills do not respond to mouse clicks
  - REQs: REQ-NF-001
  - Check: Click on any pill in the topbar; verify no shell command executes, no workspace switch occurs, and clicks pass through to components behind the pill (e.g., clicking behind pill 3 has no effect).
