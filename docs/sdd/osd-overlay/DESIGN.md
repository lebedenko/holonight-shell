# DESIGN — osd-overlay

Status: proposed. Grounded against the codebase as of commit `a7114b2` (2026-07-26). Cites
`SPEC.md` requirement IDs throughout; a full coverage table is in §13.

---

## 1. Overview

The single architectural idea: **a generic transient-state overlay, driven purely by observation
of existing service signals, normalized into a small event model, split into a testable controller
(services layer) and a dumb presentation surface (surfaces layer), bridged by the app layer.**

Three independent facts already true of this codebase make that idea concrete instead of abstract:

- `AudioService`, `BrightnessService`, `KeyboardLayoutService` already emit change signals for
  exactly the three quantities the OSD cares about (`volumeChanged`/`mutedChanged`,
  `brightnessPercentChanged`, `layoutCodeChanged`). Nothing new needs to be polled or wired into
  Hyprland IPC.
- `NotificationToastSurface` (`libs/holonight-surfaces/src/NotificationToastSurface.h/.cpp`) is a
  complete, working template for "create a layer-shell surface on demand, size it to QML content,
  destroy it cleanly" — the exact lifecycle REQ-F-012/C-004 ask for.
- `ShellApplication` (`apps/shell/app/ShellApplication.cpp`) already plays the role of "app-layer
  glue that reads a focused monitor from `ActiveWindowService` and pokes a `*Surface` class"
  (see `launcher_surface_->toggle(aws_->focusedMonitorName())`). The OSD's suppression wiring
  (REQ-F-006/C-007) and monitor routing (REQ-C-012) are both instances of that same pattern, not a
  new one.

The controller (`OsdController`, `libs/holonight-services/src/osd/`) never touches Wayland, never
touches a QQuickView, and is constructed from `OsdChannelSource*` abstractions only (REQ-C-001,
REQ-F-002, REQ-F-025) — it is exercised entirely by GTest with fake sources and a fake clock. The
surface (`OsdSurface`, `libs/holonight-surfaces/src/`) never touches `AudioService` or
`OsdController` — it exposes `showLevel()`/`showSelection()`/`hide()` and pushes plain values onto
a QML root, exactly like `NotificationToastSurface` pushes `monitorName`. `ShellApplication` is the
only class that knows about both: it connects `OsdController`'s three signals to `OsdSurface`'s
three entry points, and connects `StatusPopupSurface`/`SidebarManager`'s visibility signals back
into `OsdController::setSuppressed()`. This keeps `holonight_services` free of any
`holonight_surfaces` include (enforced by `scripts/check-architecture-boundaries.sh`, REQ-F-006/C-007)
without inventing a new intermediate "manager" class — see §10 and Key Decision 4.

---

## 2. Component Inventory

### New files

| File | Library | Responsibility |
|---|---|---|
| `libs/holonight-services/src/osd/OsdEvent.h` | holonight_services | Free-standing value types: `OsdLevelEvent`, `OsdSelectionEvent`, `using OsdEvent = std::variant<...>`. `Q_GADGET`+`Q_PROPERTY` for QML field access, `Q_DECLARE_METATYPE` for signal/QVariant marshalling (REQ-F-001). |
| `libs/holonight-services/src/osd/OsdChannelSource.h` | holonight_services | Abstract base: `channel()`, `isAvailable()` pure virtuals; `eventObserved(const OsdEvent&)`/`availableChanged(bool)` signals (REQ-F-002). |
| `libs/holonight-services/src/osd/OsdController.h/.cpp` | holonight_services | The state machine: prime-and-diff, grace period, suppression, enable/disable, hide timer (REQ-F-003/004/005/007/008, REQ-NF-002/008/009/010). |
| `libs/holonight-services/src/osd/AudioChannelSource.h/.cpp` | holonight_services | Adapts `AudioService` → `OsdLevelEvent` (channel `"audio-volume"`) (REQ-F-009). |
| `libs/holonight-services/src/osd/BrightnessChannelSource.h/.cpp` | holonight_services | Adapts `BrightnessService` → `OsdLevelEvent` (channel `"screen-brightness"`) (REQ-F-010). |
| `libs/holonight-services/src/osd/KeyboardLayoutChannelSource.h/.cpp` | holonight_services | Adapts `KeyboardLayoutService` → `OsdSelectionEvent` (channel `"keyboard-layout"`) (REQ-F-011). |
| `libs/holonight-surfaces/src/OsdSurface.h/.cpp` | holonight_surfaces | Layer-shell surface, modeled on `NotificationToastSurface` (REQ-C-002/C-004). |
| `apps/shell/qml/Osd/OsdView.qml` | holonight-shell (QML) | View root: entrance/exit choreography, Loader dispatch, input-region-safe (no handlers) (REQ-C-003). |
| `apps/shell/qml/Osd/OsdLevelRenderer.qml` | holonight-shell (QML) | Icon + label + progress bar + percentage/"Muted" text (REQ-F-014/020/023). |
| `apps/shell/qml/Osd/OsdSelectionRenderer.qml` | holonight-shell (QML) | Large short label + small full label, no bar (REQ-F-015). |
| `tests/test_osd_controller.cpp` | test_holonight_services | GTest coverage: prime-and-diff, grace period, suppression, enable/disable, thread-safety-by-construction, end-to-end flow (REQ-C-015). |
| `tests/qml/tst_OsdLevelRenderer.qml`, `tests/qml/tst_OsdSelectionRenderer.qml` | test_holonight_qml_harness | QtQuickTest coverage for icon/label mapping and non-replay of `Behavior`-driven updates. |

### Existing files modified

| File | Change | Why |
|---|---|---|
| `libs/holonight-surfaces/src/WidgetSurfacePolicy.h` | Add `[[nodiscard]] std::uint32_t anchorFlagsForPosition(WidgetPosition position);` to the public declarations. | REQ-C-006: the nine-way anchor switch must exist exactly once; `OsdSurface` needs it without calling `widgetSurfacePlacement()` (which hardcodes widget width/height). |
| `libs/holonight-surfaces/src/WidgetSurfacePolicy.cpp` | Move `anchorFlagsForPosition()` out of the anonymous namespace; `widgetSurfacePlacement()` calls the now-public function. | Same. Existing widget placement tests must pass unchanged (REQ-C-006 acceptance). |
| `libs/holonight-surfaces/CMakeLists.txt` | Add `src/OsdSurface.h` / `src/OsdSurface.cpp` to the explicit `add_library(holonight_surfaces STATIC ...)` file list. | This target lists sources explicitly (not globbed) — confirmed by reading the file; a new class must be added by hand. |
| `libs/holonight-services/CMakeLists.txt` | Add `${CMAKE_CURRENT_SOURCE_DIR}/src/osd` to `target_include_directories(holonight_services PUBLIC ...)`. | This target globs `.cpp`/`.h` sources recursively already, but include dirs are listed per-subdirectory explicitly (mirrors `src/audio`, `src/brightness`, etc.) — `#include "OsdController.h"` from `ShellApplication.cpp` needs the new subdir on the include path. |
| `libs/holonight-config/include/holonight_config/config_structs.h` | Add `OsdChannelConfig` and `OsdConfig` structs. | REQ-C-009. |
| `libs/holonight-config/include/holonight_config/config_parsers.h` | Add `OsdConfig osd;` to `ParsedConfig`. | Same. |
| `libs/holonight-config/src/ConfigParsers.cpp` | Add `parseOsd()`, called from `parseConfigTable()`. | REQ-C-009/C-011. |
| `libs/holonight-config/src/ConfigWriter.cpp` | Add `writeOsd()`, called from `ConfigWriter::write()`. | REQ-C-009 round-trip preservation. |
| `libs/holonight-core/src/ConfigService.h/.cpp` | Add `osd_` member, `osd()` getter, `osdConfigChanged()` signal; populate/diff in `applyParsedConfig()`. | Symmetric with every other config section (`widgetsConfigChanged` etc.); enables live reload (§8). |
| `libs/holonight-core/src/KeyboardLayoutService.h/.cpp` | Add `layout_name_` member, `layoutName()` accessor, `Q_PROPERTY(QString layoutName ...)`, `layoutNameChanged()` signal; `setLayoutName()` now retains the value (guarded, same style as `setLayoutCode`) instead of only deriving the code. | REQ-C-014, verbatim. |
| `libs/holonight-surfaces/src/SidebarManager.h/.cpp` | Add `void currentTabChanged(const QString& monitor_name, int tab_index);` signal, emitted from the existing `onCurrentTabChanged()`; add `[[nodiscard]] Q_INVOKABLE int currentTabForMonitor(const QString& monitor_name) const;` accessor over the existing `current_tabs_` map. | Needed so `ShellApplication` can compute brightness suppression (REQ-F-006) — today `onCurrentTabChanged` only *stores* the tab index with no way for C++ to observe or read it back. |
| `apps/shell/qml/Controls/BarIcon.qml` | Add `root.name === "brightness"` to the `UtilityIcon.qml`-dispatch condition in `_iconSource`. | REQ-F-021 routes through `BarIcon`'s existing dispatcher, same as `"keyboard"` already does. |
| `apps/shell/qml/Controls/UtilityIcon.qml` | Add a `if (root.name === "brightness")` branch to `drawUtility()` drawing a sun/brightness glyph with the existing gradient stroke helper. | REQ-F-021. |
| `apps/shell/app/ShellApplication.h` | New members `OsdController* osd_controller_`, `OsdSurface* osd_surface_` (declared immediately after `brightness_service_` — see §9 for why declaration order matters), plus two private slots for suppression recomputation and one for config application. | §9. |
| `apps/shell/app/ShellApplication.cpp` | Construct the three channel sources + `OsdController` + `OsdSurface`; register both as QML singletons; wire controller↔surface signals; wire suppression; apply/re-apply config. | §9. |
| `scripts/check-architecture-boundaries.sh` | **No change.** | See Key Decision 4 — the chosen wiring keeps `holonight-surfaces` unaware of `OsdController`, so the existing allowlist (`MonitorOccupancyService.h`, `NotificationService.h`) does not need `OsdController.h` added. |
| `apps/shell/CMakeLists.txt` | **No change required for QML registration.** | `HOLONIGHT_QML_FILES` is populated by `file(GLOB_RECURSE ... qml/*.qml CONFIGURE_DEPENDS)`; placing the three new files under `apps/shell/qml/Osd/` is sufficient for them to be bundled and reachable via `import HolonightShell`. (REQ-C-003's acceptance criterion describes a manual `qt6_add_resources()`/QRC-alias step that this repository's actual build does not use for shell QML — noted here as a correction to the literal spec text; the *requirement* — files present under `apps/shell/qml/Osd/`, reachable from `import HolonightShell` — is still fully satisfied.) |
| `tests/CMakeLists.txt` | Add `test_osd_controller.cpp` to `test_holonight_services`'s source list. | REQ-C-015. Remember: run `task configure-tests` explicitly afterward (documented stale-configure-dep gotcha). |

---

## 3. The Normalized Event Model

```cpp
// libs/holonight-services/src/osd/OsdEvent.h
#pragma once

#include <QMetaType>
#include <QString>
#include <variant>

// REQ-F-001. Free-standing structs (NOT nested inside OsdController) — see Key Decision 1 for why.
struct OsdLevelEvent {
  Q_GADGET
  Q_PROPERTY(QString channel MEMBER channel)
  Q_PROPERTY(int value MEMBER value)
  Q_PROPERTY(bool muted MEMBER muted)

 public:
  QString channel;
  int value{0};
  bool muted{false};

  bool operator==(const OsdLevelEvent&) const = default;
};

struct OsdSelectionEvent {
  Q_GADGET
  Q_PROPERTY(QString channel MEMBER channel)
  Q_PROPERTY(QString shortLabel MEMBER short_label)
  Q_PROPERTY(QString fullLabel MEMBER full_label)

 public:
  QString channel;
  QString short_label;
  QString full_label;

  bool operator==(const OsdSelectionEvent&) const = default;
};

using OsdEvent = std::variant<OsdLevelEvent, OsdSelectionEvent>;

Q_DECLARE_METATYPE(OsdLevelEvent)
Q_DECLARE_METATYPE(OsdSelectionEvent)
Q_DECLARE_METATYPE(OsdEvent)
```

This is a direct copy of the established `Q_GADGET` + `Q_PROPERTY(camelCase MEMBER lower_case)` +
defaulted `operator==` + `Q_DECLARE_METATYPE` pattern already used for `HourlyEntry`/`DailyEntry`/
`CurrentWeather` in `libs/holonight-services/src/weather/WeatherData.h` — this is not a new idiom
for this codebase.

**Naming note (flagged, not silently resolved):** REQ-F-001's acceptance text says the structs
"compile with members ... `shortLabel`, `fullLabel`". This repository's `.clang-tidy` enforces
`MemberCase: lower_case` for *all* struct members (public or private), which `WeatherData.h`
already follows by using lower_case backing members with camelCase `Q_PROPERTY` aliases. Following
the spec text literally (a C++ field spelled `shortLabel`) would fail `task tidy` (REQ-NF-005). I
resolve this in favor of the binding lint rule and the existing precedent: backing members are
`short_label`/`full_label`, the QML/GTest-visible vocabulary name `shortLabel`/`fullLabel` is the
`Q_PROPERTY` name. GTest code reads `event.short_label`, not `event.shortLabel`. `channel`/`value`/
`muted` need no such alias since they're already lower_case.

**Choice: `std::variant`, not a class hierarchy, not two entirely separate signals only.**
`OsdChannelSource::eventObserved` (REQ-F-002's literal signature) must carry one type regardless of
which of the two kinds a given source produces — a `KeyboardLayoutChannelSource` only ever emits
selection events, an `AudioChannelSource` only ever emits level events, but the base-class signal
is shared. A `std::variant<OsdLevelEvent, OsdSelectionEvent>` is copyable (required for
cross-signal marshalling), needs one `Q_DECLARE_METATYPE` (on the `using`-alias, which is a single
token — a raw `std::variant<A, B>` would break the macro on the comma), and is visited exhaustively
at the one place that needs to branch on kind (`OsdController::onEventObserved`, and the
`displayLevelEvent`/`displaySelectionEvent` emission point). A class hierarchy (`OsdEvent` base,
`OsdLevelEvent`/`OsdSelectionEvent` derived) would require heap allocation and `QVariant::fromValue`
of a pointer/`shared_ptr` to cross a signal cleanly, is not copyable in the trivial-value sense
`Q_DECLARE_METATYPE` wants, and adds RTTI/dynamic-dispatch machinery this two-kind, closed set does
not need. Rejected.

**How events reach QML.** `OsdController::displayLevelEvent(const OsdLevelEvent&)` and
`displaySelectionEvent(const OsdSelectionEvent&)` are ordinary Qt signals carrying `Q_GADGET`
values (REQ-F-008). When such a signal fires and QML has a `Connections { function
onDisplayLevelEvent(event) { ... } }` handler, the `Q_GADGET`'s `Q_PROPERTY`s are readable from JS
as `event.value`, `event.muted`, `event.channel` — no wrapper `QObject`, no separate `Q_PROPERTY`
holder needed on `OsdController` itself. In the actual production wiring (§9), QML never connects
to `OsdController` directly — `ShellApplication` does, and re-expresses the fields as plain
`QString`/`int`/`bool` properties pushed onto `OsdView.qml`'s root via `QObject::setProperty()`
(exactly as `NotificationToastSurface` pushes `monitorName` and `SidebarManager` pushes
`currentTab`). The `Q_GADGET` signal path is still what `task test`'s "Integration test wires both
signals to QML `Connections`" acceptance criterion (REQ-F-008) exercises directly against
`OsdController`, independent of how `ShellApplication` chooses to consume it.

`OsdController::hideRequested()` (no payload) is the third signal: it tells whoever is listening
"the display timeout elapsed with no new event; stop showing this."

`qRegisterMetaType<OsdEvent>()` / `<OsdLevelEvent>()` / `<OsdSelectionEvent>()` are called once in
`OsdController`'s constructor (its natural owner), which — combined with `Q_DECLARE_METATYPE` —
makes all three usable across `Qt::QueuedConnection` if a future channel source lives on a worker
thread (REQ-NF-008; see §4).

---

## 4. `OsdChannelSource` and the Three Adapters

```cpp
// libs/holonight-services/src/osd/OsdChannelSource.h
#pragma once

#include "OsdEvent.h"

#include <QObject>
#include <QString>

// REQ-F-002. The controller depends on this interface only — grep over OsdController.cpp finds
// no reference to AudioService/BrightnessService/KeyboardLayoutService.
class OsdChannelSource : public QObject {
  Q_OBJECT

 public:
  explicit OsdChannelSource(QObject* parent = nullptr) : QObject(parent) {}
  ~OsdChannelSource() override = default;

  OsdChannelSource(const OsdChannelSource&) = delete;
  OsdChannelSource& operator=(const OsdChannelSource&) = delete;
  OsdChannelSource(OsdChannelSource&&) = delete;
  OsdChannelSource& operator=(OsdChannelSource&&) = delete;

  // Stable identifier: "audio-volume" | "screen-brightness" | "keyboard-layout".
  [[nodiscard]] virtual QString channel() const = 0;

  // Synchronous snapshot, read once by OsdController right after connecting availableChanged().
  // See the CONSTANT-property race note below for why this exists.
  [[nodiscard]] virtual bool isAvailable() const = 0;

 Q_SIGNALS:
  void eventObserved(const OsdEvent& event);
  void availableChanged(bool available);
};
```

**Extension beyond the literal REQ-F-002 text:** the pure virtual `isAvailable()` is not mentioned
in the spec, which only asks for the two signals plus `channel()`. It exists to solve a real,
concrete problem the design brief asked me to address directly:

> `BrightnessService` reports availability via `hasBacklight` — a `Q_PROPERTY(... CONSTANT FINAL)`
> with **no notify signal at all** — while `AudioService::availableChanged()` is a real, firing
> signal.

If `BrightnessChannelSource` tried to satisfy "emit `availableChanged` once" by emitting it from its
own constructor, that emission would race the controller's `connect()` call — `OsdController`
receives all three already-constructed sources via its constructor (REQ-F-025) and connects to them
*after* they exist, so a signal fired during the source's own construction is silently dropped, and
`BrightnessChannelSource` would never have another chance to fire it (there's no
`hasBacklightChanged` to hang a later emission off). The fix used here is the standard "connect,
then synchronously pull the current value" pattern:

```cpp
// OsdController's constructor, once per source:
connect(source.get(), &OsdChannelSource::availableChanged, this, [this, ch = source->channel()](bool available) {
  onSourceAvailableChanged(ch, available);
});
connect(source.get(), &OsdChannelSource::eventObserved, this, &OsdController::onEventObserved);
onSourceAvailableChanged(source->channel(), source->isAvailable());  // seeds state deterministically
```

This treats `AudioChannelSource` (real, possibly-repeating `availableChanged`) and
`BrightnessChannelSource` (one-shot, `CONSTANT`-backed, never changes again) identically from the
controller's point of view: both get exactly one synchronous seed call at construction, and
`AudioChannelSource` additionally gets live updates later through the normal signal path.
`BrightnessChannelSource` never connects to anything for availability — there is nothing to connect
to — it simply implements `isAvailable() { return service_->hasBacklight(); }` and relies on the
controller's post-connect seed call.

### `AudioChannelSource`

```cpp
class AudioChannelSource : public OsdChannelSource {
  Q_OBJECT
 public:
  explicit AudioChannelSource(AudioService* service, QObject* parent = nullptr);
  [[nodiscard]] QString channel() const override { return QStringLiteral("audio-volume"); }
  [[nodiscard]] bool isAvailable() const override { return service_->available(); }

 private:
  void emitCurrentState();  // builds OsdLevelEvent{channel(), service_->volume(), service_->muted()}
  AudioService* service_;   // non-owning; ShellApplication owns AudioService
};
```

Connects `AudioService::volumeChanged` and `AudioService::mutedChanged` to the same
`emitCurrentState()` slot (REQ-F-009); connects `AudioService::availableChanged` directly (it is a
real signal) to re-run the availability-seed logic.

### `BrightnessChannelSource`

Connects only `BrightnessService::brightnessPercentChanged(int)` → builds
`OsdLevelEvent{"screen-brightness", percent, /*muted=*/false}` (REQ-F-010; brightness has no mute
concept, `muted` is always `false`). No `availableChanged` connection exists, per the note above.

### `KeyboardLayoutChannelSource`

Connects `KeyboardLayoutService::layoutCodeChanged` **and** the new `layoutNameChanged` (REQ-C-014)
to the same handler, which builds:

```cpp
OsdSelectionEvent{
    .channel = QStringLiteral("keyboard-layout"),
    .short_label = service_->layoutCode(),
    .full_label = service_->layoutName().isEmpty() ? service_->layoutCode() : service_->layoutName(),
};
```

satisfying REQ-F-011's fallback rule directly in the adapter (not the renderer — the renderer never
sees an empty `fullLabel`). `isAvailable()` always returns `true` — `KeyboardLayoutService` has no
connection/availability concept distinct from "constructed"; it emits its one-shot `true` seed and
never changes.

**Diffing on selection events compares only `short_label`.** REQ-F-011 requires "a name-only change
with an unchanged code emits nothing." Both signals feed the same handler, so a `layoutNameChanged`
with no code change still calls `eventObserved` — the *emission* still happens at the adapter; the
*suppression* of a redundant OSD is the controller's job (§5), not something the adapter special-
cases. This keeps `KeyboardLayoutChannelSource` a pure, unconditional translator (simpler to reason
about and test) and keeps all diff policy in one place.

---

## 5. `OsdController` Internals

```cpp
// libs/holonight-services/src/osd/OsdController.h
#pragma once

#include "OsdChannelSource.h"
#include "OsdEvent.h"

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

class OsdController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  using NowFn = std::function<std::chrono::steady_clock::time_point()>;

  // Sources are Qt-parented (normally to ShellApplication) and outlive the controller;
  // raw pointers per REQ-F-025 and this codebase's QObject conventions — not shared_ptr.
  explicit OsdController(std::vector<OsdChannelSource*> sources,
                         NowFn now_fn = [] { return std::chrono::steady_clock::now(); },
                         QObject* parent = nullptr);
  ~OsdController() override = default;

  OsdController(const OsdController&) = delete;
  OsdController& operator=(const OsdController&) = delete;
  OsdController(OsdController&&) = delete;
  OsdController& operator=(OsdController&&) = delete;

  Q_INVOKABLE void setSuppressed(const QString& channel, bool suppressed);   // REQ-F-005
  Q_INVOKABLE void setChannelEnabled(const QString& channel, bool enabled);  // REQ-F-007
  Q_INVOKABLE void setEnabled(bool enabled);                                 // REQ-C-010
  Q_INVOKABLE void setTimeoutMs(int timeout_ms);                            // REQ-C-011 (clamped again defensively)

#ifdef HOLONIGHT_TESTS
  [[nodiscard]] bool isHideTimerActiveForTest() const { return hide_timer_.isActive(); }
  [[nodiscard]] QString currentChannelForTest() const { return current_channel_; }
#endif

 Q_SIGNALS:
  void displayLevelEvent(const OsdLevelEvent& event);
  void displaySelectionEvent(const OsdSelectionEvent& event);
  void hideRequested();

 private Q_SLOTS:
  void onEventObserved(const OsdEvent& event);
  void onHideTimerTimeout();

 private:
  void onSourceAvailableChanged(const QString& channel, bool available);
  [[nodiscard]] bool inGracePeriod() const;
  void restartHideTimer();

  std::vector<OsdChannelSource*> sources_;  // non-owning; parented elsewhere
  NowFn now_fn_;
  std::chrono::steady_clock::time_point grace_start_;
  static constexpr auto kGracePeriod = std::chrono::milliseconds(2000);

  QHash<QString, OsdEvent> cache_;           // last known value per channel — always kept current
  QHash<QString, bool> suppressed_;          // REQ-F-005, pushed in by ShellApplication only
  QHash<QString, bool> channel_enabled_;     // REQ-F-007, from config
  bool master_enabled_{true};                // REQ-C-010, from config
  QString current_channel_;                  // last channel actually displayed (test/diagnostic visibility)
  QTimer hide_timer_;                        // single-shot, restarted on every display (REQ-F-017/019)
  int timeout_ms_{1500};
};
```

### 5.1 The clock/timer seam (design brief's explicit ask)

Two genuinely different timing concerns exist, and they get two different, independently justified
seams rather than one shared abstraction:

**Grace period (REQ-F-004) — a synchronous comparison, never an independent firing.** It never
needs to "wake up" on its own; it only gates whatever event happens to arrive while the window is
open. `inGracePeriod()` is `(now_fn_() - grace_start_) < kGracePeriod`, using an injected
`NowFn` (default `std::chrono::steady_clock::now`). A GTest constructs the controller with a lambda
closing over a `std::chrono::steady_clock::time_point` the test can advance by hand:

```cpp
auto fake_now = std::chrono::steady_clock::now();
OsdController controller({fake_source}, [&fake_now] { return fake_now; });
// ... emit three events, assert nothing ...
fake_now += std::chrono::milliseconds(2001);   // "advancing the injected clock" — no sleep, no timer
// ... emit again, assert the differing-value case fires and the equal-value case doesn't ...
```

This satisfies REQ-F-004's acceptance text ("Advancing the injected clock past 2000 ms") literally,
and costs zero wall-clock time in the test suite.

**Hide timeout (REQ-F-019/NF-010) — a real, scheduled callback.** Nothing re-polls the controller
after the last event; something must actively fire once `timeout_ms_` elapses with no further
activity. This is inherently asynchronous and needs a real `QTimer`. Rather than build a second,
parallel fake-timer abstraction, this design **reuses the existing project testing convention**:
`tests/test_notification_timeout.cpp` already tests a real, `QTimer`-driven auto-dismiss path (`
NotificationService`'s `expire_timeout_ms`) using a genuinely running Qt event loop and
`QSignalSpy::wait()` with a short configured timeout and a generous margin — no fake clock involved.
`OsdController::setTimeoutMs()` is exercised the same way in tests: set a short value (clamped no
lower than `OsdConfig::kMinTimeoutMs` = 300 ms — the clamp applies defensively at the setter, not
only at config-parse time), trigger a display event, then
`QSignalSpy(&controller, &OsdController::hideRequested).wait(600)`. `hide_timer_` is a plain,
single-shot `QTimer` member; `isHideTimerActiveForTest()` (guarded by the same `#ifdef
HOLONIGHT_TESTS` convention already used in `ShellApplication.h`) lets NF-010's "hide timer is
inactive both before the first event and after hide completes" be asserted directly — trivially
true before any event (never started) and after `onHideTimerTimeout()` runs (a single-shot `QTimer`
goes inactive the instant it fires).

Building one unified injectable-timer abstraction covering both cases was considered and rejected
(Key Decision 3, §11) — it would add an abstract scheduler interface + a production Qt-backed
implementation + a fake implementation, to cover a case (the hide timer) the codebase already has a
working, precedented, un-abstracted answer for.

### 5.2 The gating algorithm

```
onEventObserved(event):
    channel     = channelOf(event)                       // std::visit — both kinds carry `channel`
    is_first    = !cache_.contains(channel)
    changed     = is_first || !diffEquivalent(cache_[channel], event)   // see note below
    cache_[channel] = event                               // ALWAYS updated — unconditional, no gate skips this

    if is_first:            return   // silent prime                              (REQ-F-003)
    if !changed:             return   // no-op, value unchanged                    (REQ-F-003)
    if !master_enabled_:     return   // global kill switch                        (REQ-C-010)
    if !channelEnabled(ch):  return   // per-channel disable                       (REQ-F-007)
    if inGracePeriod():      return   // startup window                           (REQ-F-004)
    if isSuppressed(ch):     return   // surface-visibility suppression            (REQ-F-005)

    current_channel_ = channel
    restartHideTimer()
    emit displayLevelEvent(event) or displaySelectionEvent(event)   // by variant kind (REQ-F-008)
```

```
┌─────────────────────┐
│ onEventObserved(evt) │
└──────────┬───────────┘
           ▼
   cache_[channel] = evt   (always — see REQ-F-004's own test: cache updates even when discarded)
           ▼
    is_first? ──yes──▶ return (silent prime)
       │no
       ▼
    changed? ──no──▶ return (no-op)
       │yes
       ▼
  master enabled? ──no──▶ return                (REQ-C-010: coarsest, cheapest, checked first)
       │yes
       ▼
  channel enabled? ──no──▶ return                (REQ-F-007: per-channel, still config-driven/static)
       │yes
       ▼
  in grace period? ──yes──▶ return               (REQ-F-004: time-based, changes once per boot)
       │no
       ▼
   suppressed? ──yes──▶ return                   (REQ-F-005: most dynamic — toggles on every popup open/close)
       │no
       ▼
  restart hide timer, track current_channel_, emit display signal
```

**Ordering rationale:** coarse/static/cheap conditions are checked before increasingly
dynamic/fine-grained ones. `master_enabled_` and `channel_enabled_` are config values that change
only on a reload; `inGracePeriod()` changes exactly once (false→never-true-again) per process
lifetime; `isSuppressed()` is the one gate that legitimately flips on every popup/sidebar
open-close cycle, so it is checked last, closest to the point of emission, minimizing the number of
other checks touched by its rapid toggling. This ordering is also directly falsifiable by the
existing acceptance criteria — none of REQ-F-004/005/007/C-010's tests depend on relative ordering
among *each other* (each is tested with the others at their permissive default), so this ordering
is a design choice, not a requirement-mandated one, and is documented here rather than left
implicit.

**`diffEquivalent`, not the structs' own `operator==`, drives the diff.** `OsdLevelEvent`'s and
`OsdSelectionEvent`'s defaulted `operator==` (used for REQ-F-001's "copyable and assignable" test
and general equality checks) compares *all* fields. The controller's diff purposefully does not:
for `OsdSelectionEvent`, REQ-F-011 requires comparing only `short_label` (a `fullLabel`-only change
must not re-emit). A small free function, visited via `std::visit`, implements this narrower,
diff-specific comparison:

```cpp
bool diffEquivalent(const OsdEvent& lhs, const OsdEvent& rhs) {
  return std::visit(
      []<typename L, typename R>(const L& a, const R& b) -> bool {
        if constexpr (!std::is_same_v<L, R>) {
          return false;  // a channel's kind never changes mid-run, but stay safe
        } else if constexpr (std::is_same_v<L, OsdLevelEvent>) {
          return a.value == b.value && a.muted == b.muted;
        } else {
          return a.short_label == b.short_label;  // REQ-F-011: fullLabel excluded deliberately
        }
      },
      lhs, rhs);
}
```

**Availability transitions clear the cache.** `onSourceAvailableChanged(channel, available)`: on
`available == false`, `cache_.remove(channel)` — so a later reconnect (audio backend restart,
brightness backlight reappearing after a monitor hotplug) re-primes silently instead of diffing
against a stale value from before the disconnect. On `available == true`, no action is needed: the
next event naturally primes because the cache entry is already gone (or was never populated).

**Setter semantics are pure gate flips, no forced re-diff.** `setSuppressed(channel, false)`
followed by the *same* value that was silently suppressed does not retroactively emit — the cache
already holds that value (updated unconditionally per the algorithm above), so it fails the
`changed` check exactly like a grace-period-discarded value does after the window closes. This
symmetry (grace period and suppression behave identically w.r.t. the cache) is deliberate and is
exactly what REQ-F-004's fourth acceptance bullet already tests for grace; the same mechanism
covers REQ-F-005 for free.

**Threading (REQ-NF-008).** No production channel source in this design lives on a worker thread —
`AudioService`, `BrightnessService`, `KeyboardLayoutService` are all constructed on the GUI thread
in `ShellApplication`. Connections in `OsdController`'s constructor use the default
`Qt::AutoConnection` (not forced `Qt::DirectConnection`) specifically so that *if* a future channel
source is backed by a worker thread, Qt's automatic connection-type resolution promotes those
specific connections to queued without any change to `OsdController`. Combined with the
`qRegisterMetaType` calls noted in §3, this satisfies REQ-NF-008 by construction rather than by an
explicit thread-affinity check that has nothing to verify today.

---

## 6. Surface and View — `OsdSurface`

```cpp
// libs/holonight-surfaces/src/OsdSurface.h
#pragma once

#include "LayerShell.h"
#include "LayerSurface.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <holonight_config/config_structs.h>  // WidgetPosition

struct wl_surface;
class QQuickView;

// REQ-C-002/C-004. Pure presentation shell: no knowledge of OsdController, AudioService, or any
// channel semantics. Content is pushed in by the caller (ShellApplication) via showLevel()/
// showSelection(); position/monitor are pushed in via setPosition()/ensureSurface().
class OsdSurface : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  explicit OsdSurface(QObject* parent = nullptr);
  ~OsdSurface() override;

  OsdSurface(const OsdSurface&) = delete;
  OsdSurface& operator=(const OsdSurface&) = delete;
  OsdSurface(OsdSurface&&) = delete;
  OsdSurface& operator=(OsdSurface&&) = delete;

  void setPosition(WidgetPosition position);  // REQ-C-005/C-006

  void ensureSurface(const QString& screen_name);           // REQ-F-012
  void showLevel(const QString& channel, int value, bool muted);
  void showSelection(const QString& channel, const QString& short_label, const QString& full_label);
  void hide();  // starts the exit animation; destroySurface() runs once QML reports it finished

  [[nodiscard]] bool isActive() const { return view_ != nullptr; }

  // Called by OsdView.qml's exit-animation completion, mirroring
  // SidebarManager::onClosingAnimationFinished's exact pattern -- including its use of a deferred
  // destroy. The caller is a signal handler inside the object tree the teardown deletes, so this
  // queues destroyAfterHide() rather than destroying inline (Qt aborts on a synchronous delete).
  Q_INVOKABLE void onHideAnimationFinished();

 private Q_SLOTS:
  // Re-derives the teardown decision from current state: a show arriving between the queued post
  // and this call has already revived the OSD, and destroying then would kill a live surface.
  void destroyAfterHide();
  // REQ-C-013: content-driven. Sampled from QQuickWindow::afterAnimating (once per frame, after
  // polishItems()) rather than from implicitWidth/HeightChanged -- QtQuick.Layouts recompute their
  // implicit size during polish and not at all while the root is invisible, so those signals can
  // fire before the card has final geometry and then never again, stranding the surface at the
  // fallback size with the card overflowing it.
  void updateSurfaceSize();

 private:
  bool createSurface(const QString& screen_name);
  void applyInputRegion();       // REQ-F-024
  void pushPendingContent();     // replays last content onto a freshly (re)built view

  void destroySurface();

  LayerShell shell_;
  QQuickView* view_ = nullptr;
  LayerSurface* surface_ = nullptr;
  wl_surface* wl_surface_ = nullptr;
  QString current_screen_;
  bool pending_show_ = false;
  QString pending_screen_;
  WidgetPosition position_{WidgetPosition::CenterBottom};

  // Last content pushed, kept up to date on every showLevel()/showSelection() call regardless of
  // whether a view currently exists, so ensureSurface()'s monitor-change rebuild has something to
  // replay immediately (no one-frame flash of empty content on monitor migration).
  bool pending_is_level_{true};
  QString pending_channel_;
  int pending_value_{0};
  bool pending_muted_{false};
  QString pending_short_label_;
  QString pending_full_label_;
};
```

**Lifecycle — copied from `NotificationToastSurface::ensureSurface()` verbatim, not shared via a
new base class (Key Decision 5):** same monitor reuse check, same "monitor changed → destroy and
rebuild" branch, same `pending_show_`/`pending_screen_` deferral while `shell_.isActive()` is false,
replayed on `QWaylandClientExtension::activeChanged` (REQ-F-012, REQ-C-004).

**Anchoring (REQ-C-005/C-006):** `createSurface()` calls the now-public
`anchorFlagsForPosition(position_)` (extracted from `WidgetSurfacePolicy.cpp`'s anonymous
namespace) directly — never `widgetSurfacePlacement()`, whose `kWidgetWidth`/`kWidgetHeight`
constants the OSD does not share. Margins reuse the shared top-clearance rule directly:

```cpp
const int top = widgetPositionIsTopAnchored(position_) ? kBarHeight + kOsdMargin : kOsdMargin;
surface_->set_margin(top, kOsdMargin, kOsdMargin, kOsdMargin);
surface_->set_anchor(anchorFlagsForPosition(position_));
surface_->set_exclusive_zone(0);  // never reserves space, matching NotificationToastSurface
```

**Content-driven sizing (REQ-C-013):** no fixed width like `NotificationToastSurface`'s
`kToastWidth`. `updateSurfaceSize()` connects to the QML root's `implicitWidthChanged`/
`implicitHeightChanged` (via the `SIGNAL()`-string `connect` idiom `NotificationToastSurface`
already uses for `contentHeightChanged`) and resizes the layer surface to
`implicitWidth + 2*glowMargin` × `implicitHeight + 2*glowMargin`, with a small fallback size
(e.g. 220×96) until the first real layout pass reports.

**Input region and keyboard interactivity (REQ-F-024):** this is new ground — no existing surface
in the codebase sets an explicit empty input region (`NotificationToastSurface`,
`StatusPopupSurface`, etc. rely on being small/anchored and never needed one).
`applyInputRegion()` obtains the compositor via
`qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()->compositor()` — the same
native-interface access pattern `ExtIdleNotifyBackend.cpp` already uses for `seat()` — creates an
empty `wl_region` with `wl_compositor_create_region()` (no `wl_region_add()` calls), calls
`wl_surface_set_input_region(wl_surface_, empty_region)`, then `wl_region_destroy(empty_region)`
(the empty region is immediately destroyable per Wayland semantics; the compositor keeps its own
copy of the association). This runs both right after surface creation and again from a slot
connected to `LayerSurface::configured()`, matching REQ-F-024's acceptance text ("input region ...
after each configure") exactly.
`surface_->set_keyboard_interactivity(QtWayland::zwlr_layer_surface_v1::keyboard_interactivity_none)`
is set once at creation (matching `StatusPopupSurface`'s/`TrayMenuSurface`'s existing calls to the
same method, just with the `none` variant instead of `on_demand`/`exclusive`).

**`configured()` gating and the `SingleShotConnection` race (REQ-NF-009):** `createSurface()`
connects `LayerSurface::configured` with `Qt::SingleShotConnection` to a lambda that sets
`root->setProperty("configured", true)`, guarded exactly as CLAUDE.md documents:
`if (!isActive()) return;` before touching `root` — protecting against the case where the surface
was already torn down (rebuild on monitor change) between the configure event being queued and the
lambda running.

**`hide()` / `onHideAnimationFinished()`:** `hide()` sets `root->setProperty("hiding", true)` if a
view exists (no-op otherwise); this starts the QML exit choreography (§7). `OsdSurface` is
registered as a QML singleton (`QML_ELEMENT`/`QML_SINGLETON` + the same `reg()` call every other
early-constructed surface gets in `ShellApplication::registerQmlTypes()`), so QML calls
`OsdSurface.onHideAnimationFinished()` directly — the identical mechanism `SidebarManager` already
uses for `RightSidebar.qml`'s `onClosingAnimationFinished()` callback. `onHideAnimationFinished()`
calls `destroySurface()`.

**Monitor routing (REQ-C-012)** is deliberately *not* `OsdSurface`'s job — see §9. `OsdSurface`
only ever receives an already-resolved monitor name via `ensureSurface()`.

---

## 7. QML View Structure

`apps/shell/qml/Osd/OsdView.qml` (view root):

```qml
import QtQuick
import Holonight
import HolonightShell

Item {
    id: root
    required property string monitorName

    property string kind: "level"          // "level" | "selection" — pushed by C++
    property string channel: ""
    property int value: 0
    property bool muted: false
    property string shortLabel: ""
    property string fullLabel: ""
    property bool hiding: false
    property bool configured: false        // gated by LayerSurface::configured() (REQ-NF-009)

    property string currentChannel: ""      // QML's own copy — drives replay-vs-update, NOT the
                                             // same state as OsdController::current_channel_ (that
                                             // one exists for GTest/diagnostics only, see §5)

    readonly property int glowMargin: 16
    implicitWidth: contentLoader.item ? contentLoader.item.implicitWidth + glowMargin * 2 : 0
    implicitHeight: contentLoader.item ? contentLoader.item.implicitHeight + glowMargin * 2 : 0

    opacity: 0
    scale: 0.85
    visible: root.configured && root.opacity > 0   // "visible beats a concurrent Behavior" gotcha:
                                                     // gate on the ANIMATED property, not on `hiding`

    Behavior on opacity {
        NumberAnimation {
            id: opacityAnim
            duration: root.hiding ? 150 : 200        // REQ-F-016: 200ms in, 150ms out
            easing.type: Easing.OutCubic
            onRunningChanged: if (!running && root.hiding && root.opacity === 0) {
                OsdSurface.onHideAnimationFinished()
            }
        }
    }
    Behavior on scale {
        NumberAnimation { duration: root.hiding ? 150 : 200; easing.type: Easing.OutCubic }
    }

    // Called explicitly by OsdSurface::pushPendingContent() after the content properties land.
    // NOT an onChannelChanged handler: consecutive events on one channel leave `channel` equal to
    // what it already held, so a change-driven show would never run — see the note below.
    function present(): void {
        if (!root.configured) return
        if (root.channel !== root.currentChannel) {
            root.currentChannel = root.channel
            root.opacity = 0; root.scale = 0.85       // snap back so the entrance replays (REQ-F-018)
        }
        root.hiding = false                            // retargets an in-flight fade-out back to visible
        root.opacity = 1; root.scale = 1               // same-channel case: Behavior eases smoothly,
                                                         // no snap-back → no entrance replay (REQ-F-017)
    }
    onHidingChanged: {
        if (!root.hiding) return
        // Already invisible → assigning 0 starts no animation, so the exit callback that requests
        // the teardown would never fire and the surface would leak. Request it directly.
        if (root.opacity === 0) { OsdSurface.onHideAnimationFinished(); return }
        root.opacity = 0; root.scale = 0.85
    }

    Loader {
        id: contentLoader
        anchors.centerIn: parent
        sourceComponent: root.kind === "selection" ? selectionComponent : levelComponent
    }
    Component {
        id: levelComponent
        OsdLevelRenderer { channel: root.channel; value: root.value; muted: root.muted }
    }
    Component {
        id: selectionComponent
        OsdSelectionRenderer { shortLabel: root.shortLabel; fullLabel: root.fullLabel }
    }
}
```

**Why the show is an explicit call, not `onChannelChanged`:** driving it from the property-change
handler makes "show the OSD" conditional on `channel` actually *differing*, which is false for the
common case of two volume events in a row. On its own that only means the handler no-ops while the
OSD is already up — but if the second event lands during the 150 ms fade-out, `hiding` is cleared
(cancelling the teardown in `destroyAfterHide()`) while the opacity animation still runs to 0. The
surface then sits alive at `opacity: 0` with `currentChannel` unchanged, and no later same-channel
event can revive it; `hide()` cannot recover it either, because setting `hiding` on an
already-transparent root starts no animation and so never reaches the exit callback. The result is
an OSD that silently stops working until the shell restarts. `present()` removes the condition
entirely: every content push shows, whatever the channel and whatever the animation was doing.

**Update-in-place vs. replace, concretely:** the *only* place that decides is `present()`'s
comparison of the incoming `channel` against the QML-local `currentChannel`. Same channel → the
snap-back branch is skipped, so `opacity`/`scale` are already at 1, the `Behavior` on those
properties has nothing to animate, and the *content* fields (`value`, `muted`, `shortLabel`,
`fullLabel`) flow straight into `OsdLevelRenderer`/`OsdSelectionRenderer`'s own `Behavior on value`
(100 ms, REQ-F-017/NF-003) with no entrance replay. Different channel → snap back to 0/0.85 then
back to 1/1, replaying the 200 ms entrance (REQ-F-018) with no hide-then-show gap, because `hiding`
is never set true for a same-surface channel swap — only `OsdSurface::hide()` (driven by the
controller's `hideRequested()`, i.e. real inactivity) sets it.

**Avoiding the "required-property + Loader" gotcha:** `OsdLevelRenderer`/`OsdSelectionRenderer`
properties are assigned directly inside their own `Component {}` blocks (QML-side property binding
at the call site), not pushed post-hoc via C++ `setInitialProperties()` per branch — so the
documented "per-branch required properties fire `Required property … was not initialized`" trap
(CLAUDE.md) does not apply here; nothing is declared `required` on either renderer, and each
`Component` only ever binds the properties relevant to that renderer.

`OsdLevelRenderer.qml` (REQ-F-014/020/023):

```qml
import QtQuick
import Holonight

Item {
    id: root
    required property string channel
    required property int value
    required property bool muted

    readonly property string iconName: {
        if (root.channel === "screen-brightness") return "brightness"
        if (root.muted) return "audio-volume-muted"
        if (root.value >= 67) return "audio-volume-high"
        if (root.value >= 34) return "audio-volume-medium"
        return "audio-volume-low"
    }

    Behavior on value { NumberAnimation { duration: 100; easing.type: Easing.OutQuad } }  // REQ-F-017/NF-003

    // bar fill width tracks `value` (real position, even when muted — REQ-F-023); bar/track colors
    // and the dimmed-when-muted treatment come from HoloniightPalette.textMuted / .textDisabled /
    // .accentCyan-to-.accentViolet gradient (matching AudioIcon.qml's existing gradient), never a
    // hardcoded hex (REQ-NF-004).
    // Percentage text: root.muted ? qsTr("Muted") : (root.value + "%")   — REQ-F-023
}
```

`root.channel === "screen-brightness"` routes the icon name to `"brightness"`, rendered via
`BarIcon { name: "brightness" }` → `UtilityIcon.qml`'s new branch (REQ-F-021); all four
`audio-volume-*` names route through `BarIcon`'s existing `AudioIcon.qml` dispatch unchanged
(REQ-F-020, no new icon code needed there — `AudioIcon.qml` already implements exactly these four
names). `OsdSelectionRenderer.qml` renders `shortLabel` at ≥32pt and `fullLabel` smaller below it,
routes its icon through `BarIcon { name: "keyboard" }` (REQ-F-022, already implemented in
`UtilityIcon.qml`), and has its own `Behavior on opacity` per label to cross-fade text on update
(REQ-F-017's "smoothly fade text to new labels").

**No input handlers anywhere in `apps/shell/qml/Osd/` (REQ-F-024):** no `MouseArea`, `TapHandler`,
`HoverHandler`, `WheelHandler`, and no `focus: true`/`activeFocusOnTab: true` on any item — `grep`
over the directory is expected to return nothing, which is itself part of the acceptance criterion.

**Colors:** every color in all three files is `HoloniightPalette.<token>` (confirmed real tokens in
this codebase: `textMuted`, `textDisabled`, `textPrimary`, `textSecondary`, `accentCyan`,
`accentViolet`, `surfaceElevated`, `surfaceRaised`, `glassTint`, `borderPassive`, `borderActive`) —
zero hardcoded hex literals (REQ-NF-004).

---

## 8. Configuration

```cpp
// libs/holonight-config/include/holonight_config/config_structs.h
struct OsdChannelConfig {
  bool enabled{true};
  bool operator==(const OsdChannelConfig&) const = default;
};

struct OsdConfig {
  bool enabled{true};
  int timeout_ms{1500};
  static constexpr int kMinTimeoutMs{300};
  static constexpr int kMaxTimeoutMs{10000};
  WidgetPosition position{WidgetPosition::CenterBottom};  // explicit — enum's first value is LeftTop
  OsdChannelConfig volume;
  OsdChannelConfig brightness;
  OsdChannelConfig keyboard_layout;

  bool operator==(const OsdConfig&) const = default;
};
```

Added to `ParsedConfig` in `config_parsers.h` as `OsdConfig osd;`. Parsing follows the
`WidgetsConfig`/`CalendarConfig`/`LogoConfig` precedent, **not** the `AppearanceConfig`/
`NotificationsConfig` precedent: no `MissingDefaults` tracking, no `writeMissingDefaults()`
participation. That older mechanism exists to patch a *user's hand-edited, partial* file in place
for a small set of foundational sections; every section added after it (widgets, calendar, logo)
just parses with inline `toml++` defaults and is written back in full, unconditionally, by
`ConfigWriter::write()`. `[osd]` follows the newer pattern:

```cpp
// ConfigParsers.cpp
OsdConfig parseOsd(const toml::table& tbl) {
  OsdConfig cfg;
  const auto sec = tbl["osd"];
  cfg.enabled = sec["enabled"].value<bool>().value_or(true);

  bool timeout_missing_ignored = false;
  const int timeout = readInt(sec["timeout"], cfg.timeout_ms, "osd.timeout", timeout_missing_ignored);
  cfg.timeout_ms = clampRange(timeout, OsdConfig::kMinTimeoutMs, OsdConfig::kMaxTimeoutMs, "osd.timeout");

  if (const auto pos_opt = sec["position"].value<std::string>()) {
    const QString pos_str = QString::fromStdString(*pos_opt);
    if (const auto position = widgetPositionFromString(pos_str)) {
      cfg.position = *position;
    } else {
      qCWarning(lcConfigParsers) << "Config: osd.position" << pos_str << "is invalid; using default center-bottom";
    }
  }

  cfg.volume.enabled = tbl["osd"]["volume"]["enabled"].value<bool>().value_or(true);
  cfg.brightness.enabled = tbl["osd"]["brightness"]["enabled"].value<bool>().value_or(true);
  cfg.keyboard_layout.enabled = tbl["osd"]["keyboard_layout"]["enabled"].value<bool>().value_or(true);
  return cfg;
}
```

`readInt`/`clampRange`/`widgetPositionFromString` are all pre-existing free functions reused as-is
(REQ-C-005/C-011). An absent `[osd]` table and an empty `[osd]` table both flow through the same
`.value_or(...)` defaults — REQ-C-009's "no section" and "empty section" acceptance criteria are
satisfied identically, by construction, with no special-cased branch.

`ConfigWriter.cpp` gains a `writeOsd()` emitting all six keys unconditionally (same shape as
`writeCalendar`/`writeWidgets`), called from `ConfigWriter::write()` after `writeCalendar(out,
config.calendar);`. Because it always writes the full struct, any settings-app-driven full rewrite
preserves every `[osd]` key — REQ-C-009's round-trip acceptance criterion.

**`ConfigService`** gains `osd_` (an `OsdConfig`), `[[nodiscard]] const OsdConfig& osd() const`, and
`void osdConfigChanged();`, populated/diffed in `applyParsedConfig()` exactly like every other
section (`background_`/`backgroundChanged()`, etc.).

**Live reload: supported, deliberately.** `ConfigService` already reloads every section uniformly
through its `QFileSystemWatcher` + 200 ms debounce + `applyParsedConfig()` pipeline — there is no
per-section opt-out today, and inventing one for `[osd]` alone would be a surprising, unjustified
exception. `ShellApplication::applyOsdConfig()` (a small private method) reads `config_service_->osd()`
and pushes every field into `OsdController`/`OsdSurface` via the public setters already required to
exist for GTest purposes (§5); it is called once in `startServices()` and again whenever
`ConfigService::osdConfigChanged()` fires, mirroring `rebuildWidgets()`'s exact relationship to
`widgetsConfigChanged()`.

`OsdController`/`OsdSurface` never read `ConfigService` themselves (REQ-F-025's constructor-injection
requirement, and `OsdSurface`'s "pure presentation shell" role in §6) — `ShellApplication` is the
only thing that reads config and pushes it down, same as it is the only thing that pushes
suppression down (§9).

---

## 9. Wiring in `ShellApplication`

**Construction order.** `AudioChannelSource`/`BrightnessChannelSource`/`KeyboardLayoutChannelSource`
each hold a raw, non-owning pointer to the service they adapt, so `OsdController` must be
constructed after `audio_`, `brightness_service_`, and `keyboard_layout_` all exist. In
`ShellApplication.h`'s member-declaration order (which governs actual construction order,
independent of initializer-list order), `keyboard_layout_` is declared early and `audio_` mid-list,
but `brightness_service_` is declared quite late (after `notification_manager_`) — so
`OsdController* osd_controller_` and `OsdSurface* osd_surface_` must be declared **immediately
after `brightness_service_`**, the last of the three dependencies to come into existence:

```cpp
// ShellApplication.h, immediately after: BrightnessService* brightness_service_ = nullptr;
OsdController* osd_controller_ = nullptr;
OsdSurface* osd_surface_ = nullptr;
```

```cpp
// ShellApplication.cpp constructor, member-init list, immediately after brightness_service_(...):
osd_controller_(new OsdController(
    {std::make_shared<AudioChannelSource>(audio_),
     std::make_shared<BrightnessChannelSource>(brightness_service_),
     std::make_shared<KeyboardLayoutChannelSource>(keyboard_layout_)},
    [] { return std::chrono::steady_clock::now(); }, this)),
osd_surface_(new OsdSurface(this)),
```

Both are owned by `ShellApplication` via standard `QObject` parenting (`this`), exactly like every
other early-constructed surface (`status_popup_surface_`, `tooltip_surface_`, etc.) — no
`unique_ptr`, no special teardown ordering needed since neither depends on the shared, later-
constructed `layer_shell_`.

**QML registration** (`registerQmlTypes()`):

```cpp
reg(osd_controller_, "OsdController");
reg(osd_surface_, "OsdSurface");
```

**Controller ↔ surface wiring** (`startServices()`, alongside the other `connect()` calls already
there):

```cpp
connect(osd_controller_, &OsdController::displayLevelEvent, this, [this](const OsdLevelEvent& event) {
  osd_surface_->ensureSurface(resolveOsdMonitor());
  osd_surface_->showLevel(event.channel, event.value, event.muted);
});
connect(osd_controller_, &OsdController::displaySelectionEvent, this, [this](const OsdSelectionEvent& event) {
  osd_surface_->ensureSurface(resolveOsdMonitor());
  osd_surface_->showSelection(event.channel, event.short_label, event.full_label);
});
connect(osd_controller_, &OsdController::hideRequested, this, [this] { osd_surface_->hide(); });
```

```cpp
QString ShellApplication::resolveOsdMonitor() const {
  const QString focused = aws_->focusedMonitorName();
  if (!focused.isEmpty()) return focused;
  if (QScreen* primary = QGuiApplication::primaryScreen()) return primary->name();
  return {};
}
```

This is the same shape as the pre-existing `launcher_surface_->toggle(aws_->focusedMonitorName())`
call already in `startServices()` — REQ-C-012 is satisfied with no new class.

**Suppression wiring** (REQ-F-006/C-007/C-008):

```cpp
connect(status_popup_surface_, &StatusPopupSurface::popupVisibleChanged, this,
        &ShellApplication::updateAudioOsdSuppression);
connect(status_popup_surface_, &StatusPopupSurface::activePopupChanged, this,
        &ShellApplication::updateAudioOsdSuppression);
...
connect(sidebar_manager_.get(), &SidebarManager::sidebarOpened, this,
        &ShellApplication::updateBrightnessOsdSuppression);
connect(sidebar_manager_.get(), &SidebarManager::sidebarClosed, this,
        &ShellApplication::updateBrightnessOsdSuppression);
connect(sidebar_manager_.get(), &SidebarManager::currentTabChanged, this,
        &ShellApplication::updateBrightnessOsdSuppression);
```

```cpp
void ShellApplication::updateAudioOsdSuppression() {
  const bool suppress = status_popup_surface_->isPopupVisible() &&
                        status_popup_surface_->activePopupId() == QStringLiteral("audio");
  osd_controller_->setSuppressed(QStringLiteral("audio-volume"), suppress);
}

void ShellApplication::updateBrightnessOsdSuppression() {
  static constexpr int kQuickSettingsTabIndex = 4;  // SidebarContent.qml's tabDefinitions[4]
  bool suppress = false;
  for (const QScreen* screen : QGuiApplication::screens()) {
    const QString name = screen->name();
    if (sidebar_manager_->isOpen(name) && sidebar_manager_->currentTabForMonitor(name) == kQuickSettingsTabIndex) {
      suppress = true;
      break;
    }
  }
  osd_controller_->setSuppressed(QStringLiteral("screen-brightness"), suppress);
}
```

`updateBrightnessOsdSuppression()` needs a synchronous read-back (`currentTabForMonitor()`, a new
accessor — §2) because `SidebarManager`'s signals only announce "something changed on this
monitor"; ShellApplication still has to ask "is *any* monitor currently open on the QuickSettings
tab" to compute one global suppression flag, since `OsdController::setSuppressed` is per-channel,
not per-monitor (matching the single-surface, single-current-channel OSD model — there is only ever
one OSD, following the focused monitor, so suppression is necessarily a single boolean per
channel, aggregated across all monitors' sidebar state).

`setSuppressed("keyboard-layout", ...)` is never called anywhere — REQ-C-008 is satisfied by
absence, verifiable by `grep -r "setSuppressed(\"keyboard-layout\"" apps/ libs/` finding nothing.

**`osd.enabled = false` does not need conditional wiring (REQ-C-010).** The suppression connections
above are wired unconditionally, regardless of `osd_cfg.enabled`. This is safe: `setSuppressed()`
only ever writes an internal `OsdController` map; when `master_enabled_` is `false`,
`onEventObserved` returns before the suppression map is even consulted (§5's gating order puts
`master_enabled_` before `isSuppressed()`), so the calls are inert. Conversely, nothing about
`StatusPopupSurface`/`SidebarManager`'s own behavior is touched by `OsdController` in either
direction — REQ-C-010's "disabling OSD does not affect sidebar or audio popup visibility" holds
because that coupling was never built in this direction to begin with.

**Config application:**

```cpp
void ShellApplication::applyOsdConfig() {
  const OsdConfig& cfg = config_service_->osd();
  osd_controller_->setEnabled(cfg.enabled);
  osd_controller_->setChannelEnabled(QStringLiteral("audio-volume"), cfg.volume.enabled);
  osd_controller_->setChannelEnabled(QStringLiteral("screen-brightness"), cfg.brightness.enabled);
  osd_controller_->setChannelEnabled(QStringLiteral("keyboard-layout"), cfg.keyboard_layout.enabled);
  osd_controller_->setTimeoutMs(cfg.timeout_ms);
  osd_surface_->setPosition(cfg.position);
}
```

called once from `startServices()` and connected to `config_service_`'s new
`osdConfigChanged()` signal, matching `rebuildWidgets()`/`widgetsConfigChanged()` exactly.

---

## 10. Key Decisions with Rationale

**1. Event types are free-standing structs in `OsdEvent.h`, not nested inside the `OsdController`
class.** REQ-F-001's title says "OsdController shall define normalized event types," which reads
most naturally as nesting (`OsdController::OsdLevelEvent`). Rejected: `OsdChannelSource` (a
separate, more fundamental base class that `OsdController` depends on, not the reverse) must
reference the same type in its own signal signature (REQ-F-002's literal `eventObserved(OsdEvent)`).
Nesting the type inside `OsdController` would make the abstract source interface depend on the
concrete controller class for its own vocabulary — an inverted, circular-feeling dependency for no
benefit. Free-standing types in a shared header, included by both, avoid the inversion; "the
OsdController namespace" is satisfied in spirit (these are `OsdController`'s domain vocabulary,
declared alongside it) without the structural cost.

**2. `OsdLevelEvent`/`OsdSelectionEvent` are `Q_GADGET` value types, not passed as decomposed
primitive signal arguments.** An alternative considered: `displayLevelEvent(QString channel, int
value, bool muted)` — three primitives, trivially QML- and GTest-readable, no `Q_GADGET` needed.
Rejected because REQ-F-008's literal, tested signature is `displayLevelEvent(OsdLevelEvent)` (a
single-argument signal) — GTest's `QSignalSpy` acceptance criteria depend on that exact shape. The
`Q_GADGET` route also directly reuses an established, working codebase pattern
(`WeatherData.h`/`WeatherService`) rather than inventing a second convention for "how a value type
crosses a Qt signal into QML."

**3. One clock/timer seam is a fake-injectable comparison (grace period); the other is a real
`QTimer` exercised via `QSignalSpy::wait()` (hide timeout) — not one unified abstraction covering
both.** Considered and rejected: an `IOsdScheduler` interface with a production `QTimer`-backed
implementation and a fake, manually-advanced implementation for both concerns. The grace period
never independently "fires" — it's a pure comparison evaluated only when an event arrives, so a
scheduler abstraction would add machinery with nothing to schedule. The hide timeout genuinely does
need to fire on its own, and this codebase already has a working, precedented way to test that
(`test_notification_timeout.cpp`'s real-QTimer-plus-generous-wait style) — reusing it avoids a
second bespoke seam for a problem the project has already solved once.

**4. `ShellApplication` is the sole orchestrator wiring `OsdController` signals to `OsdSurface`
calls — no new `OsdPresenter`/`OsdManager` class in `holonight-surfaces`.** The codebase has two
existing precedents for "something in the presentation layer reacts to a service": (a)
`NotificationManager`, a `holonight-surfaces` class that takes `NotificationService*` directly and
owns per-monitor visible/queued-toast bookkeeping — real, non-trivial state that justifies a
dedicated class; and (b) `ShellApplication` itself resolving `aws_->focusedMonitorName()` and
directly calling `launcher_surface_->toggle(...)` — no intermediate class, because the routing
logic is a single line. The OSD is single-surface, single-current-channel, with no per-monitor
queue or visible-set to track — its "routing logic" (resolve a monitor, call `showLevel`/`hide`) is
exactly as thin as the launcher case, not the notification case. Introducing an `OsdPresenter`
class in `holonight-surfaces` would also force adding `OsdController.h` (and, for monitor
resolution, `ActiveWindowService.h`) to
`scripts/check-architecture-boundaries.sh`'s allowlist — two new sanctioned exceptions for a class
whose entire job fits in three `connect()` calls already idiomatic for `ShellApplication`. Rejected
in favor of doing the wiring directly in `ShellApplication`, which also keeps REQ-C-007's "the ONLY
caller of `setSuppressed()`" and this feature's monitor-routing/signal-bridging logic in the same
one place, for one auditable reason (this is the app's composition root).

**5. `OsdSurface`'s create/destroy lifecycle is an independent copy of
`NotificationToastSurface`'s pattern, not refactored into a shared base class.** REQ-C-004 permits
either ("do not duplicate pattern code; refactor if needed" — "if needed" is not "mandatory").
Extracting a `LayerShellSurfaceBase` shared by both `NotificationToastSurface` and the new
`OsdSurface` would mean modifying a surface class that already ships and works, for a feature that
does not require it, at the risk of subtly changing `NotificationToastSurface`'s behavior along the
way. The duplicated logic is small (~20 lines of create/reuse/rebuild-on-monitor-change), and a
third surface needing the identical shape would be the natural trigger for that refactor (rule of
three) — not this one. Rejected for now, noted as a reasonable future cleanup.

**6. The controller keeps its own `current_channel_` (test/diagnostic only); QML keeps a separate,
independently-driven `currentChannel` property for its own animation decision.** These are
deliberately not "the same piece of state shared across the process boundary." The controller's
copy exists so GTest can assert the end-to-end prime→suppress→update-in-place flow (REQ-C-015)
without a QML engine. QML's copy exists because "should the entrance animation replay" is
fundamentally a presentation decision that belongs where the animation lives — pushing a
`shouldReplayEntrance: bool` flag down from C++ on every event would just move the same comparison
to a different layer for no benefit, and would need to account for QML-side state the controller
cannot see (e.g., whether the previous entrance animation had actually finished). Two independent,
narrowly-scoped copies, each serving one layer's own test/behavior needs, was chosen over a single
shared source of truth that would need to cross the process/thread-agnostic Qt-signal boundary for
a decision that only matters on the QML side.

---

## 11. Alternatives Considered and Rejected (Architecture-Level)

**A single `OsdEvent` `QObject` (not a `Q_GADGET` value type) exposing `Q_PROPERTY`s, reused/mutated
in place instead of a fresh value per emission.** Would allow QML property bindings instead of
`Connections` handlers, but introduces object-identity/lifetime questions across a signal boundary
(who owns it, when is it safe to read after the signal handler returns, does QML's GC path
interact with it) that a plain copyable value type sidesteps entirely. Rejected — value semantics
are simpler and match REQ-F-001's "copyable and assignable" acceptance criterion directly.

**Polling instead of signal observation** (e.g., a `QTimer` sampling `AudioService::volume()` every
50 ms). Directly contradicts REQ-NF-001/002/010 (no timers when idle, ≤100 ms observe→display
latency, no polling anywhere) and the SPEC's own stated architecture ("Rather than polling ...").
Never seriously considered; noted here only because "observe existing signals" is this design's
foundational premise and deserves an explicit contrast.

**Making the OSD's suppression bidirectional** — e.g., `OsdController` exposing an
`isDisplayed()` query that `StatusPopupSurface` or `SidebarManager` could consult to avoid opening
on top of a visible OSD. Nothing in the SPEC asks for this, and it would immediately violate
REQ-C-001 (`holonight-services` querying surface visibility) in the *other* direction —
`holonight-surfaces` classes reaching into a `holonight-services` controller to ask about display
state before deciding to open. Rejected; suppression in this design is one-directional and
push-only (surfaces → controller), matching REQ-F-006's exact wording.

**A single `OsdChannelConfig` enum-keyed map (`QHash<Channel, bool>`) instead of three named
booleans (`volume`, `brightness`, `keyboard_layout`) on `OsdConfig`.** Would generalize better if a
fourth channel is added later (the SPEC's own Non-goals list several candidates: mic mute, Caps
Lock, etc.), but TOML doesn't have a natural array-of-enum-keyed-table shape as clean as three named
subsections, and REQ-C-009's acceptance criteria name the three keys explicitly
(`volume.enabled`, `brightness.enabled`, `keyboard_layout.enabled`). Three named `OsdChannelConfig`
members were chosen to match the spec's literal TOML shape; a future fourth channel adds a fourth
named member (small, mechanical), not a redesign.

---

## 12. Risks and Open Questions

**Suppression wiring is a real, if narrow, coupling.** `ShellApplication` must know `"audio"` is
`AudioWidget.qml`'s `popupId` string and that `SidebarContent.qml`'s `tabDefinitions[4]` is
QuickSettings — both are QML-side conventions with no compile-time link to the C++ strings used in
`updateAudioOsdSuppression()`/`updateBrightnessOsdSuppression()`. If either QML file's tab order or
popup ID is ever reordered/renamed, `ShellApplication`'s hardcoded index/string silently stops
matching and brightness/audio OSD suppression quietly breaks (no crash, no warning — the OSD just
starts appearing over an open QuickSettings panel or audio popup). This is the direct, unavoidable
cost of REQ-F-006 wanting a coarse concept ("is the audio popup showing volume right now") that no
existing signal expresses as a single boolean. Mitigation considered and rejected as overkill for
this feature's scope: promoting `"audio"` and the QuickSettings tab index to shared named constants
visible from both `apps/shell/qml/Topbar/AudioWidget.qml`/`SidebarContent.qml` and
`ShellApplication.cpp` would require a QML/C++-shared constant mechanism this codebase does not
currently have for this kind of string; a comment at each of the three call sites cross-referencing
the others is the practical mitigation applied here instead.

**Borne out in practice:** the live walkthrough of the acceptance checklist (recorded in
`TASKS.md`) found three defects — a fatal abort on every fade-out, a clipped card, and an OSD that
silently stopped working until restart — while every automated check stayed green throughout. All
three are fixed and re-verified live. The risk below is therefore not hypothetical: for this
feature, a green `task test` run carries no information about `OsdSurface.cpp` or `OsdView.qml`.

**Compositor-facing behavior here is large and only manually verifiable.** REQ-F-013/016/017/018/
024, REQ-NF-009, and most of the Presentation Acceptance checklist cannot be exercised by `task
test`/`task qml-lint`/`task build` — per this project's manual-testing protocol (shell UI is never
driven programmatically), every one of these needs a live Hyprland session and a human watching:
entrance/exit timing "feel," update-in-place vs. replace with real volume/brightness/layout
changes, click-through confirmation, and multi-monitor routing on hotplug. `task
compositor-smoke-check` gives a checklist but not automated pass/fail. This is a materially larger
manual-verification surface than most recent features in this repo (idle-management, brightness-
service) because the OSD is, by nature, almost entirely presentation timing and Wayland input-region
correctness — the two categories this project's own tooling explicitly cannot check.

**The empty-input-region mechanism (REQ-F-024) has no existing precedent in this codebase to copy.**
Every other surface either occupies the *whole* screen and relies on click-through-by-role
(none, actually — `StatusPopupSurface`'s dismiss overlay is deliberately full-input, not
click-through) or is anchored/small enough that an accidental full-input region has never mattered.
`OsdSurface` is the first surface where "small, anchored, and click-through" all matter
simultaneously, and REQ-C-013 exists specifically because REQ-F-024 alone was judged fragile enough
to need a second, independent safety net (content-driven sizing bounding the blast radius if the
region logic regresses). The `wl_compositor_create_region`/`wl_surface_set_input_region()` call
sequence in §6 is designed from the Wayland protocol spec and Qt's native-interface access pattern
already used elsewhere (`ExtIdleNotifyBackend.cpp`'s `compositor()`/`seat()` access), but should be
treated as the highest-risk, least-precedented piece of new Wayland-facing code in this feature and
verified first and most carefully in the manual checklist.

**REQ-F-001's literal field-naming and this repository's `.clang-tidy` rule genuinely conflict** —
already resolved and documented in §3, restated here because it is a place a reviewer might
reasonably expect the spec's exact wording and find `short_label`/`full_label` instead.

**`OsdConfig::position` default must be set explicitly to `WidgetPosition::CenterBottom`.** The enum's
implicit zero-value is `LeftTop` (first enumerator); relying on default member-initialization
without an explicit value would silently default the OSD to the wrong corner. Flagged here as an
easy, silent mistake to make when implementing `config_structs.h` (already written correctly in §8,
called out because it is exactly the kind of one-line slip that would pass every automated check
and only surface as "the OSD default position is wrong" in manual testing).

---

## 13. Requirements Coverage Table

| REQ ID | Satisfied by |
|---|---|
| REQ-F-001 | `OsdEvent.h` — §3 |
| REQ-F-002 | `OsdChannelSource.h` — §4 |
| REQ-F-003 | `OsdController::onEventObserved` prime/diff — §5.2 |
| REQ-F-004 | Grace period + injected `NowFn` — §5.1/5.2 |
| REQ-F-005 | `setSuppressed()` — §5, §5.2 |
| REQ-F-006 | `ShellApplication` suppression wiring — §9 |
| REQ-F-007 | `setChannelEnabled()` — §5, §5.2 |
| REQ-F-008 | `displayLevelEvent`/`displaySelectionEvent` signals — §3, §5 |
| REQ-F-009 | `AudioChannelSource` — §4 |
| REQ-F-010 | `BrightnessChannelSource` — §4 |
| REQ-F-011 | `KeyboardLayoutChannelSource` + `diffEquivalent` — §4, §5.2 |
| REQ-F-012 | `OsdSurface::ensureSurface()` — §6 |
| REQ-F-013 | `zwlr_layer_shell_v1_layer_overlay` in `createSurface()` — §6 (manual verification per §12) |
| REQ-F-014 | `OsdLevelRenderer.qml` — §7 |
| REQ-F-015 | `OsdSelectionRenderer.qml` — §7 |
| REQ-F-016 | Root entrance/exit `Behavior`s — §7 (manual verification per §12) |
| REQ-F-017 | `present()` same-channel path + renderer `Behavior on value` — §7 |
| REQ-F-018 | `present()` different-channel snap-back path — §7 |
| REQ-F-019 | `hide_timer_` / `restartHideTimer()` — §5 |
| REQ-F-020 | `OsdLevelRenderer.iconName` → existing `AudioIcon.qml` — §7 |
| REQ-F-021 | `BarIcon.qml` + `UtilityIcon.qml` `"brightness"` branch — §2, §7 |
| REQ-F-022 | `BarIcon { name: "keyboard" }`, already implemented — §7 |
| REQ-F-023 | `OsdLevelRenderer` muted bar-position/text-swap logic — §7 |
| REQ-F-024 | `OsdSurface::applyInputRegion()` + no QML input handlers — §6, §7 (highest risk, §12) |
| REQ-F-025 | `OsdController` constructor takes `OsdChannelSource*` (Qt parent-ownership) + injected `NowFn` — §5, §5.1 |
| REQ-NF-001 | No I/O in adapters/controller constructors; no surface until first event — §4, §5, §9 |
| REQ-NF-002 | Synchronous `onEventObserved` → `emit`, `Qt::AutoConnection` — §5.2 |
| REQ-NF-003 | All renderer/root transitions are `Behavior`/`NumberAnimation` — §7 |
| REQ-NF-004 | `HoloniightPalette` tokens only — §7 |
| REQ-NF-005 | Naming conventions followed throughout (§3's flagged exception explicitly resolved in the lint rule's favor) |
| REQ-NF-006 | `qCInfo`/`qCWarning` logging category in `OsdController` (implementation detail, category name e.g. `holonight.osd`) |
| REQ-NF-007 | `OsdSurface::destroySurface()` mirrors `NotificationToastSurface::destroySurface()` — §6 |
| REQ-NF-008 | `Qt::AutoConnection` + `qRegisterMetaType` — §3, §5.2 |
| REQ-NF-009 | `configured` gate + `Qt::SingleShotConnection` guard — §6, §7 |
| REQ-NF-010 | Single-shot `hide_timer_`, no surface/scene graph while hidden — §5.1, §6 |
| REQ-C-001 | `libs/holonight-services/src/osd/` — §2 |
| REQ-C-002 | `libs/holonight-surfaces/src/OsdSurface.*` — §2, §6 |
| REQ-C-003 | `apps/shell/qml/Osd/` (auto-globbed, no manual QRC step needed — noted correction in §2) |
| REQ-C-004 | `OsdSurface` copies `NotificationToastSurface`'s pattern — §6, Key Decision 5 |
| REQ-C-005 | `widgetPositionFromString()` in `parseOsd()` — §8 |
| REQ-C-006 | `anchorFlagsForPosition()` extracted to `WidgetSurfacePolicy.h` — §2, §6 |
| REQ-C-007 | `ShellApplication` is the sole `setSuppressed()` caller — §9, Key Decision 4 |
| REQ-C-008 | No `setSuppressed("keyboard-layout", ...)` call anywhere — §9 |
| REQ-C-009 | `OsdConfig` + `parseOsd()`/`writeOsd()` — §8 |
| REQ-C-010 | `master_enabled_` gate, checked first among the config gates — §5.2, §9 |
| REQ-C-011 | `clampRange()` in `parseOsd()` + defensive clamp in `setTimeoutMs()` — §5.1, §8 |
| REQ-C-012 | `ShellApplication::resolveOsdMonitor()` — §9 |
| REQ-C-013 | Content-driven `updateSurfaceSize()` — §6 |
| REQ-C-014 | `KeyboardLayoutService::layoutName`/`layoutNameChanged` — §2, §4 |
| REQ-C-015 | `tests/test_osd_controller.cpp`, fake sources + injected clock, no live services — §5.1, §2 |

No requirement is left uncovered. The two flagged tensions (REQ-F-001's literal field spelling vs.
`.clang-tidy`; REQ-C-003's literal `qt6_add_resources()` mechanism vs. this repo's actual
glob-based QML bundling) are resolved in-place in §3/§2 respectively, in favor of the codebase's
binding conventions, rather than silently implemented either way without comment.
