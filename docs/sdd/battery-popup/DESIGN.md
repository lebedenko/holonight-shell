# Battery Popup Content + Power Profile Selector — Design Document

**Project:** holonight-shell (C++23/Qt6/QML Wayland shell)
**Spec:** `docs/sdd/battery-popup/SPEC.md`
**Date:** 2026-06-02

---

## 1. Overview

This document describes the complete implementation design for the battery click-popup panel. The feature has three concerns:

1. **BatteryService extension** — add `timeRemaining`, `health`, `chargeCycles` properties fed from UPower.
2. **PowerProfilesService** — new C++ singleton connecting to power-profiles-daemon via D-Bus (dual name detection).
3. **BatteryPopupContent.qml** — new QML component wired into the existing `StatusPopup` framework, with an optional `ProfileButton` sub-component.

---

## 2. Spec Corrections vs. Source Reality

### 2.1 QML file location

The spec says `src/qml/Popups/`. That directory does not exist in the codebase. The correct
pattern, based on `AudioPopupContent.qml` and `NetworkPopupContent.qml`, is a **dedicated
subdirectory** under `src/qml/` named after the popup:

```
src/qml/BatteryPopup/BatteryPopupContent.qml
src/qml/BatteryPopup/ProfileButton.qml       (sub-component)
```

QRC paths (derived automatically from `QT_RESOURCE_ALIAS`):

```
qrc:/HolonightShell/BatteryPopup/BatteryPopupContent.qml
qrc:/HolonightShell/BatteryPopup/ProfileButton.qml
```

`StatusPopup.qml` references the popup by full QRC path in its `popupSources` map (same as
`"audio"` and `"network"`):

```qml
readonly property var popupSources: ({
    "weather":  "WeatherPopupContent.qml",
    "audio":    "qrc:/HolonightShell/AudioPopup/AudioPopupContent.qml",
    "network":  "qrc:/HolonightShell/NetworkPopup/NetworkPopupContent.qml",
    "battery":  "qrc:/HolonightShell/BatteryPopup/BatteryPopupContent.qml"  // ADD
})
```

Note: `WeatherPopupContent.qml` lives directly inside `src/qml/Topbar/` because weather was
developed before the per-directory pattern solidified. Do not follow that precedent — use the
`BatteryPopup/` subdirectory.

### 2.2 Title-less vs. title-shown

`StatusPopup.qml` decides `showTitle` based on whether `contentSource` is non-empty:

```qml
readonly property bool showTitle: root.contentSource.length === 0
```

Because `"battery"` will have a `contentSource`, `showTitle` will be `false` — the popup
owns its full panel area. The spec's "BATTERY" title label in item 1 of the layout is
therefore rendered **inside** `BatteryPopupContent.qml` itself, not by `StatusPopup.qml`'s
`titleText`. This matches how `WeatherPopupContent.qml` and `AudioPopupContent.qml` work.

### 2.3 REQ-F-021 source path correction

The spec says `src/qml/Popups/BatteryPopupContent.qml`. Correct path is
`src/qml/BatteryPopup/BatteryPopupContent.qml`.

### 2.4 Icon names for REQ-F-015

The spec guesses `power-battery-low` etc. Those are **battery charge level** icons, not power
profile icons. The correct freedesktop.org icon names for power profiles are:

| Profile      | Recommended icon name              |
|--------------|------------------------------------|
| power-saver  | `power-profile-power-saver-symbolic` or `battery-caution-symbolic` |
| balanced     | `power-profile-balanced-symbolic` or `battery-good-symbolic` |
| performance  | `power-profile-performance-symbolic` or `battery-full-charging-symbolic` |

The `power-profile-*-symbolic` names are defined by GNOME/KDE for power-profiles-daemon
integration and are present in most modern icon themes (e.g. Papirus, Adwaita >= 42). However,
they may be absent in minimal or older themes. **Verification is required on the target system
before finalising icon names.** See Section 7.3 for the fallback strategy.

The loading mechanism uses the project's existing `image://icon/` provider
(`src/surfaces/IconImageProvider.h`), which calls `QIcon::fromTheme(iconName)` and renders to
a pixmap. The `StatusPopupSurface` already registers this provider on the popup view's engine
(`view_->engine()->addImageProvider("icon", new IconImageProvider())`), so no additional
registration is needed.

Usage in QML:

```qml
Image {
    source: "image://icon/power-profile-balanced-symbolic"
    sourceSize: Qt.size(24, 24)
}
```

---

## 3. Architecture

### 3.1 Component Map

```
ShellApplication
├── BatteryService           (existing, extended)
├── PowerProfilesService     (NEW)
└── StatusPopupSurface       (existing, extended: sizeForPopupId "battery")

src/qml/
├── Topbar/StatusPopup.qml   (edit: add "battery" to popupSources)
└── BatteryPopup/            (NEW directory)
    ├── BatteryPopupContent.qml
    └── ProfileButton.qml

src/core/BatteryState.{h,cpp}  (edit: extend BatteryStateUpdate struct + parsing)
src/services/BatteryService.{h,cpp}  (edit: add 3 properties + setters)
src/services/PowerProfilesService.{h,cpp}  (NEW)
src/app/ShellApplication.{h,cpp}  (edit: construct + register + start PowerProfilesService)
```

### 3.2 Layer Ownership

`holonight_services` static library owns all service singletons. `PowerProfilesService` lives
there alongside `BatteryService`. `BatteryPopupContent.qml` and `ProfileButton.qml` belong to
the QML module (`qt6_add_qml_module`) — no separate library.

---

## 4. BatteryService Extension

### 4.1 BatteryStateUpdate struct additions (src/core/BatteryState.h)

```cpp
struct BatteryStateUpdate {
    std::optional<int>  percent;
    std::optional<bool> charging;
    std::optional<bool> discharging;
    std::optional<bool> fully_charged;
    std::optional<bool> present;
    // NEW
    std::optional<int>  time_remaining;   // seconds; TimeToEmpty or TimeToFull
    std::optional<int>  health;           // 0–100; from UPower "Capacity" (double → round)
    std::optional<int>  charge_cycles;    // raw int32; from UPower "ChargeCycles"
};
```

UPower property names and types (verified against UPower D-Bus spec):

| Field           | UPower property name | D-Bus type | Notes                                     |
|-----------------|---------------------|------------|-------------------------------------------|
| `time_remaining`| `TimeToEmpty`       | `t` (uint64, seconds) | Used when discharging |
| `time_remaining`| `TimeToFull`        | `t` (uint64, seconds) | Used when charging; prefer `TimeToFull` when `State == Charging` |
| `health`        | `Capacity`          | `d` (double, 0–100)   | `qRound()` to int |
| `charge_cycles` | `ChargeCycles`      | `i` (int32)           | May be -1 when unknown; treat ≤0 as unavailable |

**Time-remaining parsing strategy in `batteryStateUpdateFromProperties`:** Extract both
`TimeToEmpty` and `TimeToFull`; let `BatteryService::applyStateUpdate` pick which to surface
based on the current state. Alternatively, set `time_remaining` from `TimeToEmpty` when
`State == Discharging`/`PendingDischarge`, and from `TimeToFull` when `State == Charging`.
The second approach avoids extra logic in the service and is preferred: parse both in
`batteryStateUpdateFromProperties`, but only populate `update.time_remaining` from the
relevant one based on the `State` field when both are present in the same properties map. When
`State` is not present in the changed map (incremental update), leave `time_remaining` as
`std::nullopt` so the existing value is preserved.

### 4.2 BatteryService new Q_PROPERTYs (src/services/BatteryService.h)

```cpp
Q_PROPERTY(int timeRemaining  READ timeRemaining  NOTIFY timeRemainingChanged)
Q_PROPERTY(int health         READ health         NOTIFY healthChanged)
Q_PROPERTY(int chargeCycles   READ chargeCycles   NOTIFY chargeCyclesChanged)
```

Corresponding private members: `int time_remaining_{0}`, `int health_{0}`, `int charge_cycles_{0}`.

`applyStateUpdate` gains three new branches following the identical guard pattern already used:

```cpp
if (update.time_remaining.has_value()) { setTimeRemaining(*update.time_remaining); }
if (update.health.has_value())         { setHealth(*update.health); }
if (update.charge_cycles.has_value())  { setChargeCycles(*update.charge_cycles); }
```

Each private setter follows the existing equality-guard + emit pattern.

### 4.3 No API breakage

All additions are purely additive. Existing properties, signals, and the `SkipInitTag`
test-seam constructor are unchanged. Existing GTests compile and pass without modification.

---

## 5. PowerProfilesService

### 5.1 D-Bus interface facts

power-profiles-daemon exposes itself under two possible service names (depending on version):

| Name | Introduced | Object path |
|------|-----------|-------------|
| `net.hadess.PowerProfiles` | v0.1 | `/net/hadess/PowerProfiles` |
| `org.freedesktop.UPower.PowerProfiles` | v0.20+ | `/org/freedesktop/UPower/PowerProfiles` |

Each bus name uses its matching interface name. The newer service exposes
`org.freedesktop.UPower.PowerProfiles`; the legacy service exposes `net.hadess.PowerProfiles`.
Object paths differ between names (see table above).

Key interface members:

| Member | Type | Description |
|--------|------|-------------|
| `ActiveProfile` (property) | `s` | Current active profile name; writable via `org.freedesktop.DBus.Properties.Set` |
| `Profiles` (property) | `aa{sv}` | Array of dicts; each has `"Profile"` (s) and `"Driver"` (s) keys |

> **Important**: power-profiles-daemon does not expose a `SetProfile` or `SetActiveProfile`
> method on the target daemon. Profile changes are made by writing the writable
> `ActiveProfile` property through `org.freedesktop.DBus.Properties.Set`.

### 5.2 Dual-name detection strategy

`PowerProfilesService::start()` queries `DbusPropertyClient::serviceRegistered()` (already
available in the platform layer) for each service name in priority order:

```
1. org.freedesktop.UPower.PowerProfiles  (path: /org/freedesktop/UPower/PowerProfiles)
2. net.hadess.PowerProfiles              (path: /net/hadess/PowerProfiles)
```

The first registered name wins; its corresponding object path is stored. If neither is
registered, `available_` stays `false` and a `qCWarning` is emitted.

For runtime disappearance (daemon crash), `PowerProfilesService` registers a
`QDBusServiceWatcher` on the system bus watching both names with
`QDBusServiceWatcher::WatchForUnregistration`. When the watcher fires `serviceUnregistered`,
the service sets `available_ = false`, emits `availableChanged()`, clears `active_profile_`,
and logs at `qCWarning`.

For daemon restart (re-appearance), optionally watch `WatchForRegistration` and call
`start()` again. This is a nice-to-have; for the initial implementation it is acceptable to
require a shell restart.

### 5.3 PowerProfilesService public API (src/services/PowerProfilesService.h)

```cpp
class PowerProfilesService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool    available      READ available      NOTIFY availableChanged)
    Q_PROPERTY(QString activeProfile  READ activeProfile  NOTIFY activeProfileChanged)
    Q_PROPERTY(bool    hasPerformance READ hasPerformance NOTIFY hasPerformanceChanged)
    Q_PROPERTY(bool    hasBalanced    READ hasBalanced    NOTIFY hasBalancedChanged)
    Q_PROPERTY(bool    hasPowerSaver  READ hasPowerSaver  NOTIFY hasPowerSaverChanged)

public:
    struct SkipInitTag {};
    static constexpr SkipInitTag SkipInit{};

    explicit PowerProfilesService(QObject* parent = nullptr);
    explicit PowerProfilesService(DbusPropertyClientPtr dbus, QObject* parent = nullptr);
    explicit PowerProfilesService(SkipInitTag, QObject* parent = nullptr);
    ~PowerProfilesService() override = default;

    // deleted copy/move (same pattern as all other services)

    [[nodiscard]] bool    available()      const { return available_; }
    [[nodiscard]] QString activeProfile()  const { return active_profile_; }
    [[nodiscard]] bool    hasPerformance() const { return has_performance_; }
    [[nodiscard]] bool    hasBalanced()    const { return has_balanced_; }
    [[nodiscard]] bool    hasPowerSaver()  const { return has_power_saver_; }

    void start();

    Q_INVOKABLE void setProfile(const QString& profile_name);

Q_SIGNALS:
    void availableChanged();
    void activeProfileChanged();
    void hasPerformanceChanged();
    void hasBalancedChanged();
    void hasPowerSaverChanged();

private Q_SLOTS:
    void onPropertiesChanged(const QString& interface,
                             const QVariantMap& changed,
                             const QStringList& invalidated);
    void onServiceUnregistered(const QString& service);

private:
    void readInitialProperties();
    void applyProfiles(const QVariant& profiles_variant);
    void setAvailable(bool value);
    void setActiveProfile(const QString& value);
    void setHasPerformance(bool value);
    void setHasBalanced(bool value);
    void setHasPowerSaver(bool value);

    DbusPropertyClientPtr dbus_;
    QDBusServiceWatcher*  watcher_{nullptr};
    QString               service_name_;    // whichever name was found
    QString               object_path_;     // matching object path
    QString               active_profile_;
    bool                  available_{false};
    bool                  has_performance_{false};
    bool                  has_balanced_{false};
    bool                  has_power_saver_{false};
    bool                  started_{false};
};
```

### 5.4 Profiles parsing

The `Profiles` property has D-Bus type `aa{sv}`. Qt unmarshals it as `QDBusArgument` inside a
`QVariant`. Iterate the array, extract the `"Profile"` string from each dict, and set the
`has_*` flags:

```cpp
void PowerProfilesService::applyProfiles(const QVariant& profiles_variant) {
    const QDBusArgument arg = profiles_variant.value<QDBusArgument>();
    arg.beginArray();
    bool perf = false, bal = false, saver = false;
    while (!arg.atEnd()) {
        QVariantMap entry;
        arg >> entry;
        const QString name = entry.value(QStringLiteral("Profile")).toString();
        if (name == QLatin1String("performance"))  perf  = true;
        if (name == QLatin1String("balanced"))     bal   = true;
        if (name == QLatin1String("power-saver"))  saver = true;
    }
    arg.endArray();
    setHasPerformance(perf);
    setHasBalanced(bal);
    setHasPowerSaver(saver);
}
```

### 5.5 setProfile implementation

```cpp
void PowerProfilesService::setProfile(const QString& profile_name) {
    if (!available_ || service_name_.isEmpty()) {
        qCWarning(lcPowerProfiles) << "setProfile called while unavailable";
        return;
    }
    if (!dbus_->setProperty(service_name_, object_path_, interface_name_,
                            QStringLiteral("ActiveProfile"), profile_name)) {
        qCWarning(lcPowerProfiles) << "ActiveProfile property write failed";
    }
    // No optimistic update. UI waits for PropertiesChanged (REQ-F-013).
}
```

### 5.6 PropertiesChanged handling

`onPropertiesChanged` is connected via `dbus_->connectSignal(...)` pointing at the daemon's
object path and `org.freedesktop.DBus.Properties` interface. It handles `changed` map entries
for `"ActiveProfile"` (update `active_profile_`) and `"Profiles"` (call `applyProfiles()`).

---

## 6. QML Components

### 6.1 BatteryPopupContent.qml

**File:** `src/qml/BatteryPopup/BatteryPopupContent.qml`
**QRC path:** `qrc:/HolonightShell/BatteryPopup/BatteryPopupContent.qml`

The component is loaded by `StatusPopup.qml`'s `Loader` and fills the panel's interior
(x/y/width/height assigned by the Loader). It does NOT create surfaces. It imports:

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import HolonightShell
import Holonight
```

**Layout (top to bottom, all inside a `ColumnLayout` anchored to `parent`):**

```
┌──────────────────────────────────┐
│  "BATTERY"  (SectionLabel)       │  always visible
│  "87%"      (large Text)         │  always visible
│  "Charging, 1h 5m to full"       │  always visible (empty string when timeRemaining=0)
├──────────────────────────────────┤
│  HEALTH       95%                │  visible: BatteryService.health > 0
│  CYCLES       120                │  visible: BatteryService.chargeCycles > 0
├──────────────────────────────────┤
│  [⚡ Saver] [⚖ Balanced] [🔥 Perf] │  visible: PowerProfilesService.available
└──────────────────────────────────┘
```

**Time-remaining formatting** (JS helper function inside the component):

```js
function formatTimeRemaining(seconds, isCharging) {
    if (seconds <= 0) { return ""; }
    const hours = Math.floor(seconds / 3600);
    const mins  = Math.floor((seconds % 3600) / 60);
    const parts = [];
    if (hours > 0) parts.push(hours + "h");
    if (mins  > 0) parts.push(mins  + "m");
    if (parts.length === 0) { return ""; }
    return parts.join(" ") + (isCharging ? " to full" : " remaining");
}
```

The state+duration line combines `BatteryService.charging ? "Charging" : BatteryService.discharging ? "Discharging" : ...` with the formatted duration, separated by `, ` when the duration string is non-empty.

**Row components reuse inline component pattern** (as seen in `WeatherPopupContent.qml`):

```qml
component MetricRow: RowLayout { ... }
component SectionLabel: Text { color: HoloniightPalette.accentBlue; font.pixelSize: 10; ... }
```

### 6.2 ProfileButton.qml

**File:** `src/qml/BatteryPopup/ProfileButton.qml`
**QRC path:** `qrc:/HolonightShell/BatteryPopup/ProfileButton.qml`

A circular button with:
- `required property string profileName` ("power-saver" | "balanced" | "performance")
- `required property string iconName` (system icon name, see Section 2.4)
- `required property bool   isActive`
- `required property bool   isEnabled`
- `property string caption` (display label shown on hover)

**Visual states:**

| State | Fill | Border | Glow | Opacity |
|-------|------|--------|------|---------|
| Active + Enabled | `accentCyan` at 0.2 alpha | `accentCyan` | MultiEffect shadowEnabled:true | 1.0 |
| Inactive + Enabled | transparent | `borderPassive` | none | 1.0 |
| Inactive + Disabled | transparent | `borderPassive` | none | 0.45 |

Glow is applied via `MultiEffect` **declared before** the `Rectangle` fill/icon items (z-order
rule from CLAUDE.md): source is the button's background `Rectangle`, `shadowEnabled: true`,
`shadowColor: HoloniightPalette.accentCyan`, `shadowBlur: 0.45`, `shadowOpacity: 0.25`.

Caption (`Text` below the icon) is controlled by a `HoverHandler.hovered` or `MouseArea.containsMouse` binding:

```qml
Text {
    visible: hoverArea.containsMouse
    text: root.caption
    ...
}
```

Click handler:

```qml
MouseArea {
    id: hoverArea
    anchors.fill: parent
    hoverEnabled: true
    enabled: root.isEnabled
    cursorShape: root.isEnabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
    onClicked: { if (root.isEnabled) PowerProfilesService.setProfile(root.profileName); }
}
```

### 6.3 Conditional row visibility and fixed popup size

`StatusPopupSurface::sizeForPopupId` returns a fixed `QSize`. The popup geometry is set once at
surface creation time and does not change while the popup is open. This is a constraint of the
layer-shell framework: the surface is torn down and recreated on each `show()` call, so the size
is effectively per-open-session, not truly dynamic.

Given that the health row, cycles row, and profile row are all conditionally visible, there are
two options:

**Option A — Fixed size with internal empty space (recommended):** Use `kBatteryWidth = 300`,
`kBatteryHeight = 360` always. When rows are hidden, the `ColumnLayout` collapses their height to
zero (via `visible: false`), leaving whitespace at the bottom of the panel. This is simple and
consistent with all other popups, which also use fixed sizes.

**Option B — Two fixed sizes (available/unavailable profiles):** Register two popup IDs such as
`"battery"` (300x360) and `"battery-compact"` (300x260) and switch in the trigger area QML.
This is more complex and couples the triggering widget to daemon state.

Option A is chosen. The 360px height is sufficient for all rows shown simultaneously; blank space
at the bottom is acceptable for this panel size.

---

## 7. StatusPopup.qml and StatusPopupSurface.cpp Edits

### 7.1 StatusPopup.qml

Add `"battery"` entry to `popupSources`:

```qml
readonly property var popupSources: ({
    "weather":  "WeatherPopupContent.qml",
    "audio":    "qrc:/HolonightShell/AudioPopup/AudioPopupContent.qml",
    "network":  "qrc:/HolonightShell/NetworkPopup/NetworkPopupContent.qml",
    "battery":  "qrc:/HolonightShell/BatteryPopup/BatteryPopupContent.qml"
})
```

No other changes to `StatusPopup.qml` — the existing `showTitle` logic automatically hides the
title bar because `contentSource` is now non-empty.

### 7.2 StatusPopupSurface.cpp

Add two constants and a branch in `sizeForPopupId`:

```cpp
constexpr int kBatteryWidth  = 300;
constexpr int kBatteryHeight = 360;
```

```cpp
QSize StatusPopupSurface::sizeForPopupId(const QString& popup_id) {
    if (popup_id == QLatin1String("audio"))    { return {kAudioWidth,   kAudioHeight};   }
    if (popup_id == QLatin1String("network"))  { return {kNetworkWidth,  kNetworkHeight}; }
    if (popup_id == QLatin1String("weather"))  { return {kWeatherWidth,  kWeatherHeight}; }
    if (popup_id == QLatin1String("battery"))  { return {kBatteryWidth,  kBatteryHeight}; } // ADD
    return {kDefaultWidth, kDefaultHeight};
}
```

---

## 8. ShellApplication Changes

### 8.1 ShellApplication.h

Add forward declaration and member:

```cpp
class PowerProfilesService;
// ...
PowerProfilesService* power_profiles_{nullptr};
```

### 8.2 ShellApplication.cpp

Include header, construct in initializer list, register with `reg()`, call `start()`:

```cpp
#include "PowerProfilesService.h"
// ...
// In constructor initializer list:
power_profiles_(new PowerProfilesService(this)),

// In registerQmlTypes():
reg(power_profiles_, "PowerProfilesService");

// In startServices():
power_profiles_->start();
```

---

## 9. CMakeLists.txt Changes

### 9.1 HOLONIGHT_QML_FILES additions

The list must be kept **sorted** (CMake enforces this via `list(SORT ...)` + fatal error check).
Insert the two new entries in alphabetical position:

```cmake
set(HOLONIGHT_QML_FILES
    src/qml/AudioPopup/AudioVolumeSlider.qml
    ...
    src/qml/BatteryPopup/BatteryPopupContent.qml    # ADD (after Background/)
    src/qml/BatteryPopup/ProfileButton.qml          # ADD
    ...
)
```

Exact insertion point: after the last `src/qml/Background/` entry and before the first
`src/qml/Launcher/` entry (alphabetical: `Bat` < `Lau`).

### 9.2 holonight_services sources

Add inside the `add_library(holonight_services STATIC ...)` block:

```cmake
    src/services/PowerProfilesService.h
    src/services/PowerProfilesService.cpp
```

Alphabetically adjacent to `BatteryService` entries (P comes after N).

### 9.3 target_include_directories

`src/services/` is already in `target_include_directories(holonight_services PUBLIC ...)`.
`PowerProfilesService.h` placed directly in `src/services/` (not a subdirectory) is
automatically found. No new path needed.

---

## 10. Data Flow

```
UPower D-Bus
  └─ PropertiesChanged (TimeToEmpty/TimeToFull/Capacity/ChargeCycles)
       └─ BatteryService::onPropertiesChanged
            └─ batteryStateUpdateFromProperties (BatteryState.cpp)
                 └─ applyStateUpdate
                      ├─ setTimeRemaining / healthChanged / etc.
                      └─ QML bindings in BatteryPopupContent update labels + row visibility

power-profiles-daemon D-Bus
  └─ PropertiesChanged (ActiveProfile / Profiles)
       └─ PowerProfilesService::onPropertiesChanged
            ├─ setActiveProfile        → QML ProfileButton isActive bindings
            └─ applyProfiles           → QML hasPerformance/hasBalanced/hasPowerSaver bindings
                                              → ProfileButton isEnabled bindings

User clicks ProfileButton
  └─ PowerProfilesService.setProfile("performance")  [Q_INVOKABLE]
       └─ org.freedesktop.DBus.Properties.Set(interface, "ActiveProfile", "performance")
            └─ daemon processes → emits PropertiesChanged
                 └─ (loop back to above)
                 NOTE: No optimistic UI update. Button state unchanged until daemon confirms.
```

---

## 11. Key Design Decisions

### 11.1 Fixed popup size vs. content-driven size

**Decision:** Fixed at 300×360 (Option A from Section 6.3).
**Rationale:** All other popups use fixed sizes. The layer-shell framework tears down and
recreates the surface on each open, so dynamic resizing between opens is free; dynamic resize
while the popup is open would require a surface recreation, which causes a visible flash.
**Alternative considered:** Passing a different popup ID per daemon state. Rejected: couples
topbar widget to PowerProfilesService state.
**Risk:** If all optional rows are hidden (no battery health data, no daemon), there will be
visible blank space below the percentage/state lines. Acceptable; can be tuned later.

### 11.2 image://icon/ vs. bundled SVGs

**Decision:** Use `image://icon/` (system theme, via `IconImageProvider`).
**Rationale:** Profile icons are not part of the HoloNight design system; they represent
hardware functionality and should match the user's icon theme for consistency. The
`IconImageProvider` is already registered on the popup view's engine by `StatusPopupSurface`.
**Risk:** Icon names `power-profile-*-symbolic` may be absent on some systems. Fallback plan:
use `QIcon::hasThemeIcon()` in a C++ helper (or a simple `Image.status === Image.Error`
check in QML) to detect absence, and fall back to neutral symbolic icons such as
`battery-caution-symbolic`, `battery-good-symbolic`, `battery-full-charging-symbolic`.
**Verification required:** Run `gtk-icon-browser` or `find /usr/share/icons -name
"power-profile*"` on the target system before finalising icon names.

### 11.3 No optimistic update for setProfile

**Decision:** `setProfile` makes a D-Bus call; the UI waits for `PropertiesChanged`.
**Rationale:** Daemon may reject the call (hardware limitation, PolicyKit denial). An
optimistic update that has to be rolled back creates a worse UX than a ~50ms delay.
**Risk:** If the daemon is slow (>500ms), the button appears unresponsive. Mitigated by the
2-second timeout (REQ-NF-003); a future iteration could add a "pending" visual state.

### 11.4 DbusPropertyClient reuse vs. direct QDBusInterface

**Decision:** `PowerProfilesService` holds a `DbusPropertyClientPtr` for initial property reads,
signal subscription, and the writable `ActiveProfile` property update.
**Rationale:** `DbusPropertyClient::setProperty` keeps the test seam intact and avoids embedding
raw D-Bus property-write code directly in the service.

### 11.5 power-profiles-daemon interface name

Each bus name uses its matching interface name. Store the active service name, object path, and
interface name together at detection time, then use that interface for property reads, signal
filtering, and `ActiveProfile` property writes.

---

## 12. Files to Create

| File | Description |
|------|-------------|
| `src/services/PowerProfilesService.h` | New service header |
| `src/services/PowerProfilesService.cpp` | New service implementation |
| `src/qml/BatteryPopup/BatteryPopupContent.qml` | Popup root component |
| `src/qml/BatteryPopup/ProfileButton.qml` | Circular profile button sub-component |

---

## 13. Files to Edit

| File | Change |
|------|--------|
| `src/core/BatteryState.h` | Extend `BatteryStateUpdate` struct with `time_remaining`, `health`, `charge_cycles` |
| `src/core/BatteryState.cpp` | Parse `TimeToEmpty`/`TimeToFull`/`Capacity`/`ChargeCycles` in `batteryStateUpdateFromProperties` |
| `src/services/BatteryService.h` | Add 3 `Q_PROPERTY` declarations, 3 private members, 3 signals, 3 private setters |
| `src/services/BatteryService.cpp` | Add `applyStateUpdate` branches for new fields; add private setter implementations |
| `src/surfaces/StatusPopupSurface.cpp` | Add `kBatteryWidth`/`kBatteryHeight` constants; add `"battery"` branch in `sizeForPopupId` |
| `src/qml/Topbar/StatusPopup.qml` | Add `"battery"` entry to `popupSources` map |
| `src/app/ShellApplication.h` | Add forward declaration for `PowerProfilesService`; add `power_profiles_` member |
| `src/app/ShellApplication.cpp` | Include header; construct; `reg()`; `start()` |
| `CMakeLists.txt` | Add `BatteryPopupContent.qml` + `ProfileButton.qml` to `HOLONIGHT_QML_FILES`; add `PowerProfilesService.{h,cpp}` to `holonight_services` |

---

## 14. Testing Notes

- **GTest for BatteryService**: inject properties map with `TimeToEmpty = 13320` (3h 42m),
  `Capacity = 95.4`, `ChargeCycles = 120`; verify `timeRemaining() == 13320`,
  `health() == 95`, `chargeCycles() == 120`.

- **GTest for PowerProfilesService**: use `SkipInitTag` + inject a mock `DbusPropertyClient`
  (as `BatteryService` already does). Mock `serviceRegistered` to return true for one of the
  two names; verify `available() == true`. Trigger `onPropertiesChanged` with a `Profiles`
  variant; verify `has_*` flags. Trigger `onServiceUnregistered`; verify `available() == false`.

- **Manual / visual**: use `busctl` or a Python D-Bus script to simulate daemon responses.
  Run with `QT_FORCE_STDERR_LOGGING=1` per CLAUDE.md logging guidance.

---

## 15. Open Questions

1. **ActiveProfile property write behavior**: keep verifying with `busctl introspect` on target
   daemon versions. The implementation writes the writable `ActiveProfile` property because the
   checked daemon exposes no profile-setting method.

2. **Icon name availability**: run `gtk-icon-browser` or check
   `/usr/share/icons/<theme>/scalable/status/power-profile-*` before committing icon name
   constants. Add a QML `Image.onStatusChanged` handler that logs a `qCWarning` if status is
   `Image.Error`, so icon resolution failures are visible in logs.

3. **ChargeCycles = -1 vs. 0**: UPower documents `-1` as "unknown". The design treats `<= 0`
   as unavailable. Confirm this is robust: if UPower ever returns `0` as a valid cycle count,
   the row would be wrongly hidden. Given that a brand-new battery would have `0` cycles, this
   edge case should be documented and the threshold changed to `< 0` if needed.

---

## Implementation corrections (recorded post-build, verified against the live daemon)

The following design assumptions were corrected during Stage 4 after `busctl` introspection and an on-device check:

1. **No `SetActiveProfile` method.** power-profiles-daemon has no such method; the profile is changed by writing the **writable `ActiveProfile` property** via `org.freedesktop.DBus.Properties.Set(interface, "ActiveProfile", v)`. `setProfile()` does this and keeps the no-optimistic-update behavior.
2. **Per-name interface, not a shared one.** `org.freedesktop.UPower.PowerProfiles` exposes interface `org.freedesktop.UPower.PowerProfiles`; `net.hadess.PowerProfiles` exposes interface `net.hadess.PowerProfiles`. The interface string is chosen together with the bus name (not a single shared interface).
3. **`DbusPropertyClient::setProperty` added (non-pure, default false)** instead of a local `QDBusInterface` in the service — keeps the test seam intact and lets `SetProfileDoesNotUpdateOptimistically` assert the write. Existing fakes are unaffected.
4. **UPower property types** (verified on device): `TimeToEmpty`/`TimeToFull` are `x` (int64 seconds), `Capacity` is `d` (double → `qRound` for health), `ChargeCycles` is `i` (int32, `-1` = unavailable). Cycles/health rows hide on `<= 0`.
5. **Icons** `power-profile-{power-saver,balanced,performance}-symbolic` confirmed present (Adwaita). They are near-black `#222`, so they are recolored at runtime with a `MultiEffect` colorization pass (the design's fallback names were not needed).
6. **Centering**: the profile caption + button row are direct children of the popup's outer `ColumnLayout` (a nested layout would not center against full width).
