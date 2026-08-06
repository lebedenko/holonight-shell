# DESIGN — topbar-battery

## Overview

The `topbar-battery` feature adds a battery status widget to the right side of the holonight-shell top bar. A new C++ singleton, `BatteryService`, connects to UPower via the system D-Bus, reads the `Percentage`, `State`, and `IsPresent` device properties, and exposes them to QML. A new `BatteryWidget.qml` renders the visual battery shape (body, nub, fill bar, glow, charging bolt, percent text). A thin `BatterySection.qml` wraps the widget in the existing `BarSection` layout primitive, and `TopBar.qml` is updated to place the section to the left of the clock.

The design follows the exact same pattern as `ActiveWindowService`: a `QObject` + `Q_PROPERTY` + `QML_ELEMENT` + `QML_SINGLETON` C++ class wired into QML via `qmlRegisterSingletonType` in `main.cpp`, and a BarSection wrapper QML file.

---

## Components

### `BatteryService` (C++)

**Location:** `src/BatteryService.h`, `src/BatteryService.cpp`

A `QObject` subclass decorated with `QML_ELEMENT` and `QML_SINGLETON`. Exposes three read-only Q_PROPERTY values to QML:

| Property  | Type  | Description                                            |
|-----------|-------|--------------------------------------------------------|
| `percent` | `int` | Battery level 0–100, derived from UPower `Percentage` |
| `charging`| `bool`| True when UPower `State` is 1 (charging) or 4 (full)  |
| `present` | `bool`| Mirrors UPower `IsPresent`                             |

Private members follow the project naming convention (`percent_`, `charging_`, `present_`). Each property has a dedicated setter that guards against no-op writes before emitting the change signal.

**Initialization sequence** (constructor body):

1. Check `QDBusConnection::systemBus().isConnected()`. Log a warning and return early if false.
2. Call `GetDevices()` on `org.freedesktop.UPower` at path `/org/freedesktop/UPower`. This returns a list of `QDBusObjectPath` values.
3. Iterate the list and call `org.freedesktop.DBus.Properties.Get("org.freedesktop.UPower.Device", "Type")` on each path. The first path whose `Type` variant equals `2` (battery) is stored in `device_path_`.
4. If no battery path is found, leave `present_` as `false` and return. The widget will hide itself.
5. Call `readProperties()` to populate the initial `percent_`, `charging_`, and `present_` values synchronously.
6. Call `QDBusConnection::systemBus().connect(...)` to subscribe to `org.freedesktop.DBus.Properties` → `PropertiesChanged` on `device_path_`. The slot `onPropertiesChanged(QString, QVariantMap, QStringList)` handles incremental updates.

**`readProperties()`:**

Calls `org.freedesktop.DBus.Properties.GetAll("org.freedesktop.UPower.Device")` on `device_path_` and extracts `Percentage` (double), `State` (uint), and `IsPresent` (bool) from the returned `QVariantMap`. Delegates to the private setters.

**`onPropertiesChanged(QString interface, QVariantMap changed, QStringList invalidated)`:**

Checks that `interface == "org.freedesktop.UPower.Device"`. Reads `Percentage`, `State`, and `IsPresent` from `changed` if present; ignores `invalidated` (re-read is not necessary for these properties).

**Percent conversion:**

```cpp
int newPercent = qRound(percentage * 100.0);
```

**Charging logic:**

```cpp
bool newCharging = (state == 1 || state == 4);
```

**Class skeleton:**

```cpp
class BatteryService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int percent READ percent NOTIFY percentChanged)
  Q_PROPERTY(bool charging READ charging NOTIFY chargingChanged)
  Q_PROPERTY(bool present READ present NOTIFY presentChanged)

 public:
  explicit BatteryService(QObject* parent = nullptr);

  [[nodiscard]] int percent() const { return percent_; }
  [[nodiscard]] bool charging() const { return charging_; }
  [[nodiscard]] bool present() const { return present_; }

 Q_SIGNALS:
  void percentChanged();
  void chargingChanged();
  void presentChanged();

 private Q_SLOTS:
  void onPropertiesChanged(const QString& interface,
                           const QVariantMap& changed,
                           const QStringList& invalidated);

 private:
  void readProperties();
  void setPercent(int value);
  void setCharging(bool value);
  void setPresent(bool value);

  QString device_path_;
  int percent_{0};
  bool charging_{false};
  bool present_{false};
};
```

---

### `BatteryWidget.qml`

**Location:** `src/qml/Topbar/BatteryWidget.qml`

A self-contained visual component. Does not import `HolonightShell` directly — it receives `percent`, `charging`, and `present` as properties from `BatterySection.qml`, which binds them from the `BatteryService` singleton. This keeps the widget testable in isolation.

**Property surface:**

```qml
property int percent: 0
property bool charging: false
property bool present: true
```

**Computed helper:**

```qml
readonly property color levelColor: percent <= 20
    ? HoloniightPalette.error      // maps to #f7768e
    : HoloniightPalette.accentBlue // maps to #7dcfff
```

All color tokens come from `import Holonight` and `HoloniightPalette.<token>`. No hardcoded `#` hex values.

**Visibility:** The root `Item` has `visible: present`.

**Visual children (in z-order):**

| Element          | Type        | Size      | Position                           | Notes                                      |
|------------------|-------------|-----------|------------------------------------|--------------------------------------------|
| `body`           | `Rectangle` | 76 × 22 px| `x:0, y:7`                         | `radius:5`, border 1.5 px `levelColor`     |
| `nub`            | `Rectangle` | 6 × 8 px  | `x:79, y:14` (right of body)       | `radius:2`, `color: levelColor`            |
| `fill`           | `Rectangle` | variable × 12 px | `x:5, y:12` inside body    | `width: Math.max(4, 66 * percent / 100)`, `radius:3` |
| `glow`           | `Glow`      | fill body | `anchors.fill: body`               | `radius:12, samples:24, spread:0.22`       |
| `chargingIcon`   | `Text`      | —         | centered on body                   | `text:"⚡"`, `visible: charging`, 12 px JetBrains Mono |
| `percentText`    | `Text`      | —         | `x:90`, vertically centered        | `text: percent + "%"`, 13 px JetBrains Mono, `color: HoloniightPalette.onSurface` |

**Animation:**

```qml
Behavior on percent {
    NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
}
```

This is sufficient to animate the fill bar width change without any additional JavaScript, because `fill.width` is a direct binding expression on `percent`.

---

### `BatterySection.qml`

**Location:** `src/qml/Topbar/BatterySection.qml`

A thin BarSection wrapper, identical in structure to `StatusSection.qml` (clock):

```qml
import QtQuick
import Holonight
import HolonightShell

BarSection {
    BatteryWidget {
        anchors.verticalCenter: parent.verticalCenter
        percent: BatteryService.percent
        charging: BatteryService.charging
        present: BatteryService.present
    }
}
```

`BarSection` sizes itself via `implicitWidth: container.implicitWidth + 16`. Because `BatteryWidget` has a fixed outer width of 108 px, `BatterySection` will always measure 124 px wide when present. When `present` is false the widget is invisible but still occupies layout space — if that is undesirable, `BatterySection` can set `implicitWidth: BatteryService.present ? 124 : 0` to collapse entirely.

---

### Changes to `TopBar.qml`

Add `BatterySection` immediately before `StatusSection` in the right-hand `RowLayout`:

```qml
BatterySection {
    Layout.alignment: Qt.AlignVCenter
}

StatusSection {
    Layout.alignment: Qt.AlignVCenter
}
```

No other structural changes are required.

---

### Changes to `CMakeLists.txt`

**1. Add C++ sources** to `qt6_add_executable`:

```cmake
src/BatteryService.h
src/BatteryService.cpp
```

**2. Register QML resource aliases** before `qt6_add_qml_module`:

```cmake
set_source_files_properties(src/qml/Topbar/BatteryWidget.qml
    PROPERTIES QT_RESOURCE_ALIAS "Topbar/BatteryWidget.qml")
set_source_files_properties(src/qml/Topbar/BatterySection.qml
    PROPERTIES QT_RESOURCE_ALIAS "Topbar/BatterySection.qml")
```

**3. Add QML files** to `qt6_add_qml_module ... QML_FILES`:

```cmake
src/qml/Topbar/BatteryWidget.qml
src/qml/Topbar/BatterySection.qml
```

No new `target_link_libraries` entries are needed. `Qt6::DBus` is already linked.

---

### Changes to `main.cpp`

Include `BatteryService.h`, instantiate the service, and register it as a QML singleton using the same factory-lambda pattern as `ActiveWindowService`:

```cpp
#include "BatteryService.h"

// in main():
auto* battery = new BatteryService(&app);
QQmlEngine::setObjectOwnership(battery, QQmlEngine::CppOwnership);
qmlRegisterSingletonType<BatteryService>(
    "HolonightShell", 1, 0, "BatteryService",
    [battery](QQmlEngine*, QJSEngine*) -> QObject* { return battery; });
```

---

## Data Flow

```
UPower daemon (system D-Bus)
        │
        │  org.freedesktop.UPower.Device
        │  PropertiesChanged signal
        ▼
BatteryService::onPropertiesChanged()
        │  setPercent / setCharging / setPresent
        │  emit percentChanged / chargingChanged / presentChanged
        ▼
QML property bindings (BatterySection.qml)
        │  percent: BatteryService.percent
        │  charging: BatteryService.charging
        │  present: BatteryService.present
        ▼
BatteryWidget.qml
        │  fill.width  ← animated via Behavior on percent
        │  levelColor  ← percent <= 20 ? error : accentBlue
        │  chargingIcon.visible ← charging
        │  root.visible ← present
        ▼
Rendered frame on screen
```

**Startup sequence:**

1. `main()` constructs `BatteryService`. The constructor runs synchronously on the main thread.
2. `QDBusConnection::systemBus()` is checked for connectivity.
3. `GetDevices()` call enumerates UPower devices synchronously (blocking, but fast — milliseconds at most).
4. `readProperties()` performs one `GetAll` call to populate initial state.
5. `connect()` subscribes to `PropertiesChanged` — all future updates are asynchronous, event-driven.
6. `qmlRegisterSingletonType` registers the already-populated instance.
7. QML engine starts; `BatterySection` binds to `BatteryService` properties that already hold correct initial values. No initial-state flash occurs.

---

## UPower D-Bus API

| Item                  | Value                                                                 |
|-----------------------|-----------------------------------------------------------------------|
| Bus                   | System bus (`QDBusConnection::systemBus()`)                           |
| Service               | `org.freedesktop.UPower`                                              |
| Manager path          | `/org/freedesktop/UPower`                                             |
| Manager interface     | `org.freedesktop.UPower`                                              |
| Device interface      | `org.freedesktop.UPower.Device`                                       |
| Enumerate call        | `GetDevices()` → `ao` (array of object paths)                         |
| Filter criterion      | `Type` property == `2` (battery; `1` = line power, `3` = UPS, …)     |
| `Percentage`          | `d` (double, 0.0–1.0); multiply × 100 and round for display          |
| `State`               | `u` (uint): `0`=unknown, `1`=charging, `2`=discharging, `4`=full     |
| `IsPresent`           | `b` (bool)                                                            |
| Change signal path    | Device object path (e.g. `/org/freedesktop/UPower/devices/battery_BAT0`) |
| Change signal service | `org.freedesktop.UPower`                                              |
| Change signal iface   | `org.freedesktop.DBus.Properties`                                     |
| Change signal name    | `PropertiesChanged`                                                   |
| Signal signature      | `sa{sv}as` (interface name, changed map, invalidated list)            |

**`QDBusConnection::connect` call:**

```cpp
QDBusConnection::systemBus().connect(
    QStringLiteral("org.freedesktop.UPower"),
    device_path_,
    QStringLiteral("org.freedesktop.DBus.Properties"),
    QStringLiteral("PropertiesChanged"),
    this,
    SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
```

---

## Key Decisions

### `QDBusInterface` + `QDBusConnection::connect()` over a generated D-Bus adaptor

The feature needs exactly three read-only properties and one inbound signal. Generating a full D-Bus adaptor from XML (`qdbusxml2cpp`) would introduce a generated header/source pair, a build step, and a file that must not be edited manually — the same overhead that Wayland protocol generation adds. For a read-only 3-property service that queries one interface once at startup and then listens for events, the `QDBusInterface`/`QDBusConnection` API is sufficient and keeps the build graph simpler. The `ActiveWindowService` precedent shows that direct IPC wiring without code generation is the preferred style in this codebase.

### `qRound(percentage * 100)` for integer conversion

UPower reports `Percentage` as a `double` in the range `[0.0, 1.0]`. Multiplying by 100 and rounding with `qRound` correctly maps `0.996` to `100` and `0.004` to `0`, matching the SPEC acceptance criteria. Truncation via `static_cast<int>` would cause `0.996 → 99`, which would show 99% on a full battery.

### `State == 1 || State == 4` for `charging`

UPower state `1` means "actively charging" and state `4` means "fully charged (but still plugged into AC)". A user expects the ⚡ indicator to remain visible when the laptop is plugged in regardless of whether charge is flowing, so both states map to `charging = true`. State `2` (discharging) and `0` (unknown) map to `false`.

### Color mapping via `HoloniightPalette` tokens instead of hardcoded hex

The SPEC (REQ-NF-003) prohibits hardcoded `#` hex values. The `BatteryWidget` reference in `assets/dont-commit/` uses raw hex for demonstration purposes, but the production widget must translate those to palette tokens. `HoloniightPalette.error` maps to the red `#f7768e` and `HoloniightPalette.accentBlue` maps to `#7dcfff`. This keeps the widget automatically consistent with future HoloNight theme revisions.

### `Behavior on percent` animates the fill without extra JS

`fill.width` is bound as `Math.max(4, 66 * percent / 100)`. Qt's property animation system tracks `percent` as the animated property; when `percent` changes, the `NumberAnimation` interpolates the value over 250 ms with `Easing.OutCubic`, and the `fill.width` expression re-evaluates on every animation tick. No JavaScript timer or explicit `PropertyAnimation` on `fill.width` is needed.

---

## Alternatives Considered

### `/sys/class/power_supply` polling

Reading battery state from sysfs would avoid any D-Bus dependency, but it requires a polling timer. UPower sends `PropertiesChanged` within milliseconds of a hardware event — polled sysfs would either waste CPU (short interval) or lag (long interval). Polling is also inconsistent with the reactive, event-driven style established by `ActiveWindowService` and the workspace protocol. Rejected in favor of UPower (REQ-C-001).

### Generated D-Bus XML adaptor (`qdbusxml2cpp`)

`qdbusxml2cpp -p` generates a typed proxy class from `org.freedesktop.UPower.Device`'s introspection XML. This would provide compile-time type safety for property names and signal signatures. However, the generated files must not be edited manually, the build step adds complexity, and the benefit is marginal for a service that reads three properties and handles one signal. The `QDBusInterface` + `QVariantMap` path is explicit, readable, and consistent with the no-code-generation preference already demonstrated in the codebase. Rejected in favor of direct `QDBusInterface` calls.

### SVG bolt icon vs Unicode ⚡

A vector icon (SVG or Qt's `Image` with an icon source) would be pixel-perfect at any DPI but requires an asset file to be shipped and registered in the QRC. The Unicode lightning bolt character `⚡` (U+26A1) renders as a plain text glyph in JetBrains Mono without emoji substitution on most Linux desktop configurations, which is the intended appearance. Using a `Text` element keeps the implementation entirely in QML with no asset pipeline. If a future theme revision requires a custom icon, the `Text` element can be replaced without changing the surrounding layout. Unicode chosen.

---

## Known Risks

### UPower not running

On minimal Arch installs or embedded systems, `upowerd` may not be running. The guard `QDBusConnection::systemBus().isConnected()` detects a missing D-Bus daemon, but a connected D-Bus with an absent UPower service will cause the `GetDevices()` call to return a D-Bus error rather than a device list. The service must check the `QDBusReply::isValid()` result from `GetDevices()` and log a warning before returning early with `present_ = false`. The widget will hide itself. No crash will occur.

### Infrequent `Percentage` updates on some hardware

Some firmware and UPower backends only emit `PropertiesChanged` for `Percentage` at coarse 1% granularity or with multi-second debounce. This is hardware-dependent and acceptable behavior — the widget is informational and does not need sub-second precision. No polling fallback is warranted.

### `⚡` Unicode rendering varies by font

JetBrains Mono renders U+26A1 as a monochrome text glyph, not an emoji, which is the intended appearance (it inherits the `color` property). On systems where a color emoji font (Noto Color Emoji, Twemoji) has higher priority in the font fallback stack, the system may substitute a color glyph that ignores the QML `color` property. If this occurs in practice the fix is to set `font.family: "JetBrains Mono"` explicitly on the `chargingIcon` Text element (which the spec already requires) and verify that Qt's font matching resolves to the monochrome face.
