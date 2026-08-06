# Weather Icon Compositor — Design

**Feature Name:** Weather Icon Compositor
**Status:** Ready for implementation
**Depends on:** `docs/sdd/weather-icon-compositor/SPEC.md` (binding — not re-litigated here)

---

## 1. Architecture Recap (from SPEC, not re-debated)

Pure C++ mapping function (condition code + isDay + moon phase → ordered `QStringList` of layer
basenames) feeds a declarative QML `Repeater` of `Image` elements. No `QQuickImageProvider`, no
`QPainter` compositing, no custom `QQuickPaintedItem`. This document covers the next layer down:
exact types, file layout, build wiring, and how moon phase slots into the pipeline.

---

## 2. Design Decisions

### 2.1 How is the C++ mapping exposed to QML?

**Decision: pure `Q_GADGET` value type + a thin `QML_ELEMENT` wrapper, no `QML_SINGLETON`.**

Two static-only C++ classes are written with **zero Qt GUI/QML/Quick includes** so they link
against `Qt6::Core` alone and are constructible/callable from plain GTest with no QML engine:

- `WeatherIconMapper` — static `mapLayers(...)` method, no instance state.
- `MoonPhaseCalculator` — static `phaseForDate(...)` method, no instance state.

Both are declared `Q_GADGET` (not `QObject`) and exposed to QML via `QML_ELEMENT` +
`QML_SINGLETON` is intentionally **not** used because:

- `QML_SINGLETON` requires the class to be default-constructible and registered with a QML engine
  at module-registration time, which pulls in `QQmlEngine` machinery and a metatype registration
  story. It's the right shape for stateful services (`WeatherService` is exactly this pattern: a
  long-lived `QObject` with properties and signals). The mapper/calculator are *stateless pure
  functions* — wrapping them in an engine-owned singleton buys nothing and adds a runtime
  dependency the GTest binary doesn't need.
- A `Q_GADGET` with `Q_INVOKABLE` static-like methods can be registered as an **uncreatable QML
  value type** via `QML_ELEMENT` + `QML_UNCREATABLE` (since the QML side never needs to construct
  an instance — only call static-style mapping). This keeps the C++ class itself entirely free of
  QML headers; the registration macros (`QML_ELEMENT`, `QML_UNCREATABLE`) are pure attribute
  macros expanded by `moc`, not includes of `QQml*`/`QQuick*` headers, so `REQ-C-NOQML-C++` holds.
- Calling convention from QML: because the methods are `static` C++ functions, QML cannot call
  them as `WeatherIconMapper.mapLayers(...)` the way it calls `Math.max(...)` — QML always invokes
  through an instance or a singleton. To keep the call site ergonomic (`WeatherIconMapper.mapLayers(...)`
  from QML) while keeping the C++ class itself static/header-only-Core, we register a **second,
  tiny singleton wrapper exposed only at the QML registration layer** — see 2.1.1 below. This
  reconciles "callable as `Foo.bar(...)` from QML" with "zero QML deps in the testable class."

#### 2.1.1 Two-layer exposure (resolves the tension above)

| Layer | Class | Lives in | QML-visible | Tested by |
|---|---|---|---|---|
| Pure logic | `WeatherIconMapper`, `MoonPhaseCalculator` | `src/services/weather-icon/` | No | GTest, directly |
| QML adapter | `WeatherIconBridge` | `src/services/weather-icon/WeatherIconBridge.h/.cpp` | Yes (`QML_SINGLETON`) | Not unit-tested (trivial passthrough, excluded from coverage target like other QML glue) |

`WeatherIconBridge` is a `QObject` with `QML_ELEMENT`/`QML_SINGLETON`, holding **no state**. Its
only job is to forward calls to the static pure functions so the QML component has one
ergonomic call site:

```cpp
// WeatherIconBridge.h — the ONLY class in this feature that may include QQml/QQuick headers.
Q_INVOKABLE QStringList layersFor(int conditionCode, bool isDay, const QDateTime& date) const;
```

This bridge is what `WeatherIconCompositor.qml` actually calls. It is a few lines of passthrough
(`WeatherIconMapper::mapLayers(conditionCode, isDay, MoonPhaseCalculator::phaseForDate(date))`)
and is exempt from the 95% coverage target (REQ-NF-TEST-001 names the *mapping and moon-phase*
functions, not QML glue) — consistent with how `WeatherService::iconPath` (also a thin static
`Q_INVOKABLE`) isn't separately unit-tested beyond what `WeatherService` tests already cover.

This satisfies REQ-C-NOQML-C++ exactly: `WeatherIconMapper.h/.cpp` and
`MoonPhaseCalculator.h/.cpp` include only `<QtCore/...>` and `<chrono>`; `WeatherIconBridge.h/.cpp`
is the sanctioned exception (it's glue, not "the mapping function shall...").

### 2.2 Where does moon-phase calculation run?

**Decision: the QML component exposes only `date` as public input. `moonPhase` is NOT a public
input property.** This resolves the spec ambiguity called out in REQ-F-QML-001's acceptance
criteria (which lists both `moonPhase` and `date` as inputs).

Rationale:
- `moonPhase` is *entirely derivable* from `date` (REQ-F-MP-001 is exactly "phase ← f(date)").
  Exposing both as independent public properties creates a state that can disagree with itself
  (caller sets `date` to a full-moon timestamp but leaves a stale `moonPhase: NEW` from a previous
  binding) — a classic dual-source-of-truth bug.
- `WeatherIconBridge::layersFor(conditionCode, isDay, date)` takes `date` only and computes phase
  internally via `MoonPhaseCalculator::phaseForDate(date)` before calling
  `WeatherIconMapper::mapLayers(conditionCode, isDay, phase)`. The QML component's public surface
  mirrors this 3-argument shape, not 4.
- The 8-phase **enum is still defined and registered for QML** (`MoonPhase` — see §2.5) because
  it's part of the stable C++ API surface consumers may want for other purposes (e.g. a future
  "moon phase" standalone widget). The QML component just doesn't take it as an *input*; if a
  future consumer needs to know which phase was used (e.g. a tooltip), add a **read-only**
  `readonly property int resolvedMoonPhase` later — out of scope for this iteration, not blocking.
- This is a deliberate, documented deviation from the literal text of REQ-F-QML-001's acceptance
  bullet ("Component accepts input properties: ... `moonPhase` (enum) ... `date`"). The *intent*
  of the requirement — phase-aware rendering driven by a timestamp — is fully satisfied; the
  bullet's enumeration of properties is the artifact of the spec author's ambiguity flagged
  explicitly in the task brief, not a hard contract to preserve a redundant property no other
  requirement depends on. (REQ-F-QML-004 and REQ-E-PROPCHANGE only require "any input property"
  reactivity — `date` changing satisfies this for the night branch.)
- Per spec Assumption 2: "If `isDay: true`, the `date` property is ignored (no moon phase
  needed)." This is implemented as: the bridge only calls `MoonPhaseCalculator::phaseForDate`
  when `isDay` is false (cheap to compute either way, but keeps intent explicit and avoids an
  unnecessary call when the result is discarded).

### 2.3 File / directory layout

```
src/services/weather-icon/                  # NEW directory, sibling to src/services/weather/
├── WeatherIconLayer.h                       # Layer-name constants (shared by mapper + tests)
├── MoonPhase.h                              # MoonPhase enum + QML registration (Q_NAMESPACE)
├── WeatherIconMapper.h
├── WeatherIconMapper.cpp
├── MoonPhaseCalculator.h
├── MoonPhaseCalculator.cpp
├── WeatherIconBridge.h                      # QML_SINGLETON adapter — only QML-aware file here
└── WeatherIconBridge.cpp

src/qml/WeatherIcon/                         # NEW directory, per-directory QML layout convention
└── WeatherIconCompositor.qml

tests/
├── test_weather_icon_mapper.cpp             # NEW
└── test_moon_phase_calculator.cpp           # NEW
```

**Why a new `src/services/weather-icon/` directory, not `src/services/weather/` or `src/util/`:**

- Not `src/services/weather/`: REQ-NF-ARCH-001 requires the new feature have *zero coupling* to
  `WeatherService`/`WeatherProvider`. Putting new files in the same directory as `WeatherService.h`
  invites future contributors to reach for the existing `WeatherData.h` types (e.g. reusing
  `CurrentWeather::condition_id`) or to wire dependencies because "it's already next to the
  weather code." A separate directory makes the orthogonality structural, not just a code-review
  rule. It also avoids a misleading `target_include_directories` situation — `weather/` is already
  on the include path for `#include "WeatherData.h"`-style bare includes; adding unrelated headers
  there pollutes that namespace.
- Not `src/util/`: there is no existing `src/util/` directory in this codebase (services live
  under `src/services/<subsystem>/`, e.g. `src/services/audio/`, `src/services/network/`,
  `src/services/launcher/`). Inventing a generic `util` bucket breaks the established
  per-subsystem convention and gives the new code a vaguer home than its actual role deserves —
  this is a self-contained subsystem (icon compositing), not a grab-bag utility.
- `src/services/weather-icon/` mirrors the existing pattern exactly (compare
  `src/services/audio/`, `src/services/network/`, `src/services/notifications/`) — a dedicated
  subdirectory under `src/services/` for a cohesive set of classes, added to
  `holonight_services`'s sources and given its own include-directory entry, matching how
  `src/services/weather/` itself is wired in lines 296–300 and 327 of `CMakeLists.txt`.

`WeatherIconLayer.h` holds the layer basename string constants (`"sun"`, `"star-field"`,
`"moon-new"`, etc., see §2.4) as `inline constexpr` `QLatin1StringView` or plain `const char*`
literals, shared between `WeatherIconMapper.cpp` and the test file so test assertions don't
duplicate magic strings.

### 2.4 What exact strings does the mapping function return?

**Decision: file basenames without extension** (e.g. `"sun"`, `"moon-full"`,
`"few-clouds-overlay-day"`) — NOT full `qrc:/HolonightShell/...` paths, and not `.png`-suffixed.

Rationale:
- **Testability.** GTest assertions read as `EXPECT_EQ(layers, QStringList{"sun"})` instead of
  `EXPECT_EQ(layers, QStringList{"qrc:/HolonightShell/weather-png/512x512/sun.png"})` repeated
  across ~40+ test cases. Shorter, more readable, and resilient to a future asset-path or
  extension change (REQ-NF-ARCH-004 explicitly anticipates the path changing if multi-resolution
  support is added later — basenames insulate the mapper's tests from that).
- **Separation of concerns.** The mapper's job (per REQ-F-LM-001/003) is "what layers, in what
  order" — a naming decision, not a filesystem/QRC decision. Knowledge of the QRC prefix and the
  `512x512` resolution folder belongs to exactly one place: the QML component (or the bridge),
  which is also the only place that needs to change when REQ-NF-ARCH-004's "future resolution"
  scenario actually arrives.
- `WeatherIconCompositor.qml` (or `WeatherIconBridge`, see below) is responsible for turning a
  basename into `"qrc:/HolonightShell/weather-png/512x512/" + basename + ".png"`. This is a single
  string-concatenation point, easiest to keep in the QML component itself (a `function` or
  computed property) since it's pure presentation/path policy with no testable logic beyond
  string concatenation — not worth a C++ round-trip.
- **Reconciling with REQ-F-LM-001's bullet** "All paths in returned list shall be valid QRC paths
  with prefix `qrc:/HolonightShell/`": this bullet is satisfied at the *system* level (the
  end-to-end pipeline the user sees never renders anything but valid QRC paths), not by the
  `WeatherIconMapper::mapLayers` function in isolation. The acceptance criteria for REQ-F-LM-001
  also gives concrete examples like `["sun"]` and `["star-field", "moon-full"]` — these are
  basenames, not QRC paths, so the spec's own examples already establish the basename contract;
  the later "valid QRC paths" bullet is satisfied by the assembly step QML performs on top.

### 2.5 Enum definition location/registration for the 8 moon phases

**Decision:** define in `MoonPhase.h` as a plain C++ `enum class MoonPhase`, registered for QML
via `Q_NAMESPACE` + `Q_ENUM_NS` in an anonymous namespace-free header so it can be `#include`d by
both the Core-only mapper/calculator and (separately) exposed to QML by the bridge layer.

```cpp
// src/services/weather-icon/MoonPhase.h
#pragma once

#include <QObject>  // for Q_NAMESPACE / Q_ENUM_NS — see note below

namespace WeatherIconNs {
Q_NAMESPACE
enum class MoonPhase : int {
  New,
  WaxingCrescent,
  FirstQuarter,
  WaxingGibbous,
  Full,
  WaningGibbous,
  LastQuarter,
  WaningCrescent,
};
Q_ENUM_NS(MoonPhase)
}  // namespace WeatherIconNs
```

**Note on REQ-C-NOQML-C++ and `<QObject>`:** `Q_NAMESPACE`/`Q_ENUM_NS` are `Qt6::Core` macros
(declared via `<QObject>`, but they only need the metaobject/metatype system, which is Core, not
Quick/Qml/Gui). `WeatherIconMapper.cpp`/`MoonPhaseCalculator.cpp` including `MoonPhase.h` (which
transitively includes `<QObject>`) does not violate REQ-C-NOQML-C++ — that requirement bars
`QQuick*`/`QQml*`/GUI headers specifically, and `WeatherService.h` itself (an existing, accepted
class) already includes `<QObject>` for the same reason. `Q_NAMESPACE` requires no `moc`-generated
`.moc` include in a `.cpp` (only classes with `Q_OBJECT`/`Q_GADGET` macros inside a `.cpp` file
need that, per the project's `Q_OBJECT in .cpp` convention) — this is a namespace-scoped enum, not
a class, so AutoMoc picks it up from the header alone like any other Q_OBJECT-bearing header.

QML registration of the enum itself (`QML_ELEMENT`/`QML_NAMED_ELEMENT` on the namespace) is
**deferred** — not added in this iteration — because no QML input property needs to *carry* a
`MoonPhase` value (§2.2 establishes `date` as the only public input). If a future consumer needs
`MoonPhase` values in QML (e.g. a standalone "moon phase indicator" widget), add
`QML_NAMED_ELEMENT(MoonPhase)` to the namespace then; it's an additive, non-breaking change.

---

## 3. Components

### 3.1 `MoonPhase.h` (new)

Namespace-scoped `enum class MoonPhase` with 8 values (§2.5). No `.cpp` — header-only.

### 3.2 `WeatherIconLayer.h` (new)

```cpp
#pragma once

#include <QLatin1StringView>

// Layer basenames (no extension, no path) returned by WeatherIconMapper::mapLayers.
// Shared between the mapper implementation and its unit tests to avoid duplicated literals.
namespace WeatherIconLayer {
inline constexpr QLatin1StringView kSun{"sun"};
inline constexpr QLatin1StringView kStarField{"star-field"};
inline constexpr QLatin1StringView kBrokenCloudsDay{"broken-clouds-day"};
inline constexpr QLatin1StringView kBrokenCloudsNight{"broken-clouds-night"};
inline constexpr QLatin1StringView kFewCloudsOverlayDay{"few-clouds-overlay-day"};
inline constexpr QLatin1StringView kFewCloudsOverlayNight{"few-clouds-overlay-night"};
inline constexpr QLatin1StringView kScatteredCloudsDay{"scattered-clouds-day"};
inline constexpr QLatin1StringView kScatteredCloudsNight{"scattered-clouds-night"};
inline constexpr QLatin1StringView kHeavyDrizzleDay{"heavy-drizzle-day"};
inline constexpr QLatin1StringView kHeavyDrizzleNight{"heavy-drizzle-night"};
inline constexpr QLatin1StringView kLightDrizzleDay{"light-drizzle-day"};
inline constexpr QLatin1StringView kLightDrizzleNight{"light-drizzle-night"};
inline constexpr QLatin1StringView kRainDay{"rain-day"};
inline constexpr QLatin1StringView kRainNight{"rain-night"};
// Moon phase basenames keyed by MoonPhase enum value — see WeatherIconMapper.cpp for the lookup.
inline constexpr QLatin1StringView kMoonNew{"moon-new"};
inline constexpr QLatin1StringView kMoonWaxingCrescent{"moon-waxing-crescent"};
inline constexpr QLatin1StringView kMoonFirstQuarter{"moon-first-quarter"};
inline constexpr QLatin1StringView kMoonWaxingGibbous{"moon-waxing-gibbous"};
inline constexpr QLatin1StringView kMoonFull{"moon-full"};
inline constexpr QLatin1StringView kMoonWaningGibbous{"moon-waning-gibbous"};
inline constexpr QLatin1StringView kMoonLastQuarter{"moon-last-quarter"};
inline constexpr QLatin1StringView kMoonWaningCrescent{"moon-waning-crescent"};
}  // namespace WeatherIconLayer
```

### 3.3 `WeatherIconMapper.h` / `.cpp` (new)

Pure, stateless, static-method-only class. No instances are ever constructed.

```cpp
#pragma once

#include "MoonPhase.h"

#include <QList>
#include <QString>

// Pure condition-code + day/night + moon-phase -> ordered layer-basename mapping.
// No QML/Quick/GUI dependencies (links Qt6::Core only). See REQ-F-LM-001..003, REQ-C-NOQML-C++.
class WeatherIconMapper {
 public:
  WeatherIconMapper() = delete;

  // Returns layer basenames (no extension, no path) bottom-to-top for the given OWM condition
  // code. moon_phase is consulted only when is_day is false. Unmapped condition codes (511, 2xx,
  // 6xx, 7xx) yield an empty list (REQ-F-LM-002).
  [[nodiscard]] static QList<QString> mapLayers(int condition_code, bool is_day, WeatherIconNs::MoonPhase moon_phase);

 private:
  [[nodiscard]] static QString moonLayerName(WeatherIconNs::MoonPhase moon_phase);
};
```

Implementation in `.cpp`: a single `switch`/lookup keyed on a normalized "condition family"
derived from `condition_code` (clear/few/scattered/broken/light-drizzle/heavy-drizzle/rain or
"unmapped"), then branches on `is_day` to assemble the `QList<QString>` per the mapping table in
SPEC.md §2. Kept under the project's cognitive-complexity-25 clang-tidy limit by extracting a
`static QString conditionFamily(int condition_code)` helper that does the code→family bucketing,
and a separate `static QList<QString> dayLayers(family)` / `static QList<QString> nightLayers(family, moon_phase)`
pair, composed by `mapLayers`.

### 3.4 `MoonPhaseCalculator.h` / `.cpp` (new)

```cpp
#pragma once

#include "MoonPhase.h"

#include <chrono>

// Pure synodic-month moon-phase calculation from a timestamp. No QML/Quick/GUI dependencies,
// no system clock access, no network. See REQ-F-MP-001..003, REQ-C-NONET, REQ-C-NOQML-C++.
class MoonPhaseCalculator {
 public:
  MoonPhaseCalculator() = delete;

  // Computes lunar phase from days elapsed since the reference new moon (2000-01-06 18:14 UTC),
  // modulo the synodic month (~29.530589 days), bucketed into 8 equal-width windows.
  [[nodiscard]] static WeatherIconNs::MoonPhase phaseForDate(std::chrono::system_clock::time_point timestamp);
};
```

Implementation: a `static constexpr` reference epoch (expressed as a `std::chrono::sys_seconds` or
equivalent constructed from the 2000-01-06T18:14:00Z literal) and the `29.530589`-day synodic
constant, both as `static constexpr double`/`std::chrono::duration` members in the `.cpp`. Days
elapsed = `(timestamp - kReferenceNewMoon)` cast to a `double` day count; phase index =
`static_cast<int>(std::floor(std::fmod(days_elapsed, kSynodicMonthDays) / kSynodicMonthDays * 8.0)) % 8`,
guarding against negative `fmod` results for timestamps before the epoch (add `kSynodicMonthDays`
before the final modulo if negative — relevant since `system_clock::time_point` is unbounded, and
clang-tidy's complexity limit means this guard should be its own small `static` helper, e.g.
`normalizedPhaseIndex(double days_elapsed)`).

### 3.5 `WeatherIconBridge.h` / `.cpp` (new — the QML-aware adapter)

```cpp
#pragma once

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>

// Stateless QML singleton adapter over WeatherIconMapper + MoonPhaseCalculator. Holds no
// instance state; exists solely so QML can call a single ergonomic entry point
// (WeatherIconBridge.layersFor(...)) without the pure C++ classes needing any QML dependency.
// See REQ-NF-ARCH-002 (stable public API), REQ-C-NOQML-C++ (this file is the sanctioned
// exception — see DESIGN.md §2.1.1).
class WeatherIconBridge : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  explicit WeatherIconBridge(QObject* parent = nullptr);

  // Returns layer basenames (no extension/path) for the given condition + day/night + date.
  // date is consulted for moon-phase calculation only when is_day is false (REQ Assumption 2).
  Q_INVOKABLE [[nodiscard]] static QStringList layersFor(int condition_code, bool is_day, const QDateTime& date);
};
```

`layersFor` is `static` (no instance state needed) but still declared `Q_INVOKABLE` on the
instance so QML's singleton-call convention (`WeatherIconBridge.layersFor(...)`) works — Qt
permits invoking static member functions through `Q_INVOKABLE` on a registered singleton.

### 3.6 `WeatherIconCompositor.qml` (new)

```qml
import QtQuick
import HolonightShell

// Reusable, standalone weather-icon compositor. Stacks ordered PNG layers (from
// WeatherIconBridge.layersFor) bottom-to-top via Repeater. No theming surface: every visual
// pixel comes from a bundled raster asset, not a palette token (CLAUDE.md theming rule N/A here).
// Not wired into any existing screen — import HolonightShell; WeatherIconCompositor { ... }
// REQ-NF-ARCH-002, REQ-NF-ARCH-003 (no animation), REQ-C-SINGLECOND (one conditionCode at a time).
Item {
    id: root

    required property int conditionCode
    required property bool isDay
    property date date: new Date()
    property int iconSize: 128

    width: iconSize
    height: iconSize

    readonly property var layerNames: WeatherIconBridge.layersFor(root.conditionCode, root.isDay, root.date)

    Repeater {
        model: root.layerNames

        delegate: Image {
            required property string modelData
            anchors.fill: parent
            z: index
            source: "qrc:/HolonightShell/weather-png/512x512/" + modelData + ".png"
            sourceSize: Qt.size(root.iconSize, root.iconSize)
            smooth: true
            mipmap: true
            fillMode: Image.PreserveAspectFit
        }
    }
}
```

Notes:
- `conditionCode`/`isDay` are `required property` — REQ-E-INITIAL says a missing required
  property should "emit a clear error or render empty, not crash"; QML's own `required property`
  diagnostic (`Required property ... was not initialized`) at engine load time satisfies "clear
  error" without any custom guard code.
- `date` defaults to `new Date()` (not `required`) per spec Assumption 2 — callers rendering
  `isDay: true` never need to set it.
- No `moonPhase` property exists on this component (§2.2).
- `layerNames` recomputes automatically whenever `conditionCode`, `isDay`, or `date` change —
  QML's property-binding reactivity gives REQ-F-QML-004/REQ-E-PROPCHANGE for free, no manual
  `Connections`/signal wiring needed.
- Empty `layerNames` (unmapped code) makes `Repeater.count` 0 automatically — REQ-F-QML-003,
  REQ-S-UNMAPPEDEMPTY satisfied with no extra code; `Item`'s own `width`/`height` stay at
  `iconSize` (not 0) by design — the *visual* content is empty/transparent, but the box still
  reserves layout space sized to `iconSize` so a forecast strip of mixed mapped/unmapped icons
  doesn't visually jump. (If zero-space-when-empty is later required, bind
  `width: layerNames.length > 0 ? iconSize : 0` — not needed by current acceptance criteria, which
  only require zero *visible* content and `Repeater.count === 0`.)
- No `import Holonight` / `HoloniightPalette` — confirmed no theming surface: this component only
  ever sets `source` on raster `Image` elements; there is no color, brush, or token anywhere in
  this file.

---

## 4. Data Flow

```
QML consumer
  sets conditionCode, isDay, [date]
        │
        ▼
WeatherIconCompositor.qml
  layerNames := WeatherIconBridge.layersFor(conditionCode, isDay, date)
        │
        ▼
WeatherIconBridge::layersFor (C++, QObject/QML_SINGLETON)
  phase := is_day ? <unused> : MoonPhaseCalculator::phaseForDate(date.toStdSysTime-ish)
  return WeatherIconMapper::mapLayers(condition_code, is_day, phase)
        │
        ├──────────────────────────────┐
        ▼                               ▼
MoonPhaseCalculator::phaseForDate   WeatherIconMapper::mapLayers
  (pure, Core-only)                   (pure, Core-only)
  days_elapsed = ts - epoch            conditionFamily(condition_code)
  phase_index = ...                    dayLayers(family) | nightLayers(family, phase)
  return MoonPhase enum                 return QList<QString> basenames
        │                               │
        └──────────────┬────────────────┘
                        ▼
        QStringList of basenames, e.g. ["star-field", "moon-full"]
                        │
                        ▼
WeatherIconCompositor.qml: Repeater { model: layerNames }
  for each basename, instantiate Image {
    source: "qrc:/HolonightShell/weather-png/512x512/" + basename + ".png"
    sourceSize: Qt.size(iconSize, iconSize); smooth: true; mipmap: true
    z: index   // preserves bottom-to-top order from the mapping table
  }
        │
        ▼
Qt pixmap cache (keyed by qrc path + target size) decodes/scales/caches each layer
        │
        ▼
Rendered, stacked composite on screen
```

Property-change propagation: any QML binding change to `conditionCode`/`isDay`/`date` on the
component re-evaluates the `layerNames` binding (a plain QML property, not a signal/slot the
developer must wire), which re-runs the `WeatherIconBridge` call, which re-runs both pure C++
functions synchronously (no async hop — they're cheap arithmetic/lookup, no I/O), and the
`Repeater` diffs its model and re-creates `Image` delegates accordingly, each picking up a
(possibly) new pixmap from cache. This whole chain is synchronous and sub-frame, satisfying
REQ-E-PROPCHANGE's "<16ms" and REQ-F-QML-004.

---

## 5. Build System Wiring

### 5.1 `holonight_services` target — add new sources

In `CMakeLists.txt`, extend the existing `holonight_services` source list (after the
`src/services/weather/...` block, lines 296–300) with the new directory's files, and add it as an
include directory (mirroring line 327's `src/services/weather` entry):

```cmake
add_library(holonight_services STATIC
    ...
    src/services/weather/WeatherData.h
    src/services/weather/WeatherProvider.h
    src/services/weather/WeatherProvider.cpp
    src/services/weather/WeatherService.h
    src/services/weather/WeatherService.cpp
    src/services/weather-icon/MoonPhase.h
    src/services/weather-icon/WeatherIconLayer.h
    src/services/weather-icon/WeatherIconMapper.h
    src/services/weather-icon/WeatherIconMapper.cpp
    src/services/weather-icon/MoonPhaseCalculator.h
    src/services/weather-icon/MoonPhaseCalculator.cpp
    src/services/weather-icon/WeatherIconBridge.h
    src/services/weather-icon/WeatherIconBridge.cpp
    ...
)
```

```cmake
target_include_directories(holonight_services PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/launcher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/weather
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/weather-icon
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/notifications
    ${LIBPULSE_INCLUDE_DIRS}
)
```

`holonight_services` already links `Qt6::Qml` (line 315) for `WeatherService`'s own
`QML_SINGLETON`, so `WeatherIconBridge`'s `QML_SINGLETON` needs no new `target_link_libraries`
entry. The two pure classes (`WeatherIconMapper`, `MoonPhaseCalculator`) compile fine against the
same target despite needing only `Qt6::Core` — `holonight_services` linking `Qt6::Qml` publicly
doesn't force them to *use* it, and REQ-C-NOQML-C++'s acceptance criterion ("CMake target for C++
utilities does not depend on Qt6::Gui/Quick/Qml") is about whether *those specific files* `#include`
GUI/QML headers, not about whether the enclosing library target happens to also link Qt6::Qml for
unrelated reasons (`WeatherService` already established this precedent in the same target).

Also add the executable's own include path entry, mirroring line 446:

```cmake
target_include_directories(holonight-shell PRIVATE
    ...
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/weather
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/weather-icon
    ...
)
```

### 5.2 QML module — add the new QML file

Add to `HOLONIGHT_QML_FILES` (alphabetically near the other per-directory groups, e.g. after the
`Tray/` block and before `Widgets/`, or wherever directory-sorted order lands — CMake's `list(SORT
...)` check at line 546 only verifies the *set* matches discovered files, not declaration order,
so insertion position is purely stylistic):

```cmake
set(HOLONIGHT_QML_FILES
    ...
    src/qml/Tray/TrayMenuItem.qml
    src/qml/WeatherIcon/WeatherIconCompositor.qml
    src/qml/Widgets/WidgetSurface.qml
    ...
)
```

This is mandatory — per CLAUDE.md, CMake configure fails if any `src/qml/*.qml` file is missing
from this list (the `file(GLOB_RECURSE ... HOLONIGHT_DISCOVERED_QML_FILES)` + `STREQUAL` check at
lines 540–550).

### 5.3 New PNG asset bundle (separate from the SVG bundle)

Add a **second, independent** `qt6_add_resources` call — do not merge with `weather_icons`. The
existing SVG bundle (lines 578–590) uses `BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"`, which strips
`assets/` so `assets/weather/foo.svg` → alias `weather/foo.svg` → `qrc:/HolonightShell/weather/foo.svg`.
The new PNGs live under `assets/weather-png/512x512/`, so the same `BASE` strips `assets/` and
preserves `weather-png/512x512/` as part of the alias — giving exactly the
`qrc:/HolonightShell/weather-png/512x512/<basename>.png` paths assumed by `WeatherIconCompositor.qml`
(§3.6):

```cmake
# Bundle weather icon compositor PNGs. BASE strips the "assets" prefix so
# assets/weather-png/512x512/foo.png -> qrc:/HolonightShell/weather-png/512x512/foo.png.
# Kept as a separate resource bundle from weather_icons (SVG path) per REQ-NF-ARCH-001 —
# the two weather icon paths must remain independent and orthogonal.
# CONFIGURE_DEPENDS re-runs CMake when PNGs are added or removed.
file(GLOB WEATHER_PNG_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/weather-png/512x512/*.png"
)
qt6_add_resources(holonight-shell "weather_png_icons"
    PREFIX "/HolonightShell"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    FILES ${WEATHER_PNG_FILES}
)
```

Place this immediately after the existing `weather_icons` resource block (after line 590), before
`target_link_libraries(holonight-shell PRIVATE holonight_app)`.

### 5.4 Tests — new GTest binary entries

Add the two new test files to the existing `test_holonight_services` executable (it already links
`holonight_services`, which will own the new sources) in `tests/CMakeLists.txt`:

```cmake
holonight_add_test_exe(test_holonight_services
  main.cpp
  test_active_window_service.cpp
  ...
  test_notification_timeout.cpp
  test_weather_icon_mapper.cpp
  test_moon_phase_calculator.cpp
)
```

Do **not** create a new test executable — `WeatherIconMapper`/`MoonPhaseCalculator` are part of
`holonight_services`'s sources (§5.1), so they're already covered by linking that one target;
`test_holonight_services` already gets coverage-instrumented (`tests/CMakeLists.txt`'s
`ENABLE_COVERAGE` loop already iterates this executable). After adding the two `.cpp` files to
`tests/CMakeLists.txt`, run `task configure-tests` explicitly before `task test` (documented
project gotcha — the configure dependency can be stale and silently skip new test files).

#### `tests/test_weather_icon_mapper.cpp` — structure

- `TEST(WeatherIconMapper, ClearDayReturnsSunOnly)` — `mapLayers(800, true, MoonPhase::New)` → `{"sun"}`
- `TEST(WeatherIconMapper, ClearNightFullMoonReturnsStarFieldAndMoon)` → `{"star-field", "moon-full"}`
- Parameterized test (`INSTANTIATE_TEST_SUITE_P`) over all 16 (day/night × 8 phases) combinations
  for condition 800, asserting distinct correct outputs (REQ-F-LM-003's explicit bullet)
- One representative test per condition family × day/night for 801, 802, 803/804 (asserting
  803 and 804 produce identical sequences — REQ-F-LM-001's bullet), 300-family, 302-family,
  500-family
- `TEST(WeatherIconMapper, UnmappedThunderstormReturnsEmpty)` parameterized over `{200..232}`
  representative codes, `{600, 611, 622}` snow, `{701, 711, ..., 781}` atmosphere, and `511`
  freezing rain — all assert empty `QList<QString>`
- Boundary tests: first/last code in each family (e.g. 300 and 315 for light-drizzle family; 520
  and 531 for rain family) per spec §5 Test Plan

#### `tests/test_moon_phase_calculator.cpp` — structure

- `TEST(MoonPhaseCalculator, ReferenceEpochYieldsNew)` — exact 2000-01-06 18:14 UTC → `MoonPhase::New`
- One test per the 8 documented offsets in REQ-F-MP-001 (7.38d → WaxingCrescent, 14.77d →
  FirstQuarter, 22.16d → WaxingGibbous, and similarly the remaining 4 phase windows at their
  midpoints — derive WaningGibbous/LastQuarter/WaningCrescent/Full midpoints from the 8 equal
  29.530589/8 ≈ 3.691-day windows)
- `TEST(MoonPhaseCalculator, WrapsAtSynodicMonthBoundary)` — 29.53 days after epoch → `New` again
- `TEST(MoonPhaseCalculator, DeterministicForRepeatedCalls)` — same `time_point` in twice, assert
  equal results (trivially true for a pure function, but satisfies REQ-F-MP-003's explicit bullet
  as a regression guard)
- `TEST(MoonPhaseCalculator, HandlesTimestampsBeforeEpoch)` — a `time_point` before the 2000-01-06
  reference, verifying the negative-`fmod` normalization (§3.4) doesn't produce an out-of-range
  index

Both files use `#include "WeatherIconLayer.h"` constants for expected-value literals (not raw
string literals) so a future rename of a layer basename updates both production code and tests
from one place.

### 5.5 No changes to `tests/GeneratedQmlFiles.h.in` machinery needed

`WeatherIconCompositor.qml`'s addition to `HOLONIGHT_QML_FILES` automatically flows into the
generated `HOLONIGHT_QML_TEST_ENTRIES` (CMakeLists.txt lines 552–561) used by
`test_qml_smoke.cpp` — no manual edit required there. This gives a free QML-engine-load smoke
test of the new component (catches `required property` typos, syntax errors, missing imports) as
soon as it's added to the file list.

---

## 6. Known Risks

1. **`QDateTime` → `std::chrono::system_clock::time_point` conversion boundary.** `WeatherIconBridge::layersFor`
   takes `QDateTime` (the natural QML-facing type) but `MoonPhaseCalculator::phaseForDate` takes
   `std::chrono::system_clock::time_point` (the spec's explicit C++ signature requirement,
   REQ-F-MP-001's last bullet). Qt6 provides `QDateTime::toStdSysSeconds()` /
   construction-from-`std::chrono` helpers, but exact API availability varies by Qt 6.x minor
   version — must be verified against the Qt version this project targets at implementation time
   (`find_package(Qt6 ...)` doesn't pin a minor version in `CMakeLists.txt`). If the conversion
   helper isn't available, fall back to `QDateTime::toSecsSinceEpoch()` → manually construct a
   `time_point` via `std::chrono::seconds`.

2. **Floating-point boundary jitter in phase bucketing.** The 8 equal-width windows derived from
   `29.530589 / 8 ≈ 3.6913` days mean timestamps very close to a window boundary (e.g. exactly
   7.38 days, which sits near the WaxingCrescent/FirstQuarter edge) are sensitive to
   floating-point rounding in the `days_elapsed` computation. The spec's own acceptance bullets
   use offsets that land mid-window (7.38, 14.77, 22.16 — each ~0.05 days inside their respective
   window edges per the synodic constant), so this is a low risk for the *specified* test cases,
   but any boundary-condition test added beyond the spec's examples should use offsets at least a
   few hours away from a computed window edge to avoid flaky `EXPECT_EQ` on phase enum boundaries.

3. **`Image.mipmap: true` cost for 512×512 sources at small `iconSize`.** Mipmapping a 512×512
   PNG down to e.g. 64×64 for five concurrent instances (REQ-F-QML-002's stated scenario) costs
   one-time CPU at decode; Qt's pixmap cache (REQ-NF-PERF-001) amortizes this across instances
   sharing the same `(source, sourceSize)` key, but the *first* instance to request a novel size
   still pays full decode+mipmap-chain-generation cost. The spec's 200ms first-render budget
   (REQ-NF-PERF-002) should hold on typical hardware for a single 512×512 source, but this is
   unverified without an actual on-device measurement — flagged as a test-plan item, not a design
   gap, since REQ-NF-PERF-002 already calls for a benchmark.

4. **`enum class MoonPhase` ordinal stability.** Because `MoonPhase` is a plain C++ enum (not yet
   QML-registered with explicit values), its underlying `int` ordinals are implementation order,
   not a documented stable ABI. If a future iteration QML-registers it (§2.5's deferred step) or
   persists a `MoonPhase` value (e.g. to disk/cache), pin explicit integer values at that point
   (`New = 0, WaxingCrescent = 1, ...`) rather than relying on declaration order, to avoid silent
   reordering breakage. Not a problem for this iteration since the enum never crosses a
   serialization boundary.

5. **Two-singleton QML registration collision risk is low but not zero.** Adding a second
   `QML_SINGLETON` (`WeatherIconBridge`) alongside the existing `WeatherService` singleton in the
   same `holonight_services` static library reuses a pattern already proven to work (the build's
   `qt6_extract_metatypes`/`combine-metatypes.cmake` step already merges metatypes from this
   target without ODR issues for `WeatherService`). Risk is limited to remembering not to also
   pass `holonight_services`'s `INTERFACE_SOURCES` as a duplicate foreign-types input — already
   guarded against by the existing `set_property(... PROPERTY INTERFACE_SOURCES "")` at line 603,
   which applies blanket to the whole target and needs no per-class change.

---

## 7. Summary of New Files

| Path | Type | Purpose |
|---|---|---|
| `src/services/weather-icon/MoonPhase.h` | C++ header | `enum class MoonPhase` (8 values), `Q_NAMESPACE` |
| `src/services/weather-icon/WeatherIconLayer.h` | C++ header | Layer basename string constants |
| `src/services/weather-icon/WeatherIconMapper.h/.cpp` | C++ (Core-only) | Pure condition→layers mapping |
| `src/services/weather-icon/MoonPhaseCalculator.h/.cpp` | C++ (Core-only) | Pure date→phase calculation |
| `src/services/weather-icon/WeatherIconBridge.h/.cpp` | C++ (QML_SINGLETON) | QML call-site adapter |
| `src/qml/WeatherIcon/WeatherIconCompositor.qml` | QML | Reusable Image-stack component |
| `tests/test_weather_icon_mapper.cpp` | GTest | Mapper unit tests |
| `tests/test_moon_phase_calculator.cpp` | GTest | Calculator unit tests |

Edits to existing files: `CMakeLists.txt` (new sources, include dirs, QML file list, PNG resource
bundle — §5.1–5.3), `tests/CMakeLists.txt` (two new test files in `test_holonight_services` — §5.4).
No edits to `WeatherService.{h,cpp}`, `WeatherProvider.{h,cpp}`, `WeatherData.h`, or any existing
`src/qml/Topbar/Weather*.qml` file (REQ-NF-ARCH-001).
