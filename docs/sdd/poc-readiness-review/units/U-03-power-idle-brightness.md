# U-03 — Power, Idle & Brightness Management — Deep Review Findings

**Task**: T-003 · **Skill**: `qt-cpp-review` (Phase 1 deterministic lint + Phase 2 six-agent deep analysis) · **Scope**: 33 files, read-only

## Scope

- `libs/holonight-services/src/` root-level: `ActivityGateManager.{h,cpp}`, `IActivityGate.h`, `LidStateMonitor.{h,cpp}`, `LowBatteryMonitor.{h,cpp}`, `SuspendInhibitorService.{h,cpp}`, `InhibitorModel.{h,cpp}`, `BatteryService.{h,cpp}`, `PowerProfilesService.{h,cpp}` (13 files)
- `libs/holonight-services/src/idle/` — `ExtIdleNotifyBackend`, `IdleBackend`, `IdleInhibitor`, `IdleService`, `NullIdleBackend`, `ScreenSaverAdaptor` (12 files)
- `libs/holonight-services/src/brightness/` — `BrightnessBackend`, `BrightnessService`, `NullBrightnessBackend`, `SysfsBackend` (8 files)

## Prior Context

Consulted per T-003 instructions: `docs/sdd/idle-management/SPEC.md`+`DESIGN.md`, `docs/sdd/power-extensions/SPEC.md`+`DESIGN.md`, `docs/sdd/brightness-service/SPEC.md`+`DESIGN.md`. Per SPEC.md §12, `idle-management` has 7 pre-existing unchecked `TASKS.md` items — this review audits **current code**, not that stale backlog (cataloged separately in T-013).

**LowBatteryMonitor "discharging LAST" signal-order invariant** (CLAUDE.md-documented): **verified intact.** `BatteryService::applyStateUpdate()` (`BatteryService.cpp:76-110`) explicitly comments the ordering rationale ("Apply reset-triggering state flags first... Then emit discharging last so `dischargingChanged` (which also triggers threshold checks) has an up-to-date `percent_` to read") and the code matches: `setPercent()` runs at line 105, `setDischarging()` — the call that ultimately fires `LowBatteryMonitor::onDischargingChanged()` → `checkThresholds()` — runs last at line 108. No drift from the documented invariant.

## Tool Sign-off — Phase 1 Deterministic Lint

56 raw lint hits. Excluded as noise, same rationale as prior units:

- **VAR-3** (43 hits, brace-init style) — not reported.

Two flagged hits were independently investigated by Phase 2 agents and refuted:

- **ERR-6** (`SysfsBackend.cpp:63,77`, "`.arg()` has `%2` but only 1 call"): **false positive**, same shape as the U-01 `SystemInfo.cpp` precedent — both use the two-argument `QString::arg(a, b)` overload, which correctly binds both placeholders in one call. Confidence 95/100.
- **PAT-11** (`SysfsBackend.cpp:57`, "`QRegularExpression` constructed inside loop"): **effectively a false alarm** — the declaration is `static const`, so C++ magic-statics guarantee one-time construction regardless of loop position; additionally the enclosing `selectDevice()` runs exactly once per process (constructor-only), not per brightness read. Confidence 85/100. No fix required; optionally hoist the `static const` above the loop for readability only.

Remaining categories (`TMO-1` ×5, `PAT-9` ×2, `DEP-10` ×2 `.count()`/`.length()` style, `MDL-7` ×1) are low-severity; `MDL-7` (`InhibitorModel.cpp:22`, "`data()` switch has `default:`") was independently verified against `roleNames()` and found in sync (`WhoRole`/`WhyRole` handled identically in both) — false positive, same pattern as U-01's `WorkspaceModel` case.

## Confirmed Findings (confidence ≥ 80/100)

### [F-01] Duplicated, non-trivial logind session-resolution workaround across `IdleService` and `SysfsBackend`
- **Severity**: Medium
- **Effort**: M
- **Location**: `libs/holonight-services/src/idle/IdleService.cpp:172-215` (`subscribeLockedHint`) and `libs/holonight-services/src/brightness/SysfsBackend.cpp:82-116` (`resolveSessionPath`)
- **Rationale**: Both independently implement the identical three-step fallback documented in CLAUDE.md's "logind session resolution in UWSM environments" gotcha (`GetSessionByPID` → `loginctl show-seat` subprocess → `GetSession(id)`), including near-verbatim comments explaining the underlying libdbus/Qt corruption bug being worked around. This is not routine duplication — it's a subtle, previously-debugged workaround for a real crash (`dbus_message_iter_get_basic()` assertion abort documented in CLAUDE.md), and a future refinement applied to only one copy would silently reintroduce the bug in the other service. Two agents independently flagged this (API & Correctness at 80/100, Performance & Quality at 88/100) — cross-agent agreement.
- **Suggested Direction**: Extract a shared helper (e.g. `resolveActiveLogindSessionPath(QDBusConnection&)`) into a common location such as alongside `DbusPropertyClient`, and have both services call it — centralizing the documented workaround in one auditable place.

### [F-02] `InhibitorModel::setEntries()` always performs a full model reset, even when the entry list is byte-for-byte unchanged
- **Severity**: Low
- **Effort**: S
- **Location**: `libs/holonight-services/src/InhibitorModel.cpp:34-42`, invoked every 5s by `SuspendInhibitorService::finishAsyncPoll` (`SuspendInhibitorService.cpp:154-157`)
- **Rationale**: `setEntries()` unconditionally calls `beginResetModel()`/`endResetModel()` on every 5-second poll, regardless of whether the inhibitor set actually changed — the existing `count() != old_count` check only gates the cheap `countChanged()` signal, not the expensive reset. Sleep inhibitors change rarely, so most polls force the attached QML `ListView` (sidebar's Quick Settings inhibitor section) through a full delegate teardown/rebuild for no reason. Independently flagged by two agents (Model Contracts at 65/100, Performance & Quality at 85/100) — cross-agent agreement, taking the higher-confidence assessment.
- **Suggested Direction**: Add an equality check (e.g. defaulted `operator==` on `InhibitorEntry`) and early-return without touching `entries_`/emitting any signal when the new list matches the old one.

### [F-03] `SuspendInhibitorService::inhibitorModelForQml()` (and sibling) are non-const `Q_PROPERTY READ` accessors
- **Severity**: Low
- **Effort**: S
- **Location**: `libs/holonight-services/src/SuspendInhibitorService.h:35-36`
- **Rationale**: Both `inhibitorModel()` and its `Q_PROPERTY(... CONSTANT ...) READ` accessor `inhibitorModelForQml()` are declared without `const`, despite neither mutating service state — every other `READ` accessor across the entire unit (`BatteryService`, `PowerProfilesService`, `IdleService`, `BrightnessService`, `InhibitorModel::count()`) is `const`-qualified. `Q_PROPERTY ... CONSTANT` semantically reinforces that this should be a pure getter. Confidence 85/100.
- **Suggested Direction**: Mark both methods `const`.

### [F-04] `LowBatteryMonitor::sendNotification` discards the D-Bus `Notify` call result — a failed low-battery warning is completely silent
- **Severity**: Medium
- **Effort**: S
- **Location**: `libs/holonight-services/src/LowBatteryMonitor.cpp:93-108`
- **Rationale**: `notif.call(QStringLiteral("Notify"), ...)`'s returned `QDBusMessage` is discarded — no check of `reply.type() == QDBusMessage::ErrorMessage`. `warning_sent_`/`critical_sent_` are marked `true` before the call even completes, so if the notification daemon rejects it (bad hint type, daemon restart mid-call, timeout), the user silently never sees their low-battery warning for that discharge cycle, with zero log trail to diagnose it. Every structurally similar fire-and-forget D-Bus call elsewhere in this same unit (`IdleInhibitor::acquire`, `PowerProfilesService::setProfile`, `ScreenSaverAdaptor::registerService`) checks and logs on failure — this is the one exception. Confidence 82/100.
- **Suggested Direction**: Capture the returned `QDBusMessage` and log `qCWarning` with `reply.errorMessage()` on `ErrorMessage`, matching the sibling pattern.

### [F-05] `SysfsBackend` has no device-removal detection; reports a permanent, misleading 0% after the backlight device disappears
- **Severity**: Medium
- **Effort**: M
- **Location**: `libs/holonight-services/src/brightness/SysfsBackend.cpp:118-173` (`setupInotify`, `onInotifyEvent`, `readBrightness`)
- **Rationale**: `onInotifyEvent()` drains inotify events without inspecting `inotify_event.mask`, so it can't distinguish `IN_MODIFY` (brightness changed) from `IN_IGNORED` (kernel-generated when the watched device is removed — e.g. hot-unplugged eDP/USB display). Either way it calls `readBrightness()`, which on a now-missing file logs a warning and returns `0`; that `0` then propagates as a legitimate-looking `brightnessPercent`. `hasBacklight` is declared `CONSTANT` in the QML property (`BrightnessService.h:19`), so it structurally can never be re-notified even if the C++ side were fixed — the UI keeps showing a live, adjustable "0%" slider for a backlight that no longer exists. Confidence 80/100.
- **Suggested Direction**: On `IN_IGNORED` (or on `readBrightness()` failing to open the file), treat the device as gone — reset `max_brightness_`, tear down the notifier, and relax `hasBacklight` from `CONSTANT` to a `NOTIFY`-based property so QML can reflect live removal.

## Investigation Targets (confidence 60-79 — human verification needed)

Capped at 10 per skill protocol; one item at the 60/100 floor (`get`-prefixed getter naming in `IdleBackend`/`IdleService`, self-flagged by its own reviewing agent as likely a deliberate D-Bus-method-name mirror rather than a slip) was dropped to stay within the cap.

#### [I-01] `BatteryService::setPercent()` / `LowBatteryMonitor` accept out-of-range UPower percentages unclamped
- **Severity**: Medium · **Effort**: S · **Confidence**: 75/100
- **Location**: `BatteryService.cpp:131-137`, `LowBatteryMonitor.cpp:72-91`
- **Rationale**: No `std::clamp(value, 0, 100)` on the UPower-sourced percent before it feeds threshold comparisons and user-facing notification text (`"%1% remaining"`). A known class of Linux UPower/ACPI battery firmware quirks can report out-of-range percentages.
- **Suggested Direction**: Clamp in `BatteryService::setPercent()` so all downstream consumers see a sane range.

#### [I-02] `ActivityGateManager` holds raw non-owning pointers with no unregister path; Qt child-deletion order creates a dangling-pointer window on shutdown
- **Severity**: Low · **Effort**: M · **Confidence**: 72/100
- **Location**: `ActivityGateManager.h:21-27`, `.cpp:5-15`; call sites `ShellApplication.cpp:97,112,132,230-236`
- **Rationale**: `registerGate()` has no paired `unregisterGate()`. Because `activity_gate_manager_` is constructed and populated with gates *after* `calendar_service_`/`weather_`/`suspend_inhibitor_service_` are already children of `ShellApplication`, Qt's front-to-back child-deletion order tears down the gates before the manager on normal shutdown, leaving `gates_` briefly holding dangling pointers. Not currently exploitable (no event-loop re-entry happens during that synchronous teardown window today), but the invariant isn't enforced by the API.
- **Suggested Direction**: Add `unregisterGate()` called from each gate's destructor, or construct `ActivityGateManager` before the gates it tracks.

#### [I-03] `SuspendInhibitorService`/`SysfsBackend`/`IdleService` each hardcode their own `2000`ms subprocess timeout as a bare literal
- **Severity**: Low · **Effort**: S · **Confidence**: 68/100
- **Location**: `SuspendInhibitorService.cpp:60,119`; also present in `IdleService.cpp:191`, `SysfsBackend.cpp:102`
- **Rationale**: The same "external process timeout" concept appears as an unnamed `2000` in four places across two files, while `SuspendInhibitorService.cpp` has already established the `kPollIntervalMs` naming convention right next to it — inconsistent treatment within the same file.
- **Suggested Direction**: Introduce a named `kBusctlTimeoutMs`/similar constant at each site (or as part of the [F-01] shared-helper extraction, since the timeout belongs to the same duplicated logic).

#### [I-04] Dead/duplicate branch in `PowerProfilesService::parseProfileNames`
- **Severity**: Low · **Effort**: S · **Confidence**: 68/100
- **Location**: `PowerProfilesService.cpp:163-211`
- **Rationale**: The function's final fallback branch (`!value.canConvert<QDBusArgument>()` at lines 201-210) repeats the exact same `arg >> entries` extraction as the first branch, but by that point `value.userType()` is already known not to be `QDBusArgument`'s metatype id — the branch appears unreachable in practice. The `canConvert<QDBusArgument>()` probe shape is also the specific anti-pattern CLAUDE.md's D-Bus section warns against.
- **Suggested Direction**: Remove the trailing dead branch, or document the specific scenario it's meant to guard against with a test exercising it.

#### [I-05] `SysfsBackend` destructor closes the inotify fd before disabling the `QSocketNotifier`, contradicting its own header comment
- **Severity**: Low · **Effort**: S · **Confidence**: 66/100
- **Location**: `SysfsBackend.h:44-46`, `.cpp:41-48`
- **Rationale**: The header comment claims member-declaration order makes `notifier_`'s destructor "fire first," but an explicit destructor body always runs before any member destructor regardless of declaration order — so `close(ifd_)` actually executes while `notifier_` is still live and enabled. No observed corruption (single-threaded, no window for fd reuse before `notifier_` is torn down microseconds later), but the code doesn't implement the safety property its own comment claims.
- **Suggested Direction**: Call `notifier_->setEnabled(false)` (or `.reset()`) before `inotify_rm_watch`/`close` in the destructor body; correct the misleading comment.

#### [I-06] `BrightnessService::computePercent` has no clamp on raw brightness outside `[0, max_brightness_]`, asymmetric with the write-path clamp
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `BrightnessService.cpp:43-48`
- **Rationale**: `setBrightnessPercent` (write path) clamps its input to `[0,100]`, but `computePercent` (read path, used at init and on every `onBrightnessChanged`) does not — an inconsistent guard between the two directions of the same value. A driver reporting `brightness` slightly above `max_brightness`, or a stale/garbage extraction, could emit an out-of-range percent to QML.
- **Suggested Direction**: Apply the same `std::clamp(percent, 0, 100)` to `computePercent`'s result.

#### [I-07] `PowerProfilesService` never disconnects its `PropertiesChanged` D-Bus subscription on daemon restart — signal bindings accumulate
- **Severity**: Low · **Effort**: S · **Confidence**: 63/100
- **Location**: `PowerProfilesService.cpp:56-62` (`setupWatcher`), `65-99` (`initFromService`), `154-161` (`clearState`)
- **Rationale**: `initFromService()` reconnects `PropertiesChanged` on every `serviceRegistered` event, but `serviceUnregistered`'s `clearState()` never pairs it with a `disconnectSignal`. `QDBusConnection::connect()` doesn't deduplicate identical registrations, so each `power-profiles-daemon` restart adds another binding. `onPropertiesChanged` is idempotent (early-returns on unchanged values), so the practical symptom is redundant work, not visible corruption.
- **Suggested Direction**: Track connection state and pair each `connectSignal` with exactly one `disconnectSignal` across the daemon's lifetime.

#### [I-08] `BrightnessService::setBrightnessPercent` silently no-ops with no log when `max_brightness_ == 0`
- **Severity**: Low · **Effort**: S · **Confidence**: 62/100
- **Location**: `BrightnessService.cpp:50-53`
- **Rationale**: Unlike nearly every other early-return guard in this unit (`LidStateMonitor::start`, `BatteryService::start`, `PowerProfilesService::start`, and `SysfsBackend::setBrightness`'s own analogous guard three frames deeper), this one logs nothing — making it hard to tell from logs whether a brightness-key press was dropped intentionally (no backlight) or lost some other way.
- **Suggested Direction**: Add a `qCInfo`/`qCDebug` log for the no-backlight no-op case.

#### [I-09] `IdleService`'s default idle threshold (`300'000` ms) is an inline magic number, inconsistent with every sibling constant in the same unit
- **Severity**: Low · **Effort**: S · **Confidence**: 60/100
- **Location**: `libs/holonight-services/src/idle/IdleService.h:79`
- **Rationale**: Every other tunable duration in this unit (`kPollIntervalMs`, `kTrackerThresholdMs`, `kWarningTimeoutMs`, `kCriticalTimeoutMs`, `kMissingDaemonNotificationDelayMs`) is a named `static constexpr` in its `.cpp` file; this one is an inline member-initializer literal with no accompanying "5 minutes" comment.
- **Suggested Direction**: Introduce `static constexpr int kDefaultIdleThresholdMs{300'000};  // 5 minutes` matching the established convention.

#### [I-10] `SysfsBackend` ignores `QTextStream` numeric-extraction failure when reading sysfs values
- **Severity**: Low · **Effort**: S · **Confidence**: 60/100
- **Location**: `SysfsBackend.cpp:63-74` (`selectDevice`), `134-143` (`readBrightness`)
- **Rationale**: Neither `QTextStream >> max_val` nor `QTextStream >> val` checks the stream's fail state after extraction. A readable-but-non-numeric sysfs file (device teardown race, nonstandard driver) silently leaves the value at default-initialized `0`, indistinguishable from a genuine `0` reading.
- **Suggested Direction**: Check the stream's fail state (or `file.size() > 0`) after extraction and log a warning distinguishing "unreadable value" from "genuinely 0".

## Summary

| Category | Lint (reported) | Deep (confirmed ≥80) | Investigation (60-79) | Total |
|---|---|---|---|---|
| Model Contracts | 1 (MDL-7, refuted) | 0 | 0 (merged into F-02) | 0 |
| Ownership & Lifecycle | 0 | 0 | 3 | 3 |
| Thread Safety | 0 | 0 | 0 | 0 |
| API & C++ Correctness | 0 | 1 | 1 | 2 |
| Error Handling & Validation | 1 (ERR-6, refuted) | 2 | 4 | 6 |
| Performance & Code Quality | 1 (PAT-11, clarified) | 2 (1 merged into F-02, 1 into F-01) | 2 | 4 |
| **Total** | **56 raw / 2 refuted / rest low-value** | **5** | **10** | **15 actionable** |

15 actionable items (5 confirmed findings + 10 capped investigation targets). This unit's power/idle/brightness services are architecturally sound overall — the documented "discharging LAST" signal-order invariant holds exactly as specified, thread safety is clean (verified zero cross-thread constructs across all 33 files), and the abstract-backend+Null-implementation pattern (`IdleBackend`/`NullIdleBackend`, `BrightnessBackend`/`NullBrightnessBackend`) is correctly implemented. The most actionable finding is **[F-01]**: two independently-maintained copies of a subtle, previously-debugged logind workaround (documented in CLAUDE.md as fixing a real libdbus assertion crash) — a strong candidate for early consolidation since any future refinement risks silently reintroducing that crash in whichever copy isn't updated. **[F-05]** (permanent misleading 0% after backlight device removal) is a real but narrow-scope UX correctness gap worth fixing alongside any brightness-service work.
