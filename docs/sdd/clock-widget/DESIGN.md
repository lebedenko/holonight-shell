# Clock Widget — Design Document

## 1. Overview

The clock widget is a new `WidgetType::Clock` entry in the existing desktop-widget framework. It slots
alongside the `time-to-event` widget: one `WidgetManager` instance is created per `WidgetDefinition`
with `type = "clock"` and manages one wlr-layer-shell surface per target monitor. Like the countdown
widget, surfaces live on the `bottom` layer, remain mapped at all times, and toggle visibility via the
QML root's `visible` property. A single `QTimer` shared across all of a definition's monitor surfaces
drives the display; its interval is 1 s when `show_seconds = true`, or aligned to the next wall-clock
minute boundary when `show_seconds = false`. The timer is frozen while all surfaces are hidden
(workspace occupied) and resyncs to the current wall-clock time when revealed. All formatting is done
in C++ inside a new `WidgetClock` helper module (mirroring `WidgetCountdown`); QML receives
pre-formatted strings.

---

## 2. Component Inventory

### New files

| File | Role |
|---|---|
| `src/surfaces/WidgetClock.h` | Pure formatting helpers for the clock widget; unit-testable; mirrors `WidgetCountdown.h`. |
| `src/surfaces/WidgetClock.cpp` | Implementations of `formatClockTime`, `formatClockSeconds`, `formatClockDate`, and `clockTickIntervalMs`. |
| `src/qml/Widgets/ClockWidget.qml` | QML body component for the clock widget; two centred rows (time + date), MultiEffect glow, StableDigitsText, ThemeService fonts, HoloniightPalette colours. |

### Modified files

| File | Change |
|---|---|
| `src/core/ConfigService.h` | Add `WidgetType::Clock`; add `ClockConfig` struct; add `bool enabled{true}` to `WidgetDefinition`; embed `ClockConfig clock` in `WidgetDefinition`. |
| `src/core/ConfigService.cpp` | Add `parseClockFields()`; extend `parseWidgetEntry()` to handle `type == "clock"` and parse `enabled` before the type switch. |
| `src/surfaces/WidgetManager.h` | Add `clock_time_text_`, `clock_seconds_text_`, `clock_date_text_` cached strings; add private helpers `recomputeClockStrings()`, `currentClockStrings()`, `startClockTickTimer()`. |
| `src/surfaces/WidgetManager.cpp` | Branch all type-dependent code (`qmlSource`, `onTick`, `recomputeAndPropagate`, `startTickTimer`, `updateTimerState`) on `definition_.type`. |
| `src/qml/Widgets/WidgetSurface.qml` | Add `required property` declarations for `timeText`, `secondsText`, `dateText` (with empty-string defaults); add a `Loader` branch for `widgetType === "clock"` that instantiates `ClockWidget`. |
| `src/app/ShellApplication.cpp` | Skip `enabled == false` definitions in `rebuildWidgets()` before constructing a `WidgetManager`. |
| `CMakeLists.txt` | Add `ClockWidget.qml` to `HOLONIGHT_QML_FILES`; add `WidgetClock.h`/`WidgetClock.cpp` to the source list. |
| `tests/CMakeLists.txt` | Add a new `test_widget_clock.cpp` test target. |

---

## 3. Data Model

### 3.1 `ClockConfig`

```cpp
// src/core/ConfigService.h

struct ClockConfig {
  bool show_seconds{true};
  QString date_format{};   // empty = use default "dddd, d MMMM yyyy" at format time
  QString locale{};        // empty = use QLocale::system() at format time
};
```

All fields have default member initializers, satisfying the project's
`modernize-use-designated-initializers` / `cppcoreguidelines-pro-type-member-init` requirements.

`date_format` is intentionally left empty as the default stored in `ClockConfig`. The actual fallback
pattern `"dddd, d MMMM yyyy"` lives only inside `WidgetClock::formatClockDate()`; this keeps a single
source of truth for the pattern and avoids coupling ConfigService to a formatting concern (see
decision §6.3).

### 3.2 `WidgetDefinition` additions

```cpp
// src/core/ConfigService.h  (additions only)

enum class WidgetType : std::uint8_t {
  TimeToEvent,
  Clock,           // NEW
};

struct WidgetDefinition {
  WidgetType type{WidgetType::TimeToEvent};
  QStringList monitors;
  WidgetPosition position{WidgetPosition::CenterCenter};
  bool enabled{true};              // NEW: false → no WidgetManager/surfaces created
  TimeToEventConfig time_to_event; // valid when type == WidgetType::TimeToEvent
  ClockConfig clock;               // NEW: valid when type == WidgetType::Clock
  bool operator==(const WidgetDefinition&) const = default;
};
```

The existing approach — embedding type-specific structs alongside the `WidgetType` discriminator —
is preserved. `ClockConfig` is zero-cost when unused (all defaults), so there is no memory impact on
`time-to-event` definitions.

### 3.3 Type discrimination

`WidgetManager` (and `ShellApplication::rebuildWidgets`) switch on `definition_.type`. The QML root
receives `widgetType` as either `"time-to-event"` or `"clock"`; the `Loader` in `WidgetSurface.qml`
activates the matching component.

---

## 4. Data Flow

### 4.1 Static flow (startup / config parse → surface creation)

```
config.toml  [[widget]] type = "clock"
    │
    ▼
ConfigService::parseFile()
  └─ parseWidgets(table)
       └─ parseWidgetEntry(entry)
            ├─ read enabled  →  WidgetDefinition.enabled
            ├─ type == "clock" → parseClockFields(entry)
            │       returns ClockConfig { show_seconds, date_format, locale }
            │       (never returns nullopt — all fields optional)
            ├─ parseWidgetPositionField(entry, "clock") → WidgetDefinition.position
            ├─ parseWidgetMonitors(entry, "clock")      → WidgetDefinition.monitors
            └─ WidgetDefinition { type=Clock, enabled, clock, position, monitors }
                    │
                    ▼ widgetsConfigChanged() signal
ShellApplication::rebuildWidgets()
  ├─ skip defs where enabled == false (no WidgetManager constructed)
  ├─ blockersForWidget(defs, i)  — unchanged, works for any WidgetType
  └─ std::make_unique<WidgetManager>(shell, def, margin, i, blockers, occupancy)
          │
          ▼  PerMonitorLayerManager::start()
          for each QScreen:
            shouldCreateSurface() → targetedAt() && !blockedOn()
            qmlSource() → URL + initial_properties (type-dispatched)
            configureSurface() → anchors, size, margin, configured signal
```

### 4.2 Tick update (running, surface visible)

```
tick_timer_ fires (single-shot)
    │
    ▼
WidgetManager::onTick()
    ├─ definition_.type == Clock → recomputeClockStrings()
    │       calls WidgetClock::formatClockTime(now, show_seconds)  → clock_time_text_
    │       calls WidgetClock::formatClockSeconds(now)             → clock_seconds_text_
    │       calls WidgetClock::formatClockDate(now, locale, fmt)   → clock_date_text_
    │
    ├─ propagate to each surface:
    │     root->setProperty("timeText",    clock_time_text_)
    │     root->setProperty("secondsText", clock_seconds_text_)
    │     root->setProperty("dateText",    clock_date_text_)
    │
    └─ startTickTimer() (resched: 1s or minute-aligned)
```

The clock never has a "finished" state (unlike countdown), so `onTick` always reschedules.

### 4.3 Occupancy hide → timer freeze

```
MonitorOccupancyService::occupancyChanged("DP-1", false /*not empty*/)
    │
    ▼
WidgetManager::onOccupancyChanged("DP-1", false)
    └─ applyVisibility("DP-1")
          content_visible_["DP-1"] = false
          root->setVisible(false)          // QML root hidden; QQuickView stays mapped
          updateTimerState()
            anySurfaceVisible() == false → tick_timer_.stop()
```

No clock strings are updated while frozen; the cached `clock_*_text_` fields hold the last-seen
values.

### 4.4 Occupancy reveal → resync

```
MonitorOccupancyService::occupancyChanged("DP-1", true /*empty*/)
    │
    ▼
WidgetManager::onOccupancyChanged("DP-1", true)
    └─ applyVisibility("DP-1")
          content_visible_["DP-1"] = true
          // Push fresh strings BEFORE making root visible (avoids stale frame):
          recomputeClockStrings()   ← reads QDateTime::currentDateTime()
          root->setProperty("timeText",    clock_time_text_)
          root->setProperty("secondsText", clock_seconds_text_)
          root->setProperty("dateText",    clock_date_text_)
          root->setVisible(true)
          updateTimerState()
            anySurfaceVisible() == true → startTickTimer()
            startTickTimer() aligns to next wall-clock boundary (no drift)
```

This matches the existing `applyVisibility` pattern for countdown, which also pushes `remainingText`
before revealing.

### 4.5 Config hot-reload

```
User edits config.toml
    │
    ▼ (200ms debounce)
ConfigService::parseFile()  → widgetsConfigChanged()
    │
    ▼
ShellApplication::rebuildWidgets()
    widget_managers_.clear()    ← destroys all surfaces (PerMonitorLayerManager dtor)
    for each def in new defs:
        skip if !def.enabled
        make_unique<WidgetManager>(...)
        manager->start()
    emit widgetsChanged()
```

Hot-reload is a full destroy-and-rebuild. There is no in-place patch of `ClockConfig` inside a live
`WidgetManager`. This is the existing model; the clock widget inherits it unchanged.

---

## 5. Interfaces and APIs

### 5.1 `WidgetClock` free functions (`src/surfaces/WidgetClock.h`)

```cpp
#pragma once

#include <QString>

class QDateTime;
class QLocale;

// Pure formatting helpers for the clock widget, factored out of WidgetManager so they can be
// unit-tested without a live LayerShell. All functions are deterministic given their inputs.

// Returns the HH:mm portion of `now` in 24-hour format. The caller renders this at large size.
[[nodiscard]] QString formatClockTime(const QDateTime& now);

// Returns the zero-padded two-digit seconds string (e.g. "07"). Returns an empty string when
// show_seconds is false, allowing the QML side to hide the seconds element via visible binding.
[[nodiscard]] QString formatClockSeconds(const QDateTime& now, bool show_seconds);

// Returns the formatted date string for `now` using `locale` and the `date_format` Qt format
// pattern. If `date_format` is empty, uses the default pattern "dddd, d MMMM yyyy". If the
// formatted result is empty (e.g. an invalid pattern produced nothing), falls back to the default
// pattern and logs a qCWarning once via the `warned_format` flag (the caller owns the flag and
// resets it only on a config rebuild, ensuring warn-once per widget definition lifetime).
[[nodiscard]] QString formatClockDate(const QDateTime& now,
                                      const QLocale& locale,
                                      const QString& date_format,
                                      bool& warned_format);

// Returns the QTimer interval in milliseconds for the next tick, aligned to the next wall-clock
// boundary. When show_seconds is true, returns 1000. When false, returns the milliseconds
// remaining until the next full minute (minimum 1 ms). Used by WidgetManager::startTickTimer().
[[nodiscard]] int clockTickIntervalMs(const QDateTime& now, bool show_seconds);
```

The `warned_format` flag passed to `formatClockDate` is owned by `WidgetManager` as a private
`bool format_warned_{false}` member field, initialized false, and never reset within the manager's
lifetime (it is reset implicitly on config hot-reload, which destroys and recreates the manager).

### 5.2 `ConfigService` additions

```cpp
// In the anonymous namespace of ConfigService.cpp:

// Parses clock-widget-specific fields. Never returns nullopt (all fields are optional).
// Called only when type == "clock".
std::optional<ClockConfig> parseClockFields(const toml::table& entry);
```

```cpp
// In parseWidgetEntry (ConfigService.cpp), extended:

std::optional<WidgetDefinition> parseWidgetEntry(const toml::table& entry) {
  // ...type resolution...
  const bool enabled = entry["enabled"].value<bool>().value_or(true);   // NEW

  if (type == QLatin1String("time-to-event")) { /* existing path */ }

  if (type == QLatin1String("clock")) {                                   // NEW
    auto clk = parseClockFields(entry);
    // clk is always valid; no nullopt guard needed
    const auto position = parseWidgetPositionField(entry, QStringLiteral("clock"));
    if (!position) return std::nullopt;

    WidgetDefinition def;
    def.type = WidgetType::Clock;
    def.enabled = enabled;
    def.clock = *clk;
    def.position = *position;
    def.monitors = parseWidgetMonitors(entry, QStringLiteral("clock"));
    return def;
  }

  qCWarning(lcConfig) << "Config: [[widget]] has unknown or missing type" << type << "— skipping";
  return std::nullopt;
}
```

`enabled` is read unconditionally before the type switch, so it applies to both `time-to-event` and
`clock`. The `time-to-event` path must also set `def.enabled = enabled` (one-line addition).

### 5.3 `WidgetManager` additions/changes

New private fields:

```cpp
// Added to WidgetManager (after existing countdown fields):
QString clock_time_text_;
QString clock_seconds_text_;
QString clock_date_text_;
bool format_warned_{false};   // warn-once gate for invalid date_format
```

New/changed private methods:

```cpp
// Replaces currentCountdownText()/deadlineLabelText() for the clock type:
void recomputeClockStrings();       // populates clock_*_text_ via WidgetClock helpers

// Existing methods branched on definition_.type:
void recomputeAndPropagate();       // branches: countdown path (unchanged) vs clock path
void startTickTimer();              // branches: countdown stop-on-past vs clock always-resched
                                    //           + calls clockTickIntervalMs() for clock
```

`qmlSource()` branches:

```cpp
PerMonitorLayerManager::QmlSource WidgetManager::qmlSource(QScreen* screen) {
  const QUrl url{QStringLiteral("qrc:/HolonightShell/Widgets/WidgetSurface.qml")};
  if (definition_.type == WidgetType::Clock) {
    return {.url = url,
            .initial_properties = {
                {QStringLiteral("widgetType"),    QStringLiteral("clock")},
                {QStringLiteral("barMonitorName"), screen->name()},
                // Countdown fields use their default empty strings.
                {QStringLiteral("timeText"),      clock_time_text_},
                {QStringLiteral("secondsText"),   clock_seconds_text_},
                {QStringLiteral("dateText"),      clock_date_text_},
            }};
  }
  // Existing time-to-event path unchanged.
  return {.url = url,
          .initial_properties = {
              {QStringLiteral("widgetType"),       QStringLiteral("time-to-event")},
              {QStringLiteral("barMonitorName"),    screen->name()},
              {QStringLiteral("titleText"),         definition_.time_to_event.title},
              {QStringLiteral("remainingText"),     remaining_text_},
              {QStringLiteral("deadlineLabelText"), deadlineLabelText()},
          }};
}
```

`applyVisibility()` push-before-reveal for the clock type:

```cpp
if (visible) {
  if (definition_.type == WidgetType::Clock) {
    recomputeClockStrings();
    root->setProperty("timeText",    clock_time_text_);
    root->setProperty("secondsText", clock_seconds_text_);
    root->setProperty("dateText",    clock_date_text_);
  } else {
    root->setProperty("remainingText", remaining_text_);
  }
}
```

`updateTimerState()` for clock (no stop-on-past logic):

```cpp
void WidgetManager::updateTimerState() {
  if (!anySurfaceVisible()) {
    tick_timer_.stop();
    return;
  }
  recomputeAndPropagate();
  if (definition_.type == WidgetType::Clock) {
    if (!tick_timer_.isActive()) startTickTimer();
    // No "past deadline" stop — clock ticks forever.
    return;
  }
  // Existing countdown past-deadline stop logic follows.
  ...
}
```

`shouldCreateSurface()` collision-warning string: the logging line currently uses
`definition_.time_to_event.title`; for clock definitions a label string must be used instead.
Introduce a helper:

```cpp
[[nodiscard]] QString widgetLabel() const;
// Returns definition_.time_to_event.title for TimeToEvent,
// or "clock@" + widgetPositionToString(definition_.position) for Clock.
```

### 5.4 `WidgetSurface.qml` additions

```qml
// Added required properties with defaults so C++ need not set them for the wrong type:
required property string widgetType
required property string barMonitorName
required property string titleText        // time-to-event only; set via setInitialProperties
required property string remainingText    // time-to-event only
required property string deadlineLabelText// time-to-event only
property string timeText      : ""        // clock only; NOT required — default allows TTE to omit
property string secondsText   : ""        // clock only
property string dateText      : ""        // clock only
```

The countdown properties keep `required` (they are always set by C++ for TTE widgets). The three
clock properties are plain `property` with empty-string defaults; C++ always sets them explicitly for
clock widgets via `setInitialProperties`. This avoids a qmllint error: `required property` values
must always be supplied, and the TTE code path does not supply clock properties. See §8.1 for the
qmllint risk note.

New `Loader` branch:

```qml
Loader {
    anchors.fill: parent
    active: root.widgetType === "clock"
    sourceComponent: clockComponent
}

Component {
    id: clockComponent
    ClockWidget {
        timeText:    root.timeText
        secondsText: root.secondsText
        dateText:    root.dateText
    }
}
```

### 5.5 `ClockWidget.qml` structure

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import HolonightShell
import Holonight

Item {
    id: root

    required property string timeText
    required property string secondsText   // empty string when show_seconds = false
    required property string dateText

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8

        // --- Time row ---
        Item {
            id: timeContainer
            Layout.alignment: Qt.AlignHCenter
            implicitWidth:  timeRow.implicitWidth
            implicitHeight: timeRow.implicitHeight

            // Glow declared BEFORE text (z-order: renders behind per project gotcha)
            MultiEffect {
                source: timeRow
                anchors.fill: timeRow
                shadowEnabled:          true
                shadowColor:            HoloniightPalette.primary
                shadowBlur:             0.7
                shadowOpacity:          0.6
                shadowScale:            1.05
                shadowHorizontalOffset: 0
                shadowVerticalOffset:   0
                autoPaddingEnabled:     true
            }

            Row {
                id: timeRow
                anchors.centerIn: parent
                spacing: 4
                // Baseline alignment anchors the secondary element to the primary baseline.

                StableDigitsText {
                    id: bigTime
                    text:       root.timeText        // e.g. "14:23"
                    color:      HoloniightPalette.primary
                    fontFamily: ThemeService.clockFont
                    pixelSize:  Math.round(ThemeService.clockFontSize * 2.0)
                }

                StableDigitsText {
                    id: smallSeconds
                    visible:    root.secondsText.length > 0
                    text:       root.secondsText     // e.g. "07"; empty = hidden
                    color:      HoloniightPalette.primary
                    fontFamily: ThemeService.clockFont
                    pixelSize:  Math.round(ThemeService.clockFontSize * 1.0)
                    // Baseline-align to big digits: shift down by the difference in descent
                    anchors.baseline: bigTime.baseline  // requires Item baseline anchoring — see §8.2
                }
            }
        }

        // --- Date row ---
        Text {
            id: dateLabel
            Layout.alignment: Qt.AlignHCenter
            text:             root.dateText
            color:            HoloniightPalette.textSubtle
            font.family:      ThemeService.uiFont
            font.pixelSize:   ThemeService.uiFontSize
        }
    }
}
```

**Baseline alignment note**: `StableDigitsText` is an `Item`, not a `Text`; `Item` does not expose a
`baseline` anchor by default. Two options:

1. Use a `y` offset: `smallSeconds.y = bigTime.y + (bigTime.implicitHeight - smallSeconds.implicitHeight)` — this aligns the bottoms, which approximates baseline alignment for most clock fonts.
2. Expose a `readonly property real baseline` on `StableDigitsText` that delegates to `glyphRow`'s first child `Text.baselineOffset + y` and use `anchors.baseline`.

Option 1 is simpler and sufficient for most proportional clock fonts. Option 2 is more correct but
requires a `StableDigitsText` API change. The implementation phase should evaluate the actual visual
result with the `Rajdhani` font and choose accordingly.

---

## 6. Key Decisions with Rationale

### 6.1 WidgetManager generalization: branch-on-type vs separate subclass

**Decision**: branch on `definition_.type` inside the existing `WidgetManager`.

**Rationale**: The clock and countdown widgets share ~80% of infrastructure: `PerMonitorLayerManager`
lifecycle, `MonitorOccupancyService` gate, the `position_blockers_` / `content_visible_` mechanism,
`applyVisibility`, `anySurfaceVisible`, `viewForMonitor`, `configureSurface`, `layerConfig`. The
divergence is confined to: `qmlSource` initial properties, `recomputeAndPropagate`, `startTickTimer`
(no stop-on-past for clock), and `shouldCreateSurface` labeling. Factoring a separate
`ClockWidgetManager` subclass would duplicate the constructor, all the occupancy wiring, and the
timer-freeze logic without meaningful reduction in complexity. Branching is a known, tested pattern
in the codebase (e.g. the `has_time` branch in `WidgetCountdown`).

**Alternative considered**: a separate `ClockWidgetManager : PerMonitorLayerManager` with no shared
`WidgetManager` base. Deferred: the blast radius (new header, new cpp, new factory branch in
`ShellApplication`) is larger than a few if/else blocks for two widget types. Re-evaluate if a third
widget type diverges further.

### 6.2 Property bag approach for WidgetSurface

**Decision**: keep separate named properties on `WidgetSurface.qml` root (countdown props as
`required`, clock props as optional with empty defaults), rather than a single `QVariantMap
widgetData` bag.

**Rationale**: named properties are statically checked by qmllint and provide clear type safety.
A `QVariantMap` bag bypasses qmllint's type checking and makes the binding surface opaque. The small
number of properties (five countdown + three clock) does not justify the bag's indirection.

### 6.3 `date_format` default: stored in `ClockConfig` or in formatter?

**Decision**: `ClockConfig.date_format` defaults to empty string; `formatClockDate()` applies the
literal `"dddd, d MMMM yyyy"` fallback internally.

**Rationale**: the default pattern is a formatting concern, not a configuration concern. Storing it in
`ClockConfig` would require `ConfigService` to know about Qt date format strings. Centralizing it in
`WidgetClock.cpp` keeps `ConfigService` free of presentation logic (REQ-C-002) and provides a single
place to read or change the default.

### 6.4 Parse-time vs format-time validation of `date_format` and `locale`

**Decision**: tolerant at parse time (REQ-F-011, REQ-F-012); detect and log at format time.

**Rationale**: `QLocale` construction from an arbitrary string does not throw; an unrecognized tag
silently produces `QLocale::C`. Detecting the fallback case requires comparing the constructed
`QLocale` against `QLocale::system()` or checking `QLocale::language() == QLocale::C` — both of
which are format-time operations. Similarly, `date_format` invalidity is only observable when the
formatted output is empty or unexpected. Attempting parse-time validation would require constructing
`QLocale` and calling `QDateTime::toString()` in `ConfigService`, blurring the separation of concerns.
`warned_format` / `warned_locale` flags (owned by `WidgetManager`) prevent log flooding.

### 6.5 Baseline-jitter handling for the seconds element

**Decision**: use a `Row` (not `RowLayout`) with `y`-offset alignment of `smallSeconds` to approximate
bottom-of-glyph alignment, plus `StableDigitsText`'s existing fixed-width glyph cells for horizontal
stability.

**Rationale**: `StableDigitsText` already eliminates horizontal jitter for the `HH:mm` portion by
measuring the widest digit and reserving that width per cell. The same component is used for seconds.
The vertical shift ensures the second element does not jump the container height. An actual
`anchors.baseline` approach requires a `StableDigitsText` API extension; that can be added if visual
testing shows the y-offset is insufficient for the `Rajdhani` font.

### 6.6 Where the `enabled` check lives

**Decision**: `ShellApplication::rebuildWidgets()` skips `WidgetDefinition` entries with
`enabled == false` before constructing a `WidgetManager`.

**Rationale**: `WidgetManager` does not need to know about `enabled`; it is always constructed for
definitions that should produce surfaces. The check in `rebuildWidgets` is three lines and keeps
`WidgetManager`'s invariant that it always manages live surfaces. If `enabled` were checked inside
`WidgetManager`, every occupancy and collision code path would need to guard against "not enabled"
as a special state. Centralizing the check at the coordinator level is cleaner.

---

## 7. Alternatives Considered

### 7.1 `std::variant` refactor for widget config

`WidgetDefinition` could use `std::variant<TimeToEventConfig, ClockConfig>` instead of parallel
embedded structs plus a discriminator enum. This would eliminate the `type` field and the need to
check `definition_.type` at runtime.

**Deferred**: the codebase currently has exactly two widget types; introducing `std::variant` now
would require touching every call site that accesses `definition_.time_to_event` via `std::get` or
`std::visit`, updating the `operator==`, and writing a new `parseWidgetEntry` variant. The complexity
exceeds the benefit for two types. Revisit when a third widget type diverges significantly from both
existing types.

### 7.2 Separate `ClockWidgetManager` subclass

A `ClockWidgetManager : PerMonitorLayerManager` would own only the clock-specific tick logic and
QML properties. The `WidgetManager` would be renamed `CountdownWidgetManager`.

**Rejected**: the shared occupancy-gate, position-blocker, `content_visible_`, and timer-freeze
infrastructure would need to be duplicated or factored into yet another shared base. For two types
the branch-on-type approach is more maintainable. If a third type requires a completely different
surface lifecycle (e.g. interactive, not occupancy-gated), a proper hierarchy becomes worthwhile.

### 7.3 QML-side formatting

Format `HH:mm` and date strings inside `ClockWidget.qml` using `Qt.formatDateTime` and a JavaScript
`Date` object.

**Rejected** (REQ-F-005): QML-side date formatting cannot be unit-tested without a running QML
engine, is harder to control for locale, and would duplicate the `QLocale` fallback logic. The
existing `time-to-event` widget already establishes the precedent that all string formatting belongs
in C++.

### 7.4 Reusing `remainingText` for the time string

The clock time string could be pushed through the existing `remainingText` property to avoid adding
new properties to `WidgetSurface.qml`.

**Rejected**: `remainingText` is semantically a countdown string ("12d 04h") and is used by
`TimeToEventWidget` as such. Reusing it for a wall-clock HH:mm string would create a misleading
coupling and would prevent independent styling in QML (the clock time and seconds need to be rendered
at different sizes, which requires separate properties).

---

## 8. Known Risks

### 8.1 qmllint required-property handling on `WidgetSurface` root

`WidgetSurface.qml` currently declares `titleText`, `remainingText`, `deadlineLabelText` as
`required property`. These are supplied by C++ via `setInitialProperties` for TTE widgets but not for
clock widgets. The three new clock properties (`timeText`, `secondsText`, `dateText`) are declared
as plain `property` with defaults, which means C++ must supply them for clock widgets but qmllint will
not flag an omission for TTE widgets. The reverse risk is that qmllint may flag the inactive Loader's
`ClockWidget` component as missing required properties from the root — but since the Loader is
inactive, the component is never instantiated, so qmllint should not diagnose it. Verify with
`task qml-lint` after implementation.

### 8.2 Baseline jitter with `StableDigitsText` for mixed font sizes

`StableDigitsText` fixes horizontal width per glyph but does not expose a baseline anchor. When the
seconds element uses half the pixel size of the main digits, naive `Row` layout will top-align it,
making it appear visually high. The `y`-offset workaround may not perfectly match the typographic
baseline for all sizes of `Rajdhani`. Test visually and adjust; if the result is unacceptable, add a
`readonly property real baselineOffset` to `StableDigitsText` and use `anchors.baseline`.

### 8.3 Clock vs countdown property coupling in `WidgetManager`

`WidgetManager` now carries both countdown fields (`remaining_text_`, deadline logic) and clock fields
(`clock_time_text_`, `clock_seconds_text_`, `clock_date_text_`, `format_warned_`). For a TTE widget,
the clock fields are unused dead weight; for a clock widget, the countdown fields are dead weight.
This is acceptable for two types but becomes a maintenance concern as more types are added. Track
this technical debt; consider a variant or polymorphic approach at the third widget type.

### 8.4 Shared-timer second-alignment drift

`startTickTimer()` uses `clockTickIntervalMs()` which derives `ms_to_next_boundary` from
`QDateTime::currentDateTime()` at the moment of the call. A single-shot timer is used (same as the
countdown widget), so each tick re-derives alignment from the actual current time — there is no
accumulating drift. However, if the event loop is blocked for more than ~100 ms (e.g., heavy disk I/O
during config reload), the timer may fire late, causing a visible sub-second lag. This is inherent to
a Qt event-loop timer and is shared with the countdown widget; it is not a clock-specific regression.
The spec allows ±100 ms per boundary (REQ-F-013/014).

### 8.5 Hot-reload destroy-and-rebuild during visible widget

When `widgetsConfigChanged` fires while a clock widget is visible on screen, `rebuildWidgets` tears
down all layer-shell surfaces and recreates them. This causes a brief visual flash on the desktop
(surface unmaps and remaps). This is the existing hot-reload behaviour shared with the countdown
widget; it is not a clock-specific issue, but users who save their config file frequently will notice
it. A future optimisation could diff definitions and only rebuild changed managers.

---

## 9. Requirements Traceability

| REQ-ID | Satisfied by |
|---|---|
| REQ-F-001 | `ClockWidget.qml` time row; `WidgetClock::formatClockTime`, `formatClockSeconds`; `StableDigitsText` at `clockFontSize * 2` (big) and `clockFontSize * 1` (small). |
| REQ-F-002 | `ClockWidget.qml` date row `Text`; `WidgetClock::formatClockDate` with default pattern `"dddd, d MMMM yyyy"`. |
| REQ-F-003 | `ClockWidget.qml` `MultiEffect` declared before `timeRow` (z-order gotcha); `shadowEnabled: true`; `shadowColor: HoloniightPalette.primary`. |
| REQ-F-004 | `ColumnLayout { anchors.centerIn: parent }` in `ClockWidget.qml`; both rows carry `Layout.alignment: Qt.AlignHCenter`. |
| REQ-NF-001 | `ClockWidget.qml` uses only `HoloniightPalette.*` tokens; no hex literals. |
| REQ-NF-002 | Time row: `ThemeService.clockFont` / `clockFontSize * 2`; date row: `ThemeService.uiFont` / `uiFontSize`. No hardcoded `font.family`. |
| REQ-F-005 | All string construction in `WidgetClock::format*`; QML binds pre-formatted strings. No `Date` / `toLocaleString()` in QML. |
| REQ-F-006 | `WidgetClock::formatClockTime` uses `QDateTime::toString("HH:mm")` (24h). No 12-hour config key exists. |
| REQ-F-007 | `WidgetClock::formatClockDate` passes `date_format` to `QDateTime::toString(dateFormatString, locale)` when non-empty. |
| REQ-F-008 | `WidgetClock::formatClockDate` constructs `QLocale(locale_string)` when non-empty; falls back to `QLocale::system()`. |
| REQ-F-009 | `ClockConfig.show_seconds{true}`; `WidgetClock::formatClockSeconds` returns `""` when `false`; `smallSeconds.visible: secondsText.length > 0` in QML. |
| REQ-F-010 | `ShellApplication::rebuildWidgets` skips definitions with `enabled == false`; no `WidgetManager` created, no surfaces. |
| REQ-F-011 | `WidgetClock::formatClockDate` detects empty formatted result; falls back to default; `qCWarning` once via `warned_format` flag in `WidgetManager`. |
| REQ-F-012 | `WidgetClock::formatClockDate` constructs `QLocale`; checks for `QLocale::C` / invalid result; falls back to `QLocale::system()`; `qCWarning` once. |
| REQ-F-013 | `WidgetClock::clockTickIntervalMs(now, true)` returns 1000; `WidgetManager::startTickTimer()` calls it for clock type. |
| REQ-F-014 | `WidgetClock::clockTickIntervalMs(now, false)` computes `ms_to_next_minute`; single-shot timer re-derives alignment on each tick (no drift). |
| REQ-F-015 | `WidgetManager::updateTimerState()`: `tick_timer_.stop()` when `!anySurfaceVisible()`; inherited from existing countdown path. |
| REQ-F-016 | `WidgetManager::applyVisibility()`: `recomputeClockStrings()` called before `root->setVisible(true)`; `startTickTimer()` re-aligns. |
| REQ-F-017 | `WidgetManager` connects `MonitorOccupancyService::occupancyChanged` in constructor; `applyVisibility` applies the gate. Inherited, unchanged. |
| REQ-F-018 | `WidgetDefinition.position` and `WidgetDefinition.monitors` parsed by `parseWidgetPositionField` / `parseWidgetMonitors`; used in `shouldCreateSurface` / `anchorFlagsForPosition`. Inherited. |
| REQ-F-019 | `WidgetManager::applyVisibility` calls `root->setVisible(…)`; `QQuickView::hide()` is never called. Inherited, unchanged. |
| REQ-C-001 | `WidgetClock` uses only `QDateTime::currentDateTime()`; no new sockets. |
| REQ-C-002 | `parseClockFields` in `ConfigService` reads only raw TOML values; no monitor validation or coordinate logic. |
| REQ-C-003 | Same `applyVisibility` pattern as countdown; `QQuickView` stays mapped. Inherited. |
| REQ-F-020 | `WidgetManager::blockedOn()` and `shouldCreateSurface()` unchanged; work for any `WidgetType`. `widgetLabel()` provides the log label for clock. |
| REQ-F-021 | `PerMonitorLayerManager` base handles surface-init errors; `WidgetManager` logs with `widgetLabel()` and monitor name. Inherited. |
| REQ-NF-003 | `tick_timer_.stop()` in `updateTimerState` when hidden. Inherited path shared with countdown. |
| REQ-NF-004 | One `WidgetManager` per definition; no per-monitor timer or manager duplication. Inherited. |
| REQ-F-022 | `parseClockFields` returns non-null for any valid or missing-key config; `parseWidgetEntry` assembles full `WidgetDefinition`. Tested by unit test. |
| REQ-F-023 | Per-monitor `content_visible_` hash; per-monitor `applyVisibility`; collision check per monitor. Shared timer (see correction below). |
| REQ-F-024 | `ShellApplication::rebuildWidgets()` on `widgetsConfigChanged`; full destroy-and-rebuild of all managers. |

**Correction to REQ-F-023 wording**: the SPEC states "each monitor's timer and displayed time are
independent (time is not synchronized)". This is incorrect as described. The actual design uses ONE
shared `QTimer` per `WidgetDefinition` driving ALL monitors for that definition. All monitors showing
the same clock widget display the same wall-clock time (they all receive identical `timeText`,
`secondsText`, `dateText` strings from a single `recomputeAndPropagate()` call). Per-monitor
*visibility* is independent (occupancy on monitor A does not affect monitor B), and collision
detection is per-monitor, but the formatted strings are shared. This is the same model as the
countdown widget (one `tick_timer_` per `WidgetManager`, pushing `remaining_text_` to all surfaces).
The SPEC wording will need a follow-up correction.

**Correction to REQ-F-024 wording**: the SPEC states "the WidgetManager shall re-parse the widget
definition, update timers and display strings, and recreate surfaces if needed". In reality,
`ShellApplication::rebuildWidgets()` unconditionally destroys the entire `widget_managers_` vector
and rebuilds it from scratch on every `widgetsConfigChanged`. There is no in-place re-parse inside a
live `WidgetManager`. The SPEC description of the mechanism is inaccurate; the observable outcomes
(changed settings take effect on next update, `enabled=false` destroys surfaces) are correct.
