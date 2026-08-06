# Power Extensions — Architecture Design

## 1. Overview

The power-extensions feature adds four capabilities to holonight-shell:

1. **ActivityGate** — an abstract interface that services implement to be paused when the laptop lid
   closes and resumed when the lid opens, detected via UPower.
2. **LowBatteryMonitor** — a pure-C++ class that watches `BatteryService` signals and fires a
   single desktop notification per threshold crossing per discharge cycle.
3. **SuspendInhibitorService** — a QML singleton that polls `org.freedesktop.login1.Manager.ListInhibitors()`
   every 5 seconds and exposes the sleep-inhibiting entries as a `QAbstractListModel` for sidebar display.
4. **Charge Limit Display** — an extension to `BatteryService` that probes UPower and sysfs for a
   charge limit percentage and exposes it as a new `chargeLimit` property.

All four build on the existing `BatteryService` (UPower D-Bus), `NotificationService` (desktop
notifications), and right-sidebar layout. They introduce no new Qt modules.

---

## 2. Component Map

### C++ classes

| Class | File | Responsibility |
|---|---|---|
| `IActivityGate` | `src/services/IActivityGate.h` | Abstract interface. Declares pure-virtual `pauseActivity()` and `resumeActivity()`. No QObject dependency. |
| `LidStateMonitor` | `src/services/LidStateMonitor.h/.cpp` | Reads `LidIsClosed` / `LidIsPresent` from `org.freedesktop.UPower` on `/org/freedesktop/UPower`. Subscribes to `PropertiesChanged`. Emits `lidClosed(bool)`. |
| `ActivityGateManager` | `src/services/ActivityGateManager.h/.cpp` | Owns `std::vector<IActivityGate*>`. Receives `LidStateMonitor::lidClosed` and fans out `pauseActivity()` / `resumeActivity()` to all registered gates. |
| `LowBatteryMonitor` | `src/services/LowBatteryMonitor.h/.cpp` | Connects to `BatteryService` signals. Tracks `warningSent_` / `criticalSent_` booleans. Reads thresholds from `QSettings`. Fires notifications via direct D-Bus to `org.freedesktop.Notifications`. |
| `SuspendInhibitorService` | `src/services/SuspendInhibitorService.h/.cpp` | QML singleton. Polls `login1.Manager.ListInhibitors()` every 5 s. Filters entries whose `What` field contains `"sleep"`. Owns and updates an `InhibitorModel`. Implements `IActivityGate` (pauses timer on lid close). |
| `InhibitorModel` | `src/services/InhibitorModel.h/.cpp` | `QAbstractListModel` subclass. Roles: `WhoRole`, `WhyRole`. Receives a `QList<InhibitorEntry>` from the service on each poll. |
| `BatteryService` | `src/services/BatteryService.h/.cpp` | Extended with a `chargeLimit` property (int, -1 = unavailable). Probes UPower `ChargeEndThreshold` first, then sysfs paths, once at startup. |

### QML components

| File | Responsibility |
|---|---|
| `src/qml/RightSidebar/InhibitorSection.qml` | Shows active sleep inhibitors. `visible: SuspendInhibitorService.inhibitorModel.count > 0`. Iterates model rows displaying `who` and `why`. |
| `src/qml/RightSidebar/ChargeLimitRow.qml` | Shows charge limit. `visible: BatteryService.chargeLimit >= 0`. Displays "Charge limit: N%". |

---

## 3. Data Flow Diagrams

### 3.1 ActivityGate (lid-close path)

```
UPower D-Bus
    │  PropertiesChanged(LidIsClosed=true)
    ▼
LidStateMonitor::onPropertiesChanged()
    │  emits lidClosed(true)
    ▼
ActivityGateManager::onLidStateChanged(true)
    │  iterates gates_
    ├──▶ CalendarSyncManager::pauseActivity()   → stops caldav_timer_, ics_timer_
    ├──▶ WeatherService::pauseActivity()         → stops refresh_timer_
    └──▶ SuspendInhibitorService::pauseActivity() → stops poll_timer_

UPower D-Bus
    │  PropertiesChanged(LidIsClosed=false)
    ▼
LidStateMonitor::onPropertiesChanged()
    │  emits lidClosed(false)
    ▼
ActivityGateManager::onLidStateChanged(false)
    │  iterates gates_
    ├──▶ CalendarSyncManager::resumeActivity()  → immediate sync + restarts timers
    ├──▶ WeatherService::resumeActivity()        → immediate fetch + restarts timer
    └──▶ SuspendInhibitorService::resumeActivity() → immediate poll + restarts timer
```

### 3.2 LowBatteryMonitor (warning path)

```
BatteryService
    │  percentChanged()  (percent dropped to 20 while discharging)
    ▼
LowBatteryMonitor::onPercentChanged()
    │  percent <= warningThreshold_?  yes
    │  discharging?                   yes
    │  warningSent_?                  no
    │  sets warningSent_ = true
    │  fires D-Bus Notify(urgency=Normal, summary="Battery low", body="20% remaining")
    ▼
org.freedesktop.Notifications (existing daemon, e.g. mako)
```

### 3.3 SuspendInhibitorService poll cycle

```
QTimer (5 s)
    │  timeout()
    ▼
SuspendInhibitorService::poll()
    │  QDBusInterface("org.freedesktop.login1", "/org/freedesktop/login1",
    │                 "org.freedesktop.login1.Manager")
    │  .call("ListInhibitors")  →  QDBusMessage reply
    │
    │  reply.arguments()[0]  is QDBusArgument (type a(ssssuu))
    │  iterate QDBusArgument stream:
    │    for each struct(What, Who, Why, Mode, UID, PID):
    │      if What contains "sleep": collect {who=Who, why=Why}
    │
    │  inhibitor_model_->setEntries(collected)   → emits modelReset if changed
    ▼
InhibitorSection.qml
    │  count binding updates → visible: count > 0
```

### 3.4 ChargeLimit startup probe (BatteryService)

```
BatteryService::start()
    │  device_path_ found (existing UPower battery device)
    │
    ├─ 1) Try UPower property ChargeEndThreshold:
    │      dbus_->property(kUPowerService, device_path_,
    │                      kDeviceIface, "ChargeEndThreshold")
    │      → uint32 → chargeLimit_ = static_cast<int>(value)  [DONE if >= 0]
    │
    ├─ 2) Read /sys/class/power_supply/BAT0/charge_control_end_threshold
    │      QFile::readAll() → toInt()  [DONE if > 0]
    │
    ├─ 3) Read /sys/class/power_supply/BAT1/charge_control_end_threshold
    │      [DONE if > 0]
    │
    ├─ 4) Read /sys/class/power_supply/BAT0/charge_max_design
    │      [Lenovo conservation fallback; see §8.4 for caveats]
    │
    └─ 5) All fail → chargeLimit_ remains -1 → ChargeLimitRow hidden
```

---

## 4. Feature 1: ActivityGate

### 4.1 `IActivityGate` — abstract interface

```cpp
// src/services/IActivityGate.h
#pragma once

// Abstract interface for services that participate in the lid-close activity gate.
// Implementing classes must stop all periodic polling in pauseActivity() and restart
// it (plus an immediate refresh) in resumeActivity().
class IActivityGate {
 public:
  virtual ~IActivityGate() = default;

  IActivityGate(const IActivityGate&) = delete;
  IActivityGate& operator=(const IActivityGate&) = delete;
  IActivityGate(IActivityGate&&) = delete;
  IActivityGate& operator=(IActivityGate&&) = delete;

  // Called by ActivityGateManager when the lid closes. Must be idempotent.
  virtual void pauseActivity() = 0;

  // Called by ActivityGateManager when the lid opens. Must be idempotent;
  // should trigger an immediate refresh so stale data is not shown on resume.
  virtual void resumeActivity() = 0;

 protected:
  IActivityGate() = default;
};
```

`IActivityGate` is a pure C++ abstract base — no `Q_OBJECT`, no MOC overhead. Services inherit it
alongside their existing `QObject` base. This avoids the Diamond-`QObject` problem and keeps the
interface lightweight enough to mock in unit tests without a `QCoreApplication`.

### 4.2 `LidStateMonitor`

```cpp
// src/services/LidStateMonitor.h
#pragma once
#include <QObject>
#include <QVariantMap>

// Reads LidIsClosed / LidIsPresent from org.freedesktop.UPower at /org/freedesktop/UPower
// on the system bus. Subscribes to PropertiesChanged and emits lidClosed(bool) on each change.
// If LidIsPresent is false at startup, logs a single info message and does nothing further.
class LidStateMonitor : public QObject {
  Q_OBJECT
 public:
  explicit LidStateMonitor(QObject* parent = nullptr);

  // Call once from ShellApplication::startServices() after D-Bus is ready.
  void start();

  [[nodiscard]] bool lidPresent() const { return lid_present_; }
  [[nodiscard]] bool lidClosed() const  { return lid_closed_; }

 Q_SIGNALS:
  // Emitted whenever LidIsClosed changes. Not emitted on startup — callers query
  // lidClosed() for initial state.
  void lidStateChanged(bool closed);

 private Q_SLOTS:
  void onPropertiesChanged(const QString& interface,
                           const QVariantMap& changed,
                           const QStringList& invalidated);

 private:
  bool lid_present_{false};
  bool lid_closed_{false};
};
```

**D-Bus wiring:**

| Item | Value |
|---|---|
| Service | `org.freedesktop.UPower` |
| Object path | `/org/freedesktop/UPower` |
| Properties interface | `org.freedesktop.DBus.Properties` |
| Properties of interest | `LidIsPresent` (bool), `LidIsClosed` (bool) |
| Signal subscribed | `PropertiesChanged` on `org.freedesktop.UPower` |

`start()` reads `LidIsPresent` and `LidIsClosed` synchronously via
`dbus_->allProperties(kUPowerService, "/org/freedesktop/UPower", kUPowerIface)`, then connects
`PropertiesChanged`. If `LidIsPresent` is false, logs
`qCInfo(lcLid) << "Lid device not present (desktop?)"` and skips the signal subscription — the
monitor becomes a permanent no-op, gates are never paused, and no further D-Bus traffic occurs.

Note: UPower exposes lid state on the manager object (`/org/freedesktop/UPower`), not on a
per-device path. Enumerating devices (`EnumerateDevices`) and filtering by a device `Type` field
is not required and would not find lid state — lid presence is a manager-level boolean.

### 4.3 `ActivityGateManager`

```cpp
// src/services/ActivityGateManager.h
#pragma once
#include "IActivityGate.h"
#include <QObject>
#include <vector>

// Broadcasts pauseActivity()/resumeActivity() to all registered IActivityGate
// implementations when the lid closes or opens.
class ActivityGateManager : public QObject {
  Q_OBJECT
 public:
  explicit ActivityGateManager(QObject* parent = nullptr);

  // Register a gate. Ownership is NOT transferred — the gate must outlive this manager.
  void registerGate(IActivityGate* gate);

 public Q_SLOTS:
  void onLidStateChanged(bool closed);

 private:
  std::vector<IActivityGate*> gates_;
};
```

`ShellApplication` creates one `ActivityGateManager`, registers participating services, then
connects it to `LidStateMonitor::lidStateChanged`:

```cpp
// ShellApplication::startServices():
activity_gate_manager_->registerGate(calendar_sync_manager_);
activity_gate_manager_->registerGate(weather_);
activity_gate_manager_->registerGate(suspend_inhibitor_service_);
connect(lid_monitor_, &LidStateMonitor::lidStateChanged,
        activity_gate_manager_, &ActivityGateManager::onLidStateChanged);
```

### 4.4 Services that implement `IActivityGate`

| Service | Existing polling to pause | `resumeActivity()` side effect |
|---|---|---|
| `CalendarSyncManager` | `caldav_timer_`, `ics_timer_`, `notification_timer_` | Immediate sync triggered after restart |
| `WeatherService` | `refresh_timer_` | Immediate fetch on resume |
| `SuspendInhibitorService` | `poll_timer_` (new, 5 s) | Immediate `poll()` on resume |

`CalendarSyncManager` already has `setIdlePaused(bool)` for the existing idle gate. The simplest
integration adds `pauseActivity()` / `resumeActivity()` as thin wrappers that call
`setIdlePaused(true/false)`, reusing the same timer-stop/start logic. The two gates (idle and lid)
are independent: both must be open for timers to run. A simple `pause_refcount_` counter or a pair
of booleans (`idle_paused_`, `lid_paused_`) selects the final paused state.

```cpp
// In CalendarSyncManager — Activity gate integration sketch:
void CalendarSyncManager::pauseActivity() {
  lid_paused_ = true;
  applyPausedState();
}
void CalendarSyncManager::resumeActivity() {
  lid_paused_ = false;
  applyPausedState();
}
void CalendarSyncManager::applyPausedState() {
  setIdlePaused(idle_paused_ || lid_paused_);
}
```

`WeatherService` gains the same two methods controlling its existing `refresh_timer_`.

### 4.5 Graceful desktop handling

If `LidStateMonitor::start()` finds `LidIsPresent == false`:
- Logs exactly one `qCInfo` at the `holonight.lid` category.
- Does not subscribe to any signals.
- `lidStateChanged` is never emitted.
- `ActivityGateManager::onLidStateChanged` is therefore never called.
- All registered services remain permanently unpaused — correct behavior for a desktop.

---

## 5. Feature 2: LowBatteryMonitor

### 5.1 Class design

```cpp
// src/services/LowBatteryMonitor.h
#pragma once
#include <QObject>

class BatteryService;
class QSettings;

// Fires one-shot desktop notifications when battery percent crosses warning/critical
// thresholds while discharging. Not a QML singleton — pure C++ wired in ShellApplication.
class LowBatteryMonitor : public QObject {
  Q_OBJECT
 public:
  explicit LowBatteryMonitor(BatteryService* battery, QObject* parent = nullptr);

  // Test seam: inject thresholds directly (bypasses QSettings).
  void setThresholds(int warning_pct, int critical_pct);

  [[nodiscard]] int warningThreshold()  const { return warning_threshold_; }
  [[nodiscard]] int criticalThreshold() const { return critical_threshold_; }

 private Q_SLOTS:
  void onPercentChanged();
  void onChargingChanged();
  void onFullyChargedChanged();
  void onPresentChanged();

 private:
  void resetState();
  void checkThresholds();
  void sendNotification(const QString& summary, const QString& body, int urgency);

  BatteryService* battery_;
  int warning_threshold_{20};
  int critical_threshold_{10};
  bool warning_sent_{false};
  bool critical_sent_{false};
};
```

### 5.2 Threshold state machine

The monitor tracks two boolean flags per discharge cycle:

```
Initial state: warning_sent_=false, critical_sent_=false

Event: percentChanged()
  if !BatteryService.discharging → no-op
  if percent <= criticalThreshold_ && !critical_sent_:
      critical_sent_ = true
      warning_sent_  = true   ← also mark warning as sent (prevents double fire on same event)
      sendNotification("Battery critical", "N% remaining — plug in now", urgency=2 [Critical])
  else if percent <= warningThreshold_ && !warning_sent_:
      warning_sent_ = true
      sendNotification("Battery low", "N% remaining", urgency=1 [Normal])

Event: chargingChanged()   [BatteryService.charging == true]
Event: fullyChargedChanged() [BatteryService.fullyCharged == true]
Event: presentChanged()    [any present change]
  → reset: warning_sent_ = false, critical_sent_ = false
```

The reset fires on any `presentChanged()` (insert or remove) and on `chargingChanged()` /
`fullyChargedChanged()` becoming true. It does NOT fire when `charging` returns to `false` — only
the next trip below the threshold fires a new notification.

### 5.3 QSettings keys

| Key | Default | Type |
|---|---|---|
| `holonight/power/warningThreshold` | `20` | int (0–100) |
| `holonight/power/criticalThreshold` | `10` | int (0–100) |

Read once in the constructor via `QSettings settings; warning_threshold_ = settings.value(..., 20).toInt()`.
Thresholds are not observed at runtime (no watcher) — they take effect after shell restart.
If the critical threshold is >= warning threshold in the stored config, the critical threshold is
clamped to `warning_threshold_ - 1` on load (logged as a `qCWarning`).

### 5.4 Notification payload

Both notifications are sent via a direct synchronous D-Bus call to `org.freedesktop.Notifications`,
following the `CalendarSyncManager::sendNotification` pattern:

```cpp
QDBusInterface notif("org.freedesktop.Notifications",
                     "/org/freedesktop/Notifications",
                     "org.freedesktop.Notifications");
notif.call("Notify",
           "HoloNight Shell",          // app_name
           static_cast<uint>(0),       // replaces_id (0 = new)
           "battery-caution",          // app_icon (warning) or "battery-low" (critical)
           summary,                    // e.g. "Battery low"
           body,                       // e.g. "20% remaining"
           QStringList{},             // actions (none)
           QVariantMap{{"urgency", urgency_byte}},  // hints
           10000);                     // expire_timeout_ms (10 s)
```

| Threshold | `app_icon` | urgency hint | Timeout |
|---|---|---|---|
| Warning | `"battery-caution"` | `1` (Normal) | 10 000 ms |
| Critical | `"battery-low"` | `2` (Critical) | 0 (persistent) |

The `urgency` hint is a `QVariant` wrapping a `quint8` value packed in a `QVariantMap`.

### 5.5 Wiring in ShellApplication

`LowBatteryMonitor` is a plain `QObject` parented to `ShellApplication`. No QML registration.

```cpp
// ShellApplication::startServices():
low_battery_monitor_ = new LowBatteryMonitor(battery_, this);
```

No explicit `connect()` calls in `ShellApplication` — `LowBatteryMonitor`'s constructor wires
all `BatteryService` signals internally.

---

## 6. Feature 3: SuspendInhibitorService

### 6.1 Class design

```cpp
// src/services/SuspendInhibitorService.h
#pragma once
#include "IActivityGate.h"
#include "InhibitorModel.h"

#include <QObject>
#include <QQmlEngine>
#include <QTimer>

// QML singleton. Polls login1.Manager.ListInhibitors() every 5 seconds and
// exposes sleep-type inhibitors via InhibitorModel for sidebar display.
class SuspendInhibitorService : public QObject, public IActivityGate {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(InhibitorModel* inhibitorModel READ inhibitorModel CONSTANT FINAL)

 public:
  struct SkipInitTag {};
  static constexpr SkipInitTag SkipInit{};

  explicit SuspendInhibitorService(QObject* parent = nullptr);
  explicit SuspendInhibitorService(SkipInitTag, QObject* parent = nullptr);

  void start();

  [[nodiscard]] InhibitorModel* inhibitorModel() { return &model_; }

  // IActivityGate implementation
  void pauseActivity() override;
  void resumeActivity() override;

 private Q_SLOTS:
  void poll();

 private:
  InhibitorModel model_;
  QTimer poll_timer_;

  static constexpr int kPollIntervalMs{5000};
};
```

### 6.2 `InhibitorModel`

```cpp
// src/services/InhibitorModel.h
#pragma once
#include <QAbstractListModel>
#include <QString>
#include <QList>

struct InhibitorEntry {
  QString who;
  QString why;
};

class InhibitorModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
 public:
  // NOLINTNEXTLINE(cppcoreguidelines-use-enum-class, performance-enum-size)
  enum Roles { WhoRole = Qt::UserRole + 1, WhyRole };
  Q_ENUM(Roles)

  explicit InhibitorModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  // Replace the current entry list. Emits modelReset if contents differ.
  void setEntries(const QList<InhibitorEntry>& entries);

 Q_SIGNALS:
  void countChanged();

 private:
  QList<InhibitorEntry> entries_;
};
```

`count` is exposed as a `Q_PROPERTY` so QML can bind `visible: SuspendInhibitorService.inhibitorModel.count > 0` without a dedicated signal from the service itself.

### 6.3 `ListInhibitors` D-Bus parsing — avoiding `QDBusArgument` read-mode traps

`org.freedesktop.login1.Manager.ListInhibitors()` returns `a(ssssuu)`. The struct layout is
`(What, Who, Why, Mode, UID, PID)` — field order is fixed by the systemd specification.

**Safe extraction pattern** (avoids `QVariant::canConvert<T>()` traps documented in CLAUDE.md):

```cpp
void SuspendInhibitorService::poll() {
  QDBusInterface manager(
      QStringLiteral("org.freedesktop.login1"),
      QStringLiteral("/org/freedesktop/login1"),
      QStringLiteral("org.freedesktop.login1.Manager"),
      QDBusConnection::systemBus());

  const QDBusMessage reply = manager.call(QStringLiteral("ListInhibitors"));
  if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
    qCWarning(lcSuspendInhibitor) << "ListInhibitors failed:" << reply.errorMessage();
    return;
  }

  // The reply argument is a(ssssuu); Qt delivers it as QDBusArgument.
  // Check userType() explicitly — do NOT use canConvert<QDBusArgument>().
  const QVariant& raw = reply.arguments().at(0);
  if (raw.userType() != qMetaTypeId<QDBusArgument>()) {
    qCWarning(lcSuspendInhibitor) << "ListInhibitors: unexpected reply type" << raw.typeName();
    return;
  }

  QList<InhibitorEntry> entries;
  auto arg = raw.value<QDBusArgument>();
  arg.beginArray();
  while (!arg.atEnd()) {
    QString what, who, why, mode;
    uint uid = 0, pid = 0;
    arg.beginStructure();
    arg >> what >> who >> why >> mode >> uid >> pid;
    arg.endStructure();
    if (what.contains(QLatin1String("sleep"))) {
      entries.append({.who = who, .why = why});
    }
  }
  arg.endArray();

  model_.setEntries(entries);
}
```

Key points:
- Use `reply.arguments().at(0).userType() == qMetaTypeId<QDBusArgument>()` — never
  `canConvert<QDBusArgument>()`, which may trigger a write-side conversion and log
  `QDBusArgument: write from a read-only object`.
- Stream extraction (`>>`) on `QDBusArgument` is the read path; the struct fields are consumed
  in declaration order: What, Who, Why, Mode, UID, PID.
- Only entries where `What` contains `"sleep"` are kept; `"idle"`, `"shutdown"`, and `"handle-lid-switch"`
  inhibitors are filtered out — the sidebar shows only sleep-blocking processes.

`SuspendInhibitorService` does not call `Inhibit()` and does not hold an inhibitor fd
(REQ-F-016 / REQ-C-005). It is strictly diagnostic.

### 6.4 Poll interval and ActivityGate integration

`poll_timer_` is a `QTimer` with `singleShot(false)` set to 5 000 ms. On `start()`, an immediate
`poll()` call runs before the timer fires, so the model is populated within the current event
loop iteration, not 5 seconds later.

`pauseActivity()` stops `poll_timer_`. `resumeActivity()` calls `poll()` immediately, then
restarts `poll_timer_`. Both are idempotent (timer stop on an already-stopped timer is a no-op).

### 6.5 `InhibitorSection.qml`

```
// src/qml/RightSidebar/InhibitorSection.qml
import QtQuick
import QtQuick.Layouts
import Holonight
import HolonightShell

ColumnLayout {
    id: root
    visible: SuspendInhibitorService.inhibitorModel.count > 0
    spacing: 4

    Text {
        text: "Sleep inhibitors"
        color: HoloniightPalette.onSurface
        font.pixelSize: 12
        font.weight: Font.Medium
        opacity: 0.7
        Layout.fillWidth: true
    }

    Repeater {
        model: SuspendInhibitorService.inhibitorModel
        delegate: RowLayout {
            required property string who
            required property string why
            Layout.fillWidth: true
            spacing: 6
            Text {
                text: who
                color: HoloniightPalette.onSurface
                font.pixelSize: 12
                font.weight: Font.Medium
                elide: Text.ElideRight
                Layout.preferredWidth: 100
            }
            Text {
                text: why
                color: HoloniightPalette.onSurfaceVariant
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }
}
```

`InhibitorSection` is embedded in the battery section of `SidebarContent.qml` (or wherever the
battery rows live — coordinate with the existing `BrightnessSlider` and battery-info rows to avoid
layout conflicts). The `visible` binding hides the entire column including the label when no
inhibitors are present.

---

## 7. Feature 4: Charge Limit Display

### 7.1 `BatteryService` extension

Add to `BatteryService.h`:

```cpp
Q_PROPERTY(int chargeLimit READ chargeLimit NOTIFY chargeLimitChanged FINAL)

[[nodiscard]] int chargeLimit() const { return charge_limit_; }

Q_SIGNALS:
  void chargeLimitChanged();

private:
  void probeChargeLimit();
  int charge_limit_{-1};
```

Add to `BatteryStateUpdate` in `BatteryState.h`:

```cpp
std::optional<int> charge_limit;  // percent, -1 if unavailable; from UPower ChargeEndThreshold
```

### 7.2 Probe order

`probeChargeLimit()` is called from `BatteryService::start()` after `device_path_` is populated:

1. **UPower `ChargeEndThreshold`** — read via `dbus_->property(kUPowerService, device_path_, kDeviceIface, "ChargeEndThreshold")`. The value is a `uint32`; if present and > 0, use it as `charge_limit_`.

2. **sysfs `/sys/class/power_supply/BAT0/charge_control_end_threshold`** — open with `QFile`, read as integer. If > 0, use it.

3. **sysfs `/sys/class/power_supply/BAT1/charge_control_end_threshold`** — same as above for a secondary battery slot.

4. **sysfs `/sys/class/power_supply/BAT0/charge_max_design`** — Lenovo conservation-mode fallback. This file holds the designed capacity in microampere-hours (µAh), not a percentage limit. Reading it as an integer yields a large number (e.g., 57 000 000 µAh = 57 Wh). If read at this probe step, `charge_limit_` remains `-1` — this path only confirms battery presence, it does not yield a percentage charge limit. **Do not treat this file's value as a percentage.** If the earlier probes all returned 0 or empty, `charge_limit_` stays -1 and `ChargeLimitRow` remains hidden.

The probe order is attempted sequentially; the first non-negative, non-zero result wins. After
`probeChargeLimit()` returns, `charge_limit_` is either a value 1–100 or -1.

`chargeLimit` is read once at startup. Runtime changes (e.g., user configuring TLP while the shell
is running) are picked up via `PropertiesChanged` if UPower emits `ChargeEndThreshold` in the
changed map; the existing `onPropertiesChanged` slot processes it via `BatteryStateUpdate`. The
sysfs paths are not inotify-watched — changes there require a shell restart to be reflected.

### 7.3 `ChargeLimitRow.qml`

```
// src/qml/RightSidebar/ChargeLimitRow.qml
import QtQuick
import QtQuick.Layouts
import Holonight
import HolonightShell

RowLayout {
    id: root
    visible: BatteryService.chargeLimit >= 0
    Layout.fillWidth: true
    spacing: 8

    Text {
        text: "Charge limit"
        color: HoloniightPalette.onSurface
        font.pixelSize: 13
        Layout.fillWidth: true
    }

    Text {
        text: BatteryService.chargeLimit + "%"
        color: HoloniightPalette.onSurfaceVariant
        font.pixelSize: 13
    }
}
```

`ChargeLimitRow` is placed in the battery section of the sidebar below the existing battery-info
row and above the `BrightnessSlider`. It has no interactive elements (REQ-C-006).

---

## 8. Key Decisions with Rationale

**1. `IActivityGate` interface over direct signals.**
A central broadcaster calling a known method on each participant is simpler to test than
coordinating multiple signal connections from `ActivityGateManager` to each service. With a direct
method call, `ActivityGateManager` does not need to know the concrete slot names on each service —
they all share the same two-method contract. Adding a new pauseable service requires registering
with the manager; nothing else changes. The alternative (connecting `LidStateMonitor::lidClosed`
directly to each service in `ShellApplication`) spreads the connection graph across `ShellApplication`
and makes it harder to audit which services participate.

**2. Poll vs. signal for suspend inhibitors.**
`logind` does not expose a `InhibitorsChanged` signal on `org.freedesktop.login1.Manager`. The
only mechanism to detect new or removed inhibitors is to call `ListInhibitors()` periodically. A
5-second interval satisfies REQ-NF-003 (≤ 5 seconds) and is low enough overhead (one synchronous
D-Bus call returning a small struct array) to be negligible. A `QDBusServiceWatcher` on process bus
names would require knowing which bus names map to inhibitor holders — not available without polling.

**3. Extend `BatteryService` for charge limit (vs. new service).**
Charge limit comes from the same UPower device object and the same sysfs subsystem that
`BatteryService` already queries. A new service would duplicate the device-path lookup, add another
QML singleton name, and increase `ShellApplication` wiring. Extending `BatteryService` with a single
property and a one-time sysfs probe at startup avoids all of this. The property belongs conceptually
to the battery — it is a read-only battery characteristic, not a system-level service state.

**4. `LowBatteryMonitor` as a non-singleton C++ object (no QML exposure).**
Low-battery notifications are fired-and-forgotten D-Bus calls; they require no QML binding, no
model, and no user interaction. Making `LowBatteryMonitor` a QML singleton would add a registered
type name and `QML_ELEMENT` overhead for zero QML benefit. The class is pure C++ state machine
wired entirely in `ShellApplication::startServices()`.

**5. Lid state from UPower manager object, not per-device enumeration.**
UPower exposes `LidIsClosed` and `LidIsPresent` as properties of the manager interface
(`org.freedesktop.UPower`) at `/org/freedesktop/UPower`, not as properties of any enumerated device.
There is no separate "lid device" in the UPower device list — `EnumerateDevices()` returns battery,
AC adapter, and peripheral entries, none of which carry lid state. `LidStateMonitor` reads directly
from the manager object and subscribes to its `PropertiesChanged` signal.

**6. `ActivityGateManager` as a separate class (not inside `ShellApplication`).**
`ShellApplication` is already large. Keeping the gate fan-out logic in a dedicated class makes it
independently testable and keeps `ShellApplication` a wiring layer only.

---

## 9. Alternatives Considered

**logind `PrepareForSleep` signal as the gate trigger.**
`org.freedesktop.login1.Manager` emits `PrepareForSleep(bool active)` before and after suspend.
Using this as the gate trigger would catch all suspend sources (manual, lid, timer). However, it
fires immediately before sleep, not on lid close — there could be a several-second gap between lid
close and actual suspend, during which services would still be polling. It also does not help on
machines with `HandleLidSwitch=ignore` in logind.conf. UPower's `LidIsClosed` fires as soon as
the kernel ACPI event is processed, well before any sleep handshake, satisfying REQ-NF-001 (≤ 1 s
latency). The UPower approach was chosen.

**`ext-idle-notify-v1` for activity gating (reuse of existing idle backend).**
The existing idle gate already pauses `WeatherService` and `CalendarSyncManager` via
`IdleService::idleChanged`. One could argue "lid closed = user idle" and rely solely on the idle
gate. This would break on laptops where users close the lid but are not idle (e.g., the laptop is
running a long sync in the background). More importantly, the idle threshold (default 5 minutes)
would mean services run for up to 5 minutes after the lid closes before the idle gate kicks in.
A dedicated lid gate fires within 1 second. The two gates coexist independently.

**Portal `org.freedesktop.portal.Inhibit` for a keep-awake toggle instead of logind direct.**
The existing `KeepAwakeAction.qml` (already shipped) uses `IdleInhibitor` which calls logind
`Inhibit` directly. The XDG portal path would add a Flatpak/portal dependency and indirection for
no benefit in a non-sandboxed shell. Not relevant to the power-extensions feature set but noted for
future reference.

**`QFutureWatcher` / async poll for `ListInhibitors`.**
The `ListInhibitors` D-Bus call is synchronous and returns quickly (the inhibitor list is an in-memory
kernel table, not a network call). Running it synchronously in a 5-second QTimer slot is acceptable.
An async watcher would add complexity for no practical latency benefit. If profiling ever shows that
the synchronous call blocks the event loop, it can be moved to `QtConcurrent::run`.

**Separate `ChargeLimitService` for sysfs probing.**
Considered but rejected (see §8 decision 3). Added complexity for no separation gain.

---

## 10. Known Risks

**10.1 Vendor-specific sysfs charge limit paths.**
`/sys/class/power_supply/BAT0/charge_control_end_threshold` exists on ThinkPads with TLP and on
many ASUS laptops. Samsung, HP, and Dell may expose the limit under different names
(`charge_full_design`, `capacity_max`, or proprietary attributes). The current probe chain covers
the two most common paths; users on other hardware see `chargeLimit == -1` and a hidden row — a
correct, non-broken degraded experience. Adding new paths requires only extending `probeChargeLimit()`.

**10.2 `sysfs /sys/class/power_supply/BAT0/charge_max_design` misidentification.**
This file contains design capacity in µAh, not a percentage limit. The probe chain stops before
reaching this step with a non-negative percentage from earlier probes. If all earlier probes return
0 or fail, reaching step 4 does NOT yield a percentage value and `charge_limit_` must stay at -1.
The implementation must not convert µAh to a percentage (design capacity is not a charge limit).
This sysfs path was included in the spec for "Lenovo conservation fallback" — the intent may have
been `/sys/class/power_supply/BAT0/charge_control_end_threshold` (the actual Lenovo path). Clarify
with the author before implementing step 4; if the design intent cannot be confirmed, omit step 4
and document the three-path probe chain.

**10.3 `ListInhibitors` reply type on unusual logind versions.**
On standard systemd 247+, `ListInhibitors()` always returns `a(ssssuu)`. On older versions or
alternative login managers (e.g., elogind), the struct field count or order may differ. The manual
`QDBusArgument` stream extraction will produce garbage fields silently if the struct layout changes.
Guard: if `arg.currentType() != QDBusArgument::StructureType` at the beginning of the inner loop,
log a single `qCWarning` and break out of the loop rather than crashing.

**10.4 Desktop systems with no lid.**
`LidStateMonitor::start()` reads `LidIsPresent` before subscribing. On a desktop, `LidIsPresent`
is `false` (or the UPower property is absent entirely). The monitor must handle a missing property
gracefully: if `allProperties()` returns an empty or absent `LidIsPresent` key, treat it as `false`
and log the info message. This ensures no `qCWarning` or error appears in the log on desktop.

**10.5 Both idle gate and activity gate pausing `CalendarSyncManager` simultaneously.**
When the user closes the lid while idle, both `setIdlePaused(true)` (from `IdleService`) and
`pauseActivity()` (from `ActivityGateManager`) are called. The existing `timers_paused_` /
`idle_paused_` boolean in `CalendarSyncManager` handles one gate; the activity gate integration
adds `lid_paused_`. The `applyPausedState()` helper must OR both flags: timers stop if either is
true, timers restart only when both are false. Failing to track both independently would cause
timers to restart on lid-open even if the session is still idle.

---

## 11. File Layout

```
src/services/
  IActivityGate.h                         # new — pure abstract interface, no QObject
  LidStateMonitor.h
  LidStateMonitor.cpp                     # new — UPower manager-object lid watcher
  ActivityGateManager.h
  ActivityGateManager.cpp                 # new — fan-out broadcaster
  LowBatteryMonitor.h
  LowBatteryMonitor.cpp                   # new — threshold tracker, D-Bus Notify sender
  InhibitorModel.h
  InhibitorModel.cpp                      # new — QAbstractListModel for inhibitor entries
  SuspendInhibitorService.h
  SuspendInhibitorService.cpp             # new — QML singleton, poll timer, ListInhibitors call
  BatteryService.h                        # modified — add chargeLimit Q_PROPERTY
  BatteryService.cpp                      # modified — add probeChargeLimit()

src/core/
  BatteryState.h                          # modified — add std::optional<int> charge_limit

src/qml/RightSidebar/
  InhibitorSection.qml                    # new — sleep inhibitor list, visible when count > 0
  ChargeLimitRow.qml                      # new — charge limit display, visible when >= 0
```

### New ShellApplication members

```cpp
// ShellApplication.h (forward declarations + members)
class LidStateMonitor;
class ActivityGateManager;
class LowBatteryMonitor;
class SuspendInhibitorService;

LidStateMonitor*       lid_monitor_            = nullptr;
ActivityGateManager*   activity_gate_manager_  = nullptr;
LowBatteryMonitor*     low_battery_monitor_    = nullptr;
SuspendInhibitorService* suspend_inhibitor_service_ = nullptr;
```

### Registration in `ShellApplication::registerQmlTypes()`

```cpp
reg(suspend_inhibitor_service_, "SuspendInhibitorService");
// BatteryService is already registered — chargeLimit property appears automatically
```

### Wiring in `ShellApplication::startServices()`

```cpp
lid_monitor_           = new LidStateMonitor(this);
activity_gate_manager_ = new ActivityGateManager(this);

activity_gate_manager_->registerGate(calendar_sync_manager_);
activity_gate_manager_->registerGate(weather_);
activity_gate_manager_->registerGate(suspend_inhibitor_service_);

connect(lid_monitor_, &LidStateMonitor::lidStateChanged,
        activity_gate_manager_, &ActivityGateManager::onLidStateChanged);

low_battery_monitor_   = new LowBatteryMonitor(battery_, this);
// suspend_inhibitor_service_ is already constructed before startServices()

lid_monitor_->start();
suspend_inhibitor_service_->start();
```

---

## 12. CMake Integration

All new source files are added to the `add_library(holonight_services STATIC ...)` block in
`CMakeLists.txt`. No new Qt modules are required: `Qt6::DBus` is already linked, and `QFile`
(for sysfs reads) is part of `Qt6::Core`.

New QML files to add to `HOLONIGHT_QML_FILES` (maintain alphabetical order):

- `src/qml/RightSidebar/ChargeLimitRow.qml`
- `src/qml/RightSidebar/InhibitorSection.qml`

`InhibitorModel` must be registered with the QML type system so the `inhibitorModel` property is
usable from QML. Since it is exposed only through `SuspendInhibitorService.inhibitorModel` (not as
a standalone singleton), `QML_ELEMENT` without `QML_SINGLETON` is sufficient — QML accesses it via
the property, not by name. Add `QML_ELEMENT` to `InhibitorModel` and ensure its source file is in
`holonight_services`.

After any CMake/QML registration change, run `task qmltypes-check` to confirm
`SuspendInhibitorService` and `InhibitorModel` appear in the generated qmltypes file.
