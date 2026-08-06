# Design: Notification History

## Overview

This feature extends `NotificationService` and `ConfigService` with bounded persistent notification history, and adds a `NotificationsWidget` topbar icon with an unread badge. Notifications that expire or are closed by the application are moved to a JSON-backed history list; notifications dismissed by the user are never recorded. The key invariant is that `unread_count_` reflects only entries that arrived since the last sidebar open — entries loaded from disk at startup are always pre-marked `read=true` and do not contribute to the badge. The core notification live-model, timer, and placement logic are unchanged.

---

## New Components

### `NotificationHistoryConfig` struct

**Purpose:** Carries the four user-configurable knobs for the history subsystem.

**Key fields:**

```cpp
struct NotificationHistoryConfig {
    bool enabled{true};
    int  max_items{100};
    int  max_age_days{14};
    bool persist_body{true};

    bool operator==(const NotificationHistoryConfig&) const = default;
};
```

**Public interface:** Plain data struct — no methods beyond the defaulted equality operator.

**Ownership / lifetime:** Value-copied into `ConfigService` and into `NotificationService::history_config_` at construction; refreshed via `notificationHistoryChanged` signal when the config file is reloaded.

---

### `NotificationHistoryItem` struct

**Purpose:** Represents one archived notification entry stored in memory and on disk.

**Key fields:**

```cpp
struct NotificationHistoryItem {
    uint32_t              id{0};
    QString               appName;
    QString               summary;
    QString               body;        // empty when persist_body=false or absent on load
    QString               appIcon;
    QString               appClass;    // from hints["desktop-entry"] at history-write time
    int                   urgency{1};  // int cast of NotifUrgency
    qint64                timestampMs{0};
    int                   closedReason{0}; // int cast of NotifCloseReason
    bool                  read{false};
    QList<NotifAction>    actions;
};
```

**Public interface:** Plain data struct.

**Ownership / lifetime:** Stored by value in `NotificationService::history_` (a `QList<NotificationHistoryItem>`). Copies are passed to `NotificationStore::persist()` — the store never holds a reference to live service state.

**File:** Added to `src/services/notifications/NotificationTypes.h`.

---

### `NotificationStore` class

**Purpose:** Handles all async JSON I/O for the notification history file; owns no in-memory history list.

**Key fields / state:**

```cpp
// src/services/notifications/NotificationStore.h
class NotificationStore : public QObject {
    Q_OBJECT
  public:
    explicit NotificationStore(const NotificationHistoryConfig& config,
                               QObject* parent = nullptr);

    // Synchronous. Called once at startup before any signal connections are made.
    // Returns the parsed entries (valid JSON only; malformed entries are skipped with qCWarning).
    [[nodiscard]] QList<NotificationHistoryItem> load();

    // Async. Schedules a write of the full current history snapshot.
    // Thread-safe to call from the main thread only.
    void persist(QList<NotificationHistoryItem> snapshot);

    [[nodiscard]] QString historyFilePath() const;

  Q_SIGNALS:
    // Emitted on the main thread after each write completes.
    void writeCompleted();

  private Q_SLOTS:
    void onWriteFinished();

  private:
    void launchWrite(QList<NotificationHistoryItem> snapshot);
    void ensureDirectoryExists();

    NotificationHistoryConfig config_;
    QString file_path_;
    bool    write_in_flight_{false};
    bool    write_dirty_{false};
    QList<NotificationHistoryItem> pending_snapshot_;  // held only while dirty
    QFutureWatcher<void>* watcher_{nullptr};
};
```

**Ownership / lifetime:** Owned by `NotificationService` as a `NotificationStore*` (heap, `this` parent). Constructed in `NotificationService`'s constructor before `load()` is called.

---

### `NotificationsWidget.qml`

**Purpose:** Topbar widget that shows the notification bell icon with an unread badge and delegates sidebar toggle on tap.

**File:** `src/qml/Topbar/NotificationsWidget.qml`

**Key properties:**

```qml
BarSection {
    required property string barMonitorName

    // Derived from NotificationService.unreadCount
    readonly property int unreadCount: NotificationService.unreadCount

    // Tooltip content
    readonly property string unreadLabel: unreadCount + " unread"
    readonly property string appList: /* deduplicated, sorted, comma-separated app names
                                         from unread history entries — computed in JS */

    // Icon opacity: 0.55 when no unread, 1.0 when unread > 0
    // Badge dot: 6×6px violet Rectangle, visible when unreadCount > 0
    // Badge glow: MultiEffect (declared before dot) with shadowColor: HoloniightPalette.accentViolet
    // Pulse: SequentialAnimation, 0.25 ↔ 0.9 at 900ms InOutSine, runs while unreadCount > 0
}
```

**Handlers:**
- `HoverHandler` — triggers `BarTooltipArea` hover state.
- `TapHandler` — calls `SidebarManager.toggle(root.barMonitorName)` on tap.
- `BarTooltipArea` — title: `unreadLabel`; description: `appList` (elided with `Text.ElideRight`).

**Icon:** SVG from `bar-icons/` bundled assets, rendered as `Image` with `source: "qrc:/HolonightShell/bar-icons/notifications.svg"`. Opacity animates between 0.55 and 1.0 via `Behavior on opacity { NumberAnimation { duration: 120 } }`.

**Ownership / lifetime:** Instantiated inline in `StatusesSection.qml`; lifetime is the topbar surface lifetime.

---

## Changes to Existing Components

### `NotificationTypes.h`

Add the `NotificationHistoryItem` struct (defined above) after the existing `NotifAction` struct. No existing types are modified.

---

### `ConfigService.h` / `ConfigService.cpp`

**Header additions:**

```cpp
// In ConfigService.h — new struct before class declaration:
struct NotificationHistoryConfig {
    bool enabled{true};
    int  max_items{100};
    int  max_age_days{14};
    bool persist_body{true};

    bool operator==(const NotificationHistoryConfig&) const = default;
};

// New getter inside ConfigService class:
[[nodiscard]] const NotificationHistoryConfig& notificationHistory() const
    { return notification_history_; }

// New signal:
void notificationHistoryChanged();

// New private field:
NotificationHistoryConfig notification_history_;
```

**Implementation additions (`ConfigService.cpp`):**

Parse a `[notifications.history]` TOML table inside `parseFile()`. Keys: `enabled` (bool), `max_items` (int, clamped to `[1, 1000]`), `max_age_days` (int, clamped to `[0, 3650]`), `persist_body` (bool). Emit `notificationHistoryChanged()` when the parsed value differs from the current `notification_history_`.

Defaults are provided by the struct initializers; missing keys leave the defaults intact.

---

### `NotificationService.h` / `NotificationService.cpp`

**New private fields:**

```cpp
QList<NotificationHistoryItem> history_;        // append-ordered; oldest at front
int                            unread_count_{0};
NotificationStore*             store_{nullptr};  // owned, parent = this
NotificationHistoryConfig      history_config_;  // value copy, refreshed on signal
```

**New Q_PROPERTY:**

```cpp
Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
```

```cpp
[[nodiscard]] int unreadCount() const { return unread_count_; }
```

**New signal:**

```cpp
void unreadCountChanged();
```

**New private slot:**

```cpp
void onSidebarOpened(const QString& monitor_name);
```

Sets every `history_[i].read = true`, resets `unread_count_ = 0`, emits `unreadCountChanged()`, calls `store_->persist(history_)`. The `monitor_name` parameter is accepted but not filtered — any sidebar open clears all unread regardless of monitor (per REQ-F-011).

**Constructor changes:**

1. Copy `config->notificationHistory()` into `history_config_`.
2. Instantiate `store_ = new NotificationStore(history_config_, this)`.
3. Call `history_ = store_->load()` — synchronous.
4. Run age eviction on `history_` (synchronous, before any signal emission).
5. Set all loaded entries' `read = true` and `unread_count_ = 0` (REQ-F-012).
6. Connect `config_->notificationHistoryChanged()` → `onConfigChanged()` (extends existing slot to refresh `history_config_`).
7. Connect `SidebarManager::sidebarOpened` → `onSidebarOpened()`. `SidebarManager` is accessed via its `QML_SINGLETON` or passed in via a constructor parameter — TBD at implementation time; prefer constructor injection for testability.
8. Connect `store_->writeCompleted()` → a lambda that runs eviction on `history_` and emits `unreadCountChanged()` if `unread_count_` changed.

**Changes to `closeNotification()`:**

After the notification is removed from the live model and `notificationClosed` is emitted, add:

```
if (history_config_.enabled
    && (reason == NotifCloseReason::Expired || reason == NotifCloseReason::Closed)) {
    // Build NotificationHistoryItem from the (already-erased) NotificationData snapshot.
    // appClass is extracted from the captured hints["desktop-entry"] at this point.
    history_.append(item);
    ++unread_count_;
    emit unreadCountChanged();
    store_->persist(history_);
}
```

The `NotificationData` snapshot must be captured before `all_notifications_.remove(notif_id)` is called.

---

### `StatusesSection.qml`

Insert `NotificationsWidget` between `BatteryWidget` and `KeyboardLayoutWidget`:

```qml
BatteryWidget {
    barMonitorName: root.barMonitorName
    Layout.alignment: Qt.AlignVCenter
}

NotificationsWidget {
    barMonitorName: root.barMonitorName
    Layout.alignment: Qt.AlignVCenter
}

KeyboardLayoutWidget {
    barMonitorName: root.barMonitorName
    Layout.alignment: Qt.AlignVCenter
}
```

No other changes to `StatusesSection.qml`.

---

## Data Flow

### A. Notification arrives → expires → enters history

1. `NotificationServer` calls `NotificationService::addOrReplace(data)` — notification inserted into live model, `armTimeout()` starts a `QTimer`.
2. Timer fires; lambda calls `closeNotification(id, NotifCloseReason::Expired)`.
3. `closeNotification()` captures a copy of `NotificationData` before erasing it from `all_notifications_`.
4. Notification is removed from `visible_by_monitor_`, `row_order_`, and `all_notifications_`; `notificationClosed` is emitted; queue promotion runs.
5. History gate check: `history_config_.enabled == true` and `reason == Expired` → proceed.
6. `NotificationHistoryItem` is built: fields copied from the captured data; `appClass` extracted from `captured_hints["desktop-entry"]`; `timestampMs = QDateTime::currentMSecsSinceEpoch()`; `read = false`.
7. Item appended to `history_`; `++unread_count_`; `unreadCountChanged()` emitted.
8. `store_->persist(history_)` called — snapshot copy passed to the store.
9. `NotificationStore` launches a `QtConcurrent::run()` worker that serializes the snapshot to JSON and writes to `history.json`; on completion `writeCompleted()` is emitted on the main thread.
10. `writeCompleted` handler in `NotificationService` runs eviction (age then capacity) on `history_`; emits `unreadCountChanged()` if `unread_count_` was affected.
11. `NotificationsWidget.unreadCount` binding updates; badge dot appears (opacity 1.0, pulse animation starts).

### B. User clicks notification icon → sidebar opens → all marked read

1. `TapHandler` in `NotificationsWidget` calls `SidebarManager.toggle(root.barMonitorName)`.
2. `SidebarManager::toggle()` calls `openOnMonitor()`; at the end emits `sidebarOpened(monitor_name)`.
3. `NotificationService::onSidebarOpened()` fires: iterates `history_`, sets `read = true` on all entries; sets `unread_count_ = 0`; emits `unreadCountChanged()`.
4. `store_->persist(history_)` called to flush the read-state update to disk.
5. `NotificationsWidget.unreadCount` binding updates to 0: badge dot becomes invisible; icon opacity animates to 0.55.

---

## Async Write Serialization

`NotificationStore` uses two bool flags to guarantee at most one `QtConcurrent::run()` write is in flight at any time while ensuring the file eventually reflects the latest state:

- `write_in_flight_` — set to `true` when a worker is running.
- `write_dirty_` — set to `true` when `persist()` is called while a write is already in flight; in that case the new snapshot is stored in `pending_snapshot_`.

**`persist(snapshot)` logic:**

```
if (!write_in_flight_) {
    write_in_flight_ = true;
    write_dirty_ = false;
    launchWrite(std::move(snapshot));
} else {
    write_dirty_ = true;
    pending_snapshot_ = std::move(snapshot);
}
```

**`onWriteFinished()` logic (main thread, via `QFutureWatcher::finished`):**

```
write_in_flight_ = false;
emit writeCompleted();
if (write_dirty_) {
    write_dirty_ = false;
    write_in_flight_ = true;
    launchWrite(std::move(pending_snapshot_));
}
```

This guarantees:
- No two writes overlap.
- Rapid bursts of `persist()` calls collapse to at most two writes: the current in-flight write and one follow-up.
- The follow-up always carries the most recent snapshot (not an intermediate one).

---

## Eviction Logic

Two passes are always applied in order:

1. **Age eviction:** Remove all entries where `(nowMs - entry.timestampMs) > history_config_.max_age_days * 86'400'000LL`. Uses `QList::removeIf()` for a single pass. Timestamps are UTC milliseconds (REQ-C-003).

2. **Capacity eviction:** If `history_.size() > history_config_.max_items`, call `history_.remove(0, history_.size() - history_config_.max_items)`. Because entries are appended in arrival order, index 0 is always the oldest.

After either pass removes entries, if `unread_count_` now exceeds the number of remaining unread entries (i.e., some evicted entries were unread), recompute:

```
unread_count_ = static_cast<int>(
    std::ranges::count_if(history_, [](const NotificationHistoryItem& e) { return !e.read; }));
```

Eviction runs at two points:

- **Startup** — synchronously after `store_->load()` returns, before any signal emission.
- **Post-write** — on the main thread inside the `writeCompleted` signal handler in `NotificationService`.

---

## JSON Schema

File path: `$XDG_STATE_HOME/holonight/notifications/history.json`  
(defaults to `~/.local/state/holonight/notifications/history.json` per REQ-C-004).

```json
{
  "version": 1,
  "entries": [
    {
      "id": 42,
      "appName": "Firefox",
      "appClass": "org.mozilla.firefox",
      "summary": "Download complete",
      "body": "file.tar.gz",
      "appIcon": "firefox",
      "urgency": 1,
      "timestampMs": 1718000000000,
      "closedReason": 1,
      "read": true,
      "actions": [
        { "key": "open", "label": "Open file" }
      ]
    }
  ]
}
```

**Notes:**

- `body` is omitted entirely when `persist_body=false` (REQ-F-007 / REQ-C-001). On read, a missing `body` key is treated as empty string — not an error.
- `version` is written as `1` and checked on read; a mismatched version logs a `qCWarning` and the file is treated as empty (forward-compat escape hatch).
- `actions` may be an empty array `[]` when the notification carried no actions.
- All integer fields are validated for correct JSON type on load; malformed entries are skipped with `qCWarning` and do not crash the service (REQ-NF-004).
- `id` is stored as a JSON number (fits in a 53-bit JS float; `uint32_t` max is well within range).
- `timestampMs` is a JSON number (64-bit integer stored as `qint64`; `QJsonValue::toDouble` precision is adequate for epoch milliseconds through year 2255).

---

## Key Decisions & Rationale

**History list owned by `NotificationService`, not `NotificationStore`.**  
The in-memory `history_` list is always mutated on the main thread by `NotificationService`. Moving it to `NotificationStore` would require either a mutex around every access or moving all mutation logic into the store, creating a second stateful owner of notification data. The store is kept as a pure I/O helper that receives full snapshots and performs no mutation. This matches the existing design boundary between `NotificationService` (business logic) and `NotificationServer` (protocol I/O).

**Synchronous `load()` at startup.**  
History load is a one-time, startup-only cost on a local file typically smaller than 100 KB. An async load would require every caller of `unreadCount` and `history_` to guard against a "not yet loaded" state, adding complexity (a `ready` flag, deferred signal emissions, QML binding null-guards) with no measurable benefit. Typical history sizes (≤200 entries) serialize and deserialize in under 5 ms on any reasonable hardware, which is well below the 100 ms acceptance threshold in REQ-F-021.

**Dirty-flag serialization over a write queue.**  
Each `persist()` call always writes the full current list — there are no deltas to replay. A `QQueue<snapshot>` would waste memory storing intermediate snapshots that are immediately superseded. Two booleans (`write_in_flight_`, `write_dirty_`) plus one `pending_snapshot_` are sufficient to guarantee eventual consistency: the file will always reflect the state at the time of the last `persist()` call, at the cost of at most one additional write.

**Dismissed notifications excluded from history.**  
This was decided in Stage 0: a user dismissing a notification signals intent to discard, not to archive. Recording dismissed notifications would undermine the mental model ("I swiped it away") and create privacy concerns. The filter is a single `reason == NotifCloseReason::Dismissed` check at the `closeNotification()` call site (REQ-F-003 / REQ-C-002).

**`appClass` extracted from hints at history-write time.**  
`desktop-entry` is a notification hint, not a first-class field in `NotificationData`. Promoting it to `NotificationData` would widen the live model struct for a field only needed by the history subsystem. Extracting it lazily when building `NotificationHistoryItem` keeps the live model clean and defers the string lookup to the already-somewhat-expensive history-write path.

---

## Known Risks

**Clock skew on age eviction.** If the system clock jumps backward (NTP correction, timezone change, hibernation resume), entries may survive longer than `max_age_days`. The eviction predicate is a simple arithmetic check against `QDateTime::currentMSecsSinceEpoch()`; no monotonic-clock correction is applied. This is acceptable for a local history file where the user can manually delete `history.json` if needed.

**Write racing with shutdown.** If the process is killed with `SIGKILL` between a `persist()` call and the write worker completing, the entries that triggered that write may be lost. The dirty-flag pattern mitigates this only for in-flight races between two `persist()` calls — it cannot protect against abrupt termination. Worst case: entries added since the last successfully flushed write are not present on next startup. This is documented as a known limitation; graceful shutdown (`SIGTERM`) allows the in-flight `QFuture` to complete naturally.

**Large history on slow or remote filesystems.** With `max_items=200` and body text included, `history.json` may reach 100–200 KB. The async write prevents main-thread blocking, but on slow NFS mounts the worker thread may queue up. If the shell is exited before the worker finishes, the on-disk state may lag. Users with home directories on network filesystems are advised to set `persist_body=false` or reduce `max_items`.
