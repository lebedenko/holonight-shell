# SPEC — topbar-battery

## Overview

The `topbar-battery` feature displays the current battery status in the holonight-shell top bar, sourced from UPower D-Bus. The widget shows the battery percentage, fill level, and charging state on laptops; it is automatically hidden on devices without a battery. All colors are sourced from the HoloniightPalette theme, and the implementation follows the established layer-shell QML architecture.

---

## Requirements

### Functional

#### REQ-F-001: BatteryService shall enumerate UPower devices
**Requirement:** The `BatteryService` C++ singleton shall connect to the UPower D-Bus service and enumerate all devices to find the first battery device (type == 2).

**Acceptance Criterion:** 
- On a laptop, the service successfully identifies and initializes a battery device from UPower
- On a desktop with no battery, the service records zero battery devices without error

---

#### REQ-F-002: BatteryService shall expose battery percent to QML
**Requirement:** The `BatteryService` shall expose an integer `percent` property (0–100) derived from the UPower device's `Percentage` double (0.0–100.0), rounded to the nearest integer.

**Acceptance Criterion:**
- A battery at UPower percentage 0.75 exposes `percent` = 75
- A battery at UPower percentage 0.004 exposes `percent` = 0
- A battery at UPower percentage 0.996 exposes `percent` = 100

---

#### REQ-F-003: BatteryService shall expose charging state to QML
**Requirement:** The `BatteryService` shall expose a boolean `charging` property that is true when the UPower device `State` property equals 1 (charging) or 4 (fully charged and plugged in), and false otherwise.

**Acceptance Criterion:**
- When the device is plugged into AC and charging, `charging` is true
- When the device is unplugged and discharging, `charging` is false
- When the device is plugged in and fully charged, `charging` is true

---

#### REQ-F-004: BatteryService shall expose battery presence to QML
**Requirement:** The `BatteryService` shall expose a boolean `present` property that reflects the UPower device's `IsPresent` property.

**Acceptance Criterion:**
- On a laptop with a battery, `present` is true
- On a desktop with no battery, `present` is false
- If a device is removed or becomes unavailable, `present` changes to false

---

#### REQ-F-005: BatteryService shall watch for D-Bus property changes
**Requirement:** When a UPower device's properties change, the `BatteryService` shall listen to the `org.freedesktop.DBus.Properties.PropertiesChanged` signal and emit QML-compatible change notifications.

**Acceptance Criterion:**
- A QML binding to `BatteryService.percent` updates within 100 ms of a UPower D-Bus change
- A QML binding to `BatteryService.charging` updates within 100 ms of a UPower state change
- The application does not crash when D-Bus signals arrive

---

#### REQ-F-006: BatteryWidget shall display battery body rectangle
**Requirement:** The `BatteryWidget.qml` component shall render a battery body as a `Rectangle` with dimensions 76×22 px, corner radius 5, fill color from the theme's background, and a 1.5 px border in the level color.

**Acceptance Criterion:**
- The battery body is exactly 76 px wide and 22 px tall
- The body corners are visibly rounded
- The border is visible and matches the level color

---

#### REQ-F-007: BatteryWidget shall display battery cap nub
**Requirement:** The `BatteryWidget.qml` component shall render a battery cap (terminal nub) as a `Rectangle` positioned immediately to the right of the body, with dimensions 6×8 px, corner radius 2, and color matching the level color.

**Acceptance Criterion:**
- The nub is positioned directly adjacent to the right edge of the body
- The nub is 6 px wide and 8 px tall
- The nub color matches the level color (red or cyan)

---

#### REQ-F-008: BatteryWidget shall display fill bar
**Requirement:** The `BatteryWidget.qml` component shall render a fill bar inside the body as a `Rectangle` with height 12 px, corner radius 3, color matching the level color, and width proportional to the battery percent: `max(4, 66 * percent / 100)`.

**Acceptance Criterion:**
- At 0%, the fill bar is 4 px wide
- At 50%, the fill bar is approximately 33 px wide (66 × 0.5)
- At 100%, the fill bar is 66 px wide
- The fill bar is centered vertically within the body

---

#### REQ-F-009: BatteryWidget shall color the fill based on battery level
**Requirement:** The level color shall be red (`#f7768e`) when percent ≤ 20, and cyan (`#7dcfff`) when percent > 20.

**Acceptance Criterion:**
- At 20% or below, the fill bar and borders are red
- At 21% or above, the fill bar and borders are cyan
- A visual inspection at 20% and 21% shows the color transition

---

#### REQ-F-010: BatteryWidget shall apply glow effect
**Requirement:** The `BatteryWidget.qml` component shall apply a `Qt5Compat.GraphicalEffects.Glow` effect anchored to the battery body with radius 12, samples 24, and spread 0.22.

**Acceptance Criterion:**
- A soft glow extends visibly around the battery body
- The glow is subtle (not overwhelming) and matches the level color

---

#### REQ-F-011: BatteryWidget shall display charging indicator
**Requirement:** When `charging` is true, the `BatteryWidget.qml` component shall display a Unicode lightning bolt character (⚡) centered on the battery body in JetBrains Mono 12 px font, with color matching the level color. When `charging` is false, the lightning bolt shall be hidden.

**Acceptance Criterion:**
- A ⚡ symbol appears on the battery when charging and disappears when unplugged
- The symbol is centered horizontally and vertically on the body
- The symbol color matches the current level color (red at ≤20%, cyan at >20%)

---

#### REQ-F-012: BatteryWidget shall display percent text
**Requirement:** The `BatteryWidget.qml` component shall display the battery percent as text (e.g., "75%") to the right of the battery body in JetBrains Mono 13 px font, with color `#c0caf5`.

**Acceptance Criterion:**
- The text shows "0%" through "100%"
- The text is positioned immediately to the right of the battery nub
- The font is JetBrains Mono 13 px
- The text color is `#c0caf5` (light gray)

---

#### REQ-F-013: BatteryWidget shall hide when battery not present
**Requirement:** When `BatteryService.present` is false, the entire `BatteryWidget` shall be hidden (visible: false).

**Acceptance Criterion:**
- On a desktop machine, the battery widget is not visible in the top bar
- When a battery device becomes unavailable, the widget disappears
- When a battery is re-detected (e.g., on reboot with a new battery), the widget reappears

---

#### REQ-F-014: BatteryWidget shall animate percent changes
**Requirement:** The `percent` property shall use a `Behavior` with a `NumberAnimation` that animates over 250 ms with `Easing.OutCubic`.

**Acceptance Criterion:**
- When battery percent changes (e.g., 50% → 75%), the fill bar smoothly animates
- The animation duration is 250 ms
- The animation uses OutCubic easing (accelerates at the start, decelerates at the end)

---

#### REQ-F-015: BatterySection shall wrap the widget in a BarSection
**Requirement:** A `BatterySection.qml` component shall wrap the `BatteryWidget` in a `BarSection` to integrate with the top bar layout.

**Acceptance Criterion:**
- The battery widget is displayed inside a BarSection
- The BarSection sizing is consistent with other top bar sections (e.g., ClockSection)

---

#### REQ-F-016: TopBar shall include BatterySection
**Requirement:** The `TopBar.qml` component shall include the `BatterySection` in the right-side `BarSection` row, positioned before the clock.

**Acceptance Criterion:**
- The battery widget appears in the top bar's right section
- The battery widget is positioned to the left of the clock
- The layout does not introduce gaps or misalignment

---

### Non-Functional

#### REQ-NF-001: BatteryService shall initialize asynchronously
**Requirement:** The `BatteryService` D-Bus initialization shall not block the main Qt event loop or delay application startup.

**Acceptance Criterion:**
- The application launches and displays the top bar in < 500 ms even if D-Bus is slow
- QML signals connected to the service reflect updates after initial boot without stalling

---

#### REQ-NF-002: BatteryService shall handle D-Bus connection loss gracefully
**Requirement:** If the D-Bus connection is lost, the `BatteryService` shall log the error and maintain the last known state without crashing.

**Acceptance Criterion:**
- Restarting dbus does not crash holonight-shell
- The battery widget shows the last known percent and charging state during a D-Bus outage
- Connection is re-established and updates resume when D-Bus is available again

---

#### REQ-NF-003: All QML colors shall use HoloniightPalette
**Requirement:** The `BatteryWidget.qml` component shall not contain hardcoded hex color values; all colors shall be sourced from `HoloniightPalette` tokens.

**Acceptance Criterion:**
- A code review of BatteryWidget.qml shows zero hardcoded `#` hex values
- All colors import via `import Holonight` and access `HoloniightPalette.<token>`
- The widget theme updates when the system HoloNight palette changes

---

#### REQ-NF-004: Code shall pass all linting and formatting checks
**Requirement:** All C++ and QML code for the battery feature shall pass `task format-check`, `task qml-lint`, and `task tidy` without errors or warnings.

**Acceptance Criterion:**
- `task build` completes without compiler warnings
- `task qml-lint` reports zero warnings for BatteryWidget.qml and BatterySection.qml
- `task format-check` reports the code is correctly formatted

---

### Constraints

#### REQ-C-001: BatteryService shall use UPower D-Bus API
**Requirement:** The battery data shall be sourced exclusively from the UPower D-Bus service (`org.freedesktop.UPower`); no alternative battery APIs (e.g., /sys/class/power_supply, ACPI) shall be used.

**Acceptance Criterion:**
- UPower is the only D-Bus service queried for battery information
- The implementation uses the standard UPower interface path `/org/freedesktop/UPower/devices/*`

---

#### REQ-C-002: BatteryService shall register as a QML singleton
**Requirement:** The `BatteryService` shall be exposed to QML via `qmlRegisterSingletonInstance<BatteryService>()` in the C++ main function.

**Acceptance Criterion:**
- QML components can access the service via `import HolonightShell` and reference the singleton directly
- Only one instance of BatteryService exists throughout the application lifetime

---

#### REQ-C-003: BatteryWidget shall be placed in src/qml/Topbar/
**Requirement:** The `BatteryWidget.qml` and `BatterySection.qml` files shall be located in the `src/qml/Topbar/` directory following the established per-directory QML layout.

**Acceptance Criterion:**
- Files exist at `/home/andrii/projects/holonight-shell/src/qml/Topbar/BatteryWidget.qml` and `BatterySection.qml`
- Both files are registered in CMakeLists.txt with `QT_RESOURCE_ALIAS` to strip the `src/qml/` prefix

---

#### REQ-C-004: QRC paths shall use /HolonightShell/ prefix
**Requirement:** All QML resource references shall use the `qrc:/HolonightShell/` prefix (e.g., `qrc:/HolonightShell/Topbar/BatteryWidget.qml`).

**Acceptance Criterion:**
- The QML module URI in CMakeLists.txt is `HolonightShell`
- Runtime QML loading uses `qrc:/HolonightShell/` paths

---

#### REQ-C-005: Battery feature shall follow existing layer-shell architecture
**Requirement:** The `BatteryService` implementation shall follow the `QWaylandClientExtensionTemplate<T>` pattern established for other D-Bus services (e.g., `ActiveWindowService`), and the QML widget shall follow the `BarSection` composition pattern used by other top bar widgets.

**Acceptance Criterion:**
- The C++ class structure mirrors `ActiveWindowService` (QObject + Q_PROPERTY + Q_INVOKABLE)
- The QML uses the same BarSection wrapper as ClockSection and WorkspaceSection
- No new architectural patterns are introduced

---

#### REQ-C-006: No multi-battery or time-remaining support
**Requirement:** The feature shall support only the first battery device found by UPower; no interface shall display multiple batteries, remaining time, or estimated runtime.

**Acceptance Criterion:**
- On a system with multiple batteries, only the first is displayed
- The widget shows only percentage and charging state; time-remaining is not shown
- The API does not expose multi-battery interfaces

---

#### REQ-C-007: Widget shall be display-only
**Requirement:** The battery widget shall not respond to user clicks, double-clicks, or any interactive input; it shall be purely informational.

**Acceptance Criterion:**
- Clicking on the battery widget has no effect
- No context menu or tooltip is displayed
- The widget accepts no focus or keyboard input

---

## Acceptance Criteria Summary

**Visual Acceptance (manual testing on a laptop):**
- [ ] Battery body is visible with correct dimensions and color
- [ ] Fill bar width changes as battery drains/charges
- [ ] Percent text updates and matches fill level
- [ ] ⚡ symbol appears when charging, disappears when unplugged
- [ ] Color turns red at ≤20%, cyan at >20%
- [ ] Glow effect is visible and subtle
- [ ] Smooth animation when percent changes

**Functional Acceptance (automated testing):**
- [ ] `task build` succeeds without warnings
- [ ] `task qml-lint` reports zero errors in BatteryWidget.qml and BatterySection.qml
- [ ] `task format-check` passes
- [ ] D-Bus property changes update the widget within 100 ms
- [ ] Widget is hidden on desktop machines (no battery)

**Integration Acceptance:**
- [ ] BatterySection is included in TopBar.qml
- [ ] Battery widget appears to the left of the clock in the top bar
- [ ] No layout breaks or visual glitches when the battery widget is present
- [ ] Application starts and displays the bar in < 500 ms

