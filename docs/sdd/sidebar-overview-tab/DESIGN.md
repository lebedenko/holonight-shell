# Sidebar Overview Tab — Architecture Design

## Overview

This document is the authoritative architecture reference for the `sidebar-overview-tab` feature. The feature implements the Overview tab (index 0) of the right sidebar, comprising a calendar widget, a notification summary section, and session action buttons. It also introduces dynamic sidebar height — the surface height tracks the active tab's preferred content height instead of being fixed to screen height.

---

## 1. New Components

### 1.1 `CalendarConfig` struct (in `ConfigService.h`)

**Purpose**: Carries parsed `[calendar]` TOML config into the application.

**File path**: `src/core/ConfigService.h` (struct definition) and `src/core/ConfigService.cpp` (parsing)

**Key interface**:
```cpp
// ConfigService.h

enum class WeekStartDay { Monday, Sunday };

struct CalendarConfig {
    WeekStartDay weekStartDay{ WeekStartDay::Monday };
};
```

Added members in `ConfigService`:
```cpp
// private field
CalendarConfig calendar_config_;

// public getter
[[nodiscard]] CalendarConfig calendarConfig() const;

// signal
Q_SIGNAL void calendarConfigChanged();
```

**Ownership/lifetime**: Value owned by the `ConfigService` singleton. Reconstructed each time `parseFile()` runs. Signal fires after the field is updated.

---

### 1.2 `CalendarService` C++ class

**Purpose**: QML-visible singleton that bridges `ConfigService::CalendarConfig` to QML. Keeps the full `ConfigService` object (with its many config structs) out of the QML API surface.

**File paths**:
- `src/services/calendar/CalendarService.h`
- `src/services/calendar/CalendarService.cpp`

**Key interface**:
```cpp
// CalendarService.h
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class CalendarService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString weekStartDay READ weekStartDay NOTIFY weekStartDayChanged)

public:
    explicit CalendarService(QObject* parent = nullptr);

    [[nodiscard]] QString weekStartDay() const; // returns "Mon" or "Sun"

Q_SIGNALS:
    void weekStartDayChanged();

private Q_SLOTS:
    void onCalendarConfigChanged();

private:
    QString week_start_day_{ "Mon" };
};
```

**Ownership/lifetime**: Registered as a QML singleton in `ShellApplication::startLayerSurfaces()` before the QML engine loads any component. Lifetime matches the application lifetime. Connects to `ConfigService::calendarConfigChanged()` in its constructor; reads `ConfigService::instance()->calendarConfig()` to initialise `week_start_day_` and again whenever the config reloads.

---

### 1.3 `NotificationService::recentHistoryGrouped()` invokable

**Purpose**: Returns up to `maxGroups` notification groups from `history_` for the Overview section, ordered by latest arrival time descending.

**File path**: `src/services/notifications/NotificationService.h` (declaration) and `src/services/notifications/NotificationService.cpp` (implementation)

**Key interface**:
```cpp
Q_INVOKABLE QVariantList recentHistoryGrouped(int maxGroups) const;
```

Return value: a `QVariantList` where each element is a `QVariantMap` with keys:

| Key | Type | Description |
|-----|------|-------------|
| `appName` | `QString` | Application name |
| `appIcon` | `QString` | Icon name or path from the most recent item |
| `unreadCount` | `int` | Count of items in this group where `read == false` |
| `latestSummary` | `QString` | `summary` of the most recent item |
| `latestTimestampMs` | `qint64` | Timestamp of the most recent item |
| `totalCount` | `int` | Total items in this group |

Algorithm:
1. Iterate `history_` (already ordered newest-first per existing architecture).
2. Group by `appName` using a `QHash<QString, QVariantMap>` accumulator and a `QStringList` for insertion order.
3. After grouping, trim to `maxGroups` entries from the ordered list.
4. Return as `QVariantList`.

**No struct changes** — operates only on existing `NotificationHistoryItem` fields (REQ-C-004).

---

### 1.4 `SidebarManager::onContentHeightChanged()` invokable

**Purpose**: Receives the QML-computed preferred content height for a given monitor and applies it to the live Wayland surface immediately.

**File paths**: `src/surfaces/SidebarManager.h` (declaration) and `src/surfaces/SidebarManager.cpp` (implementation)

**Key interface** (additions to `SidebarManager`):
```cpp
// SidebarManager.h — new members

Q_INVOKABLE void onContentHeightChanged(const QString& monitorName, int height);

// private
QHash<QString, int> stored_heights_; // key: monitor name, value: last known preferred height
```

Behaviour of `onContentHeightChanged()`:
1. Clamp: `int clamped = std::max(height, kSidebarMinHeight);`
2. Store: `stored_heights_[monitorName] = clamped;`
3. Look up the live surface for `monitorName`. If found and currently open, call `surface->set_size(kSurfaceWidth, clamped)` then `wl_surface_commit()`.

`kSidebarMinHeight = 336` is added to `ShellConstants.h` (6 tab buttons × 56 px). `kSidebarDefaultHeight = 600` is also added as the fallback for first-open when no stored height exists.

---

### 1.5 `SidebarOverview.qml` (full implementation)

**Purpose**: QML component implementing the Overview tab content — calendar, notification summary, and session actions.

**File path**: `src/qml/RightSidebar/SidebarOverview.qml`

**Key interface**:
```qml
Item {
    id: root

    // Required by SidebarContent tab-width dispatch
    property int preferredWidth: 380

    // Drives dynamic sidebar height (REQ-F-022)
    property int preferredHeight: calendarSection.preferredHeight
                                 + notifSection.preferredHeight
                                 + sessionBar.height
                                 + contentColumn.spacing * 2
                                 + 16  // top/bottom padding

    // Internal: current month/year being viewed (JS Date fields)
    property int viewYear:  (new Date()).getFullYear()
    property int viewMonth: (new Date()).getMonth()  // 0-based

    // Populated by JS model builder, refreshed by Timer
    property var groupedNotifs: []

    // Emitted upward to switch the active sidebar tab
    signal switchTab(int index)
}
```

**Sub-components**: `CalendarSection` and `NotificationsSection` are implemented as inline `Item` elements within `SidebarOverview.qml` (not separate QML files). They are non-trivial enough to warrant separate files only if they grow beyond ~150 lines each; for the initial implementation inline is sufficient and avoids proliferating small files. The `SessionBar` is also inline.

**Internal layout** — see Section 5 for the detailed QML tree.

**Ownership/lifetime**: Loaded on demand by the `Loader` inside `SidebarContent.qml` when `currentTab === 0`. The `Loader` manages creation/destruction according to its existing `asynchronous`/`active` logic.

---

## 2. Changes to Existing Components

### 2.1 `src/core/ConfigService.h` and `ConfigService.cpp`

**Header changes**:
- Add `WeekStartDay` enum class.
- Add `CalendarConfig` struct.
- Add `CalendarConfig calendar_config_` private field.
- Add `[[nodiscard]] CalendarConfig calendarConfig() const` getter.
- Add `Q_SIGNAL void calendarConfigChanged()` signal.

**`parseFile()` changes** (ConfigService.cpp):
```cpp
// Inside parseFile(), after existing sections:
if (config.contains("calendar")) {
    auto cal = config["calendar"];
    auto dayStr = cal["week_start_day"].value_or(std::string("Mon"));
    calendar_config_.weekStartDay = (dayStr == "Sun")
        ? WeekStartDay::Sunday
        : WeekStartDay::Monday;
}
emit calendarConfigChanged();
```

The signal fires unconditionally on every reload; `CalendarService` checks whether the value actually changed before emitting its own signal.

---

### 2.2 `src/surfaces/SidebarManager.h` and `SidebarManager.cpp`

**Header changes**:
- Declare `Q_INVOKABLE void onContentHeightChanged(const QString& monitorName, int height)`.
- Add `QHash<QString, int> stored_heights_` private field.

**`openOnMonitor()` changes** (SidebarManager.cpp, ~line 134):

Before:
```cpp
int height = screen->geometry().height() - kSidebarTopMargin - kSidebarBottomMargin;
```

After:
```cpp
int storedHeight = stored_heights_.value(monitorName, 0);
int height = (storedHeight > 0)
    ? std::max(storedHeight, kSidebarMinHeight)
    : kSidebarDefaultHeight;
```

**`onContentHeightChanged()` implementation**:
```cpp
void SidebarManager::onContentHeightChanged(const QString& monitorName, int height) {
    int clamped = std::max(height, kSidebarMinHeight);
    stored_heights_[monitorName] = clamped;

    // Apply immediately if the surface is currently open
    auto* surface = surfaceForMonitor(monitorName); // existing lookup helper
    if (surface && isOpen(monitorName)) {
        surface->set_size(kSurfaceWidth, clamped);
        surface->wlSurface()->commit(); // or equivalent wl_surface_commit wrapper
    }
}
```

---

### 2.3 `src/qml/RightSidebar/SidebarContent.qml`

Add one property after the existing `preferredWidthForTab` function:

```qml
// Exposes active tab's preferred height for RightSidebar to relay upward
property int preferredHeight: loader.item?.preferredHeight ?? 0
```

No other changes. The existing `Loader`, `ScrollView`, and `contentOpacity` logic are untouched (REQ-C-001).

---

### 2.4 `src/qml/RightSidebar/RightSidebar.qml`

Add a `Connections` block that relays height changes to `SidebarManager`:

```qml
Connections {
    target: contentArea   // the SidebarContent item's id
    function onPreferredHeightChanged() {
        SidebarManager.onContentHeightChanged(
            root.barMonitorName,
            contentArea.preferredHeight
        )
    }
}
```

This fires whenever the active tab's `preferredHeight` changes: on tab switch, on Loader completion, and on dynamic content changes (e.g., new notification arriving while Overview is open).

No changes to the existing open/close animation or width animation.

---

### 2.5 `src/shell/ShellApplication.cpp`

Register `CalendarService` as a QML singleton in `startLayerSurfaces()` alongside the other service registrations:

```cpp
#include "src/services/calendar/CalendarService.h"

// In startLayerSurfaces(), before engine.load():
qmlRegisterSingletonType<CalendarService>(
    "HolonightShell", 1, 0, "CalendarService",
    [](QQmlEngine*, QJSEngine*) -> QObject* {
        return new CalendarService();
    }
);
```

If the project uses the newer `QML_SINGLETON` + `QML_ELEMENT` macro pattern (which it does based on the CLAUDE.md reference to `QML_SINGLETON` on other services), no explicit `qmlRegisterSingletonType` call is needed — CMake auto-registers it. Verify against the existing `NotificationService` registration pattern and match it exactly.

---

### 2.6 `CMakeLists.txt`

Add to `HOLONIGHT_QML_FILES` (or the equivalent source list for QML):
```cmake
src/qml/RightSidebar/SidebarOverview.qml
```

Add `CalendarService.h` and `CalendarService.cpp` to the appropriate C++ source list (alongside other services).

If a shared JS file is created for `formatRelativeTime` or `buildDayModel`, add it to QML resources with the correct `QT_RESOURCE_ALIAS`.

---

### 2.7 Other tab placeholder files (REQ-F-022 compliance)

Each of the 5 remaining placeholder tab files must expose `preferredHeight` so `SidebarContent.preferredHeight` always resolves. Add one property to each:

| File | `preferredHeight` default |
|------|--------------------------|
| `src/qml/RightSidebar/SidebarCalendar.qml` | `480` |
| `src/qml/RightSidebar/SidebarNotifications.qml` | `600` |
| `src/qml/RightSidebar/SidebarSystem.qml` | `480` |
| `src/qml/RightSidebar/SidebarQuickSettings.qml` | `400` |
| `src/qml/RightSidebar/SidebarMedia.qml` | `340` |

```qml
// Add to each placeholder root Item:
property int preferredHeight: 480  // (use file-specific value from table above)
```

These defaults are best-guess values; each tab's full implementation will replace them with dynamic values.

---

## 3. Data Flow

### Flow A — Config change → Calendar re-renders

```
TOML file changes on disk
    │
    ▼
ConfigService::parseFile()
    │  updates calendar_config_.weekStartDay
    │  emits calendarConfigChanged()
    ▼
CalendarService::onCalendarConfigChanged()  [connected in constructor]
    │  reads ConfigService::instance()->calendarConfig()
    │  updates week_start_day_ string
    │  emits weekStartDayChanged()  (only if value actually changed)
    ▼
QML binding: CalendarService.weekStartDay  [Q_PROPERTY with NOTIFY]
    │
    ▼
SidebarOverview.qml  Connections { target: CalendarService }
    │  function onWeekStartDayChanged() { rebuildDayModel() }
    ▼
buildDayModel(viewYear, viewMonth, CalendarService.weekStartDay)
    │  returns new JS array
    ▼
dayRepeater.model = newArray   →   Grid re-renders with correct column alignment
```

### Flow B — Notification arrives/expires → Overview refreshes

```
D-Bus notification arrives (or notification closed)
    │
    ▼
NotificationService: updates history_
    │  emits unreadCountChanged()
    ▼
SidebarOverview.qml  Connections { target: NotificationService }
    │  function onUnreadCountChanged() { refreshGroupedNotifs() }
    ▼
refreshGroupedNotifs()
    │  calls NotificationService.recentHistoryGrouped(3)
    │  assigns result to root.groupedNotifs
    ▼
notifRepeater.model = root.groupedNotifs   →   notification rows re-render
    │
    ▼
root.preferredHeight re-evaluates (bound to section heights)
    │
    ▼
SidebarContent.preferredHeight changes
    │
    ▼
RightSidebar Connections.onPreferredHeightChanged fires
    │
    ▼
SidebarManager.onContentHeightChanged(monitorName, newHeight)
    │  clamps, stores, calls set_size() + commit()
    ▼
Compositor resizes the surface
```

### Flow C — User opens sidebar (height negotiation)

```
User action (e.g., UtilityIcon click) → SidebarManager.toggle()
    │
    ▼
SidebarManager::openOnMonitor(monitorName, screen)
    │  storedHeight = stored_heights_.value(monitorName, 0)
    │  height = (storedHeight > 0) ? clamp(storedHeight, kSidebarMinHeight)
    │                               : kSidebarDefaultHeight (600)
    │  surface->set_size(kSurfaceWidth, height)
    │  surface is mapped / made visible
    │
    ▼
QML engine animates sidebar open (RightSidebar open animation)
    │
    ▼
SidebarContent Loader completes (Loader.status == Loader.Ready)
    │
    ▼
SidebarOverview.preferredHeight evaluates (calendar + notif + session heights)
    │
    ▼
SidebarContent.preferredHeight binding fires
    │
    ▼
RightSidebar Connections.onPreferredHeightChanged
    │
    ▼
SidebarManager.onContentHeightChanged(monitorName, actualHeight)
    │  stores actualHeight
    │  calls set_size(kSurfaceWidth, actualHeight) + commit()
    ▼
Compositor resizes surface to actual content height
(one brief snap on first-ever open; subsequent opens use stored height)
```

### Flow D — User switches tabs (instant height snap)

```
User clicks SidebarTabBar button (index N)
    │
    ▼
SidebarContent.currentTab = N
    │
    ▼
Loader source switches → new tab component instantiates
    │
    ▼
loader.item.preferredHeight resolves
    │
    ▼
SidebarContent.preferredHeight binding re-evaluates
    │
    ▼
RightSidebar Connections.onPreferredHeightChanged fires
    │
    ▼
SidebarManager.onContentHeightChanged(monitorName, newHeight)
    │  set_size() + commit()  [instant — no height animation]
    ▼
Compositor resizes surface
Width animation (180ms) plays concurrently in RightSidebar.qml (unchanged)
```

### Flow E — "View all" notifications click

```
User clicks "View all" button in NotificationsSection
    │
    ▼
root.switchTab(2)   [signal emitted on SidebarOverview]
    │
    ▼
SidebarContent.qml  Connections { target: loader.item }
    function onSwitchTab(index) { root.currentTab = index }
    │
    ▼
SidebarContent.currentTab = 2
    │  (triggers Flow D for the Notifications tab)
    ▼
Notifications tab renders
```

`SidebarContent.qml` must add a `Connections` block listening for `loader.item.switchTab` (or alternatively connect the signal in `Loader.onLoaded`). This is a minor addition to `SidebarContent.qml` beyond the `preferredHeight` property.

---

## 4. Height Negotiation Architecture

### The problem

`SidebarManager::openOnMonitor()` must provide a Wayland surface size before QML has laid out the content. The Loader inside `SidebarContent.qml` is asynchronous — `preferredHeight` is not available at the moment `set_size()` is first called.

### Two-phase sizing

**Phase 1 — Surface creation** (`openOnMonitor()`):
- Query `stored_heights_` for the monitor name.
- If a value exists (from any previous open or tab switch on this monitor), use it clamped to `kSidebarMinHeight`.
- If no value exists (first-ever open on this monitor), use `kSidebarDefaultHeight = 600`.
- Call `surface->set_size(kSurfaceWidth, height)`.

**Phase 2 — QML correction**:
- After the Loader finishes (`Loader.status == Loader.Ready`), the `preferredHeight` chain fires.
- `RightSidebar.qml`'s `Connections.onPreferredHeightChanged` calls `SidebarManager.onContentHeightChanged()`.
- `SidebarManager` calls `set_size()` again with the actual height and commits.
- On subsequent opens of the same monitor, Phase 1 already has the correct stored height, so Phase 2 is a no-op resize (same value).

### Constants (additions to `ShellConstants.h`)

```cpp
// ShellConstants.h additions
constexpr int kSidebarMinHeight = 336;     // 6 × ~56px tab buttons
constexpr int kSidebarDefaultHeight = 600; // first-open fallback
```

### Timing diagram (first open)

```
t=0   openOnMonitor()  →  set_size(w, 600)   [default]
t=1   surface mapped, QML engine begins loading
t=~5ms Loader.Ready fires → preferredHeight = 520 (example)
t=~5ms onContentHeightChanged(monitor, 520) → set_size(w, 520) + commit()
t=~5ms compositor resizes surface (1-frame snap, not visible in practice)

t=next-open  openOnMonitor()  →  set_size(w, 520)  [stored]  ← no snap
```

### `set_size` + `commit` wrapper

`SidebarManager::onContentHeightChanged()` must call both `surface->set_size()` and flush the Wayland connection. Use the same pattern already present in `openOnMonitor()` for applying size. If the existing code uses a helper method, reuse it; otherwise the pattern is:

```cpp
surface->set_size(kSurfaceWidth, clamped);
// wl_surface.commit is handled by the layer-shell surface wrapper's
// existing commit path — verify against PerMonitorLayerManager usage.
```

---

## 5. `SidebarOverview.qml` Layout

```
SidebarOverview  (id: root, Item)
│
│  property int preferredWidth: 380
│  property int preferredHeight: (contentColumn.implicitHeight
│                                 + sessionBar.height + 8)
│  property int viewYear / viewMonth   ← current calendar view
│  property var dayModel: []           ← built by buildDayModel()
│  property var groupedNotifs: []      ← from recentHistoryGrouped(3)
│  signal switchTab(int index)
│
├── ColumnLayout (id: contentColumn)
│   anchors { top: parent.top; left: parent.left; right: parent.right
│              bottom: sessionBar.top }
│   spacing: 12
│
│   ├── [CalendarSection — inline Item]  (id: calendarSection)
│   │   property int preferredHeight: calendarHeader.height
│   │                                 + dayOfWeekRow.height
│   │                                 + dayGrid.implicitHeight
│   │                                 + 24  (padding)
│   │   Layout.fillWidth: true
│   │   Layout.preferredHeight: preferredHeight
│   │
│   │   ├── RowLayout (id: calendarHeader)   ← month navigation
│   │   │   ├── Text: formatted month+year  (HoloniightPalette.onSurface)
│   │   │   ├── Item { Layout.fillWidth: true }   ← spacer
│   │   │   ├── ToolButton "<"   onClicked: prevMonth()
│   │   │   └── ToolButton ">"   onClicked: nextMonth()
│   │   │
│   │   ├── RowLayout (id: dayOfWeekRow)   ← "Mo Tu We Th Fr Sa Su" header
│   │   │   └── Repeater { model: dayHeaders }
│   │   │       └── Text (HoloniightPalette.textSubtle, centered, Layout.fillWidth)
│   │   │
│   │   └── Grid (id: dayGrid, columns: 7)
│   │       └── Repeater { model: root.dayModel }
│   │           └── Item (id: dayCell)
│   │               ├── MultiEffect   ← violet glow, visible: modelData.isToday
│   │               │   (declared FIRST per CLAUDE.md z-order rule)
│   │               │   shadowEnabled: true
│   │               │   shadowColor: HoloniightPalette.accentViolet
│   │               └── Text: modelData.day
│   │                   color: (modelData.isToday && viewIsCurrentMonth)
│   │                              ? HoloniightPalette.accentCyan
│   │                          : (!modelData.isCurrentMonth)
│   │                              ? HoloniightPalette.textSubtle
│   │                          : modelData.isWeekend
│   │                              ? HoloniightPalette.accentViolet
│   │                          : HoloniightPalette.onSurface
│   │                   font.bold: modelData.isToday && viewIsCurrentMonth
│   │
│   └── [NotificationsSection — inline Item]  (id: notifSection)
│       property int preferredHeight: (implicit from children)
│       Layout.fillWidth: true
│
│       ├── Text "Notifications"   (section header, HoloniightPalette.textSubtle)
│       │
│       ├── Repeater { id: notifRepeater; model: root.groupedNotifs }
│       │   └── Item (notification row)
│       │       ├── Image { source: "image://icon/" + modelData.appIcon }
│       │       ├── Column
│       │       │   ├── Text: modelData.appName  (onSurface)
│       │       │   └── Text: modelData.latestSummary  (textSubtle, elide)
│       │       ├── Item { Layout.fillWidth: true }
│       │       ├── Rectangle (unread badge, visible: modelData.unreadCount > 0)
│       │       │   color: HoloniightPalette.accentCyan
│       │       │   └── Text: modelData.unreadCount
│       │       └── Text: formatRelativeTime(modelData.latestTimestampMs)
│       │                  (textSubtle)
│       │
│       ├── Item (empty state, visible: root.groupedNotifs.length === 0)
│       │   └── Text "No new notifications"  (onSurfaceVariant, centered)
│       │
│       └── RowLayout (overflow row)
│           visible: overflowCount > 0
│           property int overflowCount: (totalHistoryCount - 3)  // computed below
│           ├── Text: "+N notifications"  (textSubtle)
│           ├── Item { Layout.fillWidth: true }
│           └── Button "View all"
│               onClicked: root.switchTab(2)
│
└── [SessionBar — inline Item]  (id: sessionBar)
    anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
    height: 56
    └── RowLayout (anchors.fill: parent, spacing: 0)
        ├── Item { Layout.fillWidth: true }   ← left spacer
        ├── ToolButton  icon: "system-log-out-symbolic"
        │   onClicked: SessionService.logout()
        ├── ToolButton  icon: "system-suspend-symbolic"
        │   onClicked: SessionService.sleep()
        ├── ToolButton  icon: "system-reboot-symbolic"
        │   onClicked: SessionService.reboot()
        ├── ToolButton  icon: "system-shutdown-symbolic"
        │   onClicked: SessionService.shutdown()
        └── Item { Layout.fillWidth: true }   ← right spacer
```

### Session invokables

Session commands are implemented as `Q_INVOKABLE` methods on a new lightweight `SessionService` QML singleton (or, if preferred, directly as `Q_INVOKABLE` slots on `SidebarManager`). The simpler approach is a dedicated `SessionService`:

```cpp
// src/services/session/SessionService.h
class SessionService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    Q_INVOKABLE void logout();   // loginctl terminate-session self
    Q_INVOKABLE void sleep();    // loginctl suspend
    Q_INVOKABLE void reboot();   // loginctl reboot
    Q_INVOKABLE void shutdown(); // loginctl poweroff
private:
    static void run(const QString& subcommand);
};

// SessionService.cpp
void SessionService::run(const QString& subcommand) {
    QProcess::startDetached("loginctl", { subcommand });
}
void SessionService::logout()   { run("terminate-session self"); }
void SessionService::sleep()    { run("suspend");  }
void SessionService::reboot()   { run("reboot");   }
void SessionService::shutdown() { run("poweroff"); }
```

`loginctl` is invoked with `QProcess::startDetached` (fire-and-forget). No confirmation dialog (REQ-F-017 through REQ-F-020). Add `SessionService.h/.cpp` to `CMakeLists.txt` and register the singleton in `ShellApplication::startLayerSurfaces()`.

---

## 6. Calendar Implementation Details

### Model generation

A JavaScript function `buildDayModel(year, month, weekStartDay)` runs inside `SidebarOverview.qml`. It is defined inline (not in a separate `.js` file) to avoid QML module import plumbing for a single function.

```javascript
function buildDayModel(year, month, weekStartDay) {
    // month is 0-based (JS Date convention)
    var firstDay = new Date(year, month, 1)
    var lastDay  = new Date(year, month + 1, 0)
    var today    = new Date()
    var todayY   = today.getFullYear()
    var todayM   = today.getMonth()
    var todayD   = today.getDate()

    // Compute leading offset
    // weekStartDay: "Mon" → week starts on 1 (JS getDay: Sun=0, Mon=1, ..., Sat=6)
    // weekStartDay: "Sun" → week starts on 0
    var startDow = (weekStartDay === "Sun") ? 0 : 1
    var firstDow = firstDay.getDay()  // 0=Sun..6=Sat
    var leadCount = ((firstDow - startDow) + 7) % 7

    var cells = []

    // Leading days from previous month
    var prevLast = new Date(year, month, 0)  // last day of previous month
    for (var i = leadCount - 1; i >= 0; i--) {
        var d = prevLast.getDate() - i
        var dow = new Date(year, month - 1, d).getDay()
        cells.push({
            day: d,
            month: (month - 1 + 12) % 12,
            year: (month === 0) ? year - 1 : year,
            isCurrentMonth: false,
            isToday: false,
            isWeekend: (dow === 0 || dow === 6)
        })
    }

    // Current month days
    for (var day = 1; day <= lastDay.getDate(); day++) {
        var dow2 = new Date(year, month, day).getDay()
        cells.push({
            day: day,
            month: month,
            year: year,
            isCurrentMonth: true,
            isToday: (year === todayY && month === todayM && day === todayD),
            isWeekend: (dow2 === 0 || dow2 === 6)
        })
    }

    // Trailing days to complete the last week row
    var trailCount = (7 - (cells.length % 7)) % 7
    for (var t = 1; t <= trailCount; t++) {
        var dow3 = new Date(year, month + 1, t).getDay()
        cells.push({
            day: t,
            month: (month + 1) % 12,
            year: (month === 11) ? year + 1 : year,
            isCurrentMonth: false,
            isToday: false,
            isWeekend: (dow3 === 0 || dow3 === 6)
        })
    }

    return cells
}
```

The result array has exactly `5×7 = 35` or `6×7 = 42` entries depending on month layout. `SidebarOverview.dayModel` is reassigned via `root.dayModel = buildDayModel(root.viewYear, root.viewMonth, CalendarService.weekStartDay)` whenever `viewYear`, `viewMonth`, or `CalendarService.weekStartDay` changes.

### Grid layout

```qml
Grid {
    id: dayGrid
    columns: 7
    // width fills parent; each cell is (width / 7) wide
    Repeater {
        model: root.dayModel
        delegate: Item {
            id: dayCell
            width:  dayGrid.width / 7
            height: 36

            // Glow effect declared BEFORE the text (CLAUDE.md MultiEffect z-order)
            MultiEffect {
                anchors.fill: parent
                source: dayCellBg
                shadowEnabled: modelData.isToday && viewIsCurrentMonth
                shadowColor:   HoloniightPalette.accentViolet
                shadowBlur:    0.6
                shadowScale:   1.0
            }

            Rectangle {
                id: dayCellBg
                anchors.centerIn: parent
                width:  28; height: 28
                radius: 4
                color:  "transparent"
                visible: modelData.isToday && viewIsCurrentMonth
            }

            Text {
                anchors.centerIn: parent
                text:  modelData.day
                font.bold: modelData.isToday && viewIsCurrentMonth
                color: colorForCell(modelData)
            }
        }
    }
}

function colorForCell(cell) {
    if (cell.isToday && viewIsCurrentMonth)
        return HoloniightPalette.accentCyan
    if (!cell.isCurrentMonth)
        return HoloniightPalette.textSubtle
    if (cell.isWeekend)
        return HoloniightPalette.accentViolet
    return HoloniightPalette.onSurface
}
```

### Month navigation

```qml
function prevMonth() {
    if (root.viewMonth === 0) {
        root.viewMonth = 11
        root.viewYear -= 1
    } else {
        root.viewMonth -= 1
    }
    root.dayModel = buildDayModel(root.viewYear, root.viewMonth,
                                  CalendarService.weekStartDay)
}

function nextMonth() {
    if (root.viewMonth === 11) {
        root.viewMonth = 0
        root.viewYear += 1
    } else {
        root.viewMonth += 1
    }
    root.dayModel = buildDayModel(root.viewYear, root.viewMonth,
                                  CalendarService.weekStartDay)
}

// viewIsCurrentMonth: true only when viewing the current calendar month
readonly property bool viewIsCurrentMonth: {
    var now = new Date()
    return root.viewYear === now.getFullYear()
           && root.viewMonth === now.getMonth()
}
```

---

## 7. Relative Time Formatting

Notification rows display how long ago a notification arrived using a `formatRelativeTime` JS function defined inline in `SidebarOverview.qml`:

```javascript
function formatRelativeTime(timestampMs) {
    var diffMs  = Date.now() - timestampMs
    var diffSec = Math.floor(diffMs / 1000)
    if (diffSec < 60)   return "just now"
    var diffMin = Math.floor(diffSec / 60)
    if (diffMin < 60)   return diffMin + "m ago"
    var diffHr  = Math.floor(diffMin / 60)
    if (diffHr  < 24)   return diffHr  + "h ago"
    var diffDay = Math.floor(diffHr  / 24)
    return diffDay + "d ago"
}
```

To keep relative times current without reloading the data, a `Timer` ticks once per minute while the Overview tab is active:

```qml
Timer {
    id: relativeTimeRefreshTimer
    interval: 60000
    repeat: true
    running: root.visible   // active only when Overview is visible
    onTriggered: {
        // Force Repeater to re-evaluate formatRelativeTime for all rows.
        // Simplest approach: bump a counter property that each delegate binds to.
        root.timeTickCounter++
    }
}

property int timeTickCounter: 0
```

Each notification row delegate binds its time label to `root.timeTickCounter` indirectly:

```qml
Text {
    text: root.timeTickCounter, formatRelativeTime(modelData.latestTimestampMs)
    // The comma expression ensures re-evaluation on tick
}
```

A cleaner alternative is to use a JS expression that references `root.timeTickCounter`:

```qml
text: (root.timeTickCounter >= 0)
          ? formatRelativeTime(modelData.latestTimestampMs)
          : ""
```

---

## 8. Key Decisions & Rationale

### 1. `CalendarService` vs. making `ConfigService` a QML singleton

`ConfigService` aggregates all application configuration (`NotificationHistoryConfig`, `WeatherConfig`, `CalendarConfig`, and future additions). Exposing it directly to QML would surface every internal config struct as a QML-accessible type, and `ConfigService` is not a `QML_ELEMENT`. `CalendarService` is a thin wrapper that re-emits only the calendar-relevant property, keeping the QML API surface minimal and maintaining the established pattern (same as `WeatherService` wrapping `WeatherConfig`).

### 2. JavaScript model for calendar grid vs. `QAbstractListModel`

The calendar grid contains at most 42 cells. A `QAbstractListModel` in C++ would require a new class, a custom `data()` implementation with role maps, and a C++↔QML signal roundtrip on every month change. The JS `buildDayModel()` function is ~40 lines, produces the same result synchronously, and makes the `week_start_day` pivot trivial to implement. At 42 cells the performance difference is immeasurable on any hardware this shell runs on.

### 3. `recentHistoryGrouped()` as a `Q_INVOKABLE` vs. a separate `QAbstractListModel`

The Overview section shows at most 3 notification groups. A full `QAbstractListModel` subclass is over-engineered: it would need role definitions, `rowCount()`, `data()`, incremental updates, and a separate C++ class. The `Q_INVOKABLE` returns a `QVariantList` of `QVariantMap`s that QML's `Repeater` consumes directly. The QML side refreshes it on `unreadCountChanged()` — a coarse-grained signal that fires at most once per notification event. This is exactly the pattern already used by other lightweight data queries in the shell.

### 4. Session buttons inside `SidebarOverview` vs. a separate surface

Pinning session buttons inside `SidebarOverview` means the sidebar height naturally includes them — no extra Wayland surface, no separate `LayerSurface`, no `PerMonitorLayerManager` entry. The session bar height (56 px) is part of `SidebarOverview.preferredHeight`, so the dynamic height calculation automatically accounts for it. A separate surface would introduce surface ordering issues, extra dismiss-overlay complexity, and more C++ registration.

### 5. Height fallback on first open (`kSidebarDefaultHeight = 600`)

Reading `root->property("preferredHeight")` from C++ inside `openOnMonitor()` before the QML Loader finishes returns 0 because the Loader is asynchronous. Three alternatives were considered:

- **Synchronous load** (`asynchronous: false` on Loader): breaks the existing Loader pausing architecture (REQ-C-001) and can cause visible jank on open.
- **Reading property synchronously**: always returns 0 for the first open (Loader not yet ready).
- **Default constant + QML correction**: simple, robust, and the one-time brief snap on the very first open of a given monitor is practically invisible because the open animation is already in progress.

The default-constant approach is chosen. `kSidebarDefaultHeight = 600` was selected as a reasonable visual size that errs on the side of too large rather than too small (clipping content is more jarring than a slight shrink after open).

### 6. Instant height snap (no height animation)

Animating the Wayland surface height requires multiple `set_size` + `commit` round-trips through the compositor, which introduces frame-timing dependencies and risks visual artifacts (partially-clipped content during the transition). The width animation (180 ms, already implemented in `RightSidebar.qml`) is the primary visual cue for a tab switch; the height snap is secondary. An instant height change is less jarring in practice than it sounds — it occurs simultaneously with the width animation, and the compositor's frame scheduling masks the snap within the first rendered frame of the tab content.

### 7. Inline CalendarSection and NotificationsSection vs. separate QML files

Both sections are implemented inline within `SidebarOverview.qml` rather than as separate component files. The rationale: each section is tightly coupled to `SidebarOverview`'s state (`viewYear`, `viewMonth`, `groupedNotifs`, `switchTab` signal). Extracting them to separate files would require property bindings for all shared state, adding QML boilerplate without improving modularity. If either section grows beyond ~150 lines or needs to be reused elsewhere, extraction to a named component file is straightforward.

---

## 9. Known Risks and Mitigations

### Height timing on first open

**Risk**: On the very first open of the sidebar on a given monitor, the surface is sized to `kSidebarDefaultHeight = 600` and then immediately resized by the QML correction. This may produce a one-frame visual snap.

**Mitigation**: The snap is masked by the open animation (the sidebar slides in during the resize). Accepted as a known limitation. If it proves visually objectionable, a future improvement could force `asynchronous: false` on the Loader only when `stored_heights_` has no entry for the monitor.

### Calendar grid row count is dynamic

**Risk**: `SidebarOverview.preferredHeight` is bound to `dayGrid.implicitHeight`, which changes between 5-row months (35 cells × 36 px = 1260 px distributed across 5 rows) and 6-row months. This causes a height change on the first day of a 6-row month.

**Mitigation**: The height binding is live — the sidebar automatically resizes. The change happens at midnight on month rollover, when the sidebar is unlikely to be open. No special handling required.

### `CalendarService` registration ordering

**Risk**: If `CalendarService` is registered after the QML engine begins loading components, QML files that `import HolonightShell` will fail to resolve `CalendarService`.

**Mitigation**: Register in `ShellApplication::startLayerSurfaces()` before any `engine.load()` call. This is the same constraint as all other QML singletons (`NotificationService`, `SidebarManager`, etc.) and follows the established pattern.

### `loginctl` availability

**Risk**: `SessionService` invokes `QProcess::startDetached("loginctl", ...)`. If `loginctl` is not on `$PATH`, the commands silently fail (no error feedback to the user).

**Assumption**: The shell targets standard `systemd`-based Linux distributions (Arch, Fedora, Debian/Ubuntu, etc.) where `loginctl` is always present at `/usr/bin/loginctl`. This is documented here as an explicit assumption rather than a guarded runtime check.

### App icon resolution in notification rows

**Risk**: `appIcon` in `NotificationHistoryItem` may be a freedesktop theme icon name (e.g., `"firefox"`) or an absolute file path. Rendering both through a single `Image { source: ... }` requires different URI schemes.

**Mitigation**: In QML, use:
```qml
Image {
    source: modelData.appIcon.startsWith("/")
            ? modelData.appIcon
            : "image://icon/" + modelData.appIcon
}
```
This handles both cases without changes to `NotificationHistoryItem` (REQ-C-004 compliant).

### `preferredHeight` before Loader item exists

**Risk**: `SidebarContent.preferredHeight: loader.item?.preferredHeight ?? 0` uses the null-coalescing operator. If `loader.item` is `null` (Loader not yet ready), the property returns 0, which triggers `onContentHeightChanged(monitorName, 0)`. This would clamp to `kSidebarMinHeight = 336` and resize the surface to 336 px prematurely.

**Mitigation**: Guard the `Connections` handler in `RightSidebar.qml`:
```qml
Connections {
    target: contentArea
    function onPreferredHeightChanged() {
        if (contentArea.preferredHeight > 0) {
            SidebarManager.onContentHeightChanged(
                root.barMonitorName,
                contentArea.preferredHeight
            )
        }
    }
}
```

---

## 10. File Checklist

### New files

| File | Type | Description |
|------|------|-------------|
| `src/services/calendar/CalendarService.h` | C++ header | CalendarService QML singleton |
| `src/services/calendar/CalendarService.cpp` | C++ source | CalendarService implementation |
| `src/services/session/SessionService.h` | C++ header | SessionService QML singleton |
| `src/services/session/SessionService.cpp` | C++ source | Session command invokables |

### Modified files

| File | Change summary |
|------|---------------|
| `src/core/ConfigService.h` | Add `WeekStartDay` enum, `CalendarConfig` struct, getter, signal |
| `src/core/ConfigService.cpp` | Parse `[calendar]` TOML section in `parseFile()` |
| `src/surfaces/SidebarManager.h` | Add `onContentHeightChanged()` Q_INVOKABLE, `stored_heights_` |
| `src/surfaces/SidebarManager.cpp` | Implement `onContentHeightChanged()`; update `openOnMonitor()` height logic |
| `src/shell/ShellApplication.cpp` | Register `CalendarService` and `SessionService` singletons |
| `ShellConstants.h` | Add `kSidebarMinHeight = 336`, `kSidebarDefaultHeight = 600` |
| `src/services/notifications/NotificationService.h` | Declare `recentHistoryGrouped()` Q_INVOKABLE |
| `src/services/notifications/NotificationService.cpp` | Implement `recentHistoryGrouped()` |
| `src/qml/RightSidebar/SidebarContent.qml` | Add `preferredHeight` property; add `switchTab` signal relay |
| `src/qml/RightSidebar/RightSidebar.qml` | Add `Connections` for `onPreferredHeightChanged` |
| `src/qml/RightSidebar/SidebarOverview.qml` | Full implementation (replaces placeholder) |
| `src/qml/RightSidebar/SidebarCalendar.qml` | Add `property int preferredHeight: 480` |
| `src/qml/RightSidebar/SidebarNotifications.qml` | Add `property int preferredHeight: 600` |
| `src/qml/RightSidebar/SidebarSystem.qml` | Add `property int preferredHeight: 480` |
| `src/qml/RightSidebar/SidebarQuickSettings.qml` | Add `property int preferredHeight: 400` |
| `src/qml/RightSidebar/SidebarMedia.qml` | Add `property int preferredHeight: 340` |
| `CMakeLists.txt` | Add new C++ and QML sources |
