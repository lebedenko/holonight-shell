# Battery Click-Popup Content + Power Profile Selector — EARS Specification

**Project:** holonight-shell (Qt6/QML Wayland shell)  
**Version:** 1.0  
**Date:** 2026-06-02  
**Author:** Andrii L

---

## Overview

This specification defines the content, layout, styling, sizing, and interaction model for the battery click-popup panel in holonight-shell. The popup displays battery state, health metrics, time-remaining estimate, and provides a power profile selector driven by power-profiles-daemon. A new `PowerProfilesService` C++ singleton manages daemon communication (dual D-Bus name detection), and `BatteryService` extends to expose time-remaining and health properties. The popup is a fixed-size, title-less `BatteryPopupContent.qml` component registered in the `StatusPopup` framework.

---

## Ubiquitous Language

- **Battery State**: "Charging", "Discharging", "Empty", "Fully Charged", "Pending Charge/Discharge"
- **Power Profile**: hardware power management mode (e.g., "power-saver", "balanced", "performance")
- **power-profiles-daemon**: systemd D-Bus service managing power profiles; may be absent, unreachable, or support partial profiles
- **PowerProfilesService**: new singleton bridging QML to power-profiles-daemon; handles dual D-Bus name registration
- **Time-to-Empty/Full**: seconds reported by UPower; converted to human-readable "3h 42m remaining" or "1h 5m to full" format
- **Capacity**: UPower battery health metric (0–100%); >0 indicates battery present and measurable
- **ChargeCycles**: UPower metric; >0 indicates cycle count is available from the device

---

## Functional Requirements

### Battery Data Extension

**REQ-F-001** [UBIQUITOUS]  
The system SHALL extend `BatteryService` to expose three additional properties:
- `timeRemaining` (int, seconds): extracted from UPower's TimeToEmpty or TimeToFull property
- `health` (int, 0–100): extracted from UPower's Capacity property
- `chargeCycles` (int): extracted from UPower's ChargeCycles property

**Acceptance Criterion:**  
Verify that `BatteryService` declares Q_PROPERTY for each metric, emits corresponding NOTIFY signals, and populates them in `batteryStateUpdateFromProperties` from UPower properties. GTest coverage confirms all three properties update when UPower PropertiesChanged fires.

---

**REQ-F-002** [UBIQUITOUS]  
The `BatteryStateUpdate` struct SHALL contain fields for `timeRemaining`, `health`, and `chargeCycles` to support serialization between C++ and QML.

**Acceptance Criterion:**  
Verify struct definition in BatteryService header, confirm struct is used in `batteryStateUpdateFromProperties`, and check QML binding shows all three properties accessible as `BatteryService.timeRemaining`, `BatteryService.health`, `BatteryService.chargeCycles`.

---

### Time-Remaining Format

**REQ-F-003** [EVENT-DRIVEN]  
When the user views the battery popup and `BatteryService.state` is "Discharging", the system SHALL display a time-remaining duration calculated from `timeRemaining` in the format "Xh Ym remaining", where X and Y are non-zero integers.

**Acceptance Criterion:**  
Manual test: set device to discharge with known time-remaining (e.g., via UPower mock); open popup; verify label shows "3h 42m remaining" (or equivalent) in correct format. If `timeRemaining` is 0 or unknown, no duration is shown (see REQ-F-004).

---

**REQ-F-004** [EVENT-DRIVEN]  
When the user views the battery popup and `BatteryService.state` is "Charging", the system SHALL display a time-to-full duration calculated from `timeRemaining` in the format "Xh Ym to full", where X and Y are non-zero integers.

**Acceptance Criterion:**  
Manual test: set device to charge with known time-to-full; open popup; verify label shows "1h 5m to full". If `timeRemaining` is 0 or unknown, see REQ-F-005.

---

**REQ-F-005** [STATE-DRIVEN]  
When `BatteryService.timeRemaining` is 0 or invalid, the system SHALL display only the state word (e.g., "Charging", "Discharging") without a duration suffix.

**Acceptance Criterion:**  
Manual test: set `timeRemaining` to 0; open popup; verify only "Charging" or "Discharging" appears, no "Xh Ym" suffix.

---

### Popup Content Layout

**REQ-F-006** [UBIQUITOUS]  
The battery popup SHALL display content in the following order from top to bottom:
1. Title label: "BATTERY" (uppercase, theme font, accent color)
2. Large battery percentage (e.g., "87%", theme font, large size)
3. State + duration line (e.g., "Charging, 1h 5m to full"; theme colors)
4. Health row (if Capacity > 0): label "HEALTH" + value as percentage
5. Cycles row (if ChargeCycles > 0): label "CYCLES" + value as integer
6. Power profiles row (if PowerProfilesService.available): three circular icon buttons

**Acceptance Criterion:**  
Visual test: open popup at 87% charge, charging state, capacity 95%, cycles 120, power-profiles-daemon available; verify layout shows all six elements in correct order, properly spaced, and all text is readable.

---

**REQ-F-007** [CONDITIONAL]  
The health row SHALL be hidden if `BatteryService.health` is 0 or less.

**Acceptance Criterion:**  
Manual test: inject health = 0 via UPower mock; open popup; verify health row does not appear.

---

**REQ-F-008** [CONDITIONAL]  
The cycles row SHALL be hidden if `BatteryService.chargeCycles` is 0 or less.

**Acceptance Criterion:**  
Manual test: inject chargeCycles = 0; open popup; verify cycles row does not appear.

---

**REQ-F-009** [CONDITIONAL]  
The power profiles button row SHALL be hidden if `PowerProfilesService.available` is false.

**Acceptance Criterion:**  
Manual test: run without power-profiles-daemon running; open popup; verify all three profile buttons are absent and the popup shrinks vertically.

---

### Power Profiles Service

**REQ-F-010** [UBIQUITOUS]  
A new C++ singleton `PowerProfilesService` SHALL expose the following as Q_PROPERTY:
- `available` (bool, READ-only, NOTIFY): true if power-profiles-daemon is reachable and functional
- `activeProfile` (QString, READ-only, NOTIFY): name of currently active profile (e.g., "power-saver", "balanced", "performance")
- `hasPerformance` (bool, READ-only, NOTIFY): true if the hardware supports performance profile
- `hasBalanced` (bool, READ-only, NOTIFY): true if the hardware supports balanced profile
- `hasPowerSaver` (bool, READ-only, NOTIFY): true if the hardware supports power-saver profile

**Acceptance Criterion:**  
GTest: create mock D-Bus service, instantiate PowerProfilesService, confirm all properties initialize correctly and emit NOTIFY signals when daemon state changes. QML binding test: verify all properties are accessible as `PowerProfilesService.available`, etc.

---

**REQ-F-011** [UBIQUITOUS]  
`PowerProfilesService` SHALL detect and attempt connection to power-profiles-daemon via two possible D-Bus service names at startup:
1. `org.freedesktop.UPower.PowerProfiles` (newer versions)
2. `net.hadess.PowerProfiles` (older versions)

The service SHALL prioritize the newer name, fall back to the older name if unavailable, and set `available = false` if neither is reachable.

**Acceptance Criterion:**  
GTest: mock older daemon (net.hadess) only; verify PowerProfilesService connects successfully and `available` = true. Repeat with newer name (org.freedesktop) only. Repeat with both unavailable; confirm `available` = false and no connection errors in logs.

---

**REQ-F-012** [EVENT-DRIVEN]  
`PowerProfilesService` SHALL listen to `org.freedesktop.DBus.Properties.PropertiesChanged` signals from power-profiles-daemon. When the ActiveProfile property changes, the service SHALL:
1. Update the local `activeProfile` property
2. Emit `activeProfileChanged()` signal
3. Recompute `hasPerformance`, `hasBalanced`, `hasPowerSaver` by parsing the daemon's Profiles array and emit corresponding NOTIFY signals

**Acceptance Criterion:**  
GTest: mock daemon; trigger PropertiesChanged signal changing ActiveProfile from "balanced" to "performance"; confirm activeProfileChanged() fires, `activeProfile` property is updated, and the correct has* flags reflect the updated Profiles list.

---

**REQ-F-013** [UBIQUITOUS]  
`PowerProfilesService` SHALL expose a Q_INVOKABLE `setProfile(const QString &profileName)` method that writes the daemon's writable `ActiveProfile` property through `org.freedesktop.DBus.Properties.Set`. The method SHALL NOT update the local `activeProfile` property optimistically; the UI SHALL wait for the PropertiesChanged signal from the daemon before reflecting the change.

**Acceptance Criterion:**  
GTest: call `setProfile("performance")`; verify the `ActiveProfile` D-Bus property write is made with the correct argument. Verify `activeProfile` property does not change until PropertiesChanged signal arrives from the mock daemon.

---

### Power Profile Buttons

**REQ-F-014** [UBIQUITOUS]  
The popup SHALL display a horizontal row of three circular icon buttons for Power Saver, Balanced, and Performance profiles (in that left-to-right order).

**Acceptance Criterion:**  
Visual test: open popup; verify three circular buttons are visible, left-to-right order is correct, and buttons are spaced evenly with at least 8px margin.

---

**REQ-F-015** [UBIQUITOUS]  
Each profile button SHALL display an icon sourced from the system icon theme (via `Image.source` with icon theme path or Qt icon engine) rather than bundled assets. Icon names follow freedesktop.org convention (e.g., "power-battery-low", "power-battery-medium", "power-battery-high").

**Acceptance Criterion:**  
Code review: verify Image.source points to system theme (e.g., `image://icon/power-battery-low` or via Qt icon engine), NOT to `qrc:/` or `file://` assets. Run on a Freedesktop-compliant system; confirm icons render correctly (not broken/missing).

---

**REQ-F-016** [STATE-DRIVEN]  
The button corresponding to the active profile (as reported by `PowerProfilesService.activeProfile`) SHALL display a glow effect and an accent-color fill. Inactive buttons SHALL display a dim outline and no fill.

**Acceptance Criterion:**  
Visual test: set active profile to "balanced" via daemon mock; open popup; verify balanced button shows glow + accent fill, power-saver and performance buttons show dim outline + no fill. Change active profile to "performance"; verify buttons update to reflect new active state without user clicking.

---

**REQ-F-017** [STATE-DRIVEN]  
When a profile button corresponds to an unavailable profile (as indicated by `hasPerformance`, `hasBalanced`, or `hasPowerSaver` = false), the button SHALL be visually disabled (opacity reduced, cursor no-drop, no click response).

**Acceptance Criterion:**  
Manual test: mock hardware that supports only balanced profile; open popup; verify power-saver and performance buttons appear dimmed/disabled (opacity ~0.5), clicks on them do not invoke setProfile, balanced button remains clickable.

---

**REQ-F-018** [UBIQUITOUS]  
Each button SHALL display a text caption (e.g., "Power Saver", "Balanced", "Performance") that appears ONLY on mouse hover. The caption SHALL be styled in theme font and color.

**Acceptance Criterion:**  
Visual test: open popup, move mouse away from buttons; verify captions are hidden. Hover over each button; verify caption appears below or beside icon, styled with theme font. Move mouse away; verify caption disappears.

---

**REQ-F-019** [EVENT-DRIVEN]  
When the user clicks an active (enabled) profile button, the system SHALL invoke `PowerProfilesService.setProfile(profileName)`. The UI SHALL NOT update the button state optimistically; it SHALL wait for the daemon's PropertiesChanged signal (see REQ-F-012) before showing the new active state.

**Acceptance Criterion:**  
Manual test: open popup with active profile "balanced"; click "performance" button; verify button does not change state immediately; wait for PropertiesChanged signal (mock delay ~100ms); verify button updates to show "performance" as active. Repeat with real daemon if available.

---

**REQ-F-020** [UNWANTED]  
The system SHALL NOT update the active button state in response to local button clicks if `setProfile` is not successfully invoked or if the daemon rejects the `ActiveProfile` property write.

**Acceptance Criterion:**  
GTest/Mock test: inject `ActiveProfile` property write failure (mock returns error); click button; verify activeProfile property does not change and no UI state update occurs. Button remains in its prior state.

---

### Popup Content Component & Registration

**REQ-F-021** [UBIQUITOUS]  
A new QML component `BatteryPopupContent.qml` (located in `src/qml/Popups/`) SHALL encapsulate the entire battery popup layout, styling, and interaction logic. The component SHALL be registered in the `StatusPopup` framework's `popupSources` mapping such that the popup is title-less (the component owns the entire panel).

**Acceptance Criterion:**  
Code review: verify BatteryPopupContent.qml exists, is imported in StatusPopup.qml, and mapped in popupSources with ID "battery". Verify `StatusPopupSurface.qml` does not render a separate title bar for the battery popup.

---

**REQ-F-022** [UBIQUITOUS]  
`StatusPopupSurface` SHALL register a fixed popup size for the battery popup ID ("battery") of approximately 300 pixels wide by 360 pixels tall. The component `BatteryPopupContent` SHALL fit within this boundary with appropriate internal padding.

**Acceptance Criterion:**  
Code review: verify `StatusPopupSurface::sizeForPopupId()` returns 300x360 for "battery". Visual test: open battery popup; measure on-screen dimensions; verify compact layout (narrower than default 480-wide).

---

**REQ-F-023** [UBIQUITOUS]  
The battery popup content SHALL be resizable only through changes to battery data or daemon state (e.g., hiding health/cycles/profile rows); the root size declaration SHALL NOT be responsive to window resize.

**Acceptance Criterion:**  
Visual test: open popup; observe fixed geometry (no resize handles). Change battery state or daemon availability; observe popup resizes only if rows are shown/hidden, not via window drag.

---

### Visual Style & Theming

**REQ-F-024** [CONSTRAINT]  
All colors, fonts, and visual tokens in `BatteryPopupContent.qml` and related power profile components SHALL be sourced from `HoloniightPalette` theme tokens. No hardcoded hex color values, RGB values, or color names (e.g., `"#FF0000"`, `Qt.rgba(...)`) SHALL appear in QML source.

**Acceptance Criterion:**  
Code review: grep BatteryPopupContent.qml and profile button component for `"#"` and `Qt.rgba` patterns; all matches should resolve to HoloniightPalette tokens. Run qmllint; confirm no unqualified color access.

---

**REQ-F-025** [CONSTRAINT]  
The glow effect on the active profile button SHALL use `QtQuick.Effects.MultiEffect` with `shadowEnabled: true` (NOT `Qt5Compat.GraphicalEffects.Glow`, which is unsupported on this Qt build). The glow color SHALL be derived from `HoloniightPalette` accent token.

**Acceptance Criterion:**  
Code review: verify MultiEffect import is `QtQuick.Effects`, glow effect uses `shadowEnabled: true`. Visual test: active profile button shows glow effect in accent color.

---

**REQ-F-026** [UBIQUITOUS]  
The title label "BATTERY" and metric values (percentage, health, cycles) SHALL use the theme's heading/title font family (via `ThemeService.font.family` or equivalent constant). Font size and weight SHALL match the HoloNight design reference in `assets/dont-commit/`.

**Acceptance Criterion:**  
Visual test: compare on-screen text to design assets in `assets/dont-commit/`. Font family, size, and weight match design mockups. No fallback fonts render.

---

### Constraints & Non-Functional Requirements

**REQ-C-001** [CONSTRAINT]  
The battery hover tooltip (separate surface, shown on mouse hover over the battery widget) SHALL remain unchanged by this feature. Only the click-popup is filled with content.

**Acceptance Criterion:**  
Regression test: hover over battery widget before and after; tooltip content and styling are identical.

---

**REQ-C-002** [CONSTRAINT]  
The implementation SHALL use C++23 standard and Qt6 APIs. QML components SHALL target QML module version compatible with Qt6.

**Acceptance Criterion:**  
Code review: verify C++23 syntax (e.g., `std::format`, concepts if used), Qt6 Q_PROPERTY/Q_OBJECT macros, and QML `import QtQuick 6` statements.

---

**REQ-C-003** [CONSTRAINT]  
`PowerProfilesService` SHALL follow the existing `DbusPropertyClient` pattern used by other D-Bus services in holonight-shell (e.g., `BatteryService`, `AudioService`). D-Bus introspection and PropertiesChanged monitoring SHALL be implemented via standard Qt D-Bus bindings.

**Acceptance Criterion:**  
Code review: verify PowerProfilesService structure mirrors AudioService/BatteryService (private DbusPropertyClient/QDBusConnection, Q_PROPERTY declarations, PropertiesChanged slot). No external D-Bus libraries; only Qt D-Bus.

---

**REQ-C-004** [CONSTRAINT]  
`BatteryService` extension (new properties) SHALL NOT alter existing public API or change the behavior of current properties. All new fields SHALL be additive.

**Acceptance Criterion:**  
Regression test: run existing BatteryService tests; all pass. New properties do not appear in existing test assertions.

---

**REQ-NF-001** [NON-FUNCTIONAL]  
The popup SHALL render without blocking the main UI thread. D-Bus queries (e.g., GetAllProperties for initial daemon state) SHALL be asynchronous or cached to prevent janky popups.

**Acceptance Criterion:**  
Performance test: open popup multiple times in quick succession; no frame drops or input lag observed. Monitor main thread CPU usage; stays <20%.

---

**REQ-NF-002** [NON-FUNCTIONAL]  
If `power-profiles-daemon` becomes unavailable after startup (e.g., service crash), `PowerProfilesService` SHALL detect the loss via D-Bus NameOwnerChanged signal and set `available = false`. The popup SHALL gracefully hide the profile button row and log the loss at warning level.

**Acceptance Criterion:**  
Manual test: start daemon, open popup (buttons visible); stop daemon (or simulate via mock); observe `available` becomes false, buttons disappear, no crashes or unhandled exceptions. Check logs for warning message.

---

**REQ-NF-003** [NON-FUNCTIONAL]  
All D-Bus operations (connection, property query, `ActiveProfile` write) SHALL include a timeout of at least 2 seconds to prevent UI hang if the daemon is slow or unresponsive.

**Acceptance Criterion:**  
GTest: mock slow daemon (delay `ActiveProfile` write by 3 seconds); verify no UI freeze (timeout fires before hang). Check D-Bus property write timeout configuration in code.

---

**REQ-NF-004** [NON-FUNCTIONAL]  
`BatteryService` property updates and `PowerProfilesService` availability changes SHALL be reflected in the UI within 500ms of the underlying D-Bus event.

**Acceptance Criterion:**  
Manual test (with mock/controlled delays): change battery state; measure time from D-Bus signal to UI update; confirm <500ms. Repeat for profile availability change.

---

## Detailed Interaction Model

### Initial Load

1. User clicks the battery widget in the topbar.
2. `StatusPopupSurface` shows the battery popup surface (layer-shell surface, positioned near topbar).
3. `BatteryPopupContent` loads; bindings pull current state from `BatteryService` and `PowerProfilesService`.
4. If `PowerProfilesService.available = false`, the profile button row does not render.
5. If `BatteryService.health = 0`, the health row does not render.
6. If `BatteryService.chargeCycles = 0`, the cycles row does not render.
7. Popup displays with visible rows populated from live properties.

### Power Profile Selection

1. User hovers over a profile button; caption appears.
2. If the button is disabled (profile not supported), cursor shows no-drop; click has no effect.
3. If the button is enabled, user clicks it.
4. `setProfile(profileName)` is invoked; button state does NOT change.
5. Daemon processes request; broadcasts PropertiesChanged.
6. `PowerProfilesService` receives signal, updates `activeProfile` and has* flags.
7. QML bindings update button visuals (active button now shows glow + fill, prior active button reverts to outline).

### Daemon Loss

1. Daemon crashes or is stopped.
2. `PowerProfilesService` detects NameOwnerChanged (name has no owner).
3. `available` property becomes false; availableChanged() signal fires.
4. QML Loader or Visibility binding for the profile button row triggers; row is unloaded.
5. Popup geometry may shrink.
6. Popup remains open; user can still see battery metrics.

### Battery Update

1. Underlying UPower device property changes (e.g., current charge, time-to-empty).
2. `BatteryService` receives PropertiesChanged.
3. `timeRemaining`, `health`, or `chargeCycles` properties update.
4. QML bindings trigger; labels and row visibility update.
5. Rows with 0 values remain hidden.
6. Popup may resize if a row is shown/hidden.

---

## Acceptance Verification Checklist

- [ ] `BatteryService.timeRemaining`, `.health`, `.chargeCycles` properties exist and are populated from UPower
- [ ] Time-remaining label shows "Xh Ym remaining" (discharging) or "Xh Ym to full" (charging)
- [ ] Health row hidden when `health <= 0`; cycles row hidden when `chargeCycles <= 0`
- [ ] `PowerProfilesService` detects both D-Bus names (org.freedesktop / net.hadess)
- [ ] `PowerProfilesService.available` reflects daemon availability (true/false)
- [ ] Active profile button shows glow + accent fill; inactive show dim outline
- [ ] Disabled profile buttons are unresponsive to clicks
- [ ] Profile button captions appear on hover, disappear on mouse-out
- [ ] Click setProfile; UI does NOT update until daemon PropertiesChanged arrives
- [ ] `BatteryPopupContent.qml` registered in `StatusPopup.popupSources` as "battery"
- [ ] Popup size 300x360 registered in `StatusPopupSurface.sizeForPopupId("battery")`
- [ ] No hardcoded colors; all tokens from `HoloniightPalette`
- [ ] Glow uses `QtQuick.Effects.MultiEffect` with `shadowEnabled: true`
- [ ] Hover tooltip on battery widget unchanged
- [ ] No main-thread blocking on D-Bus operations
- [ ] Daemon loss gracefully hides profile buttons, no crash

---

## References

- **Design Assets:** `assets/dont-commit/` — HoloNight color palette, typography, glow specifications
- **BatteryService:** `src/services/BatteryService.{h,cpp}` — existing implementation for extension
- **StatusPopup Framework:** `src/qml/Popups/StatusPopup.qml` — popup registration pattern
- **AudioService/D-Bus Pattern:** `src/services/AudioService.{h,cpp}` — reference for DbusPropertyClient style
- **QML Styling:** `src/qml/` — existing theme integration patterns
- **UPower D-Bus API:** https://upower.freedesktop.org/docs/Device.html
- **power-profiles-daemon D-Bus API:** https://gitlab.freedesktop.org/hadess/power-profiles-daemon/-/blob/main/README.md
