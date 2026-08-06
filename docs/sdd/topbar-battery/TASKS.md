# SDD Tasks — topbar-battery

- [x] T-001: Add `BatteryService` header with property declarations and private members
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-NF-001, REQ-NF-002, REQ-C-002, REQ-C-005
  - Check: `src/BatteryService.h` exists with Q_OBJECT, QML_ELEMENT, QML_SINGLETON decorators, three Q_PROPERTY declarations for percent/charging/present with READ accessors and NOTIFY signals, and a private slot `onPropertiesChanged(QString, QVariantMap, QStringList)` matching the class skeleton from the DESIGN.

- [x] T-002: Implement `BatteryService` constructor with D-Bus initialization and device discovery
  - REQs: REQ-F-001, REQ-F-005, REQ-NF-001, REQ-NF-002, REQ-C-001
  - Check: Constructor checks `QDBusConnection::systemBus().isConnected()`, calls `GetDevices()` on `org.freedesktop.UPower`, filters for device `Type == 2`, stores the path in `device_path_`, calls `readProperties()` to populate initial values, and subscribes to `PropertiesChanged` via `QDBusConnection::systemBus().connect()` without blocking the main loop.

- [x] T-003: Implement `BatteryService::onPropertiesChanged()` slot and helper setters
  - REQs: REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005
  - Check: `onPropertiesChanged()` checks interface name equals `"org.freedesktop.UPower.Device"`, extracts Percentage/State/IsPresent from the `changed` map, and calls `setPercent()`, `setCharging()`, `setPresent()` as needed; each setter guards against no-op writes and emits the corresponding signal.

- [x] T-004: Implement `BatteryService::readProperties()` and percent/charging conversion logic
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004
  - Check: `readProperties()` calls `org.freedesktop.DBus.Properties.GetAll()` on `device_path_`, extracts `Percentage` (double), `State` (uint), and `IsPresent` (bool), converts percent via `qRound(percentage * 100.0)`, maps charging via `state == 1 || state == 4`, and delegates to setters.

- [x] T-005: Wire `BatteryService` into CMakeLists.txt and include in main.cpp
  - REQs: REQ-C-002, REQ-C-005
  - Check: `src/BatteryService.h` and `src/BatteryService.cpp` are listed in the `qt6_add_executable` sources block in CMakeLists.txt, `#include "BatteryService.h"` is added to main.cpp, and the service is instantiated and registered via `qmlRegisterSingletonType<BatteryService>()` using the factory-lambda pattern from ActiveWindowService.

- [x] T-006: Create `BatteryWidget.qml` with visual battery shape and animations
  - REQs: REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014, REQ-NF-003
  - Check: File exists at `src/qml/Topbar/BatteryWidget.qml` with a root Item, `percent`/`charging`/`present` properties, `levelColor` computed property, body Rectangle (76×22 px, radius 5, border 1.5 px), nub Rectangle (6×8 px, radius 2, x:79), fill bar (width: max(4, 66*percent/100), radius 3, x:5, y:12), Glow effect (radius 12, samples 24, spread 0.22), charging ⚡ Text (12 px JetBrains Mono, centered, visible: charging), percent Text (13 px JetBrains Mono, x:90), Behavior on percent with 250ms OutCubic animation, and all colors sourced from HoloniightPalette with zero hardcoded hex values.

- [x] T-007: Create `BatterySection.qml` as a BarSection wrapper
  - REQs: REQ-F-015, REQ-C-005
  - Check: File exists at `src/qml/Topbar/BatterySection.qml`, imports `HolonightShell` and `Holonight`, contains a BarSection with BatteryWidget child anchored vertically centered, binds `percent: BatteryService.percent`, `charging: BatteryService.charging`, `present: BatteryService.present`, and optionally sets `implicitWidth: 0` when `!BatteryService.present` for layout collapse.

- [x] T-008: Register QML resource aliases in CMakeLists.txt for BatteryWidget and BatterySection
  - REQs: REQ-C-003, REQ-C-004
  - Check: `set_source_files_properties()` blocks exist for `src/qml/Topbar/BatteryWidget.qml` and `src/qml/Topbar/BatterySection.qml` with `QT_RESOURCE_ALIAS "Topbar/BatteryWidget.qml"` and `"Topbar/BatterySection.qml"` respectively, and both files are listed in the `qt6_add_qml_module ... QML_FILES` section.

- [x] T-009: Integrate `BatterySection` into `TopBar.qml`
  - REQs: REQ-F-016
  - Check: `BatterySection` element is added to the right-side RowLayout in TopBar.qml immediately before StatusSection (clock), with `Layout.alignment: Qt.AlignVCenter`, and the visual result shows the battery widget to the left of the clock in the rendered top bar.

- [x] T-010: Build, lint, and format verification
  - REQs: REQ-NF-004, REQ-NF-003
  - Check: `task build` completes without compiler warnings, `task qml-lint` reports zero errors for BatteryWidget.qml and BatterySection.qml, `task format-check` reports correctly formatted code, and no hardcoded `#` hex color values appear anywhere in the implementation.
