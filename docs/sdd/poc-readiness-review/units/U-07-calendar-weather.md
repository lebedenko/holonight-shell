# U-07 — Calendar & Weather Services — Deep Review Findings

**Task**: T-007 · **Skill**: `qt-cpp-review` (Phase 1 deterministic lint + Phase 2 six-agent deep analysis) · **Scope**: 29 files, read-only

## Scope

- `libs/holonight-services/src/CalendarService.{h,cpp}` (2 files)
- `libs/holonight-services/src/calendar/` — `CalDavProvider`, `CalendarCache`, `CalendarEventModel`, `CalendarSyncManager`, `CalendarTypes`, `ICalendarProvider`, `ICalParser`, `IcsProvider`, `LibsecretCredentialStorage` (16 files)
- `libs/holonight-services/src/weather/` — `WeatherData`, `WeatherProvider`, `WeatherService` (5 files)
- `libs/holonight-services/src/weather-icon/` — `MoonPhaseCalculator`, `MoonPhase`, `WeatherIconBridge`, `WeatherIconLayer`, `WeatherIconMapper` (8 files, actual count matches TASKS.md's estimate)

## Prior Context

Consulted per T-007 instructions: `docs/sdd/weather-icon-compositor/SPEC.md`+`DESIGN.md`. (`docs/sdd/calendar/SPEC.md`+`DESIGN.md` referenced by the task list does not exist under that exact name — no `docs/sdd/calendar*` directory exists; no equivalent prior-cycle doc was found for the calendar feature.)

**Mandatory check — IActivityGate consistency with U-03**: **verified consistent, no inversion bug (92/100).** `CalendarService`/`CalendarSyncManager` and `WeatherService`'s `pauseActivity()`/`resumeActivity()` both mean "stop my own background timers on pause, restart + immediate refresh on resume" — the identical semantic direction `SuspendInhibitorService` (reviewed in U-03) and `ActivityGateManager::onLidStateChanged` use. No inverted boolean semantics found across implementations.

**Mandatory check — WeatherIconCompositor non-wiring claim**: **project memory was stale; corrected here.** T-007's own instructions (and prior project memory) stated WeatherIconCompositor is "deliberately not wired into production" with only partial condition-code coverage. Independently verified via `grep`: `WeatherIconCompositor.qml`/`WeatherIconBridge` **are** referenced by `apps/shell/qml/Popups/Weather/{WeatherCurrentSection,WeatherHourlyStrip,WeatherDailyCards,WeatherPopupContent}.qml` — the compositor is wired into the weather popup screens. Only the **topbar's** compact weather icon (`Topbar/WeatherWidget.qml` → `WeatherService.iconPath()`) still uses the older SVG path — the two paths are independent, exactly as documented, but "not wired into any existing screen" is no longer accurate. `WeatherIconMapper::conditionFamily()` was also found to now cover a comprehensive set of OWM condition families (drizzle, rain, freezing rain, hail, snow, sleet, thunderstorm, mist, dust, wind, tornado, etc.) with matching PNG assets on disk — the "partial coverage" framing is also stale. **This memory correction is applied at the end of this task (see Stage 6-adjacent note); U-10's QML review should treat `WeatherIconCompositor.qml` as production-wired, not standalone/unwired.**

## Tool Sign-off — Phase 1 Deterministic Lint

128 raw lint hits — the largest and most substantively important lint yield of any unit reviewed so far, dominated by real network-error-handling gaps in the calendar/weather HTTP providers. Excluded as noise:

- **VAR-3** (77 hits, brace-init style) — not reported.
- **HDR-3** (2 hits, Windows-only) — not reported.

The remaining categories are almost entirely substantive and were deepened into concrete findings below: **ERR-9** (10, missing SSL-error handling), **ERR-5** (5, missing transfer timeout), **ERR-3** (6, reply read without error check), **PAT-12** (7, non-const range-for), **LCY-1** (2, missing `deleteLater`), **MDL-7** (1, verified false positive — `CalendarEventModel`'s `roleNames()`/`data()` role sets are in sync). `TMO-1` (9), `PAT-9` (4), `DEP-5` (2), `DEP-10` (2), `DEP-13` (1) were not elevated individually.

## Confirmed Findings (confidence ≥ 80/100)

### [F-01] `CalDavProvider::sendSync()` never actually returns `nullptr` on timeout — its documented timeout contract is dead code, so a hung CalDAV server silently produces a "successful" empty sync
- **Severity**: Critical
- **Effort**: S
- **Location**: `calendar/CalDavProvider.cpp:24-33` (`sendSync`), all 4 call sites (`resolvePrincipalUrl`, `discoverCalendars`, `fetchCalendarEvents`, `testConnection`)
- **Rationale**: `sendSync()`'s own doc comment states "Returns nullptr on timeout," and every caller branches on `reply == nullptr` to detect it — but `QNetworkAccessManager::sendCustomRequest()` **never returns `nullptr`**, and the function body never checks `reply->isRunning()` after the race between `QNetworkReply::finished` and the timeout timer; it unconditionally `return reply;`. On a genuine timeout, `reply->error()` is still `NoError` (nothing has failed yet, it just hasn't finished), so every "timeout" check silently evaluates false — the code proceeds to `readAll()` on an unfinished reply (empty/partial bytes), parses that as a valid empty response, and `fetchEvents()` returns success with zero events. `CalendarSyncManager::onSyncFinished` records this as a clean sync (clears backoff, advances `next_sync_time`), with no `syncError` signal ever emitted. Existing cached events simply age out over subsequent days with no diagnostic that sync is broken. **Confirmed independently by two agents at identical 90/100 confidence** (Error Handling and API & Correctness), on top of the original `ERR-5` lint hit — triple corroboration. `IcsProvider::httpGet()` in the same codebase gets this exactly right (checks `isRunning()`, calls `abort()`), proving this is an oversight specific to `CalDavProvider`, not an intentional design difference.
- **Suggested Direction**: Mirror `IcsProvider::httpGet()`'s pattern in `sendSync()`: after `loop.exec()`, check `reply->isRunning()`; if still running, call `reply->abort()` and return `nullptr` so the existing `reply == nullptr` guards at all 4 call sites actually fire as documented.

### [F-02] `CalendarCache` never deletes events that disappear from a provider's fetch — cancelled/deleted meetings linger in the model indefinitely
- **Severity**: High
- **Effort**: M
- **Location**: `calendar/CalendarCache.cpp:350` (`upsertEvents`), `:481` (`pruneExpired`), `CalendarSyncManager.cpp:186-194` (`onSyncFinished`)
- **Rationale**: Every sync only `INSERT ... ON CONFLICT DO UPDATE`s the events returned by `fetchEvents()` — there is no corresponding delete step for events that were present in a prior sync but are absent from the current one. `pruneExpired()` is purely a time-window sweep, unrelated to upstream presence. A user cancelling/deleting a meeting on the CalDAV/ICS server sees it keep appearing in the "upcoming events" widget — and it can keep firing pre-event notifications — until it eventually ages 30 days past its own `dtstart` via the unrelated retention window. Confidence 88/100.
- **Suggested Direction**: Add a per-`(provider_type, account_name)` reconciliation step right after `upsertEvents` in `onSyncFinished`: delete rows for that account whose `uid` is not in the set just fetched, scoped to the synced date range (to avoid deleting events outside the fetch window that were never re-fetched).

### [F-03] `CalendarCache`'s account-removal cleanup API (`upsertAccount`/`clearAccountEvents`/`removeStaleAccounts`) is fully implemented but never invoked in production — removed/renamed accounts' events persist forever
- **Severity**: High
- **Effort**: M
- **Location**: `CalendarService.cpp:117-135` (`onCalendarConfigChanged`), `calendar/CalendarCache.cpp:438,469,547`
- **Rationale**: `CalendarCache` exposes exactly the config-change-lifecycle methods needed (config-hash-change detection, per-account event clearing, stale-account pruning), with doc comments describing the intended flow — but a `grep` across `libs/`/`apps/` finds zero production call sites, only test usage. Compounding this, `CalendarService::onCalendarConfigChanged` only calls `initSyncManager()` when `sync_manager_ == nullptr` — once constructed, subsequent config-change signals are silently ignored entirely, meaning the provider list and cache are never resynced with a changed config at all. **Independently flagged by two agents from different angles** (Model Contracts: model-staleness framing, 85/100; Performance: dead-API framing, 85/100) — same root cause. A user who removes or renames a CalDAV/ICS account after the shell has been running sees its old events surface indefinitely, bounded only by the unrelated 180-day retention window.
- **Suggested Direction**: Wire `removeStaleAccounts()` into `CalendarSyncManager` construction/config-reload using the active provider account-key list, and make `onCalendarConfigChanged` actually rebuild/replace `sync_manager_` (or push new account lists into it) on every config change, not just the first.

### [F-04] `LibsecretCredentialStorage`'s constructor makes a blocking synchronous D-Bus call directly on the main/UI thread
- **Severity**: Medium
- **Effort**: S
- **Location**: `calendar/LibsecretCredentialStorage.cpp:34-48`, constructed from `CalendarService.cpp:74` (`initSyncManager`)
- **Rationale**: The constructor calls `secret_service_get_sync()`, a blocking GDBus round-trip to `org.freedesktop.secrets` (potentially including service activation). It's constructed from `CalendarService::onCalendarConfigChanged()`, which runs on the main thread every time calendar accounts are (re)configured — a real UI-freeze risk if the secrets daemon is slow to activate, unlike `lookupPassword()` (correctly worker-thread-confined via `QtConcurrent::run`). Confidence 85/100.
- **Suggested Direction**: Move construction (or at least the `secret_service_get_sync` probe) off the main thread — e.g. into the same `QtConcurrent::run` task used for connection testing, or switch to the async `secret_service_get()` callback API for the initial probe.

### [F-05] `SyncError::Kind::NetworkError` is never emitted by any provider — `CalendarService::UpcomingState::Offline` is unreachable dead code
- **Severity**: Low
- **Effort**: S
- **Location**: `CalendarService.cpp:151`, `calendar/CalDavProvider.cpp:174-176`, `IcsProvider.cpp:19-21`
- **Rationale**: `CalendarService::onSyncError` maps `Kind::NetworkError` to a distinct `UpcomingState::Offline`, but both providers' `makeConnectError()` unconditionally tag every failure (DNS failure, refused connection, timeout, SSL failure, HTTP error) as `Kind::ConnectError`. `NetworkError`/`ParseError`/`StorageError` are never constructed anywhere. Confidence 85/100.
- **Suggested Direction**: Either classify failures more precisely in the providers, or collapse the unreachable `UpcomingState`/QML branch to avoid maintaining dead conditional UI logic.

### [F-06] `CalDavProvider` never deduplicates events across calendars within one account, despite declaring the intent to
- **Severity**: Medium
- **Effort**: S
- **Location**: `calendar/CalDavProvider.cpp:344-358`
- **Rationale**: `fetchEvents()` declares `QHash<QString, int> uid_dtstamp; // UID → DTSTAMP for deduplication across calendars` but never reads or writes it — the following loop simply concatenates every calendar's events with no dedup. `IcsProvider::deduplicateByUid()` implements exactly this concept correctly for the sibling provider. If the same event UID appears in two calendars under one account (shared/subscribed calendar duplicating a personal one), both copies flow through — harmless at the `CalendarCache` upsert layer (composite PK makes it idempotent) but "which duplicate wins" is arbitrary discovery order rather than the documented "latest DTSTAMP" semantics, and any other consumer of the raw `all_events` list sees literal duplicates. Confidence 85/100 (independently corroborated by a second agent at 60/100 flagging the same dead `uid_dtstamp` variable).
- **Suggested Direction**: Implement the same `deduplicateByUid`-style pass `IcsProvider` already has (ideally as a shared free function both providers call), or remove the dead variable/comment if cross-calendar duplication is deemed not to occur in practice.

### [F-07] `CalendarService`/`WeatherService` (`IActivityGate` implementations) are parented before `ActivityGateManager`, extending the dangling-gate shutdown window U-03 already found
- **Severity**: Low
- **Effort**: M
- **Location**: `apps/shell/app/ShellApplication.cpp:97,112,132,231-234`
- **Rationale**: `calendar_service_`, `weather_`, and `suspend_inhibitor_service_` are all constructed (and thus parented as `ShellApplication` children) before `activity_gate_manager_` is created and registers them via `registerGate()` in `startServices()`. Qt's front-to-back child-deletion order means these gates are destroyed *before* the manager on shutdown, and `ActivityGateManager` has no `unregisterGate()`/`destroyed()` hookup — the exact same class of bug U-03 already found for `ActivityGateManager` in isolation, now confirmed to apply identically to the two gate implementations reviewed in this unit. Not currently exploitable (no event-loop re-entry occurs during the synchronous teardown window), matching U-03's original assessment. Confidence 80/100.
- **Suggested Direction**: See U-03's [I-02] mitigation — construct `activity_gate_manager_` before any gate it will register, or add `unregisterGate()` called from each gate's destructor.

### [F-08] `CalendarCache::open()` calls `QSqlDatabase::removeDatabase()` while a live `QSqlDatabase` handle for that connection is still in scope — violates Qt's own API contract
- **Severity**: Low
- **Effort**: S
- **Location**: `calendar/CalendarCache.cpp:250,257`
- **Rationale**: Both failure paths of `open()` call `removeDatabase(connection_name_)` while the local `QSqlDatabase database` bound to that same name is still alive (not destructed until function return) — Qt's documented contract requires no live `QSqlDatabase` object referencing the connection at removal time; violating it emits a runtime warning and leaves the connection object invalid for the remainder of its scope. `CalendarCache::close()` in the same file gets this exactly right (nested-scope destruction before `removeDatabase()`), proving the correct pattern was known but not applied consistently to `open()`'s error paths. Confidence 80/100.
- **Suggested Direction**: Mirror `close()`'s nested-scope pattern (or `database = QSqlDatabase();` to drop the reference) before calling `removeDatabase()` on both error paths.

### [F-09] SSL handshake failures on CalDAV surface as an opaque "HTTP 0" with zero certificate diagnostic
- **Severity**: Medium
- **Effort**: S
- **Location**: `calendar/CalDavProvider.cpp:225-264,266-300`
- **Rationale**: No MITM risk confirmed — `ignoreSslErrors()` is never called anywhere in this unit, Qt's safe default (abort on `sslErrors`) is preserved. But nothing connects to `QNetworkReply::sslErrors` either, so a self-signed/expired/hostname-mismatched cert (common for self-hosted CalDAV, this project's explicit target audience) produces only `"HTTP 0"` — since the handshake aborted before any HTTP status arrived — indistinguishable from server-down, wrong-port, or DNS failure. `IcsProvider` is marginally better (falls back to `errorString()` when status ≤ 0, which would include Qt's SSL error text); `CalDavProvider` has no equivalent fallback. Confidence 82/100.
- **Suggested Direction**: Add the same `errorString()` fallback `IcsProvider` already has when `status <= 0`, and/or connect `sslErrors` to log the specific `QSslError` list so the real cause is diagnosable in logs even though the connection correctly aborts.

### [F-10] Duplicated synchronous-HTTP request scaffolding between `CalDavProvider` and `IcsProvider` — already diverged
- **Severity**: Medium
- **Effort**: M
- **Location**: `calendar/CalDavProvider.cpp:16-33,174-176` vs `IcsProvider.cpp:15-31,19-21`
- **Rationale**: Both independently define the same `kHttpTimeoutMs{10000}` constant and hand-roll the identical "issue request, block on local `QEventLoop` with `QTimer::singleShot` timeout" idiom, plus near-identical `makeConnectError()` helpers differing only by string prefix. The abort-on-timeout behavior has already silently diverged (`IcsProvider` calls `reply->abort()` on timeout, `CalDavProvider` does not — directly causing [F-01]) — concrete proof the duplication is a live bug source, not just style debt. Confidence 82/100.
- **Suggested Direction**: Extract a shared `HttpSyncClient` helper (owning the timeout constant and abort-on-timeout logic) used by both providers, plus a shared `makeConnectError(providerLabel, account, detail)` builder.

### [F-11] `PAT-12` confirmed: 6 non-const range-for loops in `CalendarSyncManager.cpp` are genuinely read-only and should use `const auto&`
- **Severity**: Low
- **Effort**: S
- **Location**: `calendar/CalendarSyncManager.cpp:69,75,98,101,207,213`
- **Rationale**: All six loops iterate `std::vector<std::unique_ptr<ICalendarProvider>>` and only ever dereference `provider` for read-only calls (`runCalDavSync(*provider)`, etc.) — never reassign/swap/reset the `unique_ptr` slot. A sibling loop at line 107 in the same function already correctly uses `const auto&` for the identical access pattern, proving the const form compiles and is idiomatic here. Confidence 88/100.
- **Suggested Direction**: Change all six to `const auto&`, matching the existing correct loop at line 107.

### [F-12] Retention-window values (`-30`/`+180` days) duplicated as raw literals instead of shared with `CalendarCache`'s named constants
- **Severity**: Low
- **Effort**: S
- **Location**: `calendar/CalendarSyncManager.cpp:147` vs `CalendarCache.h:67-68` (`kRetainPastDays`/`kRetainFutureDays`)
- **Rationale**: The fetch window and the prune/retention window are supposed to stay in lockstep (fetching outside retention is pointless; retaining beyond the fetch window means stale data never refreshes), but `CalendarSyncManager` hardcodes the same values as raw literals rather than referencing `CalendarCache`'s named constants — nothing enforces they can't silently drift apart. Confidence 80/100.
- **Suggested Direction**: Expose `CalendarCache::kRetainPastDays`/`kRetainFutureDays` publicly and have `CalendarSyncManager::runProviderSync` reference them directly.

### [F-13] Unnamed, asymmetric moon-phase boundary literals in `WeatherIconBridge`, using a second independent bucketing scheme from `MoonPhaseCalculator`
- **Severity**: Low
- **Effort**: S
- **Location**: `weather-icon/WeatherIconBridge.cpp:17-39`
- **Rationale**: `variantToMoonPhase()` buckets a continuous OWM `moon_phase` value into 8 phases using 7 inline literal boundaries with deliberately asymmetric widths (quarter phases ±0.01, crescent/gibbous ~0.23) and no named constants or rationale comment — a second, independently-designed bucketing philosophy from `MoonPhaseCalculator::normalizedPhaseIndex`'s equal-width 8-window scheme (reviewed and confirmed correct in U-01's context, referenced here). Confidence 80/100.
- **Suggested Direction**: Extract named boundary constants and add a comment cross-referencing why this bucketing intentionally differs from `MoonPhaseCalculator`'s (continuous OWM-supplied value vs. date-derived index), so a future reader doesn't try to incorrectly unify the two.

## Investigation Targets (confidence 60-79 — human verification needed)

#### [I-01] `WeatherIconMapper`'s `dayLayers`/`nightLayers` switches attach `default:` to the last real case, suppressing future `-Wswitch` protection
- **Severity**: Low · **Effort**: S · **Confidence**: 76/100
- **Location**: `weather-icon/WeatherIconMapper.cpp:180-183,236-239`
- **Rationale**: Both switches already list all 25 `Family` enumerators explicitly (no current diagnostic loss), but attaching `default:` to `Family::Unmapped`'s case means a future enumerator addition would silently fall through to empty-layer behavior instead of triggering a compiler warning. The same file's `moonLayerName()` and `WeatherIconBridge::moonPhaseDescription()` already use the safer post-switch-fallback pattern with no `default:` label.
- **Suggested Direction**: Split `Unmapped` into its own case with a post-switch fallback `return {}`, matching the pattern already used elsewhere in the same module.

#### [I-02] `CalendarSyncManager`'s `in_progress_`/`backoff_` maps are keyed by account name only, not `(provider_type, account_name)` — cross-provider-type collision possible
- **Severity**: Medium · **Effort**: S · **Confidence**: 70/100
- **Location**: `calendar/CalendarSyncManager.h:97-98`, `.cpp:134-145,164`
- **Rationale**: A user can legitimately configure a CalDAV account and an ICS account both named e.g. "work" (config parser has no cross-namespace uniqueness check). `CalendarCache`'s SQL schema correctly composite-keys on `(provider_type, account_name)`, but these two in-memory maps don't — a concurrent sync of both could incorrectly skip one as "already in progress," or cross-contaminate backoff state.
- **Suggested Direction**: Key both maps by a composite string or `std::pair<QString,QString>`, matching `CalendarCache`'s existing pattern.

#### [I-03] `WeatherProvider::parseReplyBody()` duplicates the inline `check_reply` validation lambda in `checkComplete()`
- **Severity**: Low · **Effort**: S · **Confidence**: 68/100
- **Location**: `weather/WeatherProvider.cpp:42-63` vs `:174-197`
- **Rationale**: Both independently implement "read body → check error → check HTTP status == 200 → parse JSON" with slightly different error-message formats, likely written independently rather than factored from one source — flagged identically by two agents.
- **Suggested Direction**: Consolidate into one parameterized helper used by both `onGeoReply()` and `checkComplete()`.

#### [I-04] `CalendarCache::open()` failure silently and permanently disables calendar persistence with only a single startup log line
- **Severity**: Medium · **Effort**: M · **Confidence**: 68/100
- **Location**: `calendar/CalendarSyncManager.cpp:33-36`, `CalendarCache.cpp:237-263`
- **Rationale**: If `open()` fails (disk full, permission denied, corrupted file), every subsequent cache method silently no-ops via an `if (!open_) return` guard — the sync loop keeps "succeeding" from the network's perspective while every write is dropped and the model always renders empty. No path from cache-open failure to `CalendarService::lastError`/`upcomingState` exists — this failure class is invisible to the existing state machine.
- **Suggested Direction**: Propagate cache-open failure as `SyncError::Kind::StorageError` (already defined, never used) through the existing `syncError` signal path.

#### [I-05] `CalendarEventModel::roleNames()` rebuilds a 29-entry `QHash` on every call
- **Severity**: Low · **Effort**: S · **Confidence**: 68/100
- **Location**: `calendar/CalendarEventModel.cpp:85-118`
- **Rationale**: Same systemic pattern as other units — 29 entries reconstructed from scratch every call rather than a cached `static const QHash`. Only one QML consumer was found, so real-world cost is likely small, but the fix is trivial.
- **Suggested Direction**: Cache as a function-local `static const QHash`.

#### [I-06] `CalendarService::upcomingEvents()` is a `const` method returning a mutable `QAbstractListModel*`
- **Severity**: Low · **Effort**: N/A · **Confidence**: 65/100
- **Location**: `CalendarService.h:46`, `.cpp:31-36`
- **Rationale**: Standard, idiomatic Qt/QML pattern (`Q_PROPERTY READ` accessors for models require this shape since QML views need a mutable handle) — flagged only because it matches the review's literal criterion. Likely not worth changing; self-caveated by the reviewing agent.
- **Suggested Direction**: No action recommended unless the team wants to tighten model-mutation boundaries generally across the codebase.

#### [I-07] `WeatherService` retries indefinitely on an invalid/revoked API key with no distinct "authentication failed" signal
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `weather/WeatherService.cpp:362-371,391-401`
- **Rationale**: A 401 response is correctly surfaced as an error message but treated identically to a transient network blip — exponential backoff plateaus (not a tight loop) but there's no way for the user to distinguish "your API key is wrong" (permanent, actionable) from generic staleness.
- **Suggested Direction**: Special-case HTTP 401/403 to stop auto-retrying and surface a distinct "check your API key" state.

#### [I-08] `WeatherService::hourlyVariant()`/`dailyVariant()` rebuild a `QVariantList` on every property read
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `weather/WeatherService.cpp:473-489`
- **Rationale**: Cost is bounded to once per weather fetch (~10 min) for binding-driven QML access, but imperative access patterns (loop indexing, `Repeater` delegate re-reads) would hit this on every access.
- **Suggested Direction**: Cache the converted list alongside `hourly_`/`daily_`, invalidated only on `forecastChanged`.

#### [I-09] `ICalParser::accumulateVEventProp` does up to ~26 sequential string comparisons per RFC 5545 property line
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `calendar/ICalParser.cpp:277-351`
- **Rationale**: An if/else-if chain dispatches on property name for every content line of every VEVENT. Runs on a background thread at most every 15/60 minutes, so not UI-blocking — capped severity accordingly.
- **Suggested Direction**: If profiling shows it matters, replace with a `static const QHash<QString, PropertyId>` dispatch table; otherwise leave as-is given the low-frequency background execution context.

#### [I-10] No range/sanity validation on weather numeric fields from the API — malformed responses propagate nonsensical values to the UI
- **Severity**: Low · **Effort**: S · **Confidence**: 62/100
- **Location**: `weather/WeatherProvider.cpp:246-282`, `WeatherData.h:78-132`
- **Rationale**: `humidity`/`clouds` (expected 0-100), `pop` (0.0-1.0), `aqi` (1-5) are read via `.toInt(0)`/`.toDouble(0.0)` with no clamping — a schema change or a proxy error page that happens to parse as valid JSON could produce nonsensical values (humidity=500) rendered as-is with no validation layer.
- **Suggested Direction**: Add `std::clamp` bounds on the fields with known valid ranges at parse time.

## Summary

| Category | Lint (reported) | Deep (confirmed ≥80) | Investigation (60-79) | Total |
|---|---|---|---|---|
| Model Contracts | 1 (MDL-7, refuted) | 2 (merged into F-02/F-03) | 1 (merged into F-06) | 2 (net, after merges) |
| Ownership & Lifecycle | 0 | 2 (F-07, F-08) | 0 | 2 |
| Thread Safety | 0 | 1 (F-04) | 0 | 1 |
| API & C++ Correctness | 0 | 2 (mandatory-check confirmations) + 1 (merged into F-01) | 2 (I-01, I-06) | 4 |
| Error Handling & Validation | ERR-9 (10), ERR-5 (5), ERR-3 (6), all deepened | 4 (merged: F-01 partial, F-05, F-09) | 3 (I-04, I-07, I-10) | 7 |
| Performance & Code Quality | PAT-12 (7, confirmed real), LCY-1 (2, deepened) | 6 (F-01 partial, F-06 partial, F-10, F-11, F-12, F-13) | 4 (I-02, I-03, I-05, I-08, I-09) | 10 |
| **Total** | **128 raw / 1 refuted / rest deepened into findings** | **13** | **10** | **23 actionable** |

23 actionable items (13 confirmed + 10 investigation targets) — this unit's real network I/O (CalDAV HTTPS, ICS downloads, weather API) produced this pipeline's densest and highest-severity lint-to-finding conversion rate, as expected for the surface area most exposed to external failure modes. **[F-01]** is this unit's Critical: a dead-code timeout guard that turns a hung CalDAV server into a silently "successful" empty sync with zero diagnostic — confirmed by three independent sources (lint, and two deep-analysis agents at matching 90/100 confidence) and directly caused by **[F-10]**'s duplicated-then-diverged HTTP scaffolding. **[F-02]**/**[F-03]** are a matched pair worth fixing together: cancelled events never get removed from the cache, and the API that would let removed *accounts'* events get cleaned up exists but was never wired in — both point to the same gap (sync only ever adds/updates, never reconciles/deletes). The mandatory checks both resolved cleanly: no `IActivityGate` inversion bug, and the WeatherIconCompositor wiring status was found to be materially different from — and better than — what prior project memory recorded.
