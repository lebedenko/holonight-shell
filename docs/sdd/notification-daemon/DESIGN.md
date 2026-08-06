# Notification Daemon (v1) — Design Document

## 1. Overview

The notification daemon integrates `org.freedesktop.Notifications` (freedesktop spec v1.2) directly into the `holonight-shell` binary. It owns the D-Bus service name, maintains an in-memory notification model with full lifecycle tracking, and renders toasts as per-monitor layer-shell surfaces hosted in `src/qml/Notifications/`. The implementation spans three C++ class layers — `NotificationServer` (pure D-Bus protocol), `NotificationService` (business logic + QML-facing model), `NotificationToastSurface` (Wayland layer management) — plus a thin extension to `ActiveWindowService` that surfaces the focused monitor name as a reactive property. Config values are injected via the existing `ConfigService` TOML pipeline. No new Hyprland IPC socket is opened; no separate daemon binary is introduced.

---

## 2. Component Breakdown

### 2.1 `NotificationServer` — NEW
**File:** `src/services/notifications/NotificationServer.h` (and `NotificationServer.cpp`)

Pure D-Bus adapter. Owns no application state; delegates everything to `NotificationService`. Mirrors the `TrayWatcher` pattern: declares `Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")` so Qt's auto-generated introspection XML names the interface correctly (not `local.NotificationServer`). Inherits `QObject` and `QDBusContext`.

Registers on the session bus at startup:
```
QDBusConnection::sessionBus().registerService("org.freedesktop.Notifications");
QDBusConnection::sessionBus().registerObject(
    "/org/freedesktop/Notifications", this,
    QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
```

**Public D-Bus slots (Q_SLOTS):**
```
uint   Notify(QString app_name, uint replaces_id, QString app_icon,
              QString summary, QString body, QStringList actions,
              QVariantMap hints, int expire_timeout);
void   CloseNotification(uint id);
QStringList GetCapabilities();
QStringList GetServerInformation();   // returns (name,vendor,version,spec_version)
```
Note: D-Bus type for `GetServerInformation` is `(ssss)`. Qt D-Bus maps this by returning a `QStringList` of 4 elements, or a custom `QDBusArgument`-registered struct. Prefer a named struct `NotificationServerInfo` registered with `qDBusRegisterMetaType`.

**D-Bus signals (Q_SIGNALS):**
```
void NotificationClosed(uint id, uint reason);
void ActionInvoked(uint id, QString action_key);
```

The class is defined in a `.h` file to ensure correct moc processing (REQ-C-043). The `.cpp` file does NOT need `#include "NotificationServer.moc"` because the class is declared in the header.

---

### 2.2 `NotificationService` — NEW
**File:** `src/services/notifications/NotificationService.h` (and `.cpp`)

QML-facing singleton (`QML_ELEMENT`, `QML_SINGLETON`). Owns the notification model per monitor, allocates IDs, handles replace/queue/timeout logic. Exposes a `QAbstractListModel` of active visible notifications to the QML toast surface. Registered in `ShellApplication` via the same `qmlRegisterSingletonType` lambda pattern used for all other services.

**Key responsibilities:**
- ID allocation: monotonic `uint32_t next_id_` counter, starting at 1.
- Insert / replace: `uint addOrReplace(NotificationData data)` — returns existing ID on replace, freshly allocated on new.
- Per-monitor visible set (`QHash<QString, QList<uint>>`) and per-monitor FIFO queue (`QHash<QString, QList<uint>>`), each bounded by `max_visible_`.
- Timeout management: each active notification owns a `QTimer*`; start/pause/resume on hover.
- Critical priority jump: see §4.4.
- Emitting signals back to `NotificationServer` via Qt signal-slot across the Server→Service direction.
- Config injection: `NotificationService(ConfigService* config, ActiveWindowService* aws, QObject* parent)` — connects to `ConfigService::notificationsChanged` for live-reload.

**QML-exposed model roles:**
```
NotifIdRole, SummaryRole, BodyRole, AppIconRole,
ActionsRole,     // QVariantList of {key, label} maps
AccentKindRole,  // "critical" | "violet" | "cyan"
UrgencyRole,
IsResidentRole,
MonitorRole,
```

**Key Q_INVOKABLEs:**
```
Q_INVOKABLE void invokeAction(uint notif_id, QString action_key);
Q_INVOKABLE void dismiss(uint notif_id);
Q_INVOKABLE void hoverEntered(uint notif_id);
Q_INVOKABLE void hoverLeft(uint notif_id);
```

**Signals (to QML and back to NotificationServer):**
```
void notificationAdded(uint id, QString monitor_name);
void notificationUpdated(uint id);
void notificationClosed(uint id, uint reason);   // relayed → NotificationServer::NotificationClosed
void actionInvoked(uint id, QString action_key); // relayed → NotificationServer::ActionInvoked
void queueChanged(QString monitor_name);
```

---

### 2.3 `NotificationData` / `NotificationTypes` — NEW
**File:** `src/services/notifications/NotificationTypes.h`

Plain data types, no Qt parent. Usable in GTest without QApplication.

```cpp
enum class NotifUrgency : uint8_t { Low = 0, Normal = 1, Critical = 2 };
enum class NotifCloseReason : uint32_t { Expired = 1, Dismissed = 2, Closed = 3 };
enum class NotifAccentKind : uint8_t { Cyan, Violet, Critical };
enum class NotifLifecycle : uint8_t { Visible, Queued, Closed };

struct NotifAction {
    QString key;
    QString label;
};

struct NotificationData {
    uint             id{0};
    QString          app_name;
    QString          app_icon;
    QString          summary;
    QString          body;
    QList<NotifAction> actions;
    QVariantMap      hints;
    int              expire_timeout_ms{-1};  // raw from Notify; policy applied by Service
    NotifUrgency     urgency{NotifUrgency::Normal};
    bool             is_resident{false};
    QString          category;
    NotifAccentKind  accent{NotifAccentKind::Cyan};
    NotifLifecycle   lifecycle{NotifLifecycle::Queued};
    QString          monitor_name;  // assigned at arrival, immutable thereafter
};
```

A free function `NotifAccentKind accentForData(const NotificationData&)` encodes the three-way rule (urgency=2 → Critical, category prefix im./call./presence. → Violet, else Cyan) so it can be unit-tested standalone.

A free function `int effectiveTimeoutMs(const NotificationData& data, int default_timeout_ms)` applies the -1/0/>0 timeout policy:
- `expire_timeout == -1` and urgency < Critical → `default_timeout_ms`
- `expire_timeout == -1` and urgency == Critical → -1 (never)
- `expire_timeout == 0` → -1 (never)
- `expire_timeout > 0` → `expire_timeout`

---

### 2.4 `NotificationToastSurface` — NEW
**File:** `src/surfaces/NotificationToastSurface.h` (and `.cpp`)

Layer-shell surface manager for toasts on a single monitor. Models on `StatusPopupSurface` / `TooltipSurface`. One instance per active monitor (created lazily on first notification for that monitor).

Creates a single `QQuickView` per monitor at the layer `layer_overlay` (above everything) anchored to the top-right corner of the output, with `exclusive_zone = 0` so it doesn't push other panels. The view's root QML item is `qrc:/HolonightShell/Notifications/ToastStack.qml`. It receives a `required property string monitorName` via `setInitialProperties` so the QML can bind to `NotificationService`'s model filtered by monitor.

The surface is created once and kept alive while there are visible or queued notifications on that monitor; destroyed when the per-monitor queue and visible set both empty. `destroySurface()` follows the `deleteLater()` deferred teardown pattern from `StatusPopupSurface`.

Registers `IconImageProvider` on its engine, as `StatusPopupSurface::ensureSurface` does.

**Key methods:**
```cpp
void ensureSurface(const QString& screen_name);
void destroySurface();
bool isActive() const;
```

`NotificationToastSurface` instances are owned by `NotificationManager` (see §2.5).

---

### 2.5 `NotificationManager` — NEW
**File:** `src/surfaces/NotificationManager.h` (and `.cpp`)

Thin orchestrator that bridges `NotificationService` (in `holonight_services`) and the per-monitor `NotificationToastSurface` instances (in `holonight_surfaces`). Lives in `holonight_surfaces` (same layer as `StatusPopupSurface`). Holds a `QHash<QString, NotificationToastSurface*>` keyed by monitor name.

Connects to `NotificationService::notificationAdded` → calls `ensureSurface(monitor_name)`.
Connects to `NotificationService::queueChanged` → destroys surface if both queue and visible set are empty for that monitor.

This class is instantiated in `ShellApplication` alongside other surface managers.

---

### 2.6 QML Toast Components — NEW

All files live under `src/qml/Notifications/` and must be added to `HOLONIGHT_QML_FILES` in `CMakeLists.txt`. QRC prefix: `/HolonightShell/Notifications/...`.

**`ToastStack.qml`** — Root item loaded by `NotificationToastSurface`. Receives `required property string monitorName`. Uses a `Repeater` + `Column` to lay out visible `ToastItem` instances from `NotificationService` model filtered to this monitor, stacked from top with spacing. Drives entry/exit animations.

**`ToastItem.qml`** — A single toast card. Contains:
- `HudFrame` (variant `HudFrame.Popup`) with accent-colored `frameStroke` / `innerGlowColor` driven by `accentKind`.
- `MultiEffect` (glow) declared before content children, with `shadowEnabled: true` and `shadowColor` from `HoloniightPalette` based on accent.
- App icon via `Image { source: "image://icon/" + appIcon }` (uses `IconImageProvider`).
- Summary `Text` in `elide: Text.ElideRight`, `maximumLineCount: 1`.
- Body `Text` in `textFormat: Text.StyledText`, `maximumLineCount: 3`, `elide: Text.ElideRight`. The body string is preprocessed by a JS helper `stripImgTags(body)` before binding (strips `<img ...>` tags using a regex replace).
- `ToastActionBar.qml` for action buttons (shown when `actions.length > 0`).
- `MouseArea` over the non-button body area for body-click handling.

**`ToastActionBar.qml`** — A `Row` of `ToastActionButton` instances, one per action.

**`ToastActionButton.qml`** — HUD outlined button: `Rectangle` with transparent fill and `border.color` from `HoloniightPalette.accentCyan/violet/critical` based on parent accent. Click calls `NotificationService.invokeAction(notifId, actionKey)`.

All colors sourced from `HoloniightPalette.<token>` via `import Holonight`; no hex literals (REQ-C-045). Glow via `QtQuick.Effects.MultiEffect`, not `Qt5Compat` (REQ-C-046).

---

### 2.7 `ActiveWindowService` — MODIFICATION
**File:** `src/services/ActiveWindowService.h` and `ActiveWindowService.cpp` (existing)

Add a `focusedMonitor` Q_PROPERTY backed by the already-tracked `active_window_state_.focused_monitor_name` field and a new `focusedMonitorChanged(QString name)` signal.

```cpp
Q_PROPERTY(QString focusedMonitor READ focusedMonitor NOTIFY focusedMonitorChanged)
Q_SIGNALS:
    void focusedMonitorChanged(const QString& monitor_name);
```

The `focused_monitor_name` field is already populated from two sources in the existing code:
1. `applyActiveWindowEvent` handles the `focusedmon>>` IPC event via `parseHyprlandFocusedMonitorEvent`.
2. `finishCommandResponse` reads the focused monitor from the `j/monitors` JSON response via `parseHyprlandFocusedMonitorNameJson`.

Add `emit focusedMonitorChanged(focused_monitor_name)` in both of those paths when the value actually changes (guard with `if (old_name != new_name)`). No new socket. No new IPC commands. The `ActiveWindowState::focused_monitor_name` field and the `parseHyprlandFocusedMonitorEvent` / `parseHyprlandFocusedMonitorNameJson` parser functions already exist in `src/platform/HyprlandIpc.h`.

---

### 2.8 `ConfigService` — MODIFICATION
**Files:** `src/core/ConfigService.h` and `src/core/ConfigService.cpp` (existing)

Add `NotificationsConfig` struct:
```cpp
struct NotificationsConfig {
    int default_timeout_ms{5000};
    int max_visible{3};
    bool operator==(const NotificationsConfig&) const = default;
};
```

Add to `ConfigService`:
- `notifications_` member field.
- `notifications()` const accessor.
- `notificationsChanged()` signal.
- `parseNotifications(table, missing)` free function in the anonymous namespace.
- `MissingDefaults` extensions: `bool notif_default_timeout{false}`, `bool notif_max_visible{false}`.
- `writeConfig()` and `writeMissingDefaults()` extended with `[notifications]` block.

`NotificationService` constructor takes `ConfigService*` and connects to `notificationsChanged` to re-read `default_timeout_ms` and `max_visible` (applies to future notifications only, REQ-F-040).

---

### 2.9 `ShellApplication` — MODIFICATION
**File:** `src/app/ShellApplication.h` and `ShellApplication.cpp` (existing)

- Add `NotificationServer* notif_server_` field.
- Add `NotificationService* notif_service_` field.
- Add `NotificationManager* notif_manager_` field.
- Construct `NotificationService` with `ConfigService*` and `ActiveWindowService*`; construct `NotificationServer` and wire it to `NotificationService`; construct `NotificationManager`.
- Register `NotificationService` as a QML singleton via the existing `reg()` lambda.
- Call `notif_server_->start()` in `startServices()`.

---

### 2.10 CMake Registration

- Add `src/services/notifications/NotificationServer.h/.cpp`, `NotificationService.h/.cpp`, `NotificationTypes.h` to the `holonight_services` static library target in `CMakeLists.txt`.
- Add `src/surfaces/NotificationToastSurface.h/.cpp`, `NotificationManager.h/.cpp` to `holonight_surfaces`.
- Add all `src/qml/Notifications/*.qml` files to `HOLONIGHT_QML_FILES` (the list must stay exhaustive or CMake will fail with the discovery guard at line 369).
- No new `qt6_add_resources` block needed; QML files are covered by `qt6_add_qml_module`.

---

## 3. Data Model

```cpp
// NotificationTypes.h

enum class NotifUrgency : uint8_t   { Low = 0, Normal = 1, Critical = 2 };
enum class NotifCloseReason : uint32_t { Expired = 1, Dismissed = 2, Closed = 3 };
enum class NotifAccentKind : uint8_t { Cyan, Violet, Critical };
enum class NotifLifecycle : uint8_t  { Visible, Queued, Closed };

struct NotifAction { QString key; QString label; };

struct NotificationData {
    uint32_t         id{0};
    QString          app_name;
    QString          app_icon;       // raw hint value; resolved via IconImageProvider in QML
    QString          summary;
    QString          body;           // may contain markup; <img> stripped before QML binding
    QList<NotifAction> actions;
    QVariantMap      hints;          // full raw hints map kept for future use
    int              expire_timeout_ms{-1};
    NotifUrgency     urgency{NotifUrgency::Normal};
    bool             is_resident{false};
    QString          category;       // from hints["category"]
    NotifAccentKind  accent{NotifAccentKind::Cyan};
    NotifLifecycle   lifecycle{NotifLifecycle::Queued};
    QString          monitor_name;   // assigned at arrival, immutable
    // Runtime state, not persisted
    int              effective_timeout_ms{-1}; // -1 = never
    int              remaining_timeout_ms{-1}; // updated on pause/resume
};
```

Per-monitor state held inside `NotificationService`:
```cpp
QHash<uint32_t, NotificationData>    all_notifications_;      // id → data
QHash<QString, QList<uint32_t>>      visible_by_monitor_;     // monitor → ordered visible ids
QHash<QString, QList<uint32_t>>      queue_by_monitor_;       // monitor → FIFO queued ids
QHash<uint32_t, QTimer*>             timers_;                  // id → active QTimer
uint32_t                             next_id_{1};
int                                  default_timeout_ms_{5000};
int                                  max_visible_{3};
```

---

## 4. Data Flows

### 4a. Inbound Notify → Toast Shown

```
dbus client calls org.freedesktop.Notifications.Notify
  → NotificationServer::Notify(app_name, replaces_id, app_icon,
                                summary, body, actions, hints, expire_timeout)
  → calls NotificationService::addOrReplace(NotificationData built from args)
      • parse hints: urgency → NotifUrgency, category → QString,
                     resident → bool, desktop-entry (for icon fallback)
      • compute accentForData(data) → NotifAccentKind
      • compute effectiveTimeoutMs(data, default_timeout_ms_) → int
      if replaces_id > 0 && all_notifications_.contains(replaces_id):
          update fields in place, reset timer → return replaces_id
      else:
          data.id = next_id_++
          data.monitor_name = ActiveWindowService::focusedMonitor()
                              (fallback: QGuiApplication::primaryScreen()->name())
          all_notifications_.insert(data.id, data)
          place(data.id, data.monitor_name)  // see §4d
          → return data.id
  → NotificationServer returns id to D-Bus caller
  → NotificationService emits notificationAdded(id, monitor_name)
  → NotificationManager::onNotificationAdded(id, monitor_name)
      ensureSurface(monitor_name)   // creates NotificationToastSurface if absent
  → NotificationToastSurface root QML (ToastStack.qml) Repeater reacts to model change
      → ToastItem animates in (fade + slide from right, ~200ms OutCubic)
```

### 4b. Timeout and Hover Pause

```
On insert/replace with effective_timeout_ms > 0:
    timers_[id] = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(remaining_timeout_ms);
    connect(timer, &QTimer::timeout, this, [this, id]{ closeNotification(id, NotifCloseReason::Expired); });
    timer->start();

On hoverEntered(id):
    if (timers_.contains(id) && timers_[id]->isActive()):
        data.remaining_timeout_ms = timers_[id]->remainingTime()
        timers_[id]->stop()

On hoverLeft(id):
    if (timers_.contains(id) && data.remaining_timeout_ms > 0):
        timers_[id]->setInterval(data.remaining_timeout_ms)
        timers_[id]->start()

On replace: stop old timer, start fresh with new effective_timeout_ms.
```

### 4c. Action Invoked → ActionInvoked → Close

```
User clicks ToastActionButton (action_key) or body-click resolving "default"
  → QML calls NotificationService.invokeAction(id, action_key)
  → NotificationService emits actionInvoked(id, action_key)
  → NotificationServer slot connected to actionInvoked emits D-Bus ActionInvoked(id, action_key)
  → if !data.is_resident:
        closeNotification(id, NotifCloseReason::Closed)
            → stop timer, remove from visible/queue
            → model update triggers QML exit animation
            → emit notificationClosed(id, 3)
            → NotificationServer emits D-Bus NotificationClosed(id, 3)
            → promoteFromQueue(monitor_name)
```

### 4d. Overflow and Critical Priority Jump

```
place(id, monitor_name):
    visible = visible_by_monitor_[monitor_name]
    queue   = queue_by_monitor_[monitor_name]

    if visible.size() < max_visible_:
        visible.append(id)
        data.lifecycle = Visible
        startTimer(id)
        return

    // Visible set is full.
    if data.urgency == Critical:
        // Find oldest non-critical visible notification.
        bump_id = find first id in visible where all_notifications_[id].urgency != Critical
        if bump_id is found:
            // Move it to the FRONT of the queue (preserves FIFO for later promotion).
            stopTimer(bump_id)
            all_notifications_[bump_id].lifecycle = Queued
            visible.removeOne(bump_id)
            queue.prepend(bump_id)          // front, not rear — REQ-F-032
            // Insert critical into visible.
            visible.append(id)
            data.lifecycle = Visible
            startTimer(id)
            emit queueChanged(monitor_name)
            return
        // All visible are critical — fall through to queue.

    // Normal overflow: enqueue.
    queue.append(id)
    data.lifecycle = Queued
    emit queueChanged(monitor_name)

promoteFromQueue(monitor_name):
    visible = visible_by_monitor_[monitor_name]
    queue   = queue_by_monitor_[monitor_name]
    if queue.isEmpty() || visible.size() >= max_visible_: return
    promoted_id = queue.takeFirst()
    visible.append(promoted_id)
    all_notifications_[promoted_id].lifecycle = Visible
    startTimer(promoted_id)
    emit queueChanged(monitor_name)
```

### 4e. Config Live-Reload

```
User edits $XDG_CONFIG_HOME/holonight/config.toml
  → QFileSystemWatcher fires → ConfigService::debounce_timer_ → parseFile()
  → parseNotifications() extracts new default_timeout_ms, max_visible
  → if changed: emit ConfigService::notificationsChanged()
  → NotificationService::onConfigChanged()
      default_timeout_ms_ = config->notifications().default_timeout_ms
      max_visible_        = config->notifications().max_visible
      // No effect on existing timers or visible/queue sets.
```

---

## 5. Interfaces and APIs

### 5.1 D-Bus Interface (`org.freedesktop.Notifications`)

Object path: `/org/freedesktop/Notifications`

| Method | D-Bus signature | Notes |
|---|---|---|
| `Notify` | `(ssusssasa{sv}i) → u` | args: app_name, replaces_id, app_icon, summary, body, actions (a{s}), hints (a{sv}), expire_timeout |
| `CloseNotification` | `(u) → ()` | unknown id: no-op, no signal |
| `GetCapabilities` | `() → as` | returns `["body","body-markup","actions","icon-static"]` |
| `GetServerInformation` | `() → ssss` | name, vendor, version, spec_version="1.2" |

| Signal | D-Bus signature | Notes |
|---|---|---|
| `NotificationClosed` | `(uu)` | id, reason (1/2/3) |
| `ActionInvoked` | `(us)` | id, action_key |

**Qt D-Bus type mapping notes:**
- `actions` in `Notify` arrives as `QStringList` (D-Bus type `as`). The service interprets it as consecutive key/label pairs: `actions[0]`=key, `actions[1]`=label, etc.
- `hints` arrives as `QVariantMap` (D-Bus type `a{sv}`). Qt marshals variant values into `QVariant`; urgency is `uchar` accessed via `.toUInt()`, matching the pattern used by `NetworkManagerBackend` for signal strength.
- `GetServerInformation` returns a `NotificationServerInfo` struct registered with `qDBusRegisterMetaType<NotificationServerInfo>()` and `Q_DECLARE_METATYPE`. Alternatively return `QStringList` of 4 and map Qt's marshaller output.

### 5.2 QML-Exposed `NotificationService` API

Model roles (accessed via `model.get(index).roleName` in delegates or via `Repeater`/`ListView`):
```
NotifIdRole       → uint   (role name: "notifId")
SummaryRole       → string ("summary")
BodyRole          → string ("body")          // <img> stripped
AppIconRole       → string ("appIcon")       // name for image://icon/
ActionsRole       → var    ("actions")       // [{key, label}, ...]
AccentKindRole    → string ("accentKind")    // "critical"|"violet"|"cyan"
UrgencyRole       → int    ("urgency")
IsResidentRole    → bool   ("isResident")
MonitorRole       → string ("monitorName")
```

`NotificationService` exposes separate filtered list models per monitor via:
```cpp
Q_INVOKABLE QAbstractItemModel* visibleModelForMonitor(const QString& monitor_name);
```
This returns a `QSortFilterProxyModel` filtering `MonitorRole == monitor_name` and `lifecycle == Visible`. `ToastStack.qml` binds its `Repeater.model` to this.

Additional Q_INVOKABLEs:
```cpp
Q_INVOKABLE void invokeAction(uint notif_id, const QString& action_key);
Q_INVOKABLE void dismiss(uint notif_id);        // reason 2
Q_INVOKABLE void hoverEntered(uint notif_id);
Q_INVOKABLE void hoverLeft(uint notif_id);
```

### 5.3 `ActiveWindowService` Additions

```cpp
// In ActiveWindowService.h — added alongside existing focusedMonitorName() Q_INVOKABLE:
Q_PROPERTY(QString focusedMonitor READ focusedMonitor NOTIFY focusedMonitorChanged)

[[nodiscard]] QString focusedMonitor() const;   // same as focusedMonitorName()

Q_SIGNALS:
    void focusedMonitorChanged(const QString& monitor_name);
```

The implementation reads from the already-populated `active_window_state_.focused_monitor_name`. The signal is emitted in the two existing code paths that update that field:
1. In `processEventLine` when `parseHyprlandFocusedMonitorEvent` returns a value (the `focusedmon` event).
2. In `finishCommandResponse` when the `j/monitors` JSON parse sets the focused monitor at startup.

---

## 6. Key Decisions with Rationale

**Why integrated into holonight-shell rather than a separate `notificationd` binary?**
All other shell services (audio, network, battery, weather) live in-process. Splitting out a daemon would require an additional IPC channel to bridge model state into the shell's QML layer for any future notification indicator or center. The in-process model gives zero-cost direct binding. The freedesktop spec does not require a separate process; the service name `org.freedesktop.Notifications` may be owned by any process.

**Why the `NotificationServer` / `NotificationService` split rather than one class?**
The `TrayWatcher` (pure D-Bus protocol) + `TrayModel` (QML-facing model) pairing is the established project pattern. More concretely: D-Bus slots must use PascalCase to match the protocol (`Notify`, `CloseNotification`), which conflicts with the project's camelCase QML API; mixing both in one class would trigger clang-tidy `readability-identifier-naming` violations or require per-method NOLINT noise. Keeping `NotificationServer` as a protocol adapter and `NotificationService` as a clean Qt model also makes the model fully unit-testable without a live D-Bus connection.

**Why per-monitor queue rather than a global queue?**
REQ-F-034 is explicit. A global queue means a burst on one monitor would delay notifications on another. The Wayland surface for each monitor is independent, so the visible sets are independent; the queues must follow. The implementation cost is a `QHash<QString, QList<uint>>` instead of a flat `QList<uint>`.

**Why reuse `ActiveWindowService`'s IPC socket rather than opening a new one?**
`ActiveWindowService` already parses `focusedmon` events from its event stream (`parseHyprlandFocusedMonitorEvent`, `parseHyprlandFocusedMonitorNameJson`) and populates `active_window_state_.focused_monitor_name`. Adding a `Q_PROPERTY` over that existing field is a three-line change. Opening a second Hyprland socket would consume an extra file descriptor, duplicate event parsing, and introduce a race between the two streams. REQ-F-019 explicitly prohibits the extra socket.

**Why model-driven QML rather than imperative toast creation?**
`StatusPopupSurface` takes an imperative "show one popup at a time" approach which works well for singleton panels. Toasts are a collection: up to `max_visible` concurrently, animated in/out independently. A `QAbstractListModel` + `Repeater` / `ListView` in QML handles addition, removal, and reordering naturally, with Qt's property-binding animations. Imperative per-toast `QQuickView` creation would require the C++ to own N views, manage their geometry relative to each other, and update positions on removal — all work the QML layout engine does for free.

**Why `NotificationToastSurface` one-per-monitor rather than one global surface?**
Layer-shell surfaces are bound to a `wl_output`. A surface on output A cannot render on output B. The existing `LayerShellManager` creates one bar view per screen for the same reason. Toast surfaces follow the same pattern.

---

## 7. Alternatives Considered

**A. Separate `notificationd` binary communicating with the shell via D-Bus or Unix socket.**
Rejected: adds a second process, a second IPC layer, and a dependency on the shell's lifecycle. The integration point for future features (notification center, topbar badge) requires access to the shell's model anyway, so any IPC gain is temporary.

**B. Single `NotificationServer` class combining D-Bus protocol and model logic.**
Rejected for two reasons: (1) the PascalCase vs. camelCase naming collision described in §6; (2) unit-testing model logic (queue, timeout, priority) without a real D-Bus session bus becomes awkward when the D-Bus slots are entangled with business logic. Separation keeps model tests lightweight.

**C. A new Hyprland IPC socket dedicated to monitor focus for the notification daemon.**
Rejected: the existing `ActiveWindowService` socket already receives and parses `focusedmon` events. A second socket doubles resource use with no benefit. The `focusedMonitor` property addition (§2.7) makes the data available to any consumer without any new infrastructure.

**D. Global single-monitor toast surface, always on the primary monitor.**
Rejected: violates REQ-F-019 through REQ-F-022. Multi-monitor workflows (developer with side monitor, Wayland multi-head) depend on toasts appearing where the user is focused.

**E. `QQuickView` per individual toast (N views for N toasts).**
Rejected: layer-shell surfaces have significant compositor overhead; stacking multiple surfaces on the same output for the same visual group is wasteful. A single `QQuickView` per monitor hosting a `Column` of `ToastItem` delegates is the correct Wayland approach. It also avoids the per-surface z-order complications.

---

## 8. Known Risks and Open Questions

**R1: Per-monitor `NotificationToastSurface` lifecycle vs. `StatusPopupSurface` assumptions.**
`StatusPopupSurface` is a singleton that creates/destroys its surface on each toggle. `NotificationToastSurface` must persist across multiple notifications on the same monitor and only destroy when both the visible set and queue for that monitor are empty. The destroy decision must be carefully deferred to avoid destroying the surface while a QML exit animation is still running (same `deleteLater()` guard required). If destroyed too early, in-flight animations will reference a dead view.

**R2: `Text.StyledText` and `<img>` tag stripping.**
Qt's `StyledText` parser does not crash on `<img>` but renders nothing — however, it may leave whitespace artifacts or silently break surrounding text flow. A JS preprocessing step `body.replace(/<img\b[^>]*>/gi, "")` before binding to the `Text.text` property is the safe approach. This must be done in QML (or in `NotificationService.data()` for the `BodyRole`) rather than relying on Qt's parser to silently skip it, to guarantee consistent behavior across Qt versions.

**R3: Focused monitor startup race.**
`ActiveWindowService::start()` queries `j/monitors` asynchronously. If a notification arrives before the first `j/monitors` response completes, `focusedMonitor()` returns an empty string. The fallback to `QGuiApplication::primaryScreen()->name()` (REQ-F-022) handles this correctly, but the notification will be pinned to the primary monitor even if focus was elsewhere. This is documented acceptable behavior.

**R4: `a{sv}` hints marshalling in Qt D-Bus.**
The `hints` parameter of `Notify` is `a{sv}` — a dict of string to variant. Qt D-Bus represents this as `QVariantMap`, but the inner variant values may need explicit `QDBusVariant` unwrapping (`.variant()`) for complex types. The `urgency` hint is a `uint8_t` / `y` type in D-Bus; Qt may deliver it as `uchar` inside a `QVariant`, requiring `.toUInt()` cast — consistent with the `NetworkManagerBackend` pattern for signal strength. Testing with `notify-send` and `busctl` should cover this path early.

**R5: `actions` array D-Bus type ambiguity.**
The freedesktop spec describes actions as `as` (array of string) interpreted as alternating key/label pairs. Some implementations send it as `a(ss)`. Qt D-Bus will receive `as` as `QStringList`; `a(ss)` requires a registered `QList<QPair<QString,QString>>`. Empirical testing with `libnotify` / `notify-send` confirms `as` is the norm, so `QStringList` with pairwise iteration is the right starting point.

**R6: Layer-shell stacking of multiple toasts on the same monitor.**
Multiple `ToastItem` instances within the single `NotificationToastSurface`'s `QQuickView` are stacked vertically by QML layout (a `Column`). Because they share one Wayland surface, there is no compositor-level stacking concern. However, the surface size must grow dynamically as toasts are added. The `QQuickView` is set to `SizeRootObjectToView` which means the root item's `implicitHeight` must drive the surface height. The layer-shell `set_size` must be re-called when the height changes. This requires a `onHeightChanged` handler in `NotificationToastSurface` that calls `surface_->set_size(width, newHeight)` and `wl_surface_commit`.

**R7: Icon resolution for `desktop-entry` hint.**
If `app_icon` is empty but the `desktop-entry` hint is present, the QML should fall back to `"image://icon/" + desktopEntry`. Both resolve via `IconImageProvider` which calls `QIcon::fromTheme`. If the theme has no icon for the given name, `IconImageProvider` logs a warning and returns a null pixmap; the `Image` item will be invisible but will not crash. This is acceptable for v1.

**R8: `resident` hint interaction with timeout.**
A `resident` notification that has a timeout will expire normally (timer fires, reason=1, close). The `resident` flag only suppresses closure on action-button click. This is per-spec behavior but may surprise app developers who send both. Document in the implementation comments.

**R9: GTest model tests without `QApplication`.**
`NotificationData`, `NotifAccentKind`, `effectiveTimeoutMs`, `accentForData`, and the queue management logic should be extracted into pure functions testable with `QCoreApplication` only (no GUI). `NotificationService` interacts with `QTimer` and `QAbstractListModel` which both need an event loop. Tests for queue state should use `QCoreApplication` or mock timers. This mirrors the `ExtWorkspaceManager` test approach.
