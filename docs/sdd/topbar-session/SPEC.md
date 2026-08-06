# Topbar Session (Power/Logout Menu) — EARS Specification

**Document ID**: topbar-session  
**Phase**: MVP  
**Status**: Final (decisions locked from grilling synthesis)  
**Date**: 2026-05-21

---

## Overview

The topbar-session feature adds a power/session button to the rightmost area of the topbar that, when clicked, displays a fly-out popup menu with four session control actions: Lock, Logout, Reboot, and Shutdown.

---

## Subsystem 1: SessionService (C++ Singleton)

### REQ-F-001 (Ubiquitous)
**The SessionService shall expose four Q_INVOKABLE methods to QML: `lockScreen()`, `logout()`, `reboot()`, and `shutdown()`.**

- **Acceptance Criterion**: Each method is callable from QML code without errors; method signatures match the QML integration pattern used in existing services (e.g., BatteryService).

### REQ-F-002 (Ubiquitous)
**The SessionService shall be registered as a QML singleton and accessible via `SessionService.methodName()` in QML files.**

- **Acceptance Criterion**: QML can invoke SessionService methods without importing a C++ header; registration uses `QML_ELEMENT` and `qmlRegisterSingletonInstance` pattern per architecture guidelines.

### REQ-F-003 (Event-driven)
**When `logout()` is invoked, the SessionService shall execute `hyprctl dispatch exit` via `QProcess::startDetached()`.**

- **Acceptance Criterion**: Hyprland window manager terminates the session; no blocking wait in the main thread.

### REQ-F-004 (Event-driven)
**When `reboot()` is invoked, the SessionService shall execute `systemctl reboot` via `QProcess::startDetached()`.**

- **Acceptance Criterion**: System reboots; command executes with elevated privileges (via sudo or systemd user session); no confirmation dialog is shown.

### REQ-F-005 (Event-driven)
**When `shutdown()` is invoked, the SessionService shall execute `systemctl poweroff` via `QProcess::startDetached()`.**

- **Acceptance Criterion**: System powers off; command executes with elevated privileges; no confirmation dialog is shown.

### REQ-F-006 (Event-driven)
**When `lockScreen()` is invoked, the SessionService shall perform no action in this phase.**

- **Acceptance Criterion**: Method returns successfully without side effects; a comment in the code indicates it is a no-op placeholder for a future phase.

### REQ-NF-001 (Non-Functional)
**The SessionService shall not block the main Qt event loop during command execution.**

- **Acceptance Criterion**: `QProcess::startDetached()` is used, not `waitForFinished()`; UI remains responsive after invoking any action.

---

## Subsystem 2: Trigger Button (Canvas-Drawn Power Symbol)

### REQ-F-101 (Ubiquitous)
**The trigger button shall display a canvas-drawn power symbol (circle arc + vertical line) in the rightmost area of the topbar.**

- **Acceptance Criterion**: The symbol is drawn via QML Canvas or QML graphics primitives; it visually matches the power symbol geometry shown in design assets.

### REQ-F-102 (Event-driven)
**When the user clicks the trigger button, the button shall toggle the popup open if closed, or closed if open.**

- **Acceptance Criterion**: First click opens the popup; second click closes it; state is tracked by a boolean property (e.g., `popupOpen`).

### REQ-C-101 (Constraint)
**The trigger button shall not have a panel frame or octagon shape; framing decisions are deferred to topbar session 9 (topbar-hud-polish).**

- **Acceptance Criterion**: Button uses only the raw power symbol without any border or background frame around it.

### REQ-NF-101 (Non-Functional)
**The trigger button shall use colors exclusively from the HoloniightPalette theme tokens.**

- **Acceptance Criterion**: No hardcoded hex values; all colors are imported via `import Holonight` and accessed as `HoloniightPalette.<token>`.

---

## Subsystem 3: Popup Window

### REQ-F-201 (Ubiquitous)
**The popup shall be a fly-out window displayed above the trigger button and anchored to its right edge.**

- **Acceptance Criterion**: Popup x-position aligns to the right edge of the trigger button; popup y-position is above (negative offset) the button.

### REQ-F-202 (Ubiquitous)
**The popup shall have a rounded-rectangle background with fill color `#1f2335` and violet border color `#bb9af7`.**

- **Acceptance Criterion**: Background is rendered via a Rectangle with `radius` property set; border is visible and matches the specified colors; background does NOT use a panel frame.

### REQ-F-203 (Ubiquitous)
**The popup shall contain a vertical list of four items, each with an icon and a text label.**

- **Acceptance Criterion**: Items are arranged in a Column layout; icon and label are horizontally grouped per item; visual spacing matches design reference.

### REQ-F-204 (Event-driven)
**When the user clicks outside the popup, the popup shall dismiss immediately.**

- **Acceptance Criterion**: A MouseArea with `z: -1` covers the screen background; clicking it sets `popupOpen: false`.

### REQ-F-205 (Event-driven)
**When the user presses the trigger button a second time while the popup is open, the popup shall close.**

- **Acceptance Criterion**: Clicking the trigger button toggles the `popupOpen` state; popup is not visible when `popupOpen: false`.

### REQ-NF-201 (Non-Functional)
**The popup shall animate in via fade-in and slide-up with `Easing.OutCubic` timing and a duration of approximately 160ms.**

- **Acceptance Criterion**: Animation is triggered when `popupOpen` transitions from false to true; NumberAnimation and SequentialAnimationGroup are used; easing matches the specified curve.

### REQ-NF-202 (Non-Functional)
**The popup shall use colors exclusively from the HoloniightPalette theme tokens, except where overridden by this specification.**

- **Acceptance Criterion**: All dynamic colors come from `HoloniightPalette`; the hardcoded border and fill colors in REQ-F-202 are the only exceptions permitted.

### REQ-C-201 (Constraint)
**The popup shall not support multi-output positioning; it shall always render on the output containing the topbar.**

- **Acceptance Criterion**: No screen geometry calculations for popup positioning; popup is bound to the topbar's coordinate system.

---

## Subsystem 4: Popup Actions

### REQ-F-301 (Ubiquitous)
**The popup shall display four actions in the following order (top to bottom): Lock, Logout, Reboot, Shutdown.**

- **Acceptance Criterion**: Items appear in this exact sequence; each item is clickable.

### REQ-F-302 (Event-driven)
**When the user clicks the Lock action, the SessionService `lockScreen()` method shall be invoked, and the popup shall close.**

- **Acceptance Criterion**: Click handler calls `SessionService.lockScreen()`; popup closes (popupOpen set to false); no lock screen is displayed in this phase (lockScreen() is a no-op).

### REQ-F-303 (Event-driven)
**When the user clicks the Logout action, the SessionService `logout()` method shall be invoked, and the popup shall close.**

- **Acceptance Criterion**: Click handler calls `SessionService.logout()`; Hyprland session terminates.

### REQ-F-304 (Event-driven)
**When the user clicks the Reboot action, the SessionService `reboot()` method shall be invoked, and the popup shall close.**

- **Acceptance Criterion**: Click handler calls `SessionService.reboot()`; system reboots.

### REQ-F-305 (Event-driven)
**When the user clicks the Shutdown action, the SessionService `shutdown()` method shall be invoked, and the popup shall close.**

- **Acceptance Criterion**: Click handler calls `SessionService.shutdown()`; system powers off.

### REQ-C-301 (Constraint)
**No confirmation dialogs shall be displayed for any action in this phase.**

- **Acceptance Criterion**: Logout, Reboot, and Shutdown execute immediately upon click; no additional prompt is shown.

### REQ-C-302 (Constraint)
**Keyboard navigation (Tab, Enter, Escape) within the popup shall not be implemented in this phase.**

- **Acceptance Criterion**: The popup accepts only mouse clicks; keyboard control is not required.

---

## Subsystem 5: Icons and Resources

### REQ-F-401 (Ubiquitous)
**The popup actions shall display icons sourced from `assets/bar-icons/` with the following filenames:**
- Lock: `system-lock-screen-symbolic.svg`
- Logout: `system-log-out-symbolic.svg`
- Reboot: `system-reboot-symbolic.svg`
- Shutdown: `system-shutdown-symbolic.svg`

- **Acceptance Criterion**: Each icon file exists in `assets/bar-icons/`; icon filenames match exactly.

### REQ-F-402 (Ubiquitous)
**All four icon SVG files shall be bundled as QRC resources with the application using the `/HolonightShell/` resource prefix.**

- **Acceptance Criterion**: Icons are registered in `CMakeLists.txt` via `QT_RESOURCE_ALIAS`; they are accessible in QML via `qrc:/HolonightShell/...` paths; the build succeeds and icons are embedded in the binary.

### REQ-F-403 (Ubiquitous)
**Each popup action item shall display its icon to the left of its text label with consistent horizontal spacing.**

- **Acceptance Criterion**: Icon and label are contained in a Row layout; spacing between icon and label matches design reference (approximately 8–12 px).

### REQ-NF-401 (Non-Functional)
**Icon colors shall adapt to the active theme via the system icon theme, not hardcoded colors.**

- **Acceptance Criterion**: Icons are rendered using `Image` or `Image` source with no explicit color overlay, or via `ColorOverlay` with a `HoloniightPalette` color token.

---

## Subsystem 6: Animation

### REQ-F-501 (Event-driven)
**When the popup transitions from closed to open, it shall animate via a fade-in and slide-up effect.**

- **Acceptance Criterion**: Opacity animates from 0 to 1; y-position slides from a negative offset (e.g., -20px) to 0; both animations run concurrently with `Easing.OutCubic`.

### REQ-F-502 (Event-driven)
**When the popup transitions from open to closed, it shall animate via fade-out and slide-down (reverse of open animation).**

- **Acceptance Criterion**: Opacity animates to 0; y-position slides back to negative offset; duration matches the open animation (~160ms).

### REQ-NF-501 (Non-Functional)
**Animation duration shall be approximately 160 milliseconds.**

- **Acceptance Criterion**: `NumberAnimation { duration: 160 }` is used; animation is perceptibly snappy and not sluggish.

---

## Subsystem 7: Integration with Topbar

### REQ-F-601 (Ubiquitous)
**The trigger button shall be integrated into the topbar as a new component in the rightmost area, after existing sections (Audio, Network, Battery, etc.).**

- **Acceptance Criterion**: Button is added to the topbar layout; it does not overlap existing sections; it is positioned at the visual right edge of the bar.

### REQ-NF-601 (Non-Functional)
**The SessionPopup component (popup QML file) shall follow the existing QML per-directory layout pattern under `src/qml/Topbar/` with a `QT_RESOURCE_ALIAS` CMake property.**

- **Acceptance Criterion**: QML files are organized as `src/qml/Topbar/SessionSection.qml` and `src/qml/Topbar/SessionPopup.qml` (or similar); CMakeLists.txt assigns `QT_RESOURCE_ALIAS` to strip the `src/qml/` prefix; QML modules are registered correctly.

---

## Acceptance Sign-Off Criteria

### Build & Compilation
- [ ] Project builds without errors or warnings (`task build`).
- [ ] No clang-tidy violations introduced.
- [ ] QML linting passes (`task qml-lint`).

### Functional Testing
- [ ] Trigger button is visible in the topbar at the rightmost position.
- [ ] Clicking the button opens the popup with fade-in + slide-up animation.
- [ ] Clicking the button again closes the popup with fade-out + slide-down animation.
- [ ] Clicking outside the popup closes it.
- [ ] All four action labels and icons are visible in the popup.
- [ ] Clicking Lock, Logout, Reboot, or Shutdown invokes the corresponding SessionService method.
- [ ] Logout, Reboot, and Shutdown commands execute (verified in a test session or sandbox environment).

### Visual & Theme Compliance
- [ ] Button and popup colors match the HoloniightPalette theme.
- [ ] Icons render correctly from QRC resources.
- [ ] No hardcoded hex colors appear in QML code.
- [ ] Animation easing and duration match the specification.

### Code Quality
- [ ] SessionService follows the singleton pattern used in BatteryService and AudioService.
- [ ] C++ naming conventions (CamelCase classes, camelCase methods) are applied.
- [ ] QML code style matches existing sections.

---

## Out of Scope (Deferred to Future Phases)

- Actual lock screen implementation.
- Confirmation dialogs for destructive actions.
- Keyboard navigation in the popup (Tab, Escape, Enter).
- Panel frame / octagon / angled corner styling (topbar session 9: topbar-hud-polish).
- Multi-output popup positioning logic.
- Accessibility features (screen reader labels, focus indicators).

---

## References

- **Design Assets**: `assets/dont-commit/` (authoritative visual reference; consult before UI decisions).
- **Architecture Precedent**: BatteryService, AudioService (singleton D-Bus patterns); ExtWorkspaceManager (QML integration).
- **Styling Reference**: `docs/sdd/topbar-network/SPEC.md`, `docs/sdd/topbar-audio/SPEC.md` (similar popup-based features).
- **TOPBAR-PLAN.md**: Overall topbar session roadmap and phase dependencies.
