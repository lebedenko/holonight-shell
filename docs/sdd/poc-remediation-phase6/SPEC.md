# Phase 6 — Mechanical & Background Cleanup

**Status**: Specification Phase (Stage 1 of SDD)

**Scope**: 36 confirmed Low-severity findings (>80/100 confidence) from the POC readiness audit, excluding Investigation Targets (60–79%).

**Objective**: Eliminate code-quality and correctness deficiencies without introducing new features or observable behavior changes (except where a finding explicitly describes a bug).

**Change Categories**:
- **Category A (6 items)**: Hoist `QHash<int, QByteArray> roleNames()` caching from per-call construction to `static const` per class.
- **Category B (29 items)**: Low-severity confirmed findings grouped by subsystem.
- **Constraints (REQ-C)**: Project-wide build, test, lint, and behavioral integrity requirements.

---

## Project Constraints

### REQ-C-1 (Ubiquitous)
The system shall compile without error or warning under the project's C++23/Qt6 CMake build configuration (`task build`, `task configure-tests` + `task test`).

**Acceptance Criterion**: `task build` and `task test` both exit with status 0; no new compilation or link warnings introduced.

### REQ-C-2 (Ubiquitous)
The system shall pass `task format-check` and `task tidy` on all files modified in this phase.

**Acceptance Criterion**: After fixes, `task format-check` and `task tidy` report no new failures in modified files. (Pre-existing failures in unmodified files are out of scope; verify via `git diff` that only your changed files are tested.)

### REQ-C-3 (Ubiquitous)
The system shall pass `task qml-lint` without new errors or warnings in modified QML files.

**Acceptance Criterion**: After fixes, `task qml-lint` reports no new failures in QML files touched in this phase.

### REQ-C-4 (Unwanted behaviour)
If a fix changes observable behavior (e.g., ordering bugs, resource leaks, dead-code removal in a live code path), the system shall NOT break the existing test suite (`task test`).

**Acceptance Criterion**: All existing unit/integration tests continue to pass. New tests are added where a behavioral change is introduced (e.g., F-02, F-07, F-12, F-13 in Category B).

### REQ-C-5 (Ubiquitous)
The system shall NOT introduce new public API changes or backward-compatibility breaks beyond what existing fixes already require.

**Acceptance Criterion**: Changes are refactor-only, scoped to internal implementation or simple constant/const-qualification additions. No public function signatures changed except where required by a fix (e.g., adding `const` to accessor methods per F-03).

---

## Category A: QAbstractListModel roleNames() Caching

### REQ-F-1.1 (Ubiquitous) — WorkspaceModel
The system shall construct the `WorkspaceModel::roleNames()` return value as a `static const` hash exactly once per process, not on every method invocation.

**Reference**: `libs/holonight-core/src/WorkspaceModel.cpp` (4-entry role hash).

**Acceptance Criterion**:
- `WorkspaceModel::roleNames()` references a `static const QHash<int, QByteArray>` declared at class or function scope.
- Behavior unchanged: returned hash contents match the previous implementation exactly (names, codes, order).
- Unit test exists or is extended to assert role names are unchanged before/after refactor (if no existing test, add one to verify the static hash is correctly populated).

### REQ-F-1.2 (Ubiquitous) — TrayModel & DbusMenuItem
The system shall construct `TrayModel::roleNames()` and `DbusMenuItem::roleNames()` return values as `static const` hashes exactly once per process, not on every invocation.

**Reference**:
- `libs/holonight-surfaces/src/.../TrayModel.cpp` (13-entry hash).
- `libs/holonight-surfaces/src/.../DbusMenuItem.cpp` (9-entry hash).

**Acceptance Criterion**:
- Both classes define their respective `roleNames()` return values as `static const QHash<int, QByteArray>`.
- Behavior unchanged: returned hash contents and order match the previous implementation exactly.
- Existing/new unit tests verify role names are preserved.
- (Note: `DbusMenuModel` instances created per submenu; caching here reduces redundant allocations across the tray menu's delegate hierarchy.)

### REQ-F-1.3 (Ubiquitous) — WifiNetworkModel, AudioDeviceModel, AudioStreamModel
The system shall construct `roleNames()` return values as `static const` hashes in `WifiNetworkModel`, `AudioDeviceModel`, and `AudioStreamModel`, eliminating per-call heap allocation.

**Reference**:
- `libs/holonight-services/src/.../WifiNetworkModel.cpp:77`.
- `libs/holonight-services/src/.../AudioDeviceModel.cpp:35`.
- `libs/holonight-services/src/.../AudioStreamModel.cpp:37`.

**Acceptance Criterion**:
- All three classes implement `roleNames()` using `static const QHash<int, QByteArray>`.
- Behavior unchanged: role names, codes, and order match previous implementation exactly.
- Unit/integration tests for each model pass and verify role names are preserved.

### REQ-F-1.4 (Ubiquitous) — LauncherModel, NotificationRuleModel, NotificationService
The system shall construct `roleNames()` return values as `static const` hashes in `LauncherModel`, `NotificationRuleModel`, and `NotificationService`, eliminating per-call heap allocation.

**Reference**:
- `libs/holonight-services/src/.../LauncherModel.cpp:125-143`.
- `libs/holonight-services/src/.../NotificationRuleModel.cpp:189-194`.
- `libs/holonight-services/src/.../NotificationService.cpp:185-192`.

**Acceptance Criterion**:
- All three classes implement `roleNames()` using `static const QHash<int, QByteArray>`.
- Behavior unchanged: role names, codes, and order match previous implementation exactly.
- (Note: `NotificationService` wrapped by `QSortFilterProxyModel` per monitor; caching eliminates redundant calls during proxy role delegation.)
- Unit/integration tests pass and verify role names are preserved.

### REQ-F-1.5 (Ubiquitous) — CalendarEventModel
The system shall construct `CalendarEventModel::roleNames()` return value as a `static const` hash exactly once per process, not on every invocation.

**Reference**: `libs/holonight-services/src/.../CalendarEventModel.cpp:85-118` (29-entry hash; largest instance).

**Acceptance Criterion**:
- `CalendarEventModel::roleNames()` references a `static const QHash<int, QByteArray>` (at class or function scope).
- Behavior unchanged: returned hash contents and order match the previous implementation exactly (all 29 entries preserved).
- Unit test exists or is extended to verify role names are unchanged.

### REQ-F-1.6 (Ubiquitous) — FontListModel
The system shall construct `FontListModel::roleNames()` return value as a `static const` hash exactly once per process, not on every invocation.

**Reference**: `apps/settings/.../FontListModel.cpp:49` (single-entry hash).

**Acceptance Criterion**:
- `FontListModel::roleNames()` references a `static const QHash<int, QByteArray>`.
- Behavior unchanged: returned hash contents match the previous implementation exactly.
- Unit tests pass (or new test added to verify role names are preserved).

---

## Category B: Low-Severity Confirmed Findings

### Unit U-01: Core / Platform / Config

#### REQ-F-2.1 (Ubiquitous) — HyprlandIpc Regex Compilation
The system shall NOT compile a `QRegularExpression` on every call to `HyprlandIpc::keyboardLayoutCode()`.

**Reference**: `libs/holonight-core/src/HyprlandIpc.cpp:123` (or nearby; exact line may have drifted).

**Acceptance Criterion**:
- `keyboardLayoutCode()` references a `static const QRegularExpression` for the keyboard layout pattern, constructed exactly once at program start.
- Behavior unchanged: the regex pattern and matching logic produce identical `QString` results as before.
- Unit test for `keyboardLayoutCode()` passes (if existing test exists, it continues to pass).

#### REQ-F-2.2 (Ubiquitous) — HyprlandIpcClient Magic Numbers
The system shall replace unnamed magic numbers in `HyprlandIpcClient` (connect timeout, backoff interval, buffer size) with named `static const` constants.

**Reference**: `libs/holonight-core/src/HyprlandIpcClient.h` and `.cpp` (exact line numbers depend on layout).

**Acceptance Criterion**:
- All numeric literals for connect timeout, backoff, and buffer size are named as `static const` (e.g., `kConnectTimeoutMs`, `kBackoffIntervalMs`, `kBufferSize`).
- Behavior unchanged: timeouts and backoff behavior are identical before/after refactor.
- Build passes; unit/integration tests for Hyprland IPC behavior pass.

---

### Unit U-02: Surfaces / Layer-Shell

#### REQ-F-2.3 (Ubiquitous) — TrayMenuSurface Shadow Padding Duplication
The system shall consolidate the shadow-padding surface-sizing math in `TrayMenuSurface` into a single named constant.

**Reference**: `libs/holonight-surfaces/src/.../TrayMenuSurface.cpp` (shadow padding calculation at two or more sites, e.g., lines 152–164 and 306–316, or similar).

**Acceptance Criterion**:
- Shadow padding value (`kShadowPadding` or equivalent) is defined exactly once (class-level `static const` or function-level `static const`).
- Both surface-sizing code paths reference this single constant.
- Behavior unchanged: surface dimensions and shadow layout are identical before/after refactor.
- Manual verification: `task run` with live Wayland; tray menu popup shadow appearance unchanged.

#### REQ-F-2.4 (Ubiquitous) — Screen Edge Margin Consolidation
The system shall hoist `kScreenEdgeMargin` from three independent locations into the shared `ShellConstants.h`.

**Reference**:
- `libs/holonight-surfaces/src/.../TooltipSurface.cpp:20`.
- `libs/holonight-surfaces/src/.../TrayMenuSurface.cpp:20`.
- `libs/holonight-surfaces/src/.../StatusPopupGeometry.cpp:6`.

**Acceptance Criterion**:
- `ShellConstants.h` (or equivalent shared header under `libs/holonight-surfaces/include/`) defines `inline constexpr` or `static const` `kScreenEdgeMargin`.
- All three `.cpp` files remove their local definition and `#include` the shared header instead.
- Behavior unchanged: screen-edge margin calculations and surface positioning are identical before/after refactor.
- Build passes; no new `#include` circular dependencies.

---

### Unit U-03: Power / Idle / Brightness Services

#### REQ-F-2.5 (Unwanted behaviour) — InhibitorModel Unnecessary Reset
If `InhibitorModel::setEntries()` is called with a list equal to the current list, the system shall NOT call `beginResetModel()`/`endResetModel()`.

**Reference**: `libs/holonight-services/src/.../InhibitorModel.cpp:34-42`.

**Acceptance Criterion**:
- `setEntries()` compares the new list to the existing entries before signaling a reset.
- If lists are equal (by value), the method returns early without calling `begin/endResetModel()`.
- Behavior changed only in the no-op case: unnecessary model-reset signals are eliminated; no other observable change.
- Unit test exists or is added: calling `setEntries()` with identical data does NOT emit `modelReset()` signal.

#### REQ-F-2.6 (Ubiquitous) — SuspendInhibitorService Const Accessors
The system shall mark accessor methods in `SuspendInhibitorService` that do not mutate state as `const`.

**Reference**: `libs/holonight-services/src/.../SuspendInhibitorService.h:35-36` (e.g., `inhibitorModelForQml()` and siblings).

**Acceptance Criterion**:
- All accessor methods (getters) that return model pointers or cached state are marked `const`.
- Behavior unchanged: no method behavior or return values change; this is a signature refinement.
- Build passes; no new compilation errors (const-safety may catch new correct usages elsewhere).

---

### Unit U-04: Session / Window / Network / Audio Services

#### REQ-F-2.7 (Unwanted behaviour) — PulseAudio Operation Leak
If `PulseAudioBackend` subscribes to D-Bus events via `pa_context_subscribe()`, the system shall NOT leak the returned `pa_operation*` pointer.

**Reference**: `libs/holonight-services/src/.../PulseAudioBackend.cpp:144-145`.

**Acceptance Criterion**:
- `pa_operation_unref()` is called on the `pa_operation*` returned by `pa_context_subscribe()` after subscription is initiated (following PulseAudio API contract).
- Behavior changed only in memory management: resource leak eliminated; no observable API change.
- Unit test or memory-profiling verification: running the PulseAudio backend does not accumulate unreleased `pa_operation` objects over time.
- (Higher-risk item: manual verification required to confirm unref() is safe timing-wise in the async callback context.)

#### REQ-F-2.8 (Ubiquitous) — AudioDeviceModel & AudioStreamModel Fallback Reconciliation
The system shall reconcile behavioral differences between `AudioDeviceModel` and `AudioStreamModel` in their `applyRemove()` fallback paths, applying the correct behavior consistently to both.

**Reference**:
- `libs/holonight-services/src/.../AudioDeviceModel.h` and `.cpp`.
- `libs/holonight-services/src/.../AudioStreamModel.h` and `.cpp`.

**Acceptance Criterion**:
- Code review and decision log in PR notes or DESIGN.md clearly state which fallback behavior (from which model) is correct, and why the other diverged.
- Both models now implement the same fallback strategy for `applyRemove()`.
- Behavior changed only in the fallback case: one model is corrected to match the other's intended behavior.
- Unit tests for both models pass; test coverage includes the fallback path to verify consistency.
- (Higher-risk item per scope note: decision and manual verification required.)

#### REQ-F-2.9 (Ubiquitous) — NetworkManagerBackend Hardware Check Ordering
The system shall check for Wi-Fi hardware existence before enumerating saved connections in `NetworkManagerBackend::updateVisibleWifiNetworks()`.

**Reference**: `libs/holonight-services/src/.../NetworkManagerBackend.cpp:352-382`.

**Acceptance Criterion**:
- Hardware-existence check is performed first (before or instead of a full saved-connection enumeration).
- Behavior changed only in performance/correctness: avoided wasted work and eliminated possible false state when no Wi-Fi adapter is present; no visible API change.
- Unit test or integration test verifies the order: when Wi-Fi hardware is absent, the method returns early without unnecessary D-Bus calls.
- Build passes; existing network manager tests pass.

---

### Unit U-05: Desktop Integration (MIME, Theme, KDE Compat)

#### REQ-F-2.10 (Ubiquitous) — ThemeService Documentation Accuracy
The system shall update `ThemeService` header and class documentation to accurately describe its ownership of live D-Bus and file-watcher state, not as "trivial constant accessors."

**Reference**: `libs/holonight-services/src/.../ThemeService.h` and `.cpp` (doc comments).

**Acceptance Criterion**:
- Doc comments are updated to reflect actual state ownership and D-Bus subscription lifecycle.
- Behavior unchanged: no code changes, documentation-only fix.
- Clang-tidy and format checks pass on the modified file.

---

### Unit U-06: Launcher / Notifications

#### REQ-F-2.11 (Ubiquitous) — LauncherService Debounce Constant
The system shall replace the unnamed `500`ms filesystem-watcher debounce interval in `LauncherService` with a named `static const` constant.

**Reference**: `libs/holonight-services/src/.../LauncherService.cpp:252`.

**Acceptance Criterion**:
- Debounce interval `500` is defined as a named constant (e.g., `kFileWatcherDebounceMs`).
- Behavior unchanged: debounce timing and watcher behavior are identical before/after refactor.
- Build passes; launcher file-watching tests (if any) pass.

---

### Unit U-07: Calendar / Weather Services

#### REQ-F-2.12 (Unwanted behaviour) — NetworkError Dead Code
If `Kind::NetworkError` is never actually emitted by `CalendarService`, `CalDavProvider`, or `IcsProvider`, the system shall either wire the error to be emitted where appropriate (preferred) or remove the dead `UpcomingState::Offline` handling code.

**Reference**:
- `libs/holonight-services/src/.../CalendarService.cpp:151`.
- `libs/holonight-services/src/.../CalDavProvider.cpp:174-176`.
- `libs/holonight-services/src/.../IcsProvider.cpp:19-21`.

**Acceptance Criterion**:
- Code review and PR notes document the decision: wire `NetworkError` emission (preferred) or remove dead code (if unreachable by design).
- If wired: `Kind::NetworkError` is emitted from at least one provider when actual network errors occur; `UpcomingState::Offline` becomes reachable.
- If removed: the `Offline` state-handling code is deleted; no behavior change to reachable code paths.
- Unit tests pass; existing offline/error handling tests remain valid.
- (Higher-risk item: decision requires reading surrounding code; manual verification required to confirm final choice.)

#### REQ-F-2.13 (Unwanted behaviour) — Calendar/Weather ActivityGate Construction Ordering
If `CalendarService` or `WeatherService` constructs `IActivityGate` instances before `ActivityGateManager` is constructed, the system shall reorder the construction so `ActivityGateManager` is initialized first.

**Reference**: `libs/holonight-services/src/.../ShellApplication.cpp:97,112,132,231-234` (or equivalent main application initialization).

**Acceptance Criterion**:
- `ActivityGateManager` instance is constructed and registered before any `IActivityGate` subclass attempts to register with it.
- Behavior changed only in initialization order: eliminates late-binding/nullptr-dereference risk; no observable API change.
- Build passes; startup integration tests pass (if any); no crashes during initial gate registration.

#### REQ-F-2.14 (Unwanted behaviour) — CalendarCache Database Handle Leak
If `CalendarCache` calls `QSqlDatabase::removeDatabase()` while a live `QSqlDatabase` handle to that connection is still in scope, the system shall reorder so the handle is out-of-scope before `removeDatabase()` is called.

**Reference**: `libs/holonight-services/src/.../CalendarCache.cpp:250,257`.

**Acceptance Criterion**:
- Code is refactored so that any `QSqlDatabase` local/member variable referencing the connection goes out-of-scope before `removeDatabase()` is invoked.
- Behavior changed only in lifecycle: eliminates undefined-behavior warning from Qt; no observable API change.
- Build passes; no new Qt runtime warnings in logs; calendar sync tests pass.

#### REQ-F-2.15 (Ubiquitous) — CalendarSyncManager Copy Elimination
The system shall convert six range-for loops in `CalendarSyncManager` from value iteration to const-reference iteration, eliminating unnecessary copies.

**Reference**: `libs/holonight-services/src/.../CalendarSyncManager.cpp:69,75,98,101,207,213`.

**Acceptance Criterion**:
- All six identified loops use `const auto&` instead of value iteration (or equivalent const-ref type).
- Behavior unchanged: loop logic and element access are identical before/after refactor.
- Build passes; clang-tidy passes (catches this pattern in other files too, if any remain).

#### REQ-F-2.16 (Ubiquitous) — Calendar Retention Window Duplication
The system shall hoist the calendar retention-window values (-30/+180 days) into a single shared named constant, eliminating duplication between `CalendarSyncManager` and `CalendarCache`.

**Reference**:
- `libs/holonight-services/src/.../CalendarSyncManager.cpp:147`.
- `libs/holonight-services/src/.../CalendarCache.h:67-68`.

**Acceptance Criterion**:
- Retention window values are defined as a single `static const` or `inline constexpr` (e.g., in a shared header or as a class-level constant).
- Both `CalendarSyncManager` and `CalendarCache` reference this shared constant.
- Behavior unchanged: retention logic and date calculations are identical before/after refactor.
- Build passes; calendar sync and cache tests pass.

#### REQ-F-2.17 (Ubiquitous) — WeatherIconBridge Moon-Phase Constants
The system shall extract unnamed moon-phase boundary literals from `WeatherIconBridge::layersFor()` into named `static const` constants, deduplicating the bucketing scheme against `MoonPhaseCalculator`'s canonical implementation.

**Reference**:
- `libs/holonight-services/src/.../WeatherIconBridge.cpp:17-39`.
- `libs/holonight-services/src/.../MoonPhaseCalculator.h` (canonical bucketing).

**Acceptance Criterion**:
- Numeric phase-boundary literals are replaced with named constants (e.g., `kMoonPhaseBound_0`, `kMoonPhaseBound_1`, etc.) or reuse existing constants from `MoonPhaseCalculator`.
- Behavior MUST remain identical: no output of `WeatherIconBridge::layersFor()` changes for any tested date before/after refactor (verified by running existing icon-compositor tests and spot-checking against a test date matrix).
- Condition-code coverage remains unchanged: only clear/cloud/drizzle/rain families are mapped; no new fallback behavior added.
- Build passes; weather icon compositor QML tests pass.
- (Higher-risk item: manual verification required — a test matrix confirming icon outputs for sample dates is mandatory.)

---

### Unit U-08: Topbar QML

#### REQ-F-2.18 (Unwanted behaviour) — ClockSection Layout Conflicts
If `ClockSection.qml` has both anchors and an explicit `width` property on the same `Column`, the system shall remove the redundant one, keeping whichever produces correct visual layout.

**Reference**: `apps/shell/qml/Topbar/ClockSection.qml:40-49`.

**Acceptance Criterion**:
- Either anchors or explicit `width` remains (not both); the other is removed.
- Manual verification: `task run` in a live Wayland session; ClockSection layout is visually identical before/after refactor (no size/alignment shift).
- QML lints without error; component renders correctly in topbar.

#### REQ-F-2.19 (Unwanted behaviour) — WorkspaceEdgeArrow MultiEffect Z-Order
If `WorkspaceEdgeArrow.qml` declares `MultiEffect` AFTER its source element, the system shall move the `MultiEffect` declaration BEFORE the source, per project z-order convention.

**Reference**: `apps/shell/qml/Topbar/WorkspaceEdgeArrow.qml:51`.

**Acceptance Criterion**:
- `MultiEffect` element is declared before all child elements that should render above it.
- Behavior unchanged: glow appearance and z-order are identical before/after (likely fixing a visual bug where glow was incorrectly rendering on top).
- `task qml-lint` passes; manual verification: `task run` shows glow underneath intended elements.

#### REQ-F-2.20 (Ubiquitous) — StatusPopupTriggerArea Input Handler Modernization
The system shall replace click-only `MouseArea` in `StatusPopupTriggerArea.qml` with a `TapHandler` per Qt Quick modern input conventions.

**Reference**: `apps/shell/qml/Topbar/StatusPopupTriggerArea.qml:20-28`.

**Acceptance Criterion**:
- `MouseArea` with click-only logic is replaced with `TapHandler`.
- Behavior unchanged: tap/click triggering the status popup is identical before/after.
- Build passes; `task qml-lint` passes; manual verification: popup trigger responds normally to tap/click.

#### REQ-F-2.21 (Ubiquitous) — BarSection Wheel Handler Modernization
The system shall replace wheel-only `MouseArea` in `BarSection.qml` with a `WheelHandler` per Qt Quick modern input conventions.

**Reference**: `apps/shell/qml/Topbar/BarSection.qml:10-16`.

**Acceptance Criterion**:
- `MouseArea` with wheel-only logic is replaced with `WheelHandler`.
- Behavior unchanged: mouse wheel handling is identical before/after.
- Build passes; `task qml-lint` passes; manual verification: wheel scroll behavior unchanged.

#### REQ-F-2.22 (Ubiquitous) — WorkspacePill Property Readonly
The system shall mark `label` and `active` properties in `WorkspacePill.qml` as `readonly property` since they are never reassigned internally.

**Reference**: `apps/shell/qml/Topbar/WorkspacePill.qml:13-14`.

**Acceptance Criterion**:
- `label` and `active` are declared as `readonly property` (instead of mutable `property`).
- Behavior unchanged: no code path reassigns these properties; this is a correctness annotation.
- Build passes; `task qml-lint` passes.

#### REQ-F-2.23 (Ubiquitous) — Dead ID Removal (Topbar)
The system shall remove unreferenced `id` assignments from approximately 15 Topbar QML files where the id is declared but never referenced within the file or by sibling files.

**Reference**: `apps/shell/qml/Topbar/*.qml` (multiple files, exact list identified during grep/refactor).

**Acceptance Criterion**:
- All dead ids (declared but unreferenced) are removed from Topbar QML files.
- Manual verification: use `task qml-lint` and grep to confirm no references to removed ids remain.
- Behavior unchanged: no functional logic depends on id declarations; this is cleanup only.
- Build passes; all Topbar components render identically before/after.

---

### Unit U-09: Sidebar / Popup Surfaces QML

#### REQ-F-2.24 (Event-driven) — WifiNetworkDelegate Canvas Repaint Trigger
When `WifiNetworkDelegate.qml` lock-icon `Canvas` element's `connected` property changes, the system shall call `requestPaint()` to refresh the icon rendering.

**Reference**: `apps/shell/qml/RightSidebar/WifiNetworkDelegate.qml:109-142`.

**Acceptance Criterion**:
- `Canvas` element includes an `onConnectedChanged: { requestPaint() }` handler (or equivalent).
- Behavior changed only in UI correctness: lock icon now updates visibly when connection state changes (previously could go stale).
- Manual verification: `task run`, connect/disconnect from a Wi-Fi network; lock icon state visibly follows connection state.
- QML lints without error.

---

### Unit U-10: Overlay / Widgets QML

#### REQ-F-2.25 (Ubiquitous) — ComponentBehavior Pragma Consistency
The system shall audit 5 files under Launcher, Notifications, and Widgets QML and apply `pragma ComponentBehavior: Bound` consistently to all delegate components.

**Reference**: `apps/shell/qml/Launcher/*.qml`, `apps/shell/qml/Notifications/*.qml`, `apps/shell/qml/Widgets/*.qml` (exact files identified during audit).

**Acceptance Criterion**:
- All delegate/component files that should use `pragma ComponentBehavior: Bound` do so.
- Behavior unchanged: scoping and binding semantics are identical before/after (or fixed if inconsistency introduced a bug).
- Build passes; `task qml-lint` passes; manual verification confirms consistent delegate behavior.

#### REQ-F-2.26 (Unwanted behaviour) — Launcher Text topMargin Dead Code
If `Launcher.qml:249` sets `topMargin` on a `Text` element that uses `verticalCenter` anchoring, the system shall remove the dead `topMargin` property.

**Reference**: `apps/shell/qml/Launcher/Launcher.qml:249`.

**Acceptance Criterion**:
- `topMargin` property is removed (has no effect under `verticalCenter` anchoring).
- Behavior unchanged: Text element's vertical position is identical before/after (margin had no effect).
- `task qml-lint` passes; manual verification: Launcher layout unchanged.

#### REQ-F-2.27 (Ubiquitous) — BarIcon Implicit Sizing
The system shall replace hardcoded `width`/`height` in `Controls/BarIcon.qml` with `implicitWidth`/`implicitHeight`, following Qt Quick sizing conventions for reusable components.

**Reference**: `apps/shell/qml/Controls/BarIcon.qml:20-21`.

**Acceptance Criterion**:
- Hardcoded width/height are replaced with implicit sizing (or removed to let the component's natural size be used).
- Behavior unchanged: BarIcon dimensions in layouts are identical before/after (implicit sizing delegates to content size, which was already set to hardcoded values).
- Build passes; `task qml-lint` passes; manual verification: all BarIcon usages render at the correct size.

#### REQ-F-2.28 (Unwanted behaviour) — TrayItem Dead Transition
If `Tray/TrayItem.qml` has an opacity fade-out transition on a "Passive" state that is structurally unreachable, the system shall either fix the unreachable condition or remove the dead transition, matching original intent.

**Reference**: `apps/shell/qml/Tray/TrayItem.qml:42,70-75`.

**Acceptance Criterion**:
- Code review determines original intent: if Passive state was meant to be reachable, the guard condition is fixed; if truly dead, the transition is removed.
- PR notes document the decision and reasoning.
- Behavior unchanged to reachable code paths; dead code is eliminated.
- `task qml-lint` passes; build passes.

---

### Unit U-11: Settings Application

#### REQ-F-2.29 (Ubiquitous) — Mode-to-Scheme Mapping Consolidation
The system shall extract the repeated mode-to-scheme mapping ternary (or if-else chain) from `ThemeConfigFile.cpp`, `ConfigFileService.cpp`, and `SettingsEditModel.cpp` into a single shared helper function.

**Reference**: `apps/settings/src/` (5–6 call sites across the three files listed).

**Acceptance Criterion**:
- A single shared function (e.g., `modeToScheme()` or `mapThemeMode()`) is defined in a common utility header or source file under the settings app.
- All 5–6 call sites replace their hand-written ternary/if-else with a call to the shared function.
- Behavior unchanged: theme mode mapping logic is identical before/after refactor.
- Build passes; settings tests pass; theme switching UI behavior unchanged.

---

## Summary

- **Total Requirements**: 36 items (6 Category A + 29 Category B + 5 Constraints).
- **REQ-C**: Project-wide constraints (build, test, lint, behavior preservation, API stability).
- **REQ-F-1.x**: Category A (roleNames caching, 6 items).
- **REQ-F-2.1 through REQ-F-2.29**: Category B (Low-severity findings by unit, 29 items).

**Higher-Risk Items** (require judgment + manual verification):
- REQ-F-2.7 (PulseAudio unref timing)
- REQ-F-2.8 (AudioDeviceModel/AudioStreamModel reconciliation decision)
- REQ-F-2.12 (NetworkError dead-code decision)
- REQ-F-2.17 (WeatherIconBridge moon-phase bucketing — test matrix mandatory)

All other items are refactor-only with deterministic verification (lint, build, test, behavior preservation).

---

## Verification Checklist (for implementation phase)

- [ ] `task configure` and `task build` pass.
- [ ] `task test` passes (all existing tests continue to pass).
- [ ] `task format-check` and `task tidy` pass on modified files.
- [ ] `task qml-lint` passes on modified QML files.
- [ ] Manual verification completed for QML visual changes (D-002, D-003, D-004, D-005, layout/sizing).
- [ ] Manual verification completed for higher-risk items (REQ-F-2.7, REQ-F-2.8, REQ-F-2.12, REQ-F-2.17).
- [ ] New unit tests added for behavioral changes (REQ-F-2.5, REQ-F-2.9, REQ-F-2.12, REQ-F-2.14, REQ-F-2.17, REQ-F-2.24).
- [ ] PR description documents decisions for higher-risk items.
