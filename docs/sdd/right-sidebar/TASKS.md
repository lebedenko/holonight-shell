# SDD Tasks — right-sidebar

- [x] T-001: Add sidebar constants to ShellConstants.h
  - REQs: REQ-F-008, REQ-F-010, REQ-F-011
  - Check: `ShellConstants.h` contains `kSidebarTabBarWidth`, `kSidebarRightMargin`, `kSidebarBottomMargin`, `kSidebarTopMargin`, `kSidebarMaxContentWidth`.

- [x] T-002: Create SidebarManager.h header
  - REQs: REQ-F-001, REQ-F-026, REQ-F-028
  - Check: `src/surfaces/SidebarManager.h` declares `SidebarManager` with `toggle()`, `close()`, `closeAll()`, `isOpen()` methods and required virtual overrides from `PerMonitorLayerManager`; `ShellApplication.cpp` registers the instance as the `HolonightShell.SidebarManager` QML singleton.

- [x] T-003: Implement SidebarManager.cpp
  - REQs: REQ-F-001, REQ-F-005, REQ-F-026, REQ-F-027
  - Check: `src/surfaces/SidebarManager.cpp` implements `layerConfig()`, `configureSurface()`, `qmlSource()`, and mutual exclusion logic in `toggle()`.

- [x] T-004: Wire SidebarManager into ShellApplication
  - REQs: REQ-F-001
  - Check: `ShellApplication.h` declares `std::unique_ptr<SidebarManager> sidebar_manager_` after `layer_shell_`, and `ShellApplication.cpp` instantiates it in `startLayerSurfaces()`.

- [x] T-005: Create RightSidebar.qml root component
  - REQs: REQ-F-013, REQ-F-014, REQ-F-015, REQ-F-016
  - Check: `src/qml/RightSidebar/RightSidebar.qml` declares `barMonitorName` and `active` required properties, contains `SequentialAnimation` with three phases (160ms grow, 200ms slide, 120ms fade), and calls `SidebarManager.onClosingAnimationFinished()` at the end of the close animation.

- [x] T-006: Create SidebarTabBar.qml component
  - REQs: REQ-F-008, REQ-F-018, REQ-F-021
  - Check: `src/qml/RightSidebar/SidebarTabBar.qml` is exactly 64px wide, renders six `SidebarTabButton` instances, and emits `tabSelected(int index)` signal.

- [x] T-007: Create SidebarTabButton.qml with Canvas icons
  - REQs: REQ-F-021, REQ-F-029, REQ-F-030
  - Check: `src/qml/RightSidebar/SidebarTabButton.qml` contains `Canvas { onPaint }` icon rendering, uses `HoloniightPalette` colors, and declares `drawIcon()` function.

- [x] T-008: Create SidebarContent.qml with Loader
  - REQs: REQ-F-012, REQ-F-023, REQ-F-024, REQ-NF-001, REQ-NF-002
  - Check: `src/qml/RightSidebar/SidebarContent.qml` wraps a `ScrollView` containing a `Loader`, exposes `contentOpacity` property, `preferredWidthForTab()` method, and binds `Loader.active` to `root.active`.

- [x] T-009: Create tab page placeholder components
  - REQs: REQ-F-018, REQ-F-023, REQ-F-024
  - Check: All six files (`SidebarOverview.qml`, `SidebarCalendar.qml`, `SidebarNotifications.qml`, `SidebarSystem.qml`, `SidebarQuickSettings.qml`, `SidebarMedia.qml`) exist in `src/qml/RightSidebar/` with distinct `preferredWidth` values and placeholder text.

- [x] T-010: Register sidebar QML files in CMakeLists.txt
  - REQs: REQ-C-004
  - Check: All 11 new QML files appear in `HOLONIGHT_QML_FILES` in `CMakeLists.txt` with correct file paths.

- [x] T-011: Create SidebarDismissOverlay.qml surface
  - REQs: REQ-F-006, REQ-F-007
  - Check: `src/qml/RightSidebar/SidebarDismissOverlay.qml` contains `MouseArea` and `Keys.onEscapePressed` handlers, both calling `SidebarManager.close(root.barMonitorName)`.

- [x] T-012: Implement dismiss overlay lifecycle in SidebarManager
  - REQs: REQ-F-006, REQ-F-007, REQ-F-007A, REQ-C-002
  - Check: `SidebarManager::createDismissOverlay()` and `destroyDismissOverlay()` methods exist; dismiss surface uses layer-shell anchor `top | bottom | left | right`, namespace `sidebar-dismiss`, and exclusive keyboard interactivity.

- [x] T-013: Implement tab switch animation in RightSidebar.qml
  - REQs: REQ-F-017
  - Check: `RightSidebar.qml` contains `tabSwitchAnimation` `NumberAnimation` on `contentArea.width` with 180ms duration; animation starts when `currentTab` changes while sidebar is open.

- [x] T-014: Update ClockSection.qml to call SidebarManager.toggle()
  - REQs: REQ-F-005, REQ-F-031
  - Check: `src/qml/Topbar/ClockSection.qml` `MouseArea.onClicked` calls `SidebarManager.toggle(root.barMonitorName)`; the clock no longer opens `PopupSurface`.

- [x] T-015: Retain PopupSurface for session popup
  - REQs: REQ-F-031
  - Check: `PopupSurface` remains available for `SessionSection.qml`; clicking the clock no longer opens it. REQ-F-031 is satisfied by removing the temporary clock-triggered popup behavior, not by deleting the shared session popup infrastructure.

- [x] T-016: Verify build completes without errors
  - REQs: REQ-C-001, REQ-C-002, REQ-C-003, REQ-C-004, REQ-C-005
  - Check: `task build` completes without errors and `task qml-lint` reports no violations in `src/qml/RightSidebar/`.

- [ ] T-017: Add keyboard navigation
  - REQs: REQ-NF-005, REQ-NF-007
  - Status: Deferred until sidebar tabs contain real interactive content.
  - Check: Escape dismiss works now; future work should add focus traversal and arrow-key tab switching.

- [x] T-018: Close sidebar on visible workspace change
  - REQs: REQ-F-007A
  - Check: `ShellApplication` connects `ActiveWindowService::visibleWorkspaceChanged` to transient overlay dismissal, including `SidebarManager::closeAll()`.
