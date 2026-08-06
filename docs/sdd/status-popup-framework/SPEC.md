# Status Popup Framework — SPEC.md

**Feature:** Reusable popup framework for four top-bar status widgets (network, audio, battery, keyboard-layout).
**Scope:** Empty styled container only; real per-widget content deferred to later cycles.
**Target Build:** C++23/Qt6 Wayland shell.

---

## Functional Requirements

### F.1 Core Surface & Lifecycle

**REQ-F-001: StatusPopupSurface C++ Singleton Registration**
- Ubiquitous: The system shall instantiate a `StatusPopupSurface` singleton in `ShellApplication.cpp`, register it as a QML-exposed type in the `HolonightShell` module, and add it to the `holonight_surfaces` management collection.
- Acceptance: `qmltype StatusPopupSurface { Q_OBJECT Q_CLASSINFO... }` is registered in `ShellApplication.cpp` before any view is created; introspection lists the class under `HolonightShell.StatusPopupSurface`.

**REQ-F-002: Single Layer Surface Ownership**
- Ubiquitous: The system shall own and manage exactly one `QQuickView*` + `LayerSurface*` pair for all status popups, reusing or remapping the surface when switching between popup IDs.
- Acceptance: At most one `LayerSurface::zwlr_layer_surface_v1` resource exists for status popups at any time; successive `show()` calls with different IDs reuse the same surface object.

**REQ-F-003: Toggle Behavior**
- Event-driven: When `StatusPopupSurface.toggle(popupId, screenName, anchorX, anchorWidth)` is invoked and `popupId` matches the currently visible popup ID, the system shall hide the popup and emit `popupVisibleChanged(false)`.
- Event-driven: When `StatusPopupSurface.toggle(popupId, screenName, anchorX, anchorWidth)` is invoked and `popupId` does NOT match the currently visible popup ID (or no popup is visible), the system shall show that popup ID.
- Acceptance: Two successive `toggle("audio", ...)` calls hide then re-show; `toggle("audio", ...); toggle("network", ...)` shows network and hides audio.

**REQ-F-004: Show Behavior**
- Event-driven: When `StatusPopupSurface.show(popupId, screenName, anchorX, anchorWidth)` is invoked, the system shall always make `popupId` the active popup, creating or remapping the surface as needed.
- Acceptance: `show("audio", ...); show("audio", ...)` leaves audio visible; `show("audio", ...); show("network", ...)` immediately switches to network without an intermediate hide.

**REQ-F-005: Hide Behavior**
- Event-driven: When `StatusPopupSurface.hide()` is invoked, the system shall hide the popup, destroy the `QQuickView`, and emit `popupVisibleChanged(false)` and `activePopupChanged("")`.
- Acceptance: After `hide()`, `popupVisible` is `false` and `activePopupId` is empty string; calling `hide()` again does not crash or re-emit.

**REQ-F-006: Session Popup Coordination**
- Event-driven: When a status popup is about to show, the system shall call `PopupSurface.hide()` as a one-way nudge to close any open session/clock popup.
- Acceptance: Triggering a status widget while a session popup is visible causes both popups not to be visible simultaneously; this nudge is not reciprocal (session popups do not hide status popups).

### F.2 QML-Facing API & Properties

**REQ-F-007: QML Property: popupVisible**
- Ubiquitous: The system shall expose a `bool popupVisible` Q_PROPERTY with a `popupVisibleChanged` signal, reflecting whether any status popup surface is currently shown.
- Acceptance: `StatusPopupSurface.popupVisible` is `true` after any `show()` or `toggle()` that displays a popup; `false` after `hide()` or `toggle()` that closes the active popup.

**REQ-F-008: QML Property: activePopupId**
- Ubiquitous: The system shall expose a `QString activePopupId` Q_PROPERTY with an `activePopupChanged` signal, holding the ID of the currently visible popup (or empty string if none).
- Acceptance: After `show("audio", ...)`, `activePopupId === "audio"`; after `hide()`, `activePopupId === ""`.

**REQ-F-009: QML Property: pointerX**
- Ubiquitous: The system shall expose an `int pointerX` Q_PROPERTY with a `geometryChanged` signal, holding the calculated horizontal position of the pointer notch within the popup.
- Acceptance: `pointerX` value is updated whenever a popup is shown or repositioned; read-only from QML.

**REQ-F-010: QML Invokable: toggle(popupId, screenName, anchorX, anchorWidth)**
- Ubiquitous: The system shall expose a `Q_INVOKABLE void toggle(const QString& popupId, const QString& screenName, int anchorX, int anchorWidth)` slot callable from QML.
- Acceptance: Method is invokable from QML; signature matches; all parameters are passed correctly to C++ logic.

**REQ-F-011: QML Invokable: show(popupId, screenName, anchorX, anchorWidth)**
- Ubiquitous: The system shall expose a `Q_INVOKABLE void show(const QString& popupId, const QString& screenName, int anchorX, int anchorWidth)` slot callable from QML.
- Acceptance: Method is invokable from QML; signature matches; all parameters are passed correctly to C++ logic.

**REQ-F-012: QML Invokable: hide()**
- Ubiquitous: The system shall expose a `Q_INVOKABLE void hide()` slot callable from QML.
- Acceptance: Method is invokable from QML; hides the popup and clears activePopupId.

### F.3 StatusPopup.qml Container

**REQ-F-013: Empty Container Component**
- Ubiquitous: The system shall provide `StatusPopup.qml` as a reusable empty styled container, placed in `src/qml/Topbar/StatusPopup/`.
- Acceptance: Component exists, is registered in the `HolonightShell` QML module, and can be instantiated as `StatusPopup { }` in QML.

**REQ-F-014: Per-Popup-ID Size Lookup**
- Ubiquitous: The system shall store and apply per-popup-ID fixed sizes (audio ~900×600, network/battery/keyboard ~480×320) via C++ lookup in `StatusPopupSurface`.
- Acceptance: Audio popup width is 900px and height 600px; network/battery/keyboard popups are 480px × 320px; sizes are not changed by later content additions.

**REQ-F-015: Translucent Dark Panel Styling**
- Ubiquitous: The system shall style the empty popup container as a translucent dark panel with ~8px border radius and 1px active/cyan border.
- Acceptance: Visual inspection shows rounded corners, dark background with transparency, and a visible cyan edge.

**REQ-F-016: HoloniightPalette Colors (No Hardcoded Hex)**
- Ubiquitous: The system shall import `HoloniightPalette` (note double-i spelling) and source all popup colors (background, border, glow, text) exclusively from palette tokens.
- Acceptance: All `color` properties in `StatusPopup.qml` reference `HoloniightPalette.<token>`; no hex values or `Qt.rgba()` literals appear in the component.

**REQ-F-017: MultiEffect Glow (Not Qt5Compat)**
- Ubiquitous: The system shall apply glow via `QtQuick.Effects.MultiEffect` with `shadowEnabled: true`, not `Qt5Compat.GraphicalEffects.Glow`.
- Acceptance: Import is `import QtQuick.Effects` and glow implementation uses `MultiEffect { shadowEnabled: true ... }`.

**REQ-F-018: Canvas Pointer Notch**
- Ubiquitous: The system shall draw an upward-pointing notch in the popup bottom using a `Canvas` element, centered under the triggering widget.
- State-driven: While a popup is visible, the system shall update the notch position to match `StatusPopupSurface.pointerX`.
- Acceptance: A visible upward triangle or notch appears at the bottom of the popup; position changes when the popup is shown with different anchor positions.

**REQ-F-019: Canvas Property Qualification**
- Ubiquitous: The system shall qualify all property access inside Canvas `onPaint` handlers with a root id (e.g., `root.myProp`), not bare names.
- Acceptance: `qmllint` reports no "unqualified access" warnings in `StatusPopup.qml`; qmllint is run with correct `-I` include paths for `HolonightShell` module.

**REQ-F-020: Placeholder Title Header**
- Ubiquitous: The system shall display a human-readable title header (e.g., "Audio", "Network", "Battery", "Keyboard Layout") in the empty container for visual verification.
- Acceptance: The popup shows the correct localized name for each popup ID; title is readable and does not overflow the panel.

**REQ-F-021: Entry Animation**
- Ubiquitous: The system shall animate popup appearance using opacity and y-axis translation (small upward movement on entry).
- Acceptance: When a popup appears, it smoothly fades in and moves up over ~200–300ms; no jarring instant appearance.

### F.4 StatusPopupTriggerArea.qml Reusable Component

**REQ-F-022: Trigger Area Component**
- Ubiquitous: The system shall provide `StatusPopupTriggerArea.qml` as a reusable click/hover area, placed in `src/qml/Topbar/StatusPopup/`.
- Acceptance: Component exists in `src/qml/Topbar/StatusPopup/StatusPopupTriggerArea.qml` and is importable from `HolonightShell`.

**REQ-F-023: Trigger Area Drop-In Integration**
- Ubiquitous: The system shall accept `popupId` and `barMonitorName` as required properties and expose the widget's bounding geometry (`x`, `width`) for positioning calculations.
- Acceptance: `StatusPopupTriggerArea { required property string popupId; required property string barMonitorName; ... }` compiles without error.

**REQ-F-024: Toggle on Click**
- Event-driven: When a user clicks the trigger area, the system shall invoke `StatusPopupSurface.toggle(popupId, barMonitorName, triggerAreaX, triggerAreaWidth)`.
- Acceptance: Clicking the trigger area opens the popup if closed, or closes it if open.

**REQ-F-025: Active Widget Styling**
- State-driven: While `StatusPopupSurface.popupVisible && StatusPopupSurface.activePopupId === popupId`, the system shall apply active/selected styling to the trigger area.
- Acceptance: The widget containing the active trigger area is visibly highlighted (e.g., background color, border, or glow change) when its popup is open.

**REQ-F-026: Does Not Reuse PopupSurface.popupVisible**
- Ubiquitous: The system shall NOT bind widget selected-state to `PopupSurface.popupVisible`; instead use `StatusPopupSurface.activePopupId` comparison.
- Acceptance: Closing a session popup does not un-highlight a status widget; only `StatusPopupSurface` properties control the widget's active state.

### F.5 Four Status Widgets Integration

**REQ-F-027: NetworkWidget Integration**
- Ubiquitous: The system shall add a `StatusPopupTriggerArea` to `NetworkWidget.qml` with `popupId: "network"` and `barMonitorName: <propagated from bar>`.
- Acceptance: `NetworkWidget.qml` contains an instantiated `StatusPopupTriggerArea` with the correct properties; clicking it toggles the network popup.

**REQ-F-028: AudioWidget Integration**
- Ubiquitous: The system shall add a `StatusPopupTriggerArea` to `AudioWidget.qml` with `popupId: "audio"` and `barMonitorName: <propagated from bar>`.
- Acceptance: `AudioWidget.qml` contains an instantiated `StatusPopupTriggerArea` with the correct properties; clicking it toggles the audio popup.

**REQ-F-029: BatteryWidget Integration**
- Ubiquitous: The system shall add a `StatusPopupTriggerArea` to `BatteryWidget.qml` with `popupId: "battery"` and `barMonitorName: <propagated from bar>`.
- Acceptance: `BatteryWidget.qml` contains an instantiated `StatusPopupTriggerArea` with the correct properties; clicking it toggles the battery popup.

**REQ-F-030: KeyboardLayoutWidget Integration**
- Ubiquitous: The system shall add a `StatusPopupTriggerArea` to `KeyboardLayoutWidget.qml` with `popupId: "keyboard-layout"` and `barMonitorName: <propagated from bar>`.
- Acceptance: `KeyboardLayoutWidget.qml` contains an instantiated `StatusPopupTriggerArea` with the correct properties; clicking it toggles the keyboard-layout popup.

### F.6 Dismissal Mechanisms

**REQ-F-031: Widget Click Dismissal**
- Event-driven: When a user clicks the widget that owns the currently visible status popup, the system shall toggle the popup closed.
- Acceptance: Clicking the active status widget hides its popup; clicking an inactive widget opens that popup instead.

**REQ-F-032: Cross-Widget Dismissal**
- Event-driven: When a user clicks a status widget while a different status widget's popup is visible, the system shall close the currently visible popup and open the new one.
- Acceptance: Clicking network while audio popup is open closes audio and opens network in a single action.

**REQ-F-033: Outside-Click Dismissal**
- Event-driven: When a user clicks anywhere outside the status popup, the system shall close the popup.
- Acceptance: A fullscreen transparent dismiss overlay (similar to `SessionService` dismiss overlay pattern) is present and functional; clicking outside the popup bounds dismisses it.

**REQ-F-034: Keyboard Esc Dismissal**
- Event-driven: When a user presses the Esc key while a status popup is visible, the system shall close the popup.
- Requirement: Layer surface shall enable `layer_shell` keyboard interactivity, following the `TrayMenuPopup` working pattern.
- Acceptance: Pressing Esc when a status popup is open closes it; this requires enabling layer-shell keyboard focus on the surface (manually verified under `task run`).

### F.7 Positioning & Geometry

**REQ-F-035: Screen Resolution and Clamping**
- Event-driven: When a popup is shown with a `screenName`, the system shall resolve the QScreen object by name, center the popup under the anchor, and clamp the left edge to at least `kScreenEdgeMargin` pixels from the left screen boundary.
- Acceptance: A popup triggered near the right edge of a screen is repositioned to remain fully within the screen bounds; no part of the popup extends beyond the monitor edge.

**REQ-F-036: Pointer Position Calculation**
- Ubiquitous: The system shall calculate `pointerX = anchorX + (anchorWidth / 2) − popupLeft`, ensuring the pointer notch is centered under the triggering widget.
- Acceptance: `pointerX` value aligns the canvas notch with the widget center; visual inspection shows the notch pointing at the widget.

**REQ-F-037: Pointer Clamping Within Panel Radius**
- Ubiquitous: The system shall clamp `pointerX` so that the pointer stays within the rounded panel bounds (not extending past the 8px radius at the bottom corners).
- Acceptance: Even with extreme anchor positions (far left/right), the pointer notch never appears outside the popup's rounded bottom edge.

**REQ-F-038: Layer Surface Anchor and Positioning**
- Ubiquitous: The system shall anchor the layer surface `top | left` and position it relative to the resolved QScreen, with a small top-margin gap below the bar.
- Acceptance: Popup appears directly below the topbar, does not overlap the bar, and is positioned on the correct output for multi-monitor setups.

**REQ-F-039: Exclusive Zone Zero**
- Ubiquitous: The system shall set `exclusive_zone` to 0 for the status popup layer surface, allowing windows to render beneath the popup.
- Acceptance: Layer surface is configured with `exclusive_zone = 0` in the layer shell protocol.

### F.8 Multi-Monitor Support

**REQ-F-040: BarMonitorName Propagation**
- Ubiquitous: The system shall propagate `barMonitorName` (a required `string` property on each status widget) through the trigger area to `StatusPopupSurface.show/toggle` calls.
- Acceptance: Each widget pass their assigned monitor name to the trigger area; trigger invocations include the monitor name as the `screenName` parameter.

**REQ-F-041: Correct Monitor Output**
- Event-driven: When a popup is shown on a specific monitor, the system shall position and display the popup on that QScreen output only.
- Acceptance: In a multi-monitor setup, clicking a widget on monitor A shows the popup on monitor A; clicking the same widget on monitor B shows the popup on monitor B.

**REQ-F-042: Screen Edge Clamping Multi-Monitor**
- Event-driven: When a popup is shown near the edge of a non-primary monitor, the system shall clamp the popup to that monitor's bounds, not the primary screen.
- Acceptance: A popup on a secondary monitor near its right edge is repositioned to stay within that monitor's geometry, not the primary screen's geometry.

---

## Non-Functional Requirements

**REQ-NF-001: QML Module Registration**
- Ubiquitous: The system shall register all new QML components (`StatusPopup.qml`, `StatusPopupTriggerArea.qml`) in the `HolonightShell` QML module with the correct `QML_ELEMENT` annotations.
- Acceptance: `qmllint` reports no "component not found" errors; components are importable as `import HolonightShell; StatusPopup { }`.

**REQ-NF-002: QRC Asset Bundling**
- Ubiquitous: The system shall add all new QML files to `HOLONIGHT_QML_FILES` in `CMakeLists.txt`; CMake shall derive `QT_RESOURCE_ALIAS` values and fail configure if any file is missing.
- Acceptance: CMake configure succeeds; `qrc:/HolonightShell/Topbar/StatusPopup/StatusPopup.qml` is a valid QRC path.

**REQ-NF-003: C++ Naming Conventions**
- Ubiquitous: The system shall follow C++23 and clang-tidy naming rules: variable identifiers ≥3 characters, class names PascalCase, method names camelCase.
- Acceptance: `clang-tidy` reports no `readability-identifier-length` violations in new C++ code.

**REQ-NF-004: Visible Logging**
- Ubiquitous: The system shall use `qCInfo` or `qCWarning` for user-visible diagnostic output (popups shown/hidden, geometry updates); use `qCDebug` only for detailed internal tracing.
- Acceptance: Running under `task run` shows status popup lifecycle events (show, hide, resize) in console output.

**REQ-NF-005: No Hardcoded Colors**
- Ubiquitous: The system shall not use hardcoded hex color values, `Qt.rgba()` literals, or palette-independent color names in QML; all colors come from `HoloniightPalette`.
- Acceptance: `grep -r "#[0-9a-fA-F]" src/qml/Topbar/StatusPopup/` returns no color definitions; `grep -r "Qt\.rgba" src/qml/Topbar/StatusPopup/` returns no color definitions.

**REQ-NF-006: Build & Test Execution**
- Ubiquitous: The system shall build without warnings or errors using `task configure && task build`.
- Acceptance: `task build` completes with exit code 0; no compiler warnings in new code.

**REQ-NF-007: Manual Verification**
- Ubiquitous: Acceptance of dismissal mechanisms (Esc, outside-click), positioning, multi-monitor behavior, and animation timing shall be verified manually under `task run` with a live Hyprland compositor.
- Acceptance: Manual testing checklist is completed under live Wayland; results are documented.

---

## Constraints

**REQ-C-001: C++23 / Qt6**
- Ubiquitous: The system shall be implemented in C++23 and Qt 6.11+ (or later).
- Acceptance: Code compiles with Qt6 and does not use Qt5-only constructs.

**REQ-C-002: QML Module URI**
- Ubiquitous: All QML components shall be registered under the `HolonightShell` module URI and imported with `import HolonightShell`.
- Acceptance: No runtime "module not found" errors; components are discoverable in the module.

**REQ-C-003: QRC Path Prefix**
- Ubiquitous: All QML and asset paths shall use the QRC prefix `/HolonightShell/`.
- Acceptance: QRC URLs follow the pattern `qrc:/HolonightShell/...`; no `/qml/` prefix is used in QRC paths.

**REQ-C-004: Wayland Layer Shell Protocol**
- Ubiquitous: The surface shall be a layer-shell surface (`zwlr_layer_surface_v1`) compatible with Hyprland 0.55.2+.
- Acceptance: The popup renders correctly under the tested Hyprland version; no protocol violations in compositor logs.

**REQ-C-005: No Global Coordinator**
- Ubiquitous: The system shall NOT introduce a shared PopupCoordinator or mutual visibility coupling between `StatusPopupSurface` and `PopupSurface` (session/clock popups).
- Acceptance: `PopupSurface` is not modified to detect or react to status popups; only a one-way `PopupSurface.hide()` call is made by `StatusPopupSurface`.

---

## Non-Goals

The following are explicitly out of scope for this iteration:

- **Real popup content**: No audio controls, network list, battery details, or keyboard layout panel. Only an empty styled container with a placeholder title.
- **AudioService / NetworkService changes**: No new properties or signals required from backend services.
- **Settings / Persistence**: No config file for popup sizes, positions, or visibility state.
- **Session/Clock popup changes** (beyond calling `PopupSurface.hide()`): Content, styling, and behavior of existing popups are unchanged.
- **Shared PopupCoordinator**: No unified visibility or state-machine orchestration; each popup service is independent.

---

## Risks & Mitigation

**Risk 1: Layer-Shell Keyboard Focus Unreliability**
- **Description**: Hyprland layer-shell surfaces at `layer_top` may not reliably receive keyboard focus, causing Esc dismissal to fail.
- **Mitigation**: Copy the working `TrayMenuPopup` pattern for keyboard interactivity; if Esc still fails in testing, document as a known limitation and provide alternate dismissal (outside-click, widget click).
- **Verification**: Manual testing under `task run` on Hyprland 0.55.2+.

**Risk 2: Pointer Positioning at Screen Edges**
- **Description**: Complex edge-clamping logic may miscalculate pointer position on multi-monitor layouts or near screen boundaries.
- **Mitigation**: Implement clamping step-by-step with unit tests where feasible; manual visual verification on a multi-monitor Wayland session is required for final acceptance.
- **Verification**: Manual testing on multi-monitor setup; visual inspection of pointer alignment.

**Risk 3: Multi-Monitor Screen.name Resolution**
- **Description**: Qt's `Screen.name` is unreliable for layer-shell windows; bar components already receive `barMonitorName` as a workaround, but incorrect propagation could lose monitor context.
- **Mitigation**: Pass `barMonitorName` through the entire trigger-area-to-surface call chain; unit-test the propagation path; manual verification on multi-monitor setup.
- **Verification**: Multi-monitor manual testing; logs confirm correct screen name for each popup invocation.

**Risk 4: Animation Performance on Low-End Hardware**
- **Description**: Entry animation (opacity + y-axis translation) may stutter on low-end or heavily loaded systems.
- **Mitigation**: Use standard `Transition` mechanisms; if performance issues arise, document as a known limitation or provide a disable option via `ThemeService`.
- **Verification**: Manual testing on available hardware; no dropped frames expected on modern systems.

**Risk 5: Acceptance Testing for Dismissal (All Four Mechanisms)**
- **Description**: All four dismissal modes (widget click, cross-widget click, outside-click, Esc) must work correctly. Missing any one fails the feature.
- **Mitigation**: Create a detailed manual test checklist; verify each mode separately; document any failures as blockers.
- **Verification**: Manual testing under `task run`; each dismissal mechanism is tested in isolation and in combination.

---

## Summary

This specification defines a reusable status popup framework for four top-bar widgets, shipping an empty but fully styled and animated container. The framework includes positioning, multi-monitor support, four dismissal mechanisms, and a clean C++/QML API for future content expansion. All colors come from the theme; all positioning logic reuses established patterns (TooltipSurface, SessionService overlay); all acceptance criteria are independently verifiable via manual testing on live Hyprland.

Deferred to later cycles: real popup content (audio mini-pavucontrol, network list, battery info, keyboard layout panel) and any associated service changes.
