# Design — brightness-service

## Overview

`brightness-service` adds real-time display backlight control to the holonight-shell sidebar.
On construction, `BrightnessService` enumerates `/sys/class/backlight/`, selects the
highest-capacity device, and delegates reads/writes to a `BrightnessBackend` implementation.
External brightness changes (Fn keys, other apps) are tracked via inotify and reflected back to
the slider within 500 ms. On devices with no backlight the service falls back to a no-op
`NullBrightnessBackend` and the slider is hidden entirely.

The design replicates the backend-adapter pattern already established by `IdleService` /
`IdleBackend`: an abstract `BrightnessBackend` base, a production `SysfsBackend`, a
`NullBrightnessBackend` fallback, and a `BrightnessService` QML singleton that owns the backend
via `std::unique_ptr`. A second constructor overload accepts a pre-built backend for test
injection.

---

## Architecture Diagram (ASCII)

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  QML layer (HolonightShell module)                                           │
│                                                                              │
│   SidebarQuickSettings.qml                                                   │
│     └─ BrightnessSlider.qml ──────────── binds ──────────────────────────┐  │
│          Slider (0-100)                                                   │  │
│          visible: BrightnessService.hasBacklight                          │  │
│          Timer { interval: 100 }  ← throttle writes during drag          │  │
│          guard: update slider only when !pressed                          │  │
└──────────────────────────────────────────────────────────────────────────┼──┘
                                                                           │
                   Q_INVOKABLE setBrightnessPercent(int)                   │
                   Q_PROPERTY  brightnessPercent  (NOTIFY)                 │
                   Q_PROPERTY  hasBacklight       (CONSTANT after init)    │
                   Q_PROPERTY  maxBrightness      (CONSTANT after init)    │
                                                                           ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│  BrightnessService   (QML_SINGLETON, holonight_services)                     │
│                                                                              │
│  Production ctor:  factory → SysfsBackend | NullBrightnessBackend           │
│  Test ctor:        accepts std::unique_ptr<BrightnessBackend>                │
│                                                                              │
│  on backend::brightnessChanged(int raw) →                                   │
│      brightnessPercent = round(raw * 100.0 / maxBrightness_)                │
│      emit brightnessPercentChanged()                                         │
│                                                                              │
│  setBrightnessPercent(int pct) →                                             │
│      raw = round(pct * maxBrightness_ / 100.0)                              │
│      backend_->setBrightness(raw)                                            │
└────────────────────────────┬─────────────────────────────────────────────────┘
                             │  std::unique_ptr<BrightnessBackend>
          ┌──────────────────┴──────────────────┐
          ▼                                     ▼
┌──────────────────────┐          ┌──────────────────────────┐
│  SysfsBackend        │          │  NullBrightnessBackend   │
│                      │          │                          │
│  ctor: scan          │          │  maxBrightness() → 0     │
│  /sys/class/backlight│          │  currentBrightness() → 0 │
│  pick highest-max    │          │  setBrightness() → noop  │
│                      │          │  never emits signal      │
│  Read:               │          └──────────────────────────┘
│  /sys/.../brightness │
│  (sysfs file, sync)  │
│                      │
│  Write:              │
│  logind D-Bus        │
│  SetBrightness(      │
│    "backlight",      │
│    device_name,      │
│    value)            │
│                      │
│  inotify_init1()     │
│  inotify_add_watch() │
│  QSocketNotifier ────┼──► slot: drain inotify buf →
│  (Read on ifd_)      │         re-read sysfs file →
│                      │         emit brightnessChanged(raw)
│  dtor:               │
│  inotify_rm_watch()  │
│  close(ifd_)         │
└──────────────────────┘
          │
          │  D-Bus system bus
          ▼
┌──────────────────────────────────────┐
│  org.freedesktop.login1              │
│  /org/freedesktop/login1/session/... │
│  Interface: o.fd.login1.Session      │
│  Method: SetBrightness(s, s, u)      │
└──────────────────────────────────────┘
```

---

## Components

### BrightnessBackend (abstract)

**File:** `src/services/brightness/BrightnessBackend.h`

Abstract base following the same pattern as `IdleBackend`. The interface covers the three
operations the service needs from any concrete backend:

```cpp
class BrightnessBackend : public QObject {
  Q_OBJECT
 public:
  ~BrightnessBackend() override = default;

  BrightnessBackend(const BrightnessBackend&) = delete;
  BrightnessBackend& operator=(const BrightnessBackend&) = delete;
  BrightnessBackend(BrightnessBackend&&) = delete;
  BrightnessBackend& operator=(BrightnessBackend&&) = delete;

  [[nodiscard]] virtual int maxBrightness() const = 0;
  [[nodiscard]] virtual int currentBrightness() const = 0;
  virtual void setBrightness(int value) = 0;

 Q_SIGNALS:
  void brightnessChanged(int new_value);

 protected:
  explicit BrightnessBackend(QObject* parent = nullptr);
};
```

`brightnessChanged` is emitted only by concrete backends when an external agent changes the
brightness. `BrightnessService` never re-emits this signal directly; it recomputes
`brightnessPercent` from the raw value and emits `brightnessPercentChanged`.

---

### NullBrightnessBackend

**Files:** `src/services/brightness/NullBrightnessBackend.h/.cpp`

No-op implementation used when `/sys/class/backlight/` is empty (desktop with no backlight):

- `maxBrightness()` → `0`
- `currentBrightness()` → `0`
- `setBrightness(int)` → no-op
- Never emits `brightnessChanged`
- Logs nothing (not even info) — absence of a backlight is not an error condition

`BrightnessService::hasBacklight` is derived from `maxBrightness() > 0`, so a
`NullBrightnessBackend` causes the slider to be hidden without any conditional logic needed in
`BrightnessService`.

---

### SysfsBackend

**Files:** `src/services/brightness/SysfsBackend.h/.cpp`

The production backend. Lives entirely in the `holonight_services` static library.

#### Construction and device selection

```
/sys/class/backlight/          (scanned once in ctor)
    intel_backlight/
        max_brightness         → read as int
        brightness             → read as int (initial value)
    amdgpu_bl0/
        max_brightness         → read as int
        brightness             → (not read; inferior max, discarded)
```

Algorithm:
1. `QDir("/sys/class/backlight/").entryList(QDir::Dirs | QDir::NoDotAndDotDot)` — enumerate devices.
2. For each entry, read `max_brightness` into an int.
3. Track the entry with the highest value; store `device_name_` and `max_brightness_`.
4. Sanitise the device name: reject any entry whose name contains characters outside
   `[a-zA-Z0-9_-]` (REQ-NF-007). Log a warning and skip the offending entry.
5. If no valid entry is found, treat as `maxBrightness() == 0` (the factory then selects
   `NullBrightnessBackend`).

Logs at `qCInfo` level: selected device name and its max brightness.

**clang-tidy note:** `entryList` returns `QStringList`; iterate with a range-for over a `const`
reference to avoid copies. Prefer `std::ranges::max_element` over a hand-written comparison loop
(`modernize-use-ranges`).

#### Reading brightness

`readBrightness()` — private helper:

```cpp
[[nodiscard]] int readBrightness() const;
```

Opens `/sys/class/backlight/<device>/brightness`, reads the first line, converts to int with
`QFile` + `QTextStream`. Returns 0 and logs `qCWarning` on any I/O error.  
The read is synchronous and completes in < 1 ms (sysfs is a pseudo-filesystem in RAM).

`currentBrightness()` delegates to `readBrightness()`.

#### Writing brightness via logind D-Bus

`setBrightness(int value)` calls the logind session `SetBrightness` method on the system bus.
The session path is resolved in the constructor following the exact same two-step fallback used
by `IdleService::subscribeLockedHint()`:

1. `GetSessionByPID(getpid())` on the logind manager — works for DM/TTY launches.
2. If step 1 returns an empty path, query `ActiveSession` on `/org/freedesktop/login1/seat/seat0`
   — covers UWSM / systemd user service launches.

The session path is stored as `session_path_` (type `QString`) and reused for every
`setBrightness` call.

D-Bus call:
```
service:    org.freedesktop.login1
path:       <session_path_>
interface:  org.freedesktop.login1.Session
method:     SetBrightness
args:       ("backlight", device_name_, static_cast<quint32>(value))
```

Made with `QDBusInterface::asyncCall` (non-blocking). The `QDBusPendingCallWatcher` callback
logs `qCWarning` on error (REQ-NF-002: slider stays at last-known state; the failed write
does not update `current_brightness_` or emit `brightnessChanged`).

**clang-tidy note:** `value` must be cast to `quint32` before passing to `asyncCall`; the method
signature takes `u` (uint32). Do not pass a bare `int` — D-Bus type mismatch will produce a
runtime error.

#### inotify change detection

In the constructor, after the device is selected:

```cpp
ifd_ = inotify_init1(IN_CLOEXEC);   // int ifd_;
iwd_ = inotify_add_watch(ifd_,      // int iwd_;
    brightness_path_.toLocal8Bit().constData(),
    IN_MODIFY);
notifier_ = std::make_unique<QSocketNotifier>(ifd_, QSocketNotifier::Read, this);
connect(notifier_.get(), &QSocketNotifier::activated,
        this, &SysfsBackend::onInotifyEvent);
```

`onInotifyEvent` slot:

```cpp
void SysfsBackend::onInotifyEvent() {
    // Drain the inotify event buffer — sysfs may coalesce multiple IN_MODIFY events.
    std::array<char, sizeof(inotify_event) + NAME_MAX + 1> buf{};
    while (read(ifd_, buf.data(), buf.size()) > 0) {
        // consume; no per-event action needed
    }
    const int new_val = readBrightness();
    emit brightnessChanged(new_val);
}
```

`read()` in non-blocking mode: `inotify_init1(IN_CLOEXEC | IN_NONBLOCK)`. Loop until `read`
returns `-1` with `errno == EAGAIN` to drain all pending events before doing one sysfs read. This
coalesces rapid keypress bursts into a single `brightnessChanged` emission (REQ-F-009).

**clang-tidy note:** `NAME_MAX` requires `#include <climits>`. `std::array` avoids
`cppcoreguidelines-avoid-c-arrays`. The `buf` variable name is ≥ 3 characters (passes
`readability-identifier-length`). Loop variable over the read result must be named something
≥ 3 characters (not `nr` or `rd`).

#### Destructor

```cpp
SysfsBackend::~SysfsBackend() {
    if (iwd_ >= 0) {
        inotify_rm_watch(ifd_, iwd_);
    }
    if (ifd_ >= 0) {
        close(ifd_);
    }
}
```

`notifier_` (a `unique_ptr`) is destroyed before these calls complete (member order in the
header: `notifier_` declared after `ifd_` / `iwd_` so its destructor fires first). The
`QSocketNotifier` destructor disables the notifier cleanly before the fd is closed. Declare
members in this order:

```cpp
private:
    QString device_name_;
    QString brightness_path_;
    int     max_brightness_{0};
    QString session_path_;
    int     ifd_{-1};
    int     iwd_{-1};
    std::unique_ptr<QSocketNotifier> notifier_;
```

**clang-tidy note:** Initialize `int` members with `{-1}` (in-class member initializer,
`cppcoreguidelines-pro-type-member-init`). Check `>= 0` before calling `inotify_rm_watch` and
`close` to be safe if construction failed partway.

#### Private members summary

| Member | Type | Purpose |
|--------|------|---------|
| `device_name_` | `QString` | Selected device (`"intel_backlight"`) |
| `brightness_path_` | `QString` | Full sysfs brightness path |
| `max_brightness_` | `int` | `max_brightness` sysfs value |
| `session_path_` | `QString` | logind session object path |
| `ifd_` | `int` | inotify instance fd |
| `iwd_` | `int` | inotify watch descriptor |
| `notifier_` | `unique_ptr<QSocketNotifier>` | Qt event-loop integration for ifd |

---

### BrightnessService

**Files:** `src/services/brightness/BrightnessService.h/.cpp`

QML singleton registered with `QML_ELEMENT` + `QML_SINGLETON` and via
`qmlRegisterSingletonInstance<BrightnessService>()` in `src/main.cpp` before any QML view is
shown (REQ-F-017, REQ-C-002).

#### Properties

```cpp
Q_PROPERTY(int  maxBrightness       READ maxBrightness       CONSTANT             FINAL)
Q_PROPERTY(int  brightnessPercent   READ brightnessPercent   NOTIFY brightnessPercentChanged FINAL)
Q_PROPERTY(bool hasBacklight        READ hasBacklight        CONSTANT             FINAL)
```

`maxBrightness` and `hasBacklight` are `CONSTANT` — they are set once in the constructor from
the backend and never change. `CONSTANT` removes the need for a NOTIFY signal for these two
properties and allows QML engine to cache the binding.

**Note:** `hasBacklight` depends only on `maxBrightness > 0`. Because `maxBrightness` is
`CONSTANT`, `hasBacklight` is also effectively constant after construction. Making it `CONSTANT`
too is correct and matches REQ-F-006 / REQ-C-007.

#### Constructor factory

```cpp
BrightnessService::BrightnessService(QObject* parent)
    : QObject(parent),
      backend_(makeBrightnessBackend(this)) {   // free function in anonymous ns
    max_brightness_ = backend_->maxBrightness();
    brightness_percent_ = computePercent(backend_->currentBrightness());
    connect(backend_.get(), &BrightnessBackend::brightnessChanged,
            this, &BrightnessService::onBrightnessChanged);
    qCInfo(lcBrightness) << "BrightnessService ready;"
                         << "hasBacklight:" << hasBacklight()
                         << "initial:" << brightness_percent_ << "%";
}
```

`makeBrightnessBackend` (in anonymous namespace in `.cpp`):

```cpp
std::unique_ptr<BrightnessBackend> makeBrightnessBackend(QObject* parent) {
    auto candidate = std::make_unique<SysfsBackend>(parent);
    if (candidate->maxBrightness() > 0) {
        return candidate;
    }
    return std::make_unique<NullBrightnessBackend>(parent);
}
```

Test seam constructor:

```cpp
BrightnessService::BrightnessService(std::unique_ptr<BrightnessBackend> backend, QObject* parent)
    : QObject(parent),
      backend_(std::move(backend)) {
    max_brightness_ = backend_->maxBrightness();
    brightness_percent_ = computePercent(backend_->currentBrightness());
    connect(backend_.get(), &BrightnessBackend::brightnessChanged,
            this, &BrightnessService::onBrightnessChanged);
}
```

#### Percent conversion

```cpp
[[nodiscard]] int BrightnessService::computePercent(int raw) const {
    if (max_brightness_ == 0) {
        return 0;
    }
    return static_cast<int>(std::round(raw * 100.0 / max_brightness_));
}
```

Division-by-zero guard for the no-backlight case (REQ-F-005).

#### setBrightnessPercent

```cpp
void BrightnessService::setBrightnessPercent(int percent) {
    if (max_brightness_ == 0) {
        return;
    }
    const int raw = static_cast<int>(std::round(percent * max_brightness_ / 100.0));
    backend_->setBrightness(raw);
}
```

Does not update `brightness_percent_` immediately. The value is confirmed only when the backend's
`SetBrightness` D-Bus call completes and inotify fires (or the slider position is held). This
avoids showing a "confirmed" state that the hardware rejected.

**clang-tidy note:** local variable name `raw` is exactly 3 characters — passes
`readability-identifier-length`. `percent` and `max_brightness_` are also fine.

#### onBrightnessChanged slot

```cpp
void BrightnessService::onBrightnessChanged(int new_raw) {
    const int new_pct = computePercent(new_raw);
    if (new_pct == brightness_percent_) {
        return;
    }
    brightness_percent_ = new_pct;
    emit brightnessPercentChanged(brightness_percent_);
}
```

#### Signals

```cpp
Q_SIGNALS:
    void brightnessPercentChanged(int percent);
```

No `hasBacklightChanged` or `maxBrightnessChanged` signals are needed because both properties are
`CONSTANT`.

#### Logging category

```cpp
Q_LOGGING_CATEGORY(lcBrightness, "holonight.brightness")
```

Defined in `BrightnessService.cpp`. All other brightness source files log under the same category
so a single `QT_LOGGING_RULES="holonight.brightness.info=true"` shows the full feature trace
(REQ-NF-005).

---

### BrightnessSlider.qml

**File:** `src/qml/RightSidebar/BrightnessSlider.qml`

A self-contained quick-settings row. Visible only when `BrightnessService.hasBacklight` is true.

#### Structure

```
BrightnessSlider (Item, root)
  visible: BrightnessService.hasBacklight

  ColumnLayout
    Controls.Label  "Brightness"
      color: HoloniightPalette.onSurface

    Controls.Slider
      id: brightnessSlider
      from: 0; to: 100; stepSize: 1
      Layout.fillWidth: true
      value: BrightnessService.brightnessPercent   ← initial / non-drag sync
```

#### Throttle logic

```qml
Timer {
    id: writeThrottle
    interval: 100
    repeat: false
    onTriggered: BrightnessService.setBrightnessPercent(brightnessSlider.value)
}
```

`Slider.onValueChanged`:

```qml
onValueChanged: {
    if (brightnessSlider.pressed) {
        writeThrottle.restart()
    }
}
```

`Slider.onPressedChanged`:

```qml
onPressedChanged: {
    if (!pressed) {
        writeThrottle.stop()
        BrightnessService.setBrightnessPercent(brightnessSlider.value)
    }
}
```

Write-on-release (REQ-F-015): when `pressed` transitions to `false`, cancel the pending throttle
timer and call `setBrightnessPercent` immediately with the final slider value. This guarantees the
final value is always written even if the 100 ms window has not elapsed.

#### Feedback-loop guard

Incoming external brightness changes must not fight an active drag. The `Connections` handler
reads:

```qml
Connections {
    target: BrightnessService
    function onBrightnessPercentChanged(percent) {
        if (!brightnessSlider.pressed) {
            brightnessSlider.value = percent
        }
    }
}
```

When the user is dragging (`brightnessSlider.pressed == true`), external updates are silently
ignored. The initial `value: BrightnessService.brightnessPercent` declarative binding establishes
the correct starting position; after that, updates are imperative (via `Connections`) to allow
the drag guard. QML's declarative binding on `value` and the imperative `Connections` assignment
must not both be active simultaneously — use the declarative binding only for the initial value,
then let the `Connections` handler own subsequent updates.

**Pattern:** declare `value: BrightnessService.brightnessPercent` on the `Slider`. The first
time the slider is shown, QML evaluates the binding to set the initial position. Once the user
touches the slider, the binding is broken (Qt behavior for writable properties). From that point
forward only the `Connections` handler drives updates, providing the drag guard.

#### Styling

All colors use `HoloniightPalette` tokens (REQ-NF-003). No hardcoded `#` hex values. Import:

```qml
import Holonight
import HolonightShell
import QtQuick.Controls as Controls
import QtQuick.Layouts
```

The label and slider track use `HoloniightPalette.onSurface` for text and
`HoloniightPalette.accentCyan` for the filled track / handle, consistent with other
quick-settings controls.

---

### SidebarQuickSettings.qml (modification)

**File:** `src/qml/RightSidebar/SidebarQuickSettings.qml`

Add `BrightnessSlider {}` as a direct child of the root `ColumnLayout` below the existing
`KeepAwake` row. Because `BrightnessSlider` is `visible: BrightnessService.hasBacklight`, on
desktops with no backlight the element takes no space in the layout (QML layout excludes
invisible items from geometry computation by default).

The modification adds one import and one component instantiation. No structural changes to the
existing KeepAwake row are needed.

---

## Data Flow

### Startup

```
main.cpp
  │
  ├─ new BrightnessService()           ← production ctor
  │     │
  │     ├─ makeBrightnessBackend()
  │     │     ├─ new SysfsBackend()
  │     │     │     ├─ scan /sys/class/backlight/
  │     │     │     ├─ select highest-max device
  │     │     │     ├─ readBrightness() → initial raw value
  │     │     │     ├─ GetSessionByPID() → session_path_
  │     │     │     └─ inotify_init1() + inotify_add_watch() + QSocketNotifier
  │     │     └─ if maxBrightness == 0: return NullBrightnessBackend
  │     │
  │     ├─ max_brightness_ = backend_->maxBrightness()
  │     ├─ brightness_percent_ = computePercent(backend_->currentBrightness())
  │     └─ connect(brightnessChanged → onBrightnessChanged)
  │
  └─ qmlRegisterSingletonInstance<BrightnessService>(...)
        │
        └─ QML engine starts → BrightnessSlider reads BrightnessService.brightnessPercent
```

### User drag (write path)

```
User drags slider
  │
  Slider.onValueChanged fires (many times)
  │
  writeThrottle.restart()             ← 100 ms one-shot timer reset each event
  │
  (100 ms passes with no new event)
  │
  writeThrottle.onTriggered
      BrightnessService.setBrightnessPercent(value)
          │
          backend_->setBrightness(raw)
              │
              QDBusInterface::asyncCall("SetBrightness", "backlight", device, quint32(raw))
                  │
                  logind → kernel → hardware
```

On release:
```
User releases slider (Slider.pressed → false)
  │
  Slider.onPressedChanged
      writeThrottle.stop()
      BrightnessService.setBrightnessPercent(finalValue)   ← immediate, unconditional
```

### External change (read path)

```
Fn+F5 pressed (kernel / firmware changes /sys/class/backlight/<device>/brightness)
  │
  inotify IN_MODIFY event on ifd_
  │
  QSocketNotifier::activated → SysfsBackend::onInotifyEvent()
      drain inotify buffer (read until EAGAIN)
      readBrightness() → new raw int
      emit brightnessChanged(raw)
  │
  BrightnessService::onBrightnessChanged(raw)
      computePercent(raw) → new_pct
      if new_pct != brightness_percent_:
          brightness_percent_ = new_pct
          emit brightnessPercentChanged(new_pct)
  │
  BrightnessSlider Connections::onBrightnessPercentChanged(pct)
      if !brightnessSlider.pressed:
          brightnessSlider.value = pct   ← slider moves
```

---

## Key Decisions

| Decision | Choice | Rationale | Alternative considered |
|----------|--------|-----------|----------------------|
| Write mechanism | logind `SetBrightness` D-Bus | Required by SPEC (REQ-F-008, REQ-C-003); avoids root/setuid for sysfs writes; consistent with session ownership of hardware | Direct sysfs write (`echo N > brightness`) — requires write permission not available to unprivileged user processes |
| Change detection | inotify `IN_MODIFY` + `QSocketNotifier` | Zero CPU overhead between changes; sub-millisecond latency; required by REQ-F-010 | Polling timer — wastes CPU, violates REQ-F-010 |
| Device selection | Highest `max_brightness` wins | Selects the primary internal display backlight in mixed-device setups; deterministic and unambiguous (REQ-C-004) | Alphabetical first, or user config — adds complexity, no SPEC requirement |
| Fallback on no device | `NullBrightnessBackend` | Maintains `maxBrightness == 0` contract so `hasBacklight` is false and slider is hidden; no error logged | Returning `nullptr` from factory — requires null checks everywhere in service |
| No-op `CONSTANT` for `maxBrightness` / `hasBacklight` | Both `CONSTANT` Q_PROPERTY | These are set once in the constructor and never change; `CONSTANT` allows QML to avoid redundant re-evaluations and no NOTIFY signal is needed | `NOTIFY` with a `maxBrightnessChanged` signal — adds unnecessary signal complexity for a value that cannot change |
| Throttle granularity | 100 ms `Timer` | Balances hardware write frequency with responsiveness (REQ-F-014); ~10 calls/sec max during drag | 50 ms (too many D-Bus calls), 200 ms (perceptible lag at end of drag) |
| Write-on-release | Cancel timer + immediate call in `onPressedChanged` | REQ-F-015; guarantees final slider value is always written even if throttle window is still open | Rely on throttle alone — final value missed if release lands in a silent window |
| Feedback-loop guard | `if (!pressed)` in `Connections` handler | Prevents external brightness updates from resetting slider position mid-drag (REQ-F-016 vs drag UX) | Bidirectional binding with debounce flag — more complex, same result |
| inotify buffer drain strategy | `IN_NONBLOCK` + loop until `EAGAIN` | Coalesces burst events from rapid Fn key presses into a single `brightnessChanged` emission (REQ-F-009) | Read one event per `activated` signal — multiple rapid events queue multiple reads |
| Session path resolution | Two-step: `GetSessionByPID` → seat0 fallback | Matches `IdleService::subscribeLockedHint` proven pattern; covers both DM/TTY and UWSM/systemd user service launch modes | Only `GetSessionByPID` — fails for UWSM launches |
| Test seam | `unique_ptr<BrightnessBackend>` ctor overload | Matches `IdleService` / `SessionService` pattern already in codebase; allows injecting a mock backend for unit tests | Friend class / protected factory method — more invasive |
| Logging category | `"holonight.brightness"` single category for all files | One `QT_LOGGING_RULES` pattern enables the entire feature trace (REQ-NF-005) | Per-class categories — splits the log trace across multiple patterns |
| `brightnessPercent` update on write | Not updated until inotify fires | Write via D-Bus is async; premature update could show confirmed state before hardware acknowledges | Optimistic update — slider feels snappier but mismatches if write is denied (REQ-NF-002) |

---

## File Map

| File | Role |
|------|------|
| `src/services/brightness/BrightnessBackend.h` | Abstract base; `brightnessChanged` signal |
| `src/services/brightness/BrightnessBackend.cpp` | Protected ctor implementation |
| `src/services/brightness/NullBrightnessBackend.h` | No-op fallback header |
| `src/services/brightness/NullBrightnessBackend.cpp` | No-op fallback implementation |
| `src/services/brightness/SysfsBackend.h` | Production backend header |
| `src/services/brightness/SysfsBackend.cpp` | Device scan, inotify, logind D-Bus write |
| `src/services/brightness/BrightnessService.h` | QML singleton header; Q_PROPERTY declarations |
| `src/services/brightness/BrightnessService.cpp` | Factory, percent conversions, signal relay |
| `src/qml/RightSidebar/BrightnessSlider.qml` | Slider component with throttle and drag guard |
| `src/qml/RightSidebar/SidebarQuickSettings.qml` | Modified: add `BrightnessSlider {}` |
| `src/main.cpp` | Modified: `qmlRegisterSingletonInstance<BrightnessService>()` |

---

## CMake Integration

All brightness C++ source files are added directly to `holonight_services` (the existing
`STATIC` library target), following the same pattern used by `idle/`, `session/`, `audio/`, and
all other service subdirectories. There is no separate `add_subdirectory` for brightness — the
flat source list in `holonight_services` in `CMakeLists.txt` is extended:

```cmake
# In the holonight_services add_library() block:
src/services/brightness/BrightnessBackend.h
src/services/brightness/BrightnessBackend.cpp
src/services/brightness/NullBrightnessBackend.h
src/services/brightness/NullBrightnessBackend.cpp
src/services/brightness/SysfsBackend.h
src/services/brightness/SysfsBackend.cpp
src/services/brightness/BrightnessService.h
src/services/brightness/BrightnessService.cpp
```

The brightness include directory is added to `target_include_directories(holonight_services PUBLIC ...)`:

```cmake
${CMAKE_CURRENT_SOURCE_DIR}/src/services/brightness
```

No new `pkg_check_modules` calls needed — the feature uses only Linux system headers (`<sys/inotify.h>`, `<unistd.h>`) and Qt modules already linked into `holonight_services` (Qt6::Core, Qt6::DBus).

`BrightnessSlider.qml` is added to `HOLONIGHT_QML_FILES` in the executable's source list. The
list must remain alphabetically sorted (CMake `list(SORT ...)` + the discoverer cross-check
enforce this):

```cmake
src/qml/RightSidebar/BrightnessSlider.qml    # inserted before KeepAwakeAction.qml
```

After this change, `task qmltypes-check` must pass (the generated `holonight-shell.qmltypes`
must include `BrightnessService`).

---

## Known Risks

**R1 — inotify on sysfs pseudo-filesystem reliability**
inotify `IN_MODIFY` on `/sys/class/backlight/<device>/brightness` works correctly on most
kernels but is not guaranteed by the kernel ABI for all drivers. Some GPU drivers update the
sysfs value without issuing `fsnotify` events. If a driver does not emit `IN_MODIFY`, external
brightness changes will be silently missed. Mitigation: document the limitation; affected users
will still be able to drag the slider. No polling fallback is added (REQ-F-010 forbids it).

**R2 — logind session path unavailable**
On unusual session configurations, both `GetSessionByPID` and the seat0 `ActiveSession` fallback
may fail, leaving `session_path_` empty. `SysfsBackend::setBrightness` must guard against this:
if `session_path_` is empty, log `qCWarning` and return without attempting the D-Bus call.
Brightness reads and inotify still work — only writes are disabled.

**R3 — D-Bus `SetBrightness` permission denied**
logind's `SetBrightness` respects polkit. On locked-down systems the call may be denied. The
`QDBusPendingCallWatcher` callback logs the error (REQ-NF-002). The slider UI remains interactive
but brightness does not change — a clear mismatch. Mitigation: log the polkit error message;
no UI indication beyond the slider not moving (out of scope per SPEC non-goals).

**R4 — QSocketNotifier fd lifetime**
`QSocketNotifier` holds a raw fd. If `ifd_` is closed before the notifier is destroyed, the
notifier will attempt to poll an invalid fd, potentially causing undefined behaviour. Guard:
member declaration order puts `notifier_` (unique_ptr) after `ifd_` so the notifier destructor
(which calls `setEnabled(false)`) fires before `close(ifd_)` in `~SysfsBackend`. Verify member
order is never accidentally reordered.

**R5 — Concurrent inotify events during rapid Fn key presses**
With `IN_NONBLOCK` the drain loop in `onInotifyEvent` reads all pending events before doing a
single sysfs re-read. This is correct, but rapid key presses within a single Qt event-loop tick
will all be coalesced to one read. This is the desired behaviour (REQ-F-009). No additional
action required.

**R6 — `CONSTANT` property misuse**
Declaring `maxBrightness` and `hasBacklight` as `CONSTANT` means QML will never re-evaluate
them after the first access. If the backend were ever replaced at runtime (it is not — there is
no such code path), QML bindings would be stale. The design explicitly prohibits backend
replacement during the application lifetime (REQ-C-006 analogue for the service layer).

---

## Out of Scope

Per SPEC non-goals and constraints:

- DDC/CI external monitor brightness
- Gamma, colour temperature, night-light
- Per-monitor or multi-device brightness
- Keyboard backlight (`/sys/class/leds/`)
- Direct sysfs writes (only logind D-Bus is used)
- Brightness curves or profiles
- Persistence of brightness value across sessions (delegated to logind / firmware)
- UI indication of polkit denial (beyond the silent slider not moving)
- Polling fallback if inotify does not fire for a given driver
