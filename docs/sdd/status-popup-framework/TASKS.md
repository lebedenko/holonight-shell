# SDD Tasks — status-popup-framework

- [x] T-001: C++ StatusPopupSurface skeleton (header + implementation)
  - REQs: REQ-F-001, REQ-F-002, REQ-F-007, REQ-F-008, REQ-F-009, REQ-F-010, REQ-F-011, REQ-F-012, REQ-NF-003
  - Check: `src/surfaces/StatusPopupSurface.h` and `.cpp` exist with class definition, all Q_PROPERTY and Q_INVOKABLE slots declared (toggle, show, hide), per-id size lookup helper, and pending/active pattern shell connection; code compiles without errors.

- [x] T-002: ShellApplication registration and wiring (includes holonight_surfaces CMake)
  - REQs: REQ-F-001, REQ-C-002
  - Check: StatusPopupSurface is instantiated in ShellApplication ctor, registered as QML singleton "StatusPopupSurface" in registerQmlTypes(), holonight_surfaces target includes StatusPopupSurface.h/.cpp; CMake configure completes without errors.

- [x] T-003: Layer surface positioning and geometry logic (ensureSurface implementation)
  - REQs: REQ-F-035, REQ-F-036, REQ-F-037, REQ-F-038, REQ-F-039, REQ-F-014, REQ-F-040
  - Check: ensureSurface() resolves QScreen by name, computes left_margin and pointer_x_ via the documented formula with clamping, sets layer surface anchor/size/exclusive_zone/margin/keyboard_interactivity correctly, and stores popup_id; manual inspection confirms popup appears below bar with correct positioning on multi-monitor setup.

- [x] T-004: Toggle/show/hide invokable implementation (surface lifecycle control)
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006
  - Check: toggle("audio", ...) twice closes then reopens; show("audio"); show("network") switches without intermediate hide; hide() clears activePopupId and emits popupVisibleChanged(false); PopupSurface.hide() is called before any show().

- [x] T-005: StatusPopupDismissOverlay.qml (fullscreen transparent dismiss layer)
  - REQs: REQ-F-033, REQ-NF-002
  - Check: StatusPopupDismissOverlay.qml exists in src/qml/Topbar/, contains a MouseArea filling parent with onClicked handler calling StatusPopupSurface.hide(), added to HOLONIGHT_QML_FILES in CMakeLists.txt.

- [x] T-006: Dismiss surface creation and destruction (second layer-shell surface management in StatusPopupSurface)
  - REQs: REQ-F-033, REQ-NF-002
  - Check: StatusPopupSurface maintains view_dismiss_ and surface_dismiss_ members, creates dismiss surface in layer_bottom at start of ensureSurface(), destroys both surfaces atomically in destroySurface(), commits dismiss surface before popup surface.

- [x] T-007: StatusPopup.qml styled container (Canvas notch, panel, glow, entry animation)
  - REQs: REQ-F-013, REQ-F-014, REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019, REQ-F-020, REQ-F-021
  - Check: StatusPopup.qml exists in src/qml/Topbar/ with Canvas notch responding to StatusPopupSurface.pointerX, MultiEffect glow (not Qt5Compat), HoloniightPalette colors (no hex literals), placeholder title for all four popup IDs, entry animation (opacity + y-axis translate over 250ms), focus: true, Keys.onEscapePressed calling StatusPopupSurface.hide(); qmllint reports no unqualified access warnings.

- [x] T-008: StatusPopupTriggerArea.qml drop-in component (click/active-state area)
  - REQs: REQ-F-022, REQ-F-023, REQ-F-024, REQ-F-025, REQ-F-026, REQ-NF-002
  - Check: StatusPopupTriggerArea.qml exists in src/qml/Topbar/, declares required properties popupId and barMonitorName, contains MouseArea calling StatusPopupSurface.toggle() with correct parameters, exposes isActivePopup property that checks (popupVisible && activePopupId === popupId), added to HOLONIGHT_QML_FILES.

- [x] T-009: Add CMakeLists.txt entries for QML files
  - REQs: REQ-NF-002
  - Check: All three .qml files (StatusPopup, StatusPopupDismissOverlay, StatusPopupTriggerArea) are added to HOLONIGHT_QML_FILES in alphabetical order; CMake configure succeeds and QRC paths qrc:/HolonightShell/Topbar/StatusPopup*.qml are valid.

- [x] T-010: NetworkWidget integration (StatusPopupTriggerArea insertion + active-state binding)
  - REQs: REQ-F-027, REQ-F-025
  - Check: NetworkWidget.qml contains StatusPopupTriggerArea { popupId: "network"; barMonitorName: root.barMonitorName }, hoverFrame.color and border.color updated to reflect popupTrigger.isActivePopup state, MultiEffect.visible includes popupTrigger.isActivePopup; clicking widget toggles popup open/closed.

- [x] T-011: AudioWidget integration (StatusPopupTriggerArea insertion + active-state binding)
  - REQs: REQ-F-028, REQ-F-025
  - Check: AudioWidget.qml contains StatusPopupTriggerArea { popupId: "audio"; barMonitorName: root.barMonitorName }, hoverFrame.color and border.color updated to reflect popupTrigger.isActivePopup state, MultiEffect.visible includes popupTrigger.isActivePopup; clicking widget toggles popup open/closed.

- [x] T-012: BatteryWidget integration (StatusPopupTriggerArea insertion + active-state binding)
  - REQs: REQ-F-029, REQ-F-025
  - Check: BatteryWidget.qml contains StatusPopupTriggerArea { popupId: "battery"; barMonitorName: root.barMonitorName }, hoverFrame.color and border.color updated to reflect popupTrigger.isActivePopup state, MultiEffect.visible includes popupTrigger.isActivePopup; clicking widget toggles popup open/closed.

- [x] T-013: KeyboardLayoutWidget integration (StatusPopupTriggerArea insertion + active-state binding)
  - REQs: REQ-F-030, REQ-F-025
  - Check: KeyboardLayoutWidget.qml contains StatusPopupTriggerArea { popupId: "keyboard-layout"; barMonitorName: root.barMonitorName }, hoverFrame.color and border.color updated to reflect popupTrigger.isActivePopup state; clicking widget toggles popup open/closed.

- [x] T-014: Build and qmllint verification
  - REQs: REQ-NF-006, REQ-NF-001
  - Check: `task build` completes exit code 0 with no warnings in new code; `task qml-lint` reports no errors for StatusPopup.qml, StatusPopupTriggerArea.qml, StatusPopupDismissOverlay.qml, or four widget files (with correct -I include paths for HolonightShell).

- [x] T-015: Manual verification of all four dismissal paths and positioning under Wayland
  - REQs: REQ-F-031, REQ-F-032, REQ-F-033, REQ-F-034, REQ-F-035, REQ-F-036, REQ-F-037, REQ-F-041, REQ-F-042, REQ-NF-007
  - Check: Under `task run` on live Hyprland: (A) click active widget closes popup, (B) click different widget switches popup, (C) click outside popup dismisses it, (D) Esc key closes popup (or document as known limitation); popup renders below bar with correct spacing, pointer notch aligns with widget center, edge clamping prevents popup from extending beyond screen bounds, multi-monitor setup shows popup on correct monitor.
