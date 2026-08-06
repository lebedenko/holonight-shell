# DESIGN: Per-Monitor Active Window

**SDD Session:** active-window-per-monitor
**Feature:** Per-monitor active window tracking — each bar shows the active window on its own monitor's workspace, not the globally focused window
**Status:** Design
**Last Updated:** 2026-05-23

---

## Overview

The current `ActiveWindowService` maintains a single global active window state: one title, one app class, one category. Every bar on every monitor shows the same window, which is the globally focused window. On a multi-monitor setup this means a secondary monitor's bar shows the title from whichever monitor the user last clicked on.

This change replaces the single-window model with a `QHash<QString, HyprlandActiveWindow>` keyed by monitor name. Each bar queries the service by its own `Screen.name` and receives per-monitor data via a targeted `monitorWindowChanged(monitorName)` signal.

**What changes in which files:**

| File | Change |
|---|---|
| `src/HyprlandIpc.h` | New `HyprlandFocusedMonitor` struct; new `parseHyprlandFocusedMonitorEvent` overload; new JSON parser declarations |
| `src/HyprlandIpc.cpp` | Implement new parsers; replace old `parseHyprlandFocusedMonitorEvent` body |
| `src/ActiveWindowService.h` | Add per-monitor hash, focused-monitor name, `Q_INVOKABLE` getters, new signal; remove legacy `Q_PROPERTY` members |
| `src/ActiveWindowService.cpp` | Rewrite event dispatch; replace startup query; add full re-query helpers; adapt category resolution |
| `src/qml/Topbar/ActiveWindowSection.qml` | Replace direct `ActiveWindowService.title` bindings with local properties updated from `Connections` |
| `CMakeLists.txt` | No changes needed (no new files) |
| `main.cpp` | No changes needed (singleton registration unchanged) |

The old `Q_PROPERTY title`, `Q_PROPERTY appClass`, and `Q_PROPERTY category` on `ActiveWindowService` are removed. QML code that references them directly will fail at lint time; all references must move to the new `Q_INVOKABLE` getters + `Connections` pattern.

---

## Data Model

### Updated `HyprlandActiveWindow` struct

The existing struct in `HyprlandIpc.h` stores only `app_class` and `title`. A `category` field is added so each per-monitor entry carries its full resolved state:

```cpp
struct HyprlandActiveWindow {
  QString app_class;
  QString title;
  QString category;   // NEW — resolved icon token ("browser", "terminal", …)

  [[nodiscard]] bool isEmpty() const { return title.isEmpty() && app_class.isEmpty(); }
};
```

Keeping `category` in the struct avoids a parallel hash for per-monitor categories and simplifies the signal emission logic: one entry update triggers one signal.

### New `HyprlandFocusedMonitor` struct

```cpp
struct HyprlandFocusedMonitor {
  QString monitor_name;    // e.g. "DP-1", "HDMI-1"
  QString workspace_name;  // e.g. "1", "2" (numeric string or named)
};
```

### New `HyprlandMonitorInfo` struct (for `j/monitors` parsing)

```cpp
struct HyprlandMonitorInfo {
  QString name;
  int active_workspace_id;  // from monitors[i].activeWorkspace.id
};
```

### New `HyprlandClientInfo` struct (for `j/clients` parsing)

```cpp
struct HyprlandClientInfo {
  QString app_class;
  QString title;
  int workspace_id;
  int focus_history_id;
};
```

### `ActiveWindowService` in-memory state

```
monitor_windows_        QHash<QString, HyprlandActiveWindow>
  "DP-1"     → { app_class: "firefox",  title: "…", category: "browser" }
  "HDMI-1"   → { app_class: "kitty",    title: "…", category: "terminal" }
  "DP-2"     → { app_class: "",         title: "",   category: "" }   ← empty workspace

focused_monitor_name_   QString   → name of the monitor Hyprland last sent focusedmon>> for

monitor_workspaces_     QHash<QString, int>
  "DP-1"   → 1    ← active workspace ID per monitor; kept in sync from j/monitors responses
  "HDMI-1" → 2
  "DP-2"   → 5
```

`monitor_workspaces_` is the reverse lookup needed by `openwindow>>` handling: given a workspace ID from the event payload, find which monitor has it as its active workspace without issuing a new IPC query.

The existing auxiliary state (`category_cache_`, `resolved_classes_`, socket members, reconnect members) is unchanged.

---

## Sequence Diagrams

### Startup initialization

```
constructor
  └─► connectSocket()
        └─► event socket connects
              └─► onEventSocketConnected()
                    ├─► resetReconnectBackoff()
                    └─► queryAllMonitorWindows()     ← replaces queryActiveWindow()

queryAllMonitorWindows()
  └─► open command socket #1
        └─► onCommandSocketConnected()
              └─► write "j/monitors"

      data arrives
        └─► onCommandSocketReadable()
              ├─► accumulate into command_buffer_
              └─► try parseHyprlandMonitorsJson(command_buffer_)
                    if valid:
                      store monitor names + workspace IDs → monitor_workspaces_
                      finishCommandSocket(Phase::Monitors)

finishCommandSocket(Phase::Monitors)
  ├─► destroy command socket #1
  └─► open command socket #2
        └─► onCommandSocketConnected()
              └─► write "j/clients"

      data arrives
        └─► onCommandSocketReadable()
              ├─► accumulate into command_buffer_
              └─► try parseHyprlandClientsJson(command_buffer_)
                    if valid:
                      finishCommandSocket(Phase::Clients)

finishCommandSocket(Phase::Clients)
  ├─► for each monitor in monitor_workspaces_:
  │     filter clients where client.workspace_id == monitor_workspaces_[monitor]
  │     if any: pick client with lowest focus_history_id
  │     update monitor_windows_[monitor]
  │     emit monitorWindowChanged(monitor)
  │     scheduleResolveCategory(app_class, monitor)
  └─► destroy command socket #2
```

The two sequential command-socket calls (monitors → clients) are serialized using a `CommandPhase` enum:

```cpp
enum class CommandPhase { Idle, Monitors, Clients };
CommandPhase command_phase_{CommandPhase::Idle};
```

`finishCommandSocket` checks `command_phase_` and either moves to the next phase or resets to `Idle`. The existing single `command_socket_` pointer is reused across both phases; a new socket is created for each phase. The JSON buffers for both phases share `command_buffer_` (it is cleared between phases).

To hold the intermediate monitors result across the two-socket sequence, add:

```cpp
QHash<QString, int> pending_monitor_workspaces_;   // populated after Phase::Monitors
QList<HyprlandClientInfo> pending_clients_;        // populated after Phase::Clients
```

These are scratch fields, not part of the live service state; they are cleared after `Phase::Clients` completes.

### `activewindow>>` event

```
onSocketReadable() parses line
  └─► parseHyprlandActiveWindowEvent(line) → HyprlandActiveWindow{app_class, title}
        └─► setMonitorWindow(focused_monitor_name_, app_class, title)
              ├─► monitor_windows_[focused_monitor_name_] = {app_class, title, ""}
              ├─► emit monitorWindowChanged(focused_monitor_name_)
              └─► scheduleResolveCategory(app_class, focused_monitor_name_)
```

If `focused_monitor_name_` is empty (service just started, no `focusedmon>>` received yet), the event is held in the existing last-window slot temporarily until the monitor name is known. In practice, Hyprland sends `focusedmon>>` before any `activewindow>>` on startup, so this edge case is unlikely. If it does occur, the update is dropped (preferred over corrupting an unknown monitor's entry).

### `focusedmon>>` event

```
onSocketReadable() parses line
  └─► parseHyprlandFocusedMonitorEvent(line) → HyprlandFocusedMonitor{monitor_name, workspace_name}
        └─► focused_monitor_name_ = monitor_name
            (no window update, no signal — monitor switch alone changes no displayed window)
```

Only `focused_monitor_name_` is updated. No `monitorWindowChanged` signal fires. The previously focused monitor keeps its last-known window displayed.

### `openwindow>>` event

```
onSocketReadable() parses line
  └─► parseHyprlandOpenWindowEvent(line) → {address, workspace_name, app_class, title}
        └─► affected_monitor = reverse-lookup workspace_name in monitor_workspaces_
              if found:
                setMonitorWindow(affected_monitor, app_class, title)
              else:
                discard (window opened on a non-active workspace — bar unchanged)
```

Workspace-name to monitor lookup: `monitor_workspaces_` stores `monitor → workspace_id` (integer). The `openwindow>>` payload carries the workspace *name* as a string. For numeric workspaces (the common case), parse the string to int and do a reverse scan of `monitor_workspaces_` values. This is O(n) over the number of monitors (typically 1–4), which is negligible.

No IPC query is issued for `openwindow>>`. This keeps the hot path synchronous and cheap.

### `closewindow>>` and `movewindow>>` events

```
onSocketReadable() parses line
  └─► isHyprlandWorkspaceRefreshEvent(line) matches "closewindow>>" or "movewindow>>"
        └─► queryAllMonitorWindows()   ← full re-query (same as startup)
```

The full re-query is triggered because:

- **Close**: the next-most-recently-focused window must be determined from Hyprland, as the service holds no second-slot state.
- **Move**: two monitors are potentially affected (source and destination); determining both requires `j/monitors` (to learn which monitor now owns which workspace) and `j/clients` (to find the new active window on each).

A guard prevents overlapping queries: if `command_phase_ != CommandPhase::Idle`, a new `queryAllMonitorWindows()` call is dropped. Arriving events during an in-flight query are therefore not reflected until the next query completes. This is acceptable given the low frequency of close/move events.

### `workspace>>` event (monitor switches active workspace)

This event fires when a monitor changes its visible workspace (e.g., the user presses Super+2). The active workspace per monitor changes, so `monitor_workspaces_` is stale. Trigger a full re-query:

```
isHyprlandWorkspaceRefreshEvent(line) already matches "workspace>>" and "focusedmon>>"
  └─► workspace>>: re-query to refresh monitor_workspaces_ and pick new active window
```

Wait — `focusedmon>>` is in `isHyprlandWorkspaceRefreshEvent` but should *not* trigger a re-query here. It only carries the monitor name; the active workspace itself may not have changed. The event handler for `focusedmon>>` already handles it before the workspace-refresh check, so it won't reach the re-query branch. `workspace>>` does trigger a re-query because the active workspace on a monitor changed, meaning its displayed window may differ.

---

## Startup Init Flow

### Two-step IPC query sequence

Hyprland's command socket is write-once-read-all per connection: you connect, write one command, read the full response, then the server closes the connection. Two separate commands therefore require two separate socket connections.

The existing `queryActiveWindow()` already uses this pattern for `j/activewindow`. The new `queryAllMonitorWindows()` extends it to chain two connections:

```
Phase 1 (Monitors):
  connect → write "j/monitors" → read until disconnect → parse
  → store monitor names + workspace IDs in pending_monitor_workspaces_
  → open Phase 2 socket

Phase 2 (Clients):
  connect → write "j/clients" → read until disconnect → parse
  → build monitor_windows_ from pending data
  → emit per-monitor signals
  → clear pending scratch state
```

Both phases share the same `command_socket_`, `command_buffer_`, and `command_timeout_` infrastructure from the existing implementation. `command_phase_` distinguishes which phase's response is being accumulated.

### `j/monitors` JSON structure

```json
[
  {
    "name": "DP-1",
    "activeWorkspace": { "id": 1, "name": "1" }
  },
  {
    "name": "HDMI-1",
    "activeWorkspace": { "id": 3, "name": "3" }
  }
]
```

Parser extracts `name` and `activeWorkspace.id` for each entry.

### `j/clients` JSON structure

```json
[
  {
    "address": "0x...",
    "class": "firefox",
    "title": "Mozilla Firefox",
    "workspace": { "id": 1, "name": "1" },
    "focusHistoryID": 2
  },
  {
    "address": "0x...",
    "class": "kitty",
    "title": "~",
    "workspace": { "id": 1, "name": "1" },
    "focusHistoryID": 0
  }
]
```

For a given workspace ID, filter to matching clients and select the one with the **lowest** `focusHistoryID`. This is assumed to be the most recently focused window based on Hyprland's documentation and the spec note (REQ-F-002). Verify empirically during implementation: open two windows on the same workspace, focus the second one, query `j/clients`, and confirm the focused one has the lower `focusHistoryID`.

---

## Per-Monitor Re-Query

### When triggered

| Event | Re-query? | Rationale |
|---|---|---|
| `activewindow>>` | No | Covered by direct update to focused monitor |
| `focusedmon>>` | No | Only changes which monitor is focused; window state unchanged |
| `openwindow>>` | No | Handled by workspace-name lookup in `monitor_workspaces_` |
| `closewindow>>` | Yes | Need to find next-most-recent window on affected workspace |
| `movewindow>>` | Yes | Two monitors may be affected; workspace assignments may change |
| `workspace>>` | Yes | Active workspace on a monitor changed; window on that workspace differs |
| `createworkspace>>` | No | New workspace has no windows; no monitor state changes |
| `destroyworkspace>>` | Yes | A monitor may have lost its active workspace |
| Event socket reconnect | Yes | Full state refresh after reconnection |

### What queries run

Always the same two-phase sequence: `j/monitors` then `j/clients`. This is consistent, simple, and avoids per-event branching logic in the re-query path.

### How results are applied

After Phase 2 (Clients) completes:

```
for each monitor in pending_monitor_workspaces_:
  workspace_id = pending_monitor_workspaces_[monitor]
  candidates = [c for c in pending_clients_ if c.workspace_id == workspace_id]
  if candidates is empty:
    new_entry = HyprlandActiveWindow{}    ← blank
  else:
    best = min(candidates, key=focus_history_id)
    new_entry = HyprlandActiveWindow{ best.app_class, best.title, "" }

  old_entry = monitor_windows_[monitor]
  if new_entry != old_entry:
    monitor_windows_[monitor] = new_entry
    emit monitorWindowChanged(monitor)
    if new_entry.app_class != old_entry.app_class:
      scheduleResolveCategory(new_entry.app_class, monitor)
```

`monitor_workspaces_` is updated from `pending_monitor_workspaces_` at the same time (atomically at end of phase 2). This keeps the reverse-lookup cache current for subsequent `openwindow>>` events.

Comparison uses `app_class` and `title` only (not `category`, which is resolved asynchronously). This avoids spurious signals when only the category is updated.

---

## QML Integration

### Why local properties are required

`Q_INVOKABLE` methods are not reactive in QML property bindings. A binding like:

```qml
text: ActiveWindowService.titleForMonitor(Screen.name)   // NOT reactive
```

evaluates once (on construction) and never re-evaluates because the QML engine sees no `NOTIFY` signal on the call site. The binding is not re-evaluated when `monitorWindowChanged` fires.

The solution is local `property` declarations that are explicitly refreshed from a `Connections` handler:

```qml
property string localTitle: ""
property string localCategory: ""
property string localAppClass: ""

Connections {
    target: ActiveWindowService
    function onMonitorWindowChanged(monitorName) {
        if (monitorName === Screen.name) {
            localTitle    = ActiveWindowService.titleForMonitor(Screen.name)
            localCategory = ActiveWindowService.categoryForMonitor(Screen.name)
            localAppClass = ActiveWindowService.appClassForMonitor(Screen.name)
        }
    }
}

Component.onCompleted: {
    localTitle    = ActiveWindowService.titleForMonitor(Screen.name)
    localCategory = ActiveWindowService.categoryForMonitor(Screen.name)
    localAppClass = ActiveWindowService.appClassForMonitor(Screen.name)
}
```

`Component.onCompleted` handles the initial load (the service is already populated by the time QML finishes constructing, because the startup query runs before `LayerShellManager` creates any `QQuickView`).

### Updated `ActiveWindowSection.qml`

The full diff from current to new:

```qml
// BEFORE (current):
Column {
    visible: ActiveWindowService.title !== ""
    ...
    AppWindowIcon {
        category: ActiveWindowService.category
    }
    Controls.Label {
        text: ActiveWindowService.title
    }
}
BarTooltipArea {
    title: ActiveWindowService.title.length > 0
               ? ActiveWindowService.title : "Active window"
    description: ActiveWindowService.appClass.length > 0
                     ? "Focused app: " + ActiveWindowService.appClass + "."
                     : "No active window information."
}

// AFTER:
BarSection {
    id: root
    implicitWidth: 0

    property string localTitle: ""
    property string localCategory: ""
    property string localAppClass: ""

    Connections {
        target: ActiveWindowService
        function onMonitorWindowChanged(monitorName) {
            if (monitorName === Screen.name) {
                root.localTitle    = ActiveWindowService.titleForMonitor(Screen.name)
                root.localCategory = ActiveWindowService.categoryForMonitor(Screen.name)
                root.localAppClass = ActiveWindowService.appClassForMonitor(Screen.name)
            }
        }
    }

    Component.onCompleted: {
        root.localTitle    = ActiveWindowService.titleForMonitor(Screen.name)
        root.localCategory = ActiveWindowService.categoryForMonitor(Screen.name)
        root.localAppClass = ActiveWindowService.appClassForMonitor(Screen.name)
    }

    Column {
        visible: root.localTitle !== ""
        ...
        AppWindowIcon {
            category: root.localCategory
        }
        Controls.Label {
            text: root.localTitle
        }
    }
    BarTooltipArea {
        title: root.localTitle.length > 0 ? root.localTitle : "Active window"
        description: root.localAppClass.length > 0
                         ? "Focused app: " + root.localAppClass + "."
                         : "No active window information."
    }
}
```

The `import QtQuick.Window` import is required for `Screen.name` to resolve. Add it alongside the existing imports.

---

## Parser Changes

### Replace `parseHyprlandFocusedMonitorEvent`

Current signature returns `std::optional<int>` (workspace ID, discarding monitor name). Replace with:

```cpp
// HyprlandIpc.h — REMOVE:
[[nodiscard]] std::optional<int> parseHyprlandFocusedMonitorEvent(const QByteArray& line);

// HyprlandIpc.h — ADD:
[[nodiscard]] std::optional<HyprlandFocusedMonitor> parseHyprlandFocusedMonitorEvent(const QByteArray& line);
```

New implementation:

```cpp
std::optional<HyprlandFocusedMonitor> parseHyprlandFocusedMonitorEvent(const QByteArray& line) {
  constexpr QByteArrayView kPrefix{"focusedmon>>"};
  if (!line.startsWith(kPrefix)) {
    return std::nullopt;
  }
  const QByteArray payload = line.sliced(kPrefix.size());
  const qsizetype comma = payload.indexOf(',');
  if (comma < 0) {
    return std::nullopt;
  }
  return HyprlandFocusedMonitor{
      .monitor_name   = QString::fromUtf8(payload.left(comma)).trimmed(),
      .workspace_name = QString::fromUtf8(payload.sliced(comma + 1)).trimmed(),
  };
}
```

The caller in `ExtWorkspaceManager` currently uses the old `std::optional<int>` return. It must be updated to use `parsed.value().workspace_name.toInt()` to extract the workspace ID.

### New: `parseHyprlandOpenWindowEvent`

```cpp
struct HyprlandOpenWindow {
  QString address;
  QString workspace_name;
  QString app_class;
  QString title;
};

[[nodiscard]] std::optional<HyprlandOpenWindow> parseHyprlandOpenWindowEvent(const QByteArray& line);
```

Format: `openwindow>>ADDRESS,WORKSPACENAME,CLASS,TITLE`. Implementation: find first three commas, split into four segments. Title may contain commas; take everything after the third comma.

### New: `parseHyprlandMonitorsJson`

```cpp
[[nodiscard]] std::optional<QHash<QString, int>> parseHyprlandMonitorsJson(const QByteArray& response);
```

Returns `monitor_name → active_workspace_id`. Parses a JSON array; for each object extracts `name` (string) and `activeWorkspace.id` (int). Returns `std::nullopt` on malformed JSON (not an array). Skips entries with missing or zero workspace ID.

### New: `parseHyprlandClientsJson`

```cpp
[[nodiscard]] std::optional<QList<HyprlandClientInfo>> parseHyprlandClientsJson(const QByteArray& response);
```

Parses a JSON array; for each object extracts `class`, `title`, `workspace.id`, and `focusHistoryID`. Skips entries with empty class and title. Returns `std::nullopt` on malformed JSON (not an array).

### Impact on `ExtWorkspaceManager`

`ExtWorkspaceManager.cpp` calls the old `parseHyprlandFocusedMonitorEvent` and uses the returned `int` directly. After the signature change, update those call sites to:

```cpp
const auto focused = parseHyprlandFocusedMonitorEvent(line);
if (focused.has_value()) {
    const int workspace_id = focused->workspace_name.toInt();
    // existing logic using workspace_id unchanged
}
```

---

## Category Resolution

### Current mechanism

`scheduleResolveCategory(app_class)` launches `QtConcurrent::run(&scanDesktopFiles, app_class)` and wires a `QFutureWatcher` callback. The callback checks `if (app_class_ == app_class)` to avoid applying a stale result, then calls `setCategory(resolved)`.

### Per-monitor extension

The callback needs to know which monitor to update. Add a `monitor_name` parameter:

```cpp
void scheduleResolveCategory(const QString& app_class, const QString& monitor_name);
```

Updated watcher callback:

```cpp
connect(watcher, &QFutureWatcher<QString>::finished, this,
    [this, watcher, app_class, monitor_name] {
        const QString resolved = watcher->result();
        resolved_classes_.insert(app_class);
        category_cache_.insert(app_class, resolved);

        // Apply to all monitors currently showing this app_class
        for (auto it = monitor_windows_.begin(); it != monitor_windows_.end(); ++it) {
            if (it->app_class == app_class && it->category != resolved) {
                it->category = resolved;
                emit monitorWindowChanged(it.key());
            }
        }
        watcher->deleteLater();
    });
```

The stale-result guard becomes: apply the resolved category to every monitor whose current `app_class` still matches. This handles the case where the same app (e.g., Firefox) is active on multiple monitors simultaneously — all get the category update in one pass. The old guard `if (app_class_ == app_class)` is replaced by the per-entry check inside the loop.

The `monitor_name` parameter is no longer used in the callback (because the loop covers all matching monitors), but it remains in the call signature for clarity and is kept as a named parameter. If no monitors show that app class by the time the scan completes (the user switched away), no signals fire and no state is changed — this is the correct stale-result behavior.

### Cache interaction

`category_cache_` and `resolved_classes_` are unchanged. If a second monitor opens the same app class while a scan is in flight, `resolved_classes_` will not contain the class yet, so a second scan is launched. This is a minor inefficiency (two concurrent scans for the same app class) but is harmless — the second scan's result will also be cached. An in-flight guard (`QSet<QString> scanning_classes_`) could be added to prevent duplicate scans, but this is an optimization for a future pass.

---

## `ActiveWindowService` Full Interface

### Header

```cpp
class ActiveWindowService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  explicit ActiveWindowService(QObject* parent = nullptr);
  ~ActiveWindowService() override = default;

  ActiveWindowService(const ActiveWindowService&) = delete;
  ActiveWindowService& operator=(const ActiveWindowService&) = delete;
  ActiveWindowService(ActiveWindowService&&) = delete;
  ActiveWindowService& operator=(ActiveWindowService&&) = delete;

  Q_INVOKABLE [[nodiscard]] QString titleForMonitor(const QString& monitor_name) const;
  Q_INVOKABLE [[nodiscard]] QString appClassForMonitor(const QString& monitor_name) const;
  Q_INVOKABLE [[nodiscard]] QString categoryForMonitor(const QString& monitor_name) const;

 Q_SIGNALS:
  void monitorWindowChanged(const QString& monitor_name);

 private Q_SLOTS:
  void onSocketReadable();

 private:
  enum class CommandPhase { Idle, Monitors, Clients };

  void connectSocket();
  void queryAllMonitorWindows();
  void onEventSocketConnected();
  void onEventSocketDisconnected();
  void onEventSocketError(QLocalSocket::LocalSocketError error);
  void onCommandSocketConnected();
  void onCommandSocketReadable();
  void onCommandSocketDisconnected();
  void onCommandSocketError(QLocalSocket::LocalSocketError error);
  void scheduleReconnect();
  void resetReconnectBackoff();
  void finishCommandSocket(bool parse_buffer);
  void applyMonitorWindowsFromPending();
  void setMonitorWindow(const QString& monitor_name,
                        const QString& app_class,
                        const QString& title);
  void scheduleResolveCategory(const QString& app_class, const QString& monitor_name);
  [[nodiscard]] static QString socketBasePath();
  [[nodiscard]] static QString eventSocketPath();
  [[nodiscard]] static QString commandSocketPath();
  [[nodiscard]] static QString scanDesktopFiles(const QString& app_class);
  [[nodiscard]] static QString mapCategoriesToIcon(const QString& categories_field);

  // Socket infrastructure (unchanged from current)
  QLocalSocket* socket_{nullptr};
  QLocalSocket* command_socket_{nullptr};
  QTimer* connect_timeout_{nullptr};
  QTimer* reconnect_timer_{nullptr};
  QTimer* command_timeout_{nullptr};
  QByteArray buffer_;
  QByteArray command_buffer_;
  int reconnect_delay_ms_{1000};
  bool reconnect_scheduled_{false};

  // Per-monitor state
  QHash<QString, HyprlandActiveWindow> monitor_windows_;
  QHash<QString, int> monitor_workspaces_;
  QString focused_monitor_name_;

  // Two-phase query scratch state
  CommandPhase command_phase_{CommandPhase::Idle};
  QHash<QString, int> pending_monitor_workspaces_;
  QList<HyprlandClientInfo> pending_clients_;

  // Category resolution cache (unchanged)
  QHash<QString, QString> category_cache_;
  QSet<QString> resolved_classes_;
};
```

The legacy `title_`, `app_class_`, `category_` member variables and their `Q_PROPERTY` declarations are removed entirely.

---

## Key Decisions with Rationale

### 1. Full re-query on close/move vs incremental update

On `closewindow>>`, the closed window's address is known, but not which workspace it was on or which monitor that workspace belongs to without additional lookups. An incremental approach would require the service to track a map from window address to workspace — state that is not currently maintained and would need its own consistency management.

On `movewindow>>`, both the source and destination monitors are affected, requiring at minimum `j/monitors` to determine the current workspace-to-monitor mapping (which may have changed if the move triggered a workspace switch).

Full re-query trades a minor latency increase (two IPC round-trips on close/move) for correctness and simplicity. Close and move events are low-frequency (user-initiated), so the latency is imperceptible.

### 2. Keep singleton vs per-monitor instances

A per-monitor `ActiveWindowService` instance would allow each bar to hold its own state without coordination. However:

- Hyprland's event socket is a single global stream. Multiple service instances would all need to connect and parse the same stream, multiplying socket connections.
- Category resolution caching is most effective when centralized. Per-instance caches would re-scan desktop files independently.
- The singleton pattern is already established and tested in the project. Deviating from it introduces registration complexity in `main.cpp`.

The targeted `monitorWindowChanged(monitorName)` signal gives QML per-monitor granularity without splitting the C++ side.

### 3. `Q_INVOKABLE` getters instead of per-monitor `Q_PROPERTY`

A `Q_PROPERTY` requires a dedicated `NOTIFY` signal. With N monitors, N properties would be needed (`titleForDP1`, `titleForHDMI1`, …), but monitor names are runtime data — they cannot be known at compile time. `Q_INVOKABLE` getters parameterized by monitor name are the only viable approach.

The reactive-update gap (getters not re-evaluated by the binding engine on signal) is bridged by the local-property + `Connections` pattern in QML (see QML Integration section).

### 4. `monitor_workspaces_` reverse-lookup cache for `openwindow>>`

The alternative for `openwindow>>` is to issue a `j/monitors` IPC query to find which monitor owns the new window's workspace. This adds a network round-trip on every window open — unacceptable latency for a common operation (opening a terminal, browser tab, etc.).

The cache is kept in sync by every re-query (close/move/workspace-switch), which are the events that change workspace-to-monitor assignments. `openwindow>>` itself does not change which workspace a monitor is viewing, so the cache remains valid.

### 5. `focusedmon>>` does not trigger a window signal

When the user moves focus to another monitor without changing any window, `focused_monitor_name_` updates but no window data changes. Emitting `monitorWindowChanged` here would cause QML to re-read identical data — a no-op update with unnecessary cost. The signal is only emitted when `monitor_windows_[monitor]` actually changes content.

### 6. Remove legacy `Q_PROPERTY` members

Keeping the old `title`, `appClass`, `category` properties alongside the new per-monitor API would create two sources of truth. Any QML that accidentally uses the old properties would show the old single-monitor behavior, creating subtle per-monitor bugs that are hard to test. Clean removal forces all QML consumers to migrate to the correct pattern.

---

## Alternatives Considered

### A. Per-monitor service instances

Each `LayerShellManager` bar gets its own `ActiveWindowService(monitorName)` instance. The instance subscribes to the shared event socket and filters events by its own monitor name.

**Rejected** because: multiple socket connections to `.socket2.sock` (one per monitor) create unnecessary file descriptor pressure; category resolution caching cannot be shared; the QML_SINGLETON pattern cannot model multiple instances without a factory; registration complexity in `main.cpp` is significant.

### B. Per-workspace service

A service keyed by workspace ID rather than monitor name. Each workspace has its own active window. The QML reads its current workspace ID from `WorkspaceModel` and queries by workspace.

**Rejected** because: workspaces are not stable identifiers for bars — a bar displays its monitor's currently-active workspace, which changes when the user switches workspaces. Tracking by workspace would require the QML to watch for workspace switches and update its workspace-ID reference, adding complexity and a second reactive dependency. Monitor name is a more stable identifier for a given bar.

### C. Polling instead of event-driven

A `QTimer` polling `j/clients` every 200ms to refresh per-monitor windows.

**Rejected** because: polling at 200ms introduces up to 200ms latency for window changes; it generates unnecessary IPC traffic (5 queries/second per monitor); it wastes CPU cycles when no windows change. Event-driven updates are already the established pattern for all other services.

### D. Incremental update on close/move with address tracking

Maintain a `QHash<QString, int>` from window address to workspace ID. On `closewindow>>`, look up the workspace, determine the monitor, then query `j/clients` for that workspace only.

**Rejected** because: the address-to-workspace cache must be populated at startup and kept in sync across all events — effectively duplicating Hyprland's own window management state. The surface area for bugs is high. The full re-query approach is simpler and correct with negligible real-world overhead.

---

## Known Risks

### R1: `focusHistoryID` ordering ambiguity

**Description:** The SPEC notes that `focusHistoryID = 0` is *assumed* to be the most recently focused window, but this must be verified empirically. If Hyprland uses the opposite convention (highest ID = most recent), the startup init will select the *least* recently focused window on each workspace, showing stale windows at launch.

**Mitigation:** Before implementing `applyMonitorWindowsFromPending`, open two windows on the same workspace, focus the second one, then query `j/clients` and log the `focusHistoryID` values. If the focused window has the higher ID, use `max` instead of `min` in the selection logic. Add a comment in the implementation referencing this verification.

### R2: Monitor name mismatch between Qt and Hyprland

**Description:** `Screen.name` comes from Qt's `QScreen`, which reads the wl_output name set by Hyprland. Hyprland uses the DRM connector name (e.g., `DP-1`, `HDMI-A-1`). Qt may normalize or truncate the name. If even one character differs (e.g., `HDMI-A-1` vs `HDMI-1`), monitor lookup will silently fail and the bar will show a blank window section.

**Mitigation:** During testing, log `Screen.name` from QML and compare against `hyprctl monitors | grep name`. Add an assertion or warning in `categoryForMonitor` when the requested monitor name is not in `monitor_windows_` (log the available keys to aid debugging). If names diverge, add a normalization step.

### R3: `openwindow>>` workspace name parsing for non-numeric workspaces

**Description:** `monitor_workspaces_` uses `int` workspace IDs. The `openwindow>>` payload carries the workspace *name* as a string. For numeric workspaces (`"1"`, `"2"`), `toInt()` works. For named workspaces (`"discord"`, `"music"`), `toInt()` returns 0 and the event is silently dropped.

**Mitigation:** Named workspaces are declared out of scope for MVP (SPEC REQ-C-001). If named workspaces are needed in the future, change `monitor_workspaces_` to `QHash<QString, QString>` keyed by workspace name (string). For now, a `qCInfo` log when `toInt()` returns 0 will surface the issue in testing.

### R4: Overlapping re-queries on rapid close/move events

**Description:** If the user closes multiple windows rapidly (e.g., closing all windows in a workspace), each `closewindow>>` event triggers `queryAllMonitorWindows()`. The guard `command_phase_ != Idle` drops subsequent requests while a query is in flight. The final state of `monitor_windows_` after the in-flight query may be stale (it reflects the state at query time, before the later close events were processed).

**Mitigation:** After `finishCommandSocket(Phase::Clients)` resets `command_phase_` to `Idle`, check whether any re-query was requested during the in-flight period and issue another query if so. A `bool requery_pending_` flag serves this purpose. This is a minor consistency improvement; the visible latency is at most one additional query time (~50ms), which is acceptable.

### R5: Category resolution applied to wrong monitor after app switch

**Description:** If the user switches the focused window from Firefox to kitty between when the scan starts and when it completes, the watcher callback checks `it->app_class == app_class` for each monitor. If the focused monitor now shows kitty (not Firefox), Firefox's category is not applied. This is correct behavior. However, if a different monitor still shows Firefox, it does receive the category — also correct.

No risk in practice; the stale-result guard is per-entry, not per-service.

### R6: `ExtWorkspaceManager` call site breakage

**Description:** The `parseHyprlandFocusedMonitorEvent` signature change from `std::optional<int>` to `std::optional<HyprlandFocusedMonitor>` will break compilation at `ExtWorkspaceManager.cpp` call sites. This is a compile-time error (will not silently pass), but must be caught and fixed as part of this implementation.

**Mitigation:** Identify all call sites via `grep -r parseHyprlandFocusedMonitorEvent src/` before starting implementation. Update each to use `focused->workspace_name.toInt()` for the workspace ID.

---

## References

- **Spec:** `docs/sdd/active-window-per-monitor/SPEC.md`
- **Current service:** `src/ActiveWindowService.h`, `src/ActiveWindowService.cpp`
- **Hyprland IPC parsers:** `src/HyprlandIpc.h`, `src/HyprlandIpc.cpp`
- **Current QML:** `src/qml/Topbar/ActiveWindowSection.qml`
- **Top-bar root:** `src/qml/Topbar/TopBar.qml`
- **Prior session design (window title):** `docs/sdd/topbar-window-title/DESIGN.md`
- **ExtWorkspaceManager (focusedmon consumer):** `src/ExtWorkspaceManager.h`, `src/ExtWorkspaceManager.cpp`
- **Code style:** `.clang-format`, `.clang-tidy`
- **Project instructions:** `CLAUDE.md`
