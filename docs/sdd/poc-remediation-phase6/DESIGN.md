# Phase 6 — Mechanical & Background Cleanup: Design

**Status**: Design Phase (Stage 2 of SDD)
**Input**: `docs/sdd/poc-remediation-phase6/SPEC.md`

This document grounds SPEC.md's 36 findings in the current tree (commit `3fd9628`), confirms
where the audit's line numbers have drifted, and fixes concrete resolutions for the four
higher-risk items plus the shared-constant/helper consolidation shape. Everything else in
Category B is mechanical (rename a literal to a named constant, hoist a `QHash` to `static
const`, move a QML element, delete a dead `id`) and is not re-litigated here beyond confirming
the pattern to follow.

---

## 1. Components Affected

Grouped by subsystem, mirroring SPEC's unit numbering. Paths verified against the current tree.

### C++ — Services / Core / Platform
| Unit | Files |
|---|---|
| Category A (roleNames) | `libs/holonight-core/src/WorkspaceModel.cpp`, `libs/holonight-surfaces/src/TrayModel.cpp`, `libs/holonight-surfaces/src/DbusMenuItem.cpp`, `libs/holonight-services/src/network/WifiNetworkModel.cpp`, `libs/holonight-services/src/audio/AudioDeviceModel.cpp`, `libs/holonight-services/src/audio/AudioStreamModel.cpp`, `libs/holonight-services/src/launcher/LauncherModel.cpp`, `libs/holonight-services/src/notifications/NotificationRuleModel.cpp`, `libs/holonight-services/src/notifications/NotificationService.cpp`, `libs/holonight-services/src/calendar/CalendarEventModel.cpp`, `apps/settings/src/FontListModel.cpp` |
| U-01 Core/Platform/Config | `libs/holonight-platform/src/HyprlandIpc.cpp`, `libs/holonight-platform/src/HyprlandIpcClient.*` |
| U-02 Surfaces/Layer-Shell | `libs/holonight-surfaces/src/TrayMenuSurface.cpp`, `libs/holonight-surfaces/src/TooltipSurface.cpp`, `libs/holonight-surfaces/src/StatusPopupGeometry.cpp`, `libs/holonight-surfaces/src/ShellConstants.h` (existing, extended) |
| U-03 Power/Idle/Brightness | `libs/holonight-services/src/InhibitorModel.cpp` (actual path — SPEC's `.../InhibitorModel.cpp` unqualified), `libs/holonight-services/src/SuspendInhibitorService.h` |
| U-04 Session/Window/Network/Audio | `libs/holonight-services/src/audio/PulseAudioBackend.cpp`, `libs/holonight-services/src/audio/AudioDeviceModel.cpp`, `libs/holonight-services/src/audio/AudioStreamModel.cpp`, `libs/holonight-services/src/audio/AudioService.cpp` (test-only touch, see §4.2), `libs/holonight-services/src/network/NetworkManagerBackend.cpp` |
| U-05 Desktop Integration | `libs/holonight-services/src/ThemeService.h`, `libs/holonight-services/src/ThemeService.cpp` (docs only) |
| U-06 Launcher/Notifications | `libs/holonight-services/src/launcher/LauncherService.cpp` |
| U-07 Calendar/Weather | `libs/holonight-services/src/CalendarService.cpp` (actual path — not under `calendar/`), `libs/holonight-services/src/calendar/CalDavProvider.cpp`, `libs/holonight-services/src/calendar/IcsProvider.cpp`, `libs/holonight-services/src/calendar/HttpSyncClient.cpp` (new — see §4.3), `apps/shell/app/ShellApplication.cpp` + `.h`, `libs/holonight-services/src/calendar/CalendarCache.cpp`, `libs/holonight-services/src/calendar/CalendarCache.h`, `libs/holonight-services/src/calendar/CalendarSyncManager.cpp`, `libs/holonight-services/src/weather-icon/WeatherIconBridge.cpp` |
| U-11 Settings App | `apps/settings/src/ThemeConfigFile.h/.cpp`, `apps/settings/src/ConfigFileService.cpp`, `apps/settings/src/SettingsEditModel.cpp` |

### QML
| Unit | Files |
|---|---|
| U-08 Topbar | `apps/shell/qml/Topbar/ClockSection.qml`, `.../WorkspaceEdgeArrow.qml`, `.../StatusPopupTriggerArea.qml`, `.../BarSection.qml`, `.../WorkspacePill.qml`, plus ~15 files with dead `id`s (enumerated at implementation time via a project-wide qmllint/grep pass, not individually named in SPEC) |
| U-09 Sidebar/Popup | `apps/shell/qml/RightSidebar/.../WifiNetworkDelegate.qml` (exact subpath TBD at implementation — confirm via `find` since SPEC gives only the filename) |
| U-10 Overlay/Widgets | 5 files under Launcher/Notifications/Widgets QML (`pragma ComponentBehavior` consistency), `apps/shell/qml/Launcher/Launcher.qml`, `apps/shell/qml/Controls/BarIcon.qml`, `apps/shell/qml/Tray/TrayItem.qml` |

No new files are introduced except the two described in §5 (`ShellConstants.h` gets new entries,
not a new file; `ThemeConfigFile` gets one new static method — no new file).

---

## 2. Data Flow / Architecture Impact

This phase does **not** restructure any subsystem. 32 of 36 items are strictly localized:
a per-call allocation becomes a function-local static, a magic number gets a name, a duplicated
constant moves to one already-existing shared header, a QML element is reordered or its handler
type is swapped. None of these change what data flows where or which component owns which
responsibility.

Four items are behavior-preserving-but-observable (REQ-C-4): F-2.5 (skip redundant model reset),
F-2.9 (reorder an early-return check — no new state), F-2.14 (reorder cleanup calls — no new
state), F-2.17 (extract constants with a mandated zero-output-change constraint). These are still
local to their existing function — no new components, no new signals, no new call graph edges.

**The one architectural item is F-2.13** (`apps/shell/app/ShellApplication.cpp`). Today,
`ActivityGateManager` is one of the *last* children constructed (in `startServices()`, after
`suspend_inhibitor_service_`), while `CalendarService` (ctor init-list, member 2) and
`WeatherService` (member 17) — both `IActivityGate` implementers — are constructed far earlier.
`ActivityGateManager::registerGate()` only stores raw `IActivityGate*` pointers
(`libs/holonight-services/src/ActivityGateManager.cpp:5`, `gates_.push_back(gate)`); it never
takes ownership and gates never unregister on destruction.

Qt destroys `QObject` children in the **same order they were added** (front-to-back over
`QObjectPrivate::children`), i.e. construction order, not reverse. Under current ordering,
`calendar_service_` and `weather_` — added early — are destroyed *before*
`activity_gate_manager_`, which is added late and destroyed last. During `~ShellApplication()`,
there is therefore a window where `activity_gate_manager_` is still alive and still holds
dangling pointers to already-destroyed `CalendarService`/`WeatherService` in `gates_`. If
`lid_monitor_`'s `lidStateChanged` signal is still queued (it uses a normal, non-blocking
connection) and fires during that window, `ActivityGateManager::onLidStateChanged()`
(`ActivityGateManager.cpp:7-15`) calls `gate->pauseActivity()`/`resumeActivity()` on a
use-after-free pointer.

**Corrected construction order**: construct `activity_gate_manager_` before any
`IActivityGate`-implementing service. Concretely, in `ShellApplication.cpp`'s member
initializer list, move `activity_gate_manager_(new ActivityGateManager(this))` to be
initialized before `calendar_service_`, `weather_`, and `suspend_inhibitor_service_` — the three
current/future `IActivityGate` implementers (`CalendarService.h:19`, presumably `WeatherService.h`,
`SuspendInhibitorService.h`, all inheriting `IActivityGate`). The member declaration order in
`ShellApplication.h` must be updated to match (C++ initializes members in declaration order
regardless of init-list order; `-Wreorder` will catch a mismatch). With this order,
`activity_gate_manager_` is added to `this`'s children list first, so it is destroyed **first**
during teardown — before any gate it references — eliminating the dangling-pointer window. The
`registerGate()` calls themselves stay in `startServices()` (all objects are alive at that point
regardless of construction order, so no need to move them); only the `new ActivityGateManager(this)`
call and its home in the member list move. `lid_monitor_` can stay where it is since it is not
an `IActivityGate` implementer and only ever calls into `activity_gate_manager_`, which will now
outlive it in the relevant sense (destroyed first, meaning it detaches its signal/slot connection
to `lid_monitor_` before `lid_monitor_` itself is torn down — safe either way since Qt
auto-disconnects on either side's destruction).

No other item in this phase touches construction/ownership graphs.

---

## 3. Interfaces / APIs (REQ-C-5 compliance)

No public method signatures change except:
- **F-2.6**: `SuspendInhibitorService::inhibitorModel()`, `inhibitorModelForQml()` gain `const`
  overloads or the existing methods become `const`-qualified where callers only read
  (`libs/holonight-services/src/SuspendInhibitorService.h:35-36`). Verify no caller mutates
  through the returned pointer in a way that requires non-const access before dropping the
  non-const overload; if any does, add a `const` overload alongside rather than replacing.
- **F-2.14 / CalendarCache.h**: `kRetainPastDays` / `kRetainFutureDays`
  (`CalendarCache.h:73-74`) move from `private` to `public` (or gain public static accessor
  methods) so `CalendarSyncManager.cpp:147` can reference them instead of duplicating `-30`/`180`
  as literals. This is a visibility widening, not a behavior change — no existing caller is
  affected.
- **New helper**: `ThemeConfigFile::schemeForMode(const QString& mode)` (static, new declaration
  in `apps/settings/src/ThemeConfigFile.h`) — additive, does not change any existing signature.
- **New constant**: `libs/holonight-surfaces/src/ShellConstants.h` gains `kScreenEdgeMargin`
  (moving the existing 3-file-duplicated `static constexpr int kScreenEdgeMargin = 8;` into the
  shared header) and a shadow-padding constant for F-2.3 — additive only.

All Category A `roleNames()` changes are pure implementation swaps (per-call `QHash` construction
→ function-local `static const QHash`); the public `roleNames()` signature and returned content
are unchanged.

No QML component's public property/signal surface changes (F-2.20/F-2.21 swap `MouseArea` for
`TapHandler`/`WheelHandler` internally; F-2.22 adds `readonly` to properties already never
reassigned, which is not observable from outside).

---

## 4. Key Decisions — Higher-Risk Items

### 4.1 F-2.7 — PulseAudio `pa_operation_unref()` leak

**Finding location, re-verified**: every `pa_operation*`-returning call in
`PulseAudioBackend.cpp` (`queryServerInfo`, `querySinks`, `querySources`, `querySinkInputs`,
`querySourceOutputs`, all four `handle*Event` methods, and all mutation methods like
`setDeviceVolume`) already correctly captures the return value and calls
`pulseSystem()->pa_operation_unref(operation)` when non-null. **The one exception** is
`Impl::onContextReady()` (`PulseAudioBackend.cpp:158-159`):

```cpp
pulseSystem()->pa_context_subscribe(context, static_cast<pa_subscription_mask_t>(subscription_mask), nullptr,
                                    nullptr);
```

The return value (a `pa_operation*`) is discarded entirely — not even assigned to a local. This
leaks the operation object for the lifetime of the context (once per successful connect/reconnect
cycle, so it accumulates across every reconnect during a flaky-network session).

**Resolution**: capture the return value and unref it exactly like every other call site in this
file:

```cpp
pa_operation* subscribe_op = pulseSystem()->pa_context_subscribe(
    context, static_cast<pa_subscription_mask_t>(subscription_mask), nullptr, nullptr);
if (subscribe_op != nullptr) {
  pulseSystem()->pa_operation_unref(subscribe_op);
}
```

**Why this is safe in PulseAudio's async callback model**: `pa_operation_unref()` only drops the
caller's reference to the operation handle; it does not cancel the operation or free the
underlying async request. The subscribe call passes `nullptr` for its completion callback here
(no callback attached, `PA_SUBSCRIPTION_...` just arms the `subscribeCallback` set separately via
`pa_context_set_subscribe_callback` in `connectNewContext`), so there is nothing pending on this
operation to race — unref is safe to call immediately after the `pa_context_subscribe()` call
returns, on the same thread, exactly as the file's own `queryServerInfo()`/`querySinks()`/etc.
already do for their (also nullptr-callback-adjacent or callback-bearing) operations. No lock
change is needed since `onContextReady()` runs on PulseAudio's mainloop thread inside the existing
callback context, same as all sibling methods that already unref correctly.

**No test needed for this leak itself** (a `pa_operation` leak has no user-observable behavior —
it's a resource accounting fix); REQ-C-4 lists F-2.7 as needing *manual verification* rather than
an automated test because there is no practical way to assert non-leak via `ctest` against the
real `libpulse`. Manual verification: run the shell with a PulseAudio mock/real backend through
several forced reconnects (`PulseAudioBackend::setReconnectBackoffScheduleForTests` + killing/
restarting `pulseaudio`) and confirm no `pa_operation` warnings appear in verbose PA client logs,
plus confirm existing `PulseAudioBackend` unit tests (if any exist — check
`tests/test_pulse_audio_backend.cpp` at implementation time) still pass unchanged.

### 4.2 F-2.8 — `AudioDeviceModel` / `AudioStreamModel` `applyRemove()` divergence

**Re-verified against current tree**: `AudioDeviceModel::applyRemove()`
(`libs/holonight-services/src/audio/AudioDeviceModel.cpp:74-83`) and
`AudioStreamModel::applyRemove()` (`.../AudioStreamModel.cpp:77-86`) are **already textually
identical** — both do a linear scan by id, `beginRemoveRows`/`removeAt`/`endRemoveRows` on match,
and silently no-op (no signal, no warning) if the id is not found. `git log` on both files shows
only the single monorepo-layout-move commit (`8613d32`), i.e. no divergence-introducing commit
has landed since; this most likely means the divergence described in the original audit was
already incidentally resolved during Phase 4's PulseAudio/audio-model work (see
`project_poc_remediation_phase4.md`: "sink/source signal split") before this phase started.

The one place I found real (but different) asymmetry is one layer up, in
`AudioService.cpp:171-193`: `onSinkRemoved()` additionally resets `default_output_id_` and emits
`defaultOutputIdChanged()` when the removed device was the default output
(`AudioService.cpp:173-176`); `onSourceRemoved()` has no equivalent because
`AudioService` tracks only `default_output_id_` — there is no `default_input_id_` member or
`defaultInputId` property at all (confirmed: `grep` for `default_input_id_` in
`AudioService.h`/`.cpp` returns nothing). This is a pre-existing, larger design gap (no default
capture-device tracking), not the `applyRemove()` divergence SPEC describes, and is out of scope
for a Low-severity mechanical-cleanup phase — flagging it here for visibility but **not**
including it in Phase 6's change set.

**Resolution**: treat F-2.8 as **already resolved** for the specific `applyRemove()` bodies named
in the finding. Canonical behavior (both models, confirmed identical): linear scan, silent no-op
on not-found. Rationale for keeping silent-no-op as canonical (rather than adding a warning): a
"remove for unknown id" is an expected race in the subscribe-driven model (a `REMOVE` event can
arrive after the model has already been cleared via `clear()`, e.g. during a reconnect), so
logging it would produce routine warning-log noise, not a real problem signal.

**Action for Phase 6**: no code change to `applyRemove()` itself. Add one regression test (new,
since no `AudioDeviceModel`/`AudioStreamModel` unit tests currently exist under `tests/`) that
locks in the invariant: constructing each model, adding a device/stream, calling `applyRemove()`
with a non-existent id (assert row count unchanged, no `rowsRemoved` signal), then with the real
id (assert removal). Running the same test body against both models (e.g. a small templated test
fixture or two near-identical `TEST_F`s) documents that they are intentionally kept consistent
going forward, satisfying the SPEC's "make both consistent" instruction even though no functional
change is required today.

### 4.3 F-2.12 — `Kind::NetworkError` dead code

**Re-verified**: `SyncError::Kind` (`libs/holonight-services/src/calendar/CalendarTypes.h:54-59`)
has four variants: `ConnectError`, `NetworkError`, `ParseError`, `StorageError`.
`CalendarService::onSyncError()` (`libs/holonight-services/src/CalendarService.cpp:172-181`)
already has a real consumer for `Kind::NetworkError` — it maps to `UpcomingState::Offline`
(distinct from `UpcomingState::ConnectError` for the other three kinds). But **no producer ever
constructs a `SyncError` with `Kind::NetworkError`**: `CalDavProvider.cpp:156-158`'s
`makeConnectError()` and `IcsProvider.cpp:13-14`'s equivalent (unnamed, grep-matched) both
hardcode `SyncError::Kind::ConnectError` unconditionally.

Tracing further upstream: both providers' HTTP path funnels through
`HttpSyncClient::runAndAwait()` (`libs/holonight-services/src/calendar/HttpSyncClient.cpp:32-58`),
which **does** have the raw signal needed to distinguish the two cases — `reply->error()`
(`QNetworkReply::NetworkError`) — but the local `makeError()` helper
(`HttpSyncClient.cpp:13-15`) throws that information away and always returns
`Kind::ConnectError`, for both the timeout path (line 45) and the generic-failure path (line 52).

This is **not** truly unreachable/dead in the "no real distinction exists" sense — `QNetworkReply`
genuinely separates network-layer failures (`ConnectionRefusedError`, `RemoteHostClosedError`,
`HostNotFoundError`, `TimeoutError`, `TemporaryNetworkFailureError`, `NetworkSessionFailedError`,
`UnknownNetworkError`, all in Qt's documented "1–99, valid on all connections" range) from
content/protocol/server failures (auth, 404, 500, malformed response, all ≥ 200). The shell's own
local timeout synthesis (`runAndAwait`'s `reply->isRunning()` branch, line 42-46) is unconditionally
a network-layer failure by construction.

**Resolution: wire it up (preferred, per SPEC), not remove.**

1. `HttpSyncClient.cpp`'s `makeError()` gains a `SyncError::Kind kind` parameter (default
   `Kind::ConnectError` to keep existing call sites that don't care compiling unchanged, though in
   practice both call sites in `runAndAwait` will pass an explicit kind).
2. Classify with a small local helper:
   ```cpp
   bool isNetworkLayerError(QNetworkReply::NetworkError error) {
     return error >= QNetworkReply::ConnectionRefusedError &&
            error <= QNetworkReply::UnknownNetworkError;  // Qt's documented network-layer range
   }
   ```
3. Timeout branch (line 45): always `Kind::NetworkError` (a timed-out request is a connectivity
   failure by definition here — no reply was ever obtained).
4. Generic failure branch (line 48-52): `isNetworkLayerError(reply->error())` picks
   `Kind::NetworkError`; everything else (auth challenges, 4xx/5xx HTTP status, content errors)
   keeps `Kind::ConnectError`.
5. `CalDavProvider.cpp` and `IcsProvider.cpp` need no change — they only construct
   `Kind::ConnectError` for their *own* local failures (e.g. keyring lookup failure at
   `CalDavProvider.cpp:256/275`, which is correctly `ConnectError`, not network-related); the HTTP
   path already routes through `HttpSyncClient` and inherits whatever `Kind` it returns via
   `std::unexpected(result.error())` pass-through (confirmed at `CalDavProvider.cpp:208,240,267`).

**Observable effect**: when the shell is genuinely offline (no WiFi, DNS failure, connection
refused, request timeout), `CalendarService::upcomingState` will now surface
`UpcomingState::Offline` instead of the generic `UpcomingState::ConnectError` it currently always
shows — this is the intended, previously-dead UI distinction becoming live. HTTP-level failures
(bad credentials, server error) continue to show `ConnectError`, unchanged.

**New test required** (REQ-C-4): a test for `HttpSyncClient` (or, if `HttpSyncClient` isn't
currently unit-testable in isolation because it owns a real `QNetworkAccessManager`, a test at the
`CalDavProvider`/`IcsProvider` level using their existing fake-transport test seam if one exists,
otherwise add a minimal injectable-transport seam) asserting: a request that fails with
`QNetworkReply::HostNotFoundError` (or `ConnectionRefusedError`) yields `Kind::NetworkError`; a
request that completes with HTTP 401/404/500 yields `Kind::ConnectError`; a request that times out
yields `Kind::NetworkError`. Also extend `CalendarService`'s existing `onSyncError` test coverage
(if present) to confirm `Kind::NetworkError` now actually drives `UpcomingState::Offline` end to
end from a real provider failure, not just from a hand-constructed `SyncError`.

### 4.4 F-2.17 — `WeatherIconBridge` moon-phase boundary literals

**Re-verified**: `WeatherIconBridge.cpp`'s `variantToMoonPhase()` (lines 12-41) and
`MoonPhaseCalculator.cpp`'s `normalizedPhaseIndex()` (lines 20-27) are **two structurally
different algorithms operating on different domains**, not one duplicating the other's constant:

- `MoonPhaseCalculator` computes a moon phase from a **date**: elapsed days since a fixed
  astronomical reference new moon, modulo the synodic month (`kSynodicMonthDays = 29.530589`),
  then `floor`-divided into 8 **equal-width** buckets (each exactly 1/8 of the cycle, i.e. 12.5%
  wide, boundaries at 0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875 of the cycle).
- `WeatherIconBridge::variantToMoonPhase()` classifies an **externally supplied 0–1 fraction**
  (from the weather API's own `moon_phase` field, passed through the `QVariant moon_phase`
  parameter) using **unequal, hand-tuned bucket widths**: narrow ~2%-wide windows centered on the
  four "named" phases (New ≈0/1, First Quarter ≈0.25, Full ≈0.5, Last Quarter ≈0.75: e.g.
  `0.24 ≤ v ≤ 0.26` → `FirstQuarter`) and wide ~23%-wide windows for the four "in-between" phases
  (Waxing/Waning Crescent/Gibbous). This is deliberately *not* a uniform eighths split — it snaps
  values near an exact quarter to the crisp quarter-phase icon and only falls back to a
  crescent/gibbous icon well away from those points.

Because the two algorithms solve different problems (date→phase vs. externally-given
fraction→phase) with genuinely different bucket geometry, **reusing `MoonPhaseCalculator`'s
constants would change `WeatherIconBridge`'s output** (it would replace the ~2%-wide named-phase
windows with ~12.5%-wide ones), which directly violates SPEC's "STRICT no-output-change"
constraint. The correct reading of SPEC's "extract to named constants / reuse
`MoonPhaseCalculator`'s constants" is therefore: **extract to named constants; do not merge the
two algorithms.**

**Resolution**: in `WeatherIconBridge.cpp`'s anonymous namespace, replace the seven literal
boundaries with named `constexpr double` constants that document what each represents, e.g.:

```cpp
// variantToMoonPhase() classifies an external 0..1 phase fraction (weather-API convention: 0/1 =
// new moon, 0.5 = full). These are hand-tuned, NOT a uniform eighths split like
// MoonPhaseCalculator's date-based bucketing (different input domain — see class comment) — the
// four named quarter/full/new phases get a narrow ~2%-wide window so near-exact values snap to
// the crisp icon; the four in-between phases get the remaining ~23%-wide bands.
constexpr double kMoonNewBandHalfWidth = 0.01;       // New: v < 0.01 || v > 0.99
constexpr double kMoonFirstQuarterLow = 0.24;
constexpr double kMoonFirstQuarterHigh = 0.26;
constexpr double kMoonFullLow = 0.49;
constexpr double kMoonFullHigh = 0.51;
constexpr double kMoonLastQuarterLow = 0.74;
constexpr double kMoonLastQuarterHigh = 0.76;
```

and rewrite each `if`/`else if` branch to reference these instead of the bare literals, with
**identical comparison operators** (`<`, `<=`, `>=`, `>`) preserved exactly as today so floating
point boundary behavior is bit-for-bit unchanged (e.g. `val >= 0.01 && val < 0.24` stays
`val >= kMoonNewBandHalfWidth && val < kMoonFirstQuarterLow`, not silently normalized to `<=`).

**Test matrix (mandatory per SPEC)**: add a unit test (or QML/C++ test depending on where
`WeatherIconBridge` is currently covered) enumerating representative and boundary inputs for every
branch: `{-0.01→New(clamped/invalid path), 0.0, 0.009, 0.01, 0.239, 0.24, 0.25, 0.26, 0.261, 0.489,
0.49, 0.5, 0.51, 0.511, 0.739, 0.74, 0.75, 0.76, 0.761, 0.99, 0.991, 1.0}` and assert the exact
same `MoonPhase` enum value as today's implementation for each. This should be written by first
capturing the *current* (pre-change) output for each input as the expected value, then verifying
the constants-only refactor reproduces it exactly — this is the mechanism that proves
"STRICT no-output-change."

---

## 5. Constant / Header Consolidation Design

### 5.1 `ShellConstants.h` (existing — extend, do not create)
Location: `libs/holonight-surfaces/src/ShellConstants.h` (already exists, holds `kBarHeight`,
`kSidebar*` geometry constants). Add:

```cpp
// Distance kept between a popup/menu surface and the screen edge, applied uniformly so no
// surface can render flush against (or past) an output boundary.
inline constexpr int kScreenEdgeMargin = 8;

// Shadow padding reserved around TrayMenuSurface content so MultiEffect's drop shadow has room
// to render without being clipped by the surface bounds. (exact value confirmed at
// implementation time from the two current duplicated definitions in TrayMenuSurface.cpp)
inline constexpr int kTrayMenuShadowPadding = /* value from current duplicated sites */;
```

Consumers to update: `TooltipSurface.cpp:20` (remove local `kScreenEdgeMargin`, include
`ShellConstants.h`, use `kScreenEdgeMargin`), `TrayMenuSurface.cpp:20` (same, plus its
shadow-padding duplication per F-2.3), `StatusPopupGeometry.cpp:6` (same). All three files are in
`libs/holonight-surfaces/src/`, same directory as `ShellConstants.h`, so this is a same-directory
include, no new CMake target dependency.

### 5.2 Calendar retention window
No new header — reuse the constants that already exist as `private` members of `CalendarCache`
(`CalendarCache.h:73-74`, `kRetainPastDays{30}`, `kRetainFutureDays{180}`). Move them to the
`public:` section of `CalendarCache` (or add public `static constexpr` accessors alongside the
private originals, whichever keeps `CalendarCache`'s public surface tidier — prefer just widening
visibility since these are already named, documented, and correctly scoped). Update
`CalendarSyncManager.cpp:147` to use `CalendarCache::kRetainPastDays`/`kRetainFutureDays` instead
of `-30`/`180` literals. This keeps a single canonical definition co-located with the class that
owns the retention *behavior* (pruning), which the consumer (`CalendarSyncManager`, which decides
the sync *query* range) then reads — arguably the more natural direction of dependency than
inventing a third shared-constants file for two call sites in two already-related classes.

### 5.3 Mode → scheme mapping helper
New static method on the existing `ThemeConfigFile` class (not a new file — `ThemeConfigFile.h`/
`.cpp` already own `normalizeMode`, `normalizeScheme`, `modeForScheme`, `defaultScheme`, the
natural home for the inverse mapping):

```cpp
// apps/settings/src/ThemeConfigFile.h
[[nodiscard]] static QString schemeForMode(const QString& mode);

// apps/settings/src/ThemeConfigFile.cpp
QString ThemeConfigFile::schemeForMode(const QString& mode) {
  return normalizeMode(mode) == QLatin1String("light") ? QStringLiteral("holonight-light")
                                                        : defaultScheme();
}
```

Confirmed call sites to replace (6 total, matching SPEC's "5-6x"):
`ThemeConfigFile.cpp:59` (inside `loadAppearance()`, uses `legacy_mode` local — substitute
`schemeForMode(legacy_mode)` after inlining, or call `schemeForMode` directly on the raw stored
value since it already normalizes internally), `ThemeConfigFile.cpp:102` (`writeMode()`),
`ConfigFileService.cpp:33`, `SettingsEditModel.cpp:28-29`, `:62-63`, `:122`. All six currently
resolve to exactly `normalizeMode(x) == "light" ? "holonight-light" : defaultScheme()` — verified
textually identical across all six sites, so this is a safe mechanical replacement with zero
behavior change. `apps/settings` already depends on `ThemeConfigFile` from `ConfigFileService.cpp`
and `SettingsEditModel.cpp` (both already call `ThemeConfigFile::normalizeMode`), so no new
include-dependency is introduced.

### 5.4 Category A `roleNames()` pattern (applies uniformly to all 6 items)
No shared file. Each class's `.cpp` changes from:
```cpp
QHash<int, QByteArray> SomeModel::roleNames() const {
  return {
      {RoleA, "roleA"},
      ...
  };
}
```
to:
```cpp
QHash<int, QByteArray> SomeModel::roleNames() const {
  static const QHash<int, QByteArray> kRoles{
      {RoleA, "roleA"},
      ...
  };
  return kRoles;
}
```
Function-local `static const`, C++11 magic-statics guarantee thread-safe one-time init. No class
header changes. Applies verbatim to all 11 sites across the 6 REQ-F-1.x items (`WorkspaceModel`,
`TrayModel`, `DbusMenuItem`, `WifiNetworkModel`, `AudioDeviceModel`, `AudioStreamModel`,
`LauncherModel`, `NotificationRuleModel`, `NotificationService`, `CalendarEventModel`,
`FontListModel`).

---

## 6. Alternatives Considered

- **Shared `roleNames()` mixin/base class** (e.g. a `RoleNameCache<T>` CRTP helper or a
  `QHash`-returning template mixin): rejected per prior user decision recorded in SPEC. Rationale
  confirmed by inspection — the 11 role hashes vary in size (4 to 29 entries) and enum type
  (`Role`, `Roles`, unscoped ints in a couple of older files), so a generic mixin would need either
  a virtual-dispatch indirection (defeats the point of a `static const` fast path) or a
  non-trivial template contract per class; the per-class `static const` is simpler, more
  discoverable at the call site, and is only ~3 lines of mechanical change per file.
- **Centralize all magic numbers in one file** (a single project-wide `Constants.h`): rejected.
  Only genuinely *duplicated-across-files* constants (`kScreenEdgeMargin`, tray shadow padding,
  the calendar retention window) get hoisted to a shared location; single-use magic numbers
  (`HyprlandIpcClient`'s timeout/backoff/buffer sizes, `LauncherService`'s 500ms debounce) become
  named `static const` constants **at their existing use site**, per SPEC's explicit unit-by-unit
  grouping. A single mega-constants-file would create an artificial coupling between unrelated
  subsystems (audio timeout constants next to QML layout constants) for no benefit — nothing
  reads across those boundaries.
- **F-2.12: remove `Kind::NetworkError` instead of wiring it up**: rejected — see §4.3; a genuine,
  currently-discarded signal (`QNetworkReply::error()`) exists upstream, so removal would delete a
  real, useful UI distinction (offline vs. server-rejected) rather than dead code.
- **F-2.17: unify with `MoonPhaseCalculator`**: rejected — see §4.4; the two functions solve
  different problems (external fraction vs. date-derived) with deliberately different bucket
  geometry; unifying would violate the SPEC's explicit zero-output-change constraint.
- **F-2.8: add a shared `applyRemove` helper across the two model classes**: considered (a free
  function templated on the id-comparison + begin/endRemoveRows calls) since the bodies are now
  identical, but rejected for this phase — the two classes hold different element types
  (`AudioDevice` vs `AudioStream`) and the existing project convention (per §6 above, already
  rejecting a `roleNames` mixin for the same reason) is to keep per-class model logic explicit
  rather than introduce a generic templated base. If a third model ever needs the same pattern,
  revisit.

---

## 7. Known Risks

- **F-2.7 (PulseAudio unref)**: low risk — mechanical addition matching an established in-file
  pattern — but only verifiable by manual reconnect-cycle testing against real `libpulse` (no
  automated leak-detection in this repo's test suite); regressions would only surface as slow
  memory growth over many reconnects, not a crash, so `task test` passing is not sufficient
  evidence of correctness here.
- **F-2.8 (audio model consistency)**: risk that the original audit's divergence was real at the
  time it ran and has since been silently fixed by unrelated Phase 4 work, meaning the "decision"
  documented here (§4.2) is really just describing already-shipped behavior. Low risk either way
  since the resolution is "add a locking test," not a functional change — worst case is a
  slightly redundant test.
- **F-2.12 (NetworkError wire-up)**: medium risk — this is the one item in Category B that
  changes real user-facing state (`UpcomingState::Offline` becomes reachable for the first time).
  Risk: the `QNetworkReply::NetworkError` range boundary (`ConnectionRefusedError` through
  `UnknownNetworkError`) must be double-checked against the exact Qt version in use at
  implementation time (enum values are stable across Qt6 but worth a doc lookup, not just
  memory) before hardcoding the range check.
- **F-2.13 (construction reordering)**: medium risk — reordering a 15+ member initializer list in
  `ShellApplication.cpp` touches the file every other Phase-6 QML-adjacent change does not; a
  transposition error (declaration order in `.h` not matching init-list order in `.cpp`) fails
  the build immediately under `-Wreorder` (treated as error in this project's warning
  configuration per REQ-C-1), so this is self-checking at build time, but the *runtime* proof
  (that the dangling-pointer window is actually closed) is not something `task test` alone can
  demonstrate — it requires either a targeted destruction-order unit test (constructing a
  minimal `ActivityGateManager` + fake gate + fake `LidStateMonitor` harness and asserting no
  crash under ASan/valgrind on teardown with a queued signal) or manual verification with a
  sanitizer build.
- **F-2.17 (moon-phase constants)**: the primary risk is a transcription error while copying the
  seven literals into named constants (e.g. `<=` vs `<` on a boundary) that silently changes
  which icon renders for values exactly on a boundary (0.24, 0.26, 0.49, 0.51, 0.74, 0.76) —
  mitigated by the mandatory before/after test matrix in §4.4, but only if that matrix is
  actually run against the pre-change code first to capture true "before" values, not
  hand-derived from reading the branches.
- **U-08/U-09/U-10 QML items**: general risk of subtle visual regression (element z-order,
  padding, hit-testing area) even for "obviously safe" changes like `MouseArea`→`TapHandler`/
  `WheelHandler` swaps (differing default `acceptedButtons`/propagation semantics) or dead-`id`
  removal (a qmllint pass is necessary but not sufficient — an `id` referenced only from a
  now-also-being-deleted binding could look "dead" via static grep but its removal changes
  binding evaluation order in edge cases). All of U-08/U-09/U-10 require the manual `task run`
  verification SPEC already calls for; no amount of `qml-lint`/build passing substitutes for it.
- **General**: 29 of 36 findings touch files with **no existing unit test** covering the
  changed function (e.g. `AudioDeviceModel`, `AudioStreamModel`, `InhibitorModel`,
  `HyprlandIpcClient`). `task test` passing after these changes proves "did not break what was
  already tested," not "is correct" — the new tests called out in §8 close some of this gap, but
  purely mechanical items (roleNames hoisting, magic-number naming) remain exercised only by
  compilation + manual/lint checks.

---

## 8. Testing Strategy

Maps each REQ-C-4 behavioral item to its verification path.

| Item | Change | Test |
|---|---|---|
| F-2.5 | `InhibitorModel::setEntries()` skips reset when list unchanged | New test: call `setEntries()` twice with an equal `QList<InhibitorEntry>`; assert no `rowsAboutToBeReset`/`modelReset` signal (via `QSignalSpy`) on the second call, and `countChanged()` not re-emitted. Requires `InhibitorEntry` to support `operator==` (add if missing — check `InhibitorEntry` struct definition at implementation time). |
| F-2.7 | `pa_context_subscribe()` unref | Manual verification only (per SPEC) — reconnect-cycle soak test against live/mock PulseAudio, watch for PA client-side operation-leak warnings; existing `PulseAudioBackend` tests (if any) must still pass unchanged. |
| F-2.8 | `applyRemove()` consistency (already consistent — see §4.2) | New locking test: shared-shape `TEST_F` (or two near-identical tests) against `AudioDeviceModel` and `AudioStreamModel` asserting not-found id is a silent no-op and existing id is removed with correct signals. |
| F-2.9 | `updateVisibleWifiNetworks()` early-return reorder | Existing `NetworkManagerBackend`/`QtNetworkManagerBackend` tests (if present) extended with a case: no wireless device present → saved-connection enumeration is skipped entirely (assert via a call-count/spy on whatever seam exposes `savedWifiConnections()`, or assert the returned state's network list is empty without touching the saved-connections path). |
| F-2.12 | `Kind::NetworkError` wire-up in `HttpSyncClient` | New test(s): network-layer `QNetworkReply::NetworkError` values (`HostNotFoundError`, `ConnectionRefusedError`, timeout) → `Kind::NetworkError`; HTTP-status/content failures → `Kind::ConnectError`. Extend `CalendarService` sync-error test coverage to confirm `Kind::NetworkError` drives `UpcomingState::Offline`. |
| F-2.14 | `CalendarCache::open()` removeDatabase-while-handle-alive reorder | New test: force `initOrMigrate()` (or `database.open()`) failure and assert `CalendarCache::open()` returns `false` cleanly with no Qt SQL warning about a connection still in use — capture via `QTest::ignoreMessage`-equivalent or log-category interception per this repo's established pattern (note CLAUDE.md caveat: `QTest::ignoreMessage` is unenforced in this repo's gtest harness — use whatever mechanism the existing `CalendarCache` tests already use for warning assertions, or fall back to asserting `QSqlDatabase::contains(connection_name)` is false after the failure path, which is directly observable regardless of log capture). |
| F-2.17 | Moon-phase constant extraction | New test matrix (§4.4) — 20+ input/expected-`MoonPhase` pairs spanning every branch and boundary, captured from current behavior before refactor, re-verified after. |
| F-2.13 | `ShellApplication` construction reordering | Primarily a build-time guarantee (`-Wreorder`); for runtime proof, a targeted test at the `ActivityGateManager` level: construct manager before a fake `IActivityGate` gate, destroy both in that order, confirm no crash/UB — best done under ASan if this project's `task test` config supports it (verify `task configure-tests` sanitizer flags at implementation time), otherwise document as manual-verify via `task compositor-smoke-check` plus a lid-close/open cycle during shutdown. |

All other 29 items (Category A roleNames, U-01/U-02/U-05/U-06/U-11 constant/doc changes, U-08/
U-09/U-10 QML mechanical fixes except F-2.24, F-2.15 range-for copies) are covered by
REQ-C-1/C-2/C-3 (build, format, tidy, qml-lint) plus the existing test suite continuing to pass
unchanged (REQ-C-4's "shall NOT break" clause) — no new test is required because no new branch or
state transition is introduced.

F-2.24 (`WifiNetworkDelegate.qml` Canvas repaint on `connectedChanged`) is event-driven and
explicitly called out in SPEC as needing manual verification (toggle a WiFi network's connected
state while its delegate is visible in the sidebar popup, confirm the lock icon repaints without
requiring a scroll/re-layout to force it) — no practical automated QML test covers `Canvas`
repaint timing in this harness.
