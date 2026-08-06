# Desktop Widgets — Architecture Design

## Overview & Scope

Desktop Widgets are configurable, occupancy-gated layer-shell surfaces that render on the `bottom` layer (above wallpaper, below normal windows). Each widget is a lightweight `QQuickView` anchored to one of nine positions on each targeted monitor. Widgets are visible only when the currently-visible workspace on their monitor is empty, and are hidden (but not destroyed) when any window occupies that workspace.

Scope of this document: the C++ and QML design for the first widget type, `time-to-event`. The architecture is extensible; future widget types plug in via the `WidgetDefinition` variant and a new QML component.

Out of scope: interaction (all widgets are `Qt::WindowTransparentForInput` in v1), non-Hyprland compositors (REQ-C-001), per-direction margin (REQ-C-003).

---

## Component Inventory

| Class / File | Layer | Responsibility |
|---|---|---|
| `WidgetDefinition`, `TimeToEventConfig`, `WidgetsConfig` in `src/core/ConfigService.h/.cpp` | core | TOML structs, `operator==`, parsing, `widgetsChanged` signal |
| `MonitorOccupancyService` — `src/services/MonitorOccupancyService.h/.cpp` | services | Combines `HyprlandWorkspaceService` + `ActiveWindowService` state; emits `occupancyChanged(monitorName, isEmpty)` |
| `WidgetManager` — `src/surfaces/WidgetManager.h/.cpp` | surfaces | Owns `QQuickView` + `LayerSurface` per (monitor, widget); lifecycle, collision, show/hide, config reload |
| `src/qml/Widgets/WidgetSurface.qml` | QML | Root item for every widget surface; receives `deadline`, `title`, `showSeconds`, `hasTime`, `barMonitorName` as `required property`; hosts `Loader` for the widget type component |
| `src/qml/Widgets/TimeToEventWidget.qml` | QML | Renders title / countdown / date label; contains the `Timer` element; all colors from `HoloniightPalette` |
| `ShellApplication` — `src/app/ShellApplication.h/.cpp` | app | Add `MonitorOccupancyService*` and `std::unique_ptr<WidgetManager>` members; wire in `startServices()` / `startLayerSurfaces()` |
| `CMakeLists.txt` | build | Add new `.h/.cpp` to their respective static libraries; add new `.qml` files to `HOLONIGHT_QML_FILES` |

---

## The PerMonitorLayerManager Multiplicity Decision

### The Problem

`PerMonitorLayerManager` manages exactly **one** `QQuickView + LayerSurface` per monitor, keyed by `QScreen*`. Widgets need **multiple** surfaces per monitor — one for each `WidgetDefinition` — so the 1:1 assumption is violated.

### Options Considered

**Option A — Generalize `PerMonitorLayerManager` to N surfaces per monitor.**
Change `unordered_map<QScreen*, MonitorSurface>` to `unordered_map<QScreen*, vector<MonitorSurface>>` and add an index or tag. This adds complexity that bleeds into `BackgroundManager` and the other two existing subclasses (`LayerShellManager`, `NotificationToastSurface` via `NotificationManager`). Every virtual (`configureSurface`, `qmlSource`) gains a new parameter. The existing callers and tests all need updating. The base class accumulates widget-specific concerns.

**Option B — `WidgetManager` does NOT inherit `PerMonitorLayerManager`; reimplements lifecycle directly.**
Full duplication of the hotplug wiring (`screenAdded`/`screenRemoved`), the `QWaylandWindow` null-check, and the `wl_surface` acquisition. Maintenance hazard if the base ever changes.

**Option C — One `WidgetManager` instance per `WidgetDefinition`, each managing one-surface-per-monitor.**
Each instance holds a `WidgetDefinition` and inherits `PerMonitorLayerManager` unchanged. The 1:1 invariant is preserved. `ShellApplication` or a coordinator holds a `QList<std::unique_ptr<WidgetManager>>`. On config reload the coordinator destroys all instances and recreates them for the new definition list.

### Recommendation: Option C

Option C is cleanest. It reuses the hotplug machinery and Wayland protocol sequencing verbatim, adds zero complexity to the base class, and maps naturally onto the TOML `[[widget]]` array. The coordinator pattern (a `WidgetCoordinator` or inline logic in `ShellApplication`) handles teardown and rebuild on reload.

**Concrete impact:**
- `WidgetManager : public PerMonitorLayerManager` — the class is parameterized by a single `WidgetDefinition` at construction.
- `ShellApplication` gains `QList<std::unique_ptr<WidgetManager>> widget_managers_` (not a raw pointer; lifetime is tied to the `LayerShell`).
- On `configService::widgetsChanged`: the coordinator calls `widget_managers_.clear()`, then recreates one `WidgetManager` per valid `WidgetDefinition`, calls `start()` on each.
- Collision detection runs at coordinator level before creating `WidgetManager` instances: see the Collision section.

**Why not option A:** Pollutes a stable base class; breaks the existing three subclasses.
**Why not option B:** Duplicates ~60 lines of hairy Wayland plumbing that has already been debugged.

---

## Data Flow Diagrams

### (a) Startup Widget Creation

```
main()
  ShellApplication::startServices()
    MonitorOccupancyService::start()         # subscribes to AWS + HWS
  ShellApplication::startShell()
    LayerShell created
  ShellApplication::startLayerSurfacesWhenReady()
    [LayerShell::activeChanged or 3s fallback]
  ShellApplication::startLayerSurfaces()
    layer_shell_manager_->start()            # bars
    background_manager_->start()             # wallpapers
    [new] coordinator creates WidgetManagers
      for each WidgetDefinition (collision-checked, in config order):
        WidgetManager::start()
          for each QScreen* in QGuiApplication::screens():
            if monitorFilter(screen->name()) passes:
              createSurface(screen)           # QQuickView + LayerSurface
              view->hide() if workspace occupied
```

### (b) Occupancy Transition Hide/Show

```
Hyprland event (openwindow / closewindow / workspace / etc.)
  -> ActiveWindowService processes event line
       state.monitor_workspaces updated (monitor -> workspace_id)
  -> HyprlandWorkspaceService processes event
       model_->setOccupiedWorkspaceIds(new_set)

MonitorOccupancyService::onActiveWindowEvent(monitorName)
  visibleWsId  = aws_->monitor_workspaces[monitorName]
  isOccupied   = hws_->occupied_workspace_ids.contains(visibleWsId)
  if changed:
    emit occupancyChanged(monitorName, isEmpty=!isOccupied)

WidgetManager::onOccupancyChanged(monitorName, isEmpty)
  for each surface matching monitorName:
    if isEmpty:  view->show();  timer->resume()
    else:        view->hide();  timer->freeze()
```

### (c) Config Live-Reload

```
File system event (QFileSystemWatcher)
  -> ConfigService debounce (200 ms)
  -> ConfigService::parseFile()
      local_widgets = parseWidgets(table)
      if local_widgets != widgets_:
        widgets_ = local_widgets
        emit widgetsChanged()

ShellApplication / coordinator::onWidgetsChanged()
  widget_managers_.clear()           # destroys all QQuickViews + LayerSurfaces
  collisionMap.clear()
  for each WidgetDefinition in configService_->widgets().definitions:
    validate; collision-check; warn-once
    widget_managers_.push_back(make_unique<WidgetManager>(...))
    widget_managers_.back()->start()
```

### (d) Countdown Tick

```
C++ side (WidgetManager, one per WidgetDefinition):
  tick_timer_ (QTimer, interval 1s or 60s, synced to minute boundary)
    -> on timeout: compute remaining = deadline - QDateTime::currentDateTime()
                   remainingText_ = formatCountdown(remaining, showSeconds_)
                   emit remainingTextChanged()

QML side (WidgetSurface.qml root):
  property string remainingText   // bound to WidgetManager::remainingText

TimeToEventWidget.qml:
  Text { text: root.remainingText }   // updates on property change
  // No QML Timer element — all timing lives in C++
```

**Rationale for C++-side timer:** A single `QTimer` per `WidgetDefinition` serves all monitor instances simultaneously (all surfaces for one widget show the same countdown per REQ-C-006). Freezing is a clean `timer.stop()` / `timer.start()`, with a resync of the display value on resume. Hidden surfaces do zero work. The `remainingText` string is computed once and propagated to all views via property change notification. A QML `Timer` per view would require `N × monitors` timers and per-view freeze/resync logic — more moving parts and harder to test.

---

## Config Schema

### TOML Format

```toml
[widgets]
margin = 32   # logical pixels, applied uniformly; default 32

[[widget]]
type = "time-to-event"
title = "Flight departs"
deadline = "2026-07-15T14:30:00"   # ISO 8601 date or datetime (local time)
position = "center-top"             # optional; default "center-center"
monitors = ["DP-1", "eDP-1"]       # optional; empty/absent = all monitors
show_seconds = false                # optional; default false
```

### C++ Structs (add to `src/core/ConfigService.h`)

```cpp
enum class WidgetPosition : uint8_t {
    LeftTop, CenterTop, RightTop,
    LeftCenter, CenterCenter, RightCenter,
    LeftBottom, CenterBottom, RightBottom,
};

struct TimeToEventConfig {
    QString title;
    QDateTime deadline;
    bool has_time{false};      // true iff deadline string contained "T" + time component
    bool show_seconds{false};

    bool operator==(const TimeToEventConfig&) const = default;
};

using WidgetTypeConfig = std::variant<TimeToEventConfig>;  // extend for future types

struct WidgetDefinition {
    WidgetTypeConfig type_config;
    QStringList monitors;           // empty = all monitors
    WidgetPosition position{WidgetPosition::CenterCenter};

    bool operator==(const WidgetDefinition&) const = default;
};

struct WidgetsConfig {
    int margin{32};
    QList<WidgetDefinition> definitions;

    bool operator==(const WidgetsConfig&) const = default;
};
```

`WidgetPosition` needs a free function `widgetPositionFromString(const QString&) -> std::optional<WidgetPosition>` used by the parser.

### Parser Notes

- Parse `[widgets]` table for `margin` with `readPositiveInt`.
- Iterate `tbl["widget"].as_array()` (TOML `[[widget]]` maps to a `toml::array` of `toml::table` nodes).
- For each entry: read `type` string first; if unknown, `qCWarning` + `continue` (REQ-F-028).
- For `time-to-event`: require `title` (REQ-F-026) and `deadline` (REQ-F-027); parse `deadline` by checking for `'T'` substring to set `has_time`; use `QDateTime::fromString(str, Qt::ISODate)` for datetime and `QDate::fromString(str, Qt::ISODate)` + `.startOfDay()` for date-only.
- `position` defaults to `CenterCenter` with no warning (REQ-F-029).
- `show_seconds` defaults to `false` with no warning (REQ-F-030).
- Warn once per undefined monitor name at parse time (REQ-F-011); compare against `QGuiApplication::screens()` names.

---

## Key Interfaces / Public APIs

### `WidgetsConfig` additions to `ConfigService`

```cpp
// In ConfigService public section:
[[nodiscard]] const WidgetsConfig& widgets() const { return widgets_; }

// In Q_SIGNALS:
void widgetsChanged();

// In private:
WidgetsConfig widgets_;
```

### `MonitorOccupancyService` (`src/services/MonitorOccupancyService.h`)

```cpp
class MonitorOccupancyService : public QObject {
    Q_OBJECT
public:
    explicit MonitorOccupancyService(
        HyprlandWorkspaceService* hws,
        ActiveWindowService* aws,
        QObject* parent = nullptr);

    // True iff the workspace currently visible on monitorName has no windows.
    // Returns true (empty) for unknown monitor names (conservative default).
    [[nodiscard]] bool isMonitorEmpty(const QString& monitorName) const;

Q_SIGNALS:
    void occupancyChanged(const QString& monitorName, bool isEmpty);

private Q_SLOTS:
    void onMonitorWindowChanged(const QString& monitorName);
    void onWorkspaceOccupancyChanged();   // connected to WorkspaceModel::revisionChanged

private:
    [[nodiscard]] bool computeEmpty(const QString& monitorName) const;

    HyprlandWorkspaceService* hws_;   // for occupied_workspace_ids (via WorkspaceModel)
    ActiveWindowService* aws_;
    QHash<QString, bool> last_state_; // monitorName -> last emitted isEmpty value
};
```

**Signal/field consumption:**
- `ActiveWindowService::monitorWindowChanged(const QString& monitorName)` — fires when the visible window on a monitor changes; `MonitorOccupancyService` re-evaluates that monitor.
- `ActiveWindowService` internal `monitor_workspaces` is not directly accessible; instead `MonitorOccupancyService` relies on `HyprlandWorkspaceService`'s underlying `WorkspaceModel` for `occupied_workspace_ids`.
- `WorkspaceModel::revisionChanged()` — fires whenever `setOccupiedWorkspaceIds` runs; `MonitorOccupancyService` re-evaluates all tracked monitors.
- To get "which workspace is visible on monitor X": `MonitorOccupancyService` needs a new accessor on `ActiveWindowService`:

```cpp
// Add to ActiveWindowService public section:
[[nodiscard]] int visibleWorkspaceIdForMonitor(const QString& monitorName) const;
// Returns active_window_state_.monitor_workspaces.value(monitorName, -1)
```

- To get `occupied_workspace_ids`: add accessor to `WorkspaceModel` (or `HyprlandWorkspaceService` exposes the model):

```cpp
// WorkspaceModel already has occupied_workspace_ids_ private; add:
[[nodiscard]] bool isWorkspaceOccupied(int wsId) const;
// Returns occupied_workspace_ids_.contains(wsId)
```

### `WidgetManager` (`src/surfaces/WidgetManager.h`)

```cpp
class WidgetManager : public PerMonitorLayerManager {
    Q_OBJECT
public:
    WidgetManager(
        LayerShell& shell,
        WidgetDefinition definition,
        int margin,                        // from WidgetsConfig::margin
        MonitorOccupancyService* occupancy,
        QObject* parent = nullptr);

    // Unique stable identifier for logging and collision map (config-order index
    // is assigned by the coordinator before construction).
    void setIndex(int index);

protected:
    [[nodiscard]] LayerConfig layerConfig() const override;
    void configureSurface(LayerSurface& surface, QScreen* screen) override;
    [[nodiscard]] QmlSource qmlSource(QScreen* screen) override;

private Q_SLOTS:
    void onOccupancyChanged(const QString& monitorName, bool isEmpty);
    void onTick();

private:
    [[nodiscard]] bool targetedAt(const QString& monitorName) const;
    [[nodiscard]] QString formatCountdown() const;
    [[nodiscard]] QVariantMap buildInitialProperties(QScreen* screen) const;
    void startTimer();
    void syncTimerInterval();

    WidgetDefinition definition_;
    int margin_;
    int index_{0};
    MonitorOccupancyService* occupancy_;
    QTimer tick_timer_;
    QString remaining_text_;
};
```

**`PerMonitorLayerManager` filter:** `createSurface` is called for every `QScreen` by the base. `WidgetManager` skips creation when `!targetedAt(screen->name())` inside `qmlSource` — but since the base calls `createSurface` unconditionally, `WidgetManager` should either (a) override `qmlSource` to return an empty/null URL causing the view to load nothing, or (b) track skipped screens. Better: override a new virtual `bool shouldCreateSurface(QScreen*)` — but that changes the base class. **Preferred approach without modifying the base:** implement `configureSurface` and `qmlSource` normally for all screens, and immediately call `view->hide()` post-creation for non-targeted screens — but those surfaces still consume Wayland resources.

**Cleanest resolution:** add one protected virtual to `PerMonitorLayerManager`:

```cpp
// New virtual in PerMonitorLayerManager (protected):
[[nodiscard]] virtual bool shouldCreateSurface(QScreen* screen) const { return true; }
```

`WidgetManager` overrides it to check `targetedAt(screen->name())`. This is a minimal, non-breaking addition to the base class.

---

## Occupancy Gate Design

### Approach: Compose Existing Services (Option A)

`MonitorOccupancyService` subscribes to both `ActiveWindowService` and `WorkspaceModel` — no new Hyprland IPC socket. This avoids duplicating the event parsing already done by `HyprlandWorkspaceService` and `ActiveWindowService`.

**Data consumed:**
- `ActiveWindowService::monitorWindowChanged(monitorName)` — triggers re-evaluation for that monitor.
- `ActiveWindowService::visibleWorkspaceIdForMonitor(monitorName)` (new accessor, see above) — returns the workspace ID currently displayed on that monitor.
- `WorkspaceModel::isWorkspaceOccupied(wsId)` (new accessor) — checks `occupied_workspace_ids_`.
- `WorkspaceModel::revisionChanged()` — triggers re-evaluation of all monitors (because `setOccupiedWorkspaceIds` was called, meaning window open/close on any workspace).

**`computeEmpty(monitorName)` logic:**
```
int wsId = aws_->visibleWorkspaceIdForMonitor(monitorName);
if (wsId == -1) return true;  // unknown monitor -> treat as empty
return !workspaceModel_->isWorkspaceOccupied(wsId);
```

**Why not dedicate a new IPC consumer:** `ActiveWindowService` already opens the Hyprland event socket and parses `workspace>>`, `openwindow>>`, `closewindow>>`, `focusedmon>>` events. A second IPC socket for the same data stream would be redundant and fragile. Composing the two existing services is the lowest-duplication path.

**Why `MonitorOccupancyService` is its own class (not inlined into `WidgetManager`):** Multiple `WidgetManager` instances for different widget definitions all need occupancy for the same monitors. A shared singleton avoids N redundant observers on the same signals.

**Registration:** `MonitorOccupancyService` is constructed in `ShellApplication::startServices()` alongside `aws_` and `workspace_service_`. It is NOT registered as a QML singleton — only `WidgetManager` consumes it.

---

## Positioning & Layer-Shell Anchoring Design

### Position Enum to Layer-Shell Anchors

Widgets are **content-sized surfaces** (not fullscreen). The layer-shell `set_anchor` + `set_margin` approach is used rather than a fullscreen surface with internal QML positioning. This is cleaner: the compositor places the surface at the right coordinates, no QML geometry math needed.

For a content-sized surface, `set_size(w, h)` sets the fixed pixel dimensions and `set_anchor` pins the surface to one or more edges. The correct anchor combination per position:

| Position | `set_anchor` flags | Effective placement |
|---|---|---|
| `left-top` | `anchor_left \| anchor_top` | Top-left corner |
| `center-top` | `anchor_top` | Top edge, centered horizontally |
| `right-top` | `anchor_right \| anchor_top` | Top-right corner |
| `left-center` | `anchor_left` | Left edge, centered vertically |
| `center-center` | _(no anchor)_ | Center of monitor |
| `right-center` | `anchor_right` | Right edge, centered vertically |
| `left-bottom` | `anchor_left \| anchor_bottom` | Bottom-left corner |
| `center-bottom` | `anchor_bottom` | Bottom edge, centered horizontally |
| `right-bottom` | `anchor_right \| anchor_bottom` | Bottom-right corner |

Layer-shell margins (`set_margin(top, right, bottom, left)`) express offset from the pinned edge(s). For corner anchors the margin applies to both edges; for edge-only anchors it applies to the pinned edge only.

### Bar Height Offset

`kBarHeight = 64` is defined as a `static constexpr int` in `src/surfaces/LayerShellManager.cpp`. For top-anchored positions the effective top margin is `kBarHeight + margin`. `WidgetManager` reads this constant (either by duplicating it in `WidgetManager.cpp` or by moving it to a shared header, e.g., `src/surfaces/ShellConstants.h`).

**Recommended:** extract `kBarHeight` into `src/surfaces/ShellConstants.h` as `inline constexpr int kBarHeight = 64;` and include it in both `LayerShellManager.cpp` and `WidgetManager.cpp`.

### Surface Sizing

Widget surfaces are **fixed-size**: `WidgetManager` picks a content rectangle based on widget type. For `time-to-event` a fixed size (e.g., 280 × 120 logical pixels) is set via `surface.set_size(kWidgetWidth, kWidgetHeight)`. The QML root item fills the view (`SizeRootObjectToView` resize mode, inherited from `PerMonitorLayerManager::createSurface`).

Future widget types can override sizing by providing different constants.

### `configureSurface` implementation sketch

```cpp
void WidgetManager::configureSurface(LayerSurface& surface, QScreen* /*screen*/) {
    surface.set_anchor(anchorForPosition(definition_.position));
    surface.set_size(kWidgetWidth, kWidgetHeight);
    surface.set_exclusive_zone(-1);
    const int top_margin = isTopAnchored(definition_.position)
        ? kBarHeight + margin_ : margin_;
    surface.set_margin(top_margin,
                       margin_,
                       margin_,
                       margin_);
}
```

---

## Countdown Timer & Freeze Design

All timing lives in C++. `WidgetManager` owns one `QTimer tick_timer_` per `WidgetDefinition` (shared across all monitor surfaces of that widget).

### Tick Interval and Minute Sync

- `show_seconds = true`: interval = 1000 ms.
- `show_seconds = false`: interval = 60 000 ms, but the first fire is delayed to the next wall-clock minute boundary.

```cpp
void WidgetManager::startTimer() {
    const QDateTime now = QDateTime::currentDateTime();
    if (definition_.show_seconds()) {
        tick_timer_.setInterval(1000);
        tick_timer_.start();
    } else {
        // Delay first tick to the start of the next minute.
        int msToNextMinute = (60 - now.time().second()) * 1000 - now.time().msec();
        QTimer::singleShot(msToNextMinute, this, [this]() {
            onTick();
            tick_timer_.setInterval(60000);
            tick_timer_.start();
        });
    }
}
```

### Freeze on Hide / Resume on Show

```cpp
// On workspace occupied:
tick_timer_.stop();
// (remaining_text_ retains its last value — surfaces are hidden so stale display is irrelevant)

// On workspace empty (resume + resync):
remaining_text_ = formatCountdown();   // recompute from current wall clock
propagateRemainingText();
tick_timer_.stop();
startTimer();   // re-derive next tick time from current wall clock
```

The resync call to `startTimer()` on resume prevents drift (REQ-F-006): if the timer was frozen for 30 seconds in the middle of a minute, it restarts the minute-boundary alignment from the current time.

### Countdown Format (`formatCountdown`)

```
remaining = deadline - QDateTime::currentDateTime()
if remaining <= 0: return "Now"

days    = remaining.days()
hours   = remaining.hours() % 24
minutes = remaining.minutes() % 60
seconds = remaining.seconds() % 60  (only if show_seconds)

Start from the first non-zero unit:
  if days    > 0: output "%1d"  padded
  if hours   > 0: output "%02dh"
  if minutes > 0: output "%02dm"
  if seconds > 0 && show_seconds: output "%02ds"
```

### REQ-F-023 Sub-minute with `show_seconds = false`

When the deadline is fewer than 60 seconds away and `show_seconds = false`:
- `days = 0, hours = 0, minutes = 0` — none of the non-zero conditions fire.
- Display: **"00m"** (minutes field always shown as the minimum unit when `show_seconds = false`).
- Implementation: after the adaptive logic, if `show_seconds = false` and the output string is empty, append `"00m"`.
- The timer still ticks once per minute (it does not switch to per-second); "00m" is static until the deadline passes and flips to "Now".
- No rounding up: "00m" reflects that there is less than 1 minute remaining, not "01m".

---

## Collision Detection & Warn-Once Design

### Algorithm

Collision detection runs in the coordinator (inline in `ShellApplication` or a thin `WidgetCoordinator` helper) before `WidgetManager` instances are created, once per config load.

```
claimedSlots: QSet<QPair<QString, WidgetPosition>>  // (monitorName, position)
warnedCollisions: QSet<QPair<int, QString>>          // (widgetIndex, monitorName)

for each WidgetDefinition def at config-order index i:
    effectiveMonitors = def.monitors.isEmpty()
        ? allConnectedMonitorNames()
        : def.monitors

    // Also check configured-but-not-yet-connected monitors (REQ-F-011)
    for each monitorName in effectiveMonitors:
        slot = {monitorName, def.position}
        if claimedSlots.contains(slot):
            key = {i, monitorName}
            if !warnedCollisions.contains(key):
                qCWarning(lcWidgets) << "Widget" << i
                    << "dropped on" << monitorName
                    << "at position" << positionName(def.position)
                    << "— collision with earlier widget";
                warnedCollisions.insert(key)
            // skip this monitor for this widget
        else:
            claimedSlots.insert(slot)
            // record that this widget is allowed on this monitor

pass the per-widget allowed-monitors list to each WidgetManager constructor
```

`warnedCollisions` is stored in the coordinator, not in `WidgetManager` itself. It is cleared on config reload (after which new warnings are issued once for the new config — REQ-F-016: "reload the configuration — a new warning is logged, but after that, repeated reloads do not log additional warnings").

The `WidgetManager` receives an `allowedMonitors: QStringList` at construction. This is the set of monitors for which it should create surfaces (the intersection of `def.monitors` with non-colliding slots). `shouldCreateSurface(screen)` checks `allowedMonitors.contains(screen->name())`.

---

## QML Component Design

### Directory Structure

```
src/qml/Widgets/
    WidgetSurface.qml         # root item for every widget view
    TimeToEventWidget.qml     # time-to-event content
```

### `WidgetSurface.qml`

```qml
import QtQuick
import HolonightShell
import Holonight

Item {
    id: root

    // Set via setInitialProperties in WidgetManager::qmlSource()
    required property string widgetType        // "time-to-event"
    required property string barMonitorName    // for future monitor-specific logic
    required property string remainingText     // updated by C++ property binding
    required property string titleText
    required property string deadlineLabelText // formatted deadline date/time
    required property bool   showSeconds

    Loader {
        anchors.fill: parent
        source: root.widgetType === "time-to-event"
            ? "TimeToEventWidget.qml"
            : ""
        onLoaded: {
            item.remainingText = Qt.binding(() => root.remainingText)
            item.titleText     = Qt.binding(() => root.titleText)
            item.deadlineLabel = Qt.binding(() => root.deadlineLabelText)
        }
    }
}
```

**Note on `setInitialProperties` + `onXChanged`:** Per CLAUDE.md, `required property` set via `setInitialProperties` fires `onXChanged` during construction. `WidgetSurface.qml` should guard any `onRemainingTextChanged` handler with a `ready` flag set in `Component.onCompleted` if it has side effects.

### `TimeToEventWidget.qml`

Three-element vertical layout: title (top, small), countdown (center, large/prominent), date label (bottom, small). All colors from `HoloniightPalette`.

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Holonight

ColumnLayout {
    id: root

    required property string remainingText
    required property string titleText
    required property string deadlineLabel

    spacing: 4

    Text {
        id: titleLabel
        Layout.alignment: Qt.AlignHCenter
        text: root.titleText
        font.family: ThemeService.titleFont
        font.pixelSize: ThemeService.titleFontSize
        color: HoloniightPalette.onSurfaceVariant
    }

    Text {
        id: countdownLabel
        Layout.alignment: Qt.AlignHCenter
        text: root.remainingText
        font.family: ThemeService.clockFont
        font.pixelSize: ThemeService.clockFontSize * 2
        color: HoloniightPalette.primary
    }

    // Glow on countdown: MultiEffect declared BEFORE any overlaid elements (CLAUDE.md z-order rule)
    MultiEffect {
        source: countdownLabel
        anchors.fill: countdownLabel
        shadowEnabled: true
        shadowColor: HoloniightPalette.primary
        shadowBlur: 0.6
        shadowOpacity: 0.8
    }

    Text {
        id: dateLabelItem
        Layout.alignment: Qt.AlignHCenter
        text: root.deadlineLabel
        font.family: ThemeService.uiFont
        font.pixelSize: ThemeService.uiFontSize
        color: HoloniightPalette.onSurfaceVariant
    }
}
```

**Theming rules enforced:**
- No hardcoded hex values — all colors via `HoloniightPalette.<token>`.
- No hardcoded font family strings — all via `ThemeService.*Font` and `ThemeService.*FontSize` properties.
- Import `Holonight` (not `HolonightShell`) for palette access (note double-i: `HoloniightPalette`).
- `MultiEffect` declared before any elements that must render above it per CLAUDE.md.

**No `Text.StyledText` link color issue:** These are plain `Text` elements with no HTML content, so the `StyledText` / `<a>` color pitfall does not apply.

---

## CMake / QRC Wiring Changes

### New source files

**`holonight_core` library** (`CMakeLists.txt` lines 217–234):
- No new files; `WidgetsConfig` and its structs are added to the existing `ConfigService.h/.cpp`.

**`holonight_services` library** (`CMakeLists.txt` lines 258–303):
- Add:
  ```cmake
  src/services/MonitorOccupancyService.h
  src/services/MonitorOccupancyService.cpp
  ```
- `target_include_directories` already includes `src/services/`.

**`holonight_surfaces` library** (`CMakeLists.txt` lines 331–384):
- Add:
  ```cmake
  src/surfaces/WidgetManager.h
  src/surfaces/WidgetManager.cpp
  ```
- Also: new optional `src/surfaces/ShellConstants.h` (if extracting `kBarHeight`).

**`holonight_app` library** (`CMakeLists.txt` lines 390–407):
- No new files; `ShellApplication.cpp` gains `#include "WidgetManager.h"` and `#include "MonitorOccupancyService.h"`.

### New QML files

Add to `HOLONIGHT_QML_FILES` (the list must remain sorted; insert alphabetically):
```cmake
src/qml/Widgets/TimeToEventWidget.qml
src/qml/Widgets/WidgetSurface.qml
```

The CMake configure step will FATAL_ERROR if these are missing from the filesystem or if any `src/qml/*.qml` is absent from the list (the `HOLONIGHT_DISCOVERED_QML_FILES` glob check at line 509–519).

### No new QRC resources

Widget QML components are bundled automatically via `qt6_add_qml_module` with the `QML_FILES` list. No separate `qt6_add_resources` call is needed unless widget-specific non-QML assets (icons, etc.) are added later.

---

## Key Decisions with Rationale

- **Option C for multiplicity (one `WidgetManager` per definition):** Reuses `PerMonitorLayerManager` unchanged. One small virtual addition (`shouldCreateSurface`) is the only base-class change. Coordinator-level teardown on reload maps cleanly to `QList::clear()`. Alternative (A: generalize base) rejected — pollutes stable class used by bars and wallpapers. Alternative (B: no inheritance) rejected — duplicates debugged Wayland plumbing.

- **C++-side countdown timer:** One `QTimer` per `WidgetDefinition` shared across all monitor instances. Stop/start freeze cycle with `startTimer()` resync on resume. All surfaces update via property propagation from a single source of truth. QML-side `Timer` per view rejected — N timers, harder freeze/resync, inconsistent state possible across surfaces.

- **`MonitorOccupancyService` composes existing services:** Subscribes to `ActiveWindowService::monitorWindowChanged` and `WorkspaceModel::revisionChanged`. Requires two small new accessors (`visibleWorkspaceIdForMonitor` on AWS, `isWorkspaceOccupied` on WorkspaceModel). New Hyprland IPC socket rejected — duplicates existing event parsing.

- **Anchor+margin positioning, content-sized surface:** Layer-shell anchor+margin handles placement; compositor performs geometry math. QML root just fills the view. Fullscreen surface with internal QML positioning rejected — wastes compositor resources; widgets would consume full-screen input region even with `WindowTransparentForInput`.

- **Collision detection in coordinator, before `WidgetManager` construction:** `WidgetManager` receives an `allowedMonitors` list; it never creates a surface for a colliding slot. Warn-once dedup lives in the coordinator's `warnedCollisions` set, cleared on config reload (one warning per (widget, monitor) per config load cycle, per REQ-F-016).

- **"00m" for sub-minute with `show_seconds = false`:** The adaptive format produces an empty string for < 60s remaining when seconds are suppressed. A minimum-unit fallback emits "00m". No rounding up. Timer continues at 1-minute interval; "00m" is stable until deadline passes.

---

## Alternatives Considered

- **`WidgetManager` as a non-`PerMonitorLayerManager` class with manual screen lifecycle:** Provides maximum flexibility but duplicates ~60 lines of Wayland boilerplate. Rejected in favor of Option C.

- **A dedicated `WidgetSurface` class (not `QQuickView`):** Following the `PopupSurface`/`TooltipSurface` pattern (manual `QWindow` lifecycle). These classes manage a single floating surface with custom placement logic; widgets need per-monitor replication which `PerMonitorLayerManager` provides. Rejected.

- **Inline occupancy logic in `WidgetManager`:** Each `WidgetManager` subscribes independently to AWS + WorkspaceModel. Works, but creates N duplicate signal connections for N widget definitions targeting the same monitor. `MonitorOccupancyService` is the deduplicated version.

- **Full-screen `bottom`-layer surface with internal QML widget positioning:** Mirrors the wallpaper approach. Simple Wayland setup, but requires the full monitor area as the input-transparent surface (wastes compositor memory for alpha regions), and internal QML anchor math for nine positions. Rejected in favor of content-sized surfaces.

---

## Known Risks & Open Questions

- **`QQuickView::hide()` / `show()` and wlr-layer-shell remap:** The design assumes that `QQuickView::hide()` unmaps the Wayland surface (sends `wl_surface.attach(null)` or equivalent) and `show()` remaps it. This has not been verified against Hyprland's `wlr-layer-shell` implementation. If `hide()` only hides the Qt window without unmapping the Wayland surface, the widget would remain visible to the compositor. **Mitigation:** Verify empirically during the implementation sprint using `wl-spy` or Hyprland's debug overlay. If `hide()` does not unmap, the fallback is to set `surface.set_size(0, 0)` on hide and restore the real size on show, or to use a 1×1 transparent surface.

- **Property propagation to hidden `QQuickView` instances:** When `view->hide()` is called, Qt may stop processing property updates for the view's QML tree. The design relies on `remaining_text_` being pushed into the view at show-time (after `startTimer()` resync). Confirm that `setProperty("remainingText", ...)` on a hidden view's root object succeeds and the value is retained until show.

- **REQ-F-023 sub-minute with `show_seconds = false` — behavior pinned:** Under 1 minute remaining with `show_seconds = false`, the display shows "00m" (not rounded up to "01m"), the timer still fires at the next minute boundary (not accelerated), and it flips to "Now" when the deadline passes. No rounding is performed. This is the specified behavior and is not ambiguous.

- **Minute boundary sync on rapid config reload:** If a config reload happens in the middle of the `QTimer::singleShot` delay for minute-boundary sync, the single-shot fires against the old `WidgetManager` instance (which is being destroyed). The `this` capture in the lambda becomes a dangling pointer. **Mitigation:** Use a `QTimer` member (not `singleShot`) initialized to the remaining-ms interval, and connect `destroyed()` or simply ensure `widget_managers_.clear()` stops all timers via normal `QObject` destructor cascade before the new managers are started.

- **Monitor names configured but not yet connected (REQ-F-011):** `shouldCreateSurface` checks `allowedMonitors.contains(screen->name())`. For a monitor that is hot-plugged after startup, `handleScreenAdded` in the base class is called; `WidgetManager` must have the configured monitor name in its `allowedMonitors` list even before the screen appears. This works naturally: `allowedMonitors` is derived from config (which includes the future monitor name), not from `QGuiApplication::screens()` at construction time.

- **`kBarHeight` duplication:** Currently `kBarHeight = 64` is a `static constexpr` local to `LayerShellManager.cpp` (line 11). Extracting it to `src/surfaces/ShellConstants.h` is a minor refactor that must not break `LayerShellManager`'s existing behavior. Verify that the constant is not overridden dynamically (confirmed: it is not; the bar geometry is fixed at 64 logical pixels).

- **TOML `[[widget]]` parse:** `toml::table::at("widget")` returns the array of tables; verify the key is `"widget"` (singular), not `"widgets"`, since TOML `[[widget]]` appends to `tbl["widget"]`, while `[widgets]` is a separate singular table. Both keys coexist; the parser reads `tbl["widgets"]` for the `margin` global and `tbl["widget"]` (as `as_array()`) for the definitions.
