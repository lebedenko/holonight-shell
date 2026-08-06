# SDD Tasks — poc-remediation-phase3

## Foundation (Item 1: GuardedProcessRunner)

- [x] T-001: Implement GuardedProcessRunner helper (libs/holonight-services/src/process/)
  - REQs: REQ-F-001, REQ-NF-001
  - Check: GuardedProcessRunner.h/.cpp compile and define a free function `void runGuardedProcess(const QString& program, const QStringList& arguments, int timeout_ms, std::function<void(GuardedProcessResult)> callback)` using only public Qt6 and standard C++ types, with no clang-tidy warnings related to API stability.

- [x] T-002: GTest GuardedProcessRunner three paths (fast completion, startup error, timeout-kill)
  - REQs: REQ-F-002
  - Check: Three independent test cases verify the callback fires exactly once (via hand-written fake counter) in each scenario (fast finish, start error, timeout); all three pass with zero or multiple-call failures.

## MimeService Deduplication (Item 1, depends on T-001)

- [x] T-003: Refactor MimeService to use GuardedProcessRunner (three call sites: runXdgSettings, ProcessMimeResolver::queryDefault, ProcessMimeResolver::setDefault)
  - REQs: REQ-F-003
  - Check: All three call sites replaced with `runGuardedProcess()` invocations, preserving exact 5000ms timeouts and existing success/failure semantics; refactored code compiles with no clang-tidy warnings.

- [x] T-004: Regression test MimeService with three scenarios (success, unavailable, timeout)
  - REQs: REQ-F-003
  - Check: Hand-written fake command-runner exercises same mock scenarios as before refactor (successful query, xdg-settings unavailable, timeout-kill); output/error categorization identical before and after.

## KdeCompatService Timeout Guard (Item 3, depends on T-001)

- [x] T-005: Refactor KdeCompatService to use GuardedProcessRunner (runUpdateDesktopDatabase and runKbuildsycoca)
  - REQs: REQ-F-007, REQ-F-008
  - Check: Both methods replaced with `runGuardedProcess(5000ms)` invocations; `rebuild_in_progress_` reset (via `setRebuildInProgress(false)`) on every GuardedProcessResult path (timeout, error, normal finish), not just via direct signal handlers.

- [x] T-006: GTest KdeCompatService timeout guards (both methods, process hang scenario)
  - REQs: REQ-F-007, REQ-F-008
  - Check: Hand-written fake process that never finishes; verify `rebuild_in_progress_` reads `false` and elapsed time stays within timeout+100ms for both `runUpdateDesktopDatabase()` and `runKbuildsycoca()`.

## SystemInfoService Bounded D-Bus Timeout (Item 4, independent)

- [x] T-007: Add setTimeout(1000ms) to readAccountsService for both QDBusInterface instances (manager and user)
  - REQs: REQ-F-009, REQ-F-010, REQ-NF-003
  - Check: `SystemInfoService.cpp:readAccountsService()` adds `manager.setTimeout(kAccountsDbusTimeoutMs)` and `user.setTimeout(kAccountsDbusTimeoutMs)` before any `call()` or `property()` invocations; `kAccountsDbusTimeoutMs` defined as file-scope `constexpr int{1000}`; property declarations remain `CONSTANT`, SidebarTabBar.qml untouched.
  - **Implementation note:** the `QDBusInterface`+`setTimeout()` mechanism above was implemented, then empirically disproven by its own T-008 test — `QDBusInterface`'s constructor does an eager synchronous `Introspect` call *before* `setTimeout()` takes effect, so an unresponsive service still hung ~26s. Replaced with raw `QDBusMessage::createMethodCall()` + `QDBusConnection::call(msg, QDBus::Block, kAccountsDbusTimeoutMs)` for `FindUserById` and both property reads — no implicit introspection step. See DESIGN.md Item 4's post-implementation correction. Also added a `setDbusConnection()`/`resetDbusConnection()` test seam (mirroring `QtDbusPropertyClient`'s existing precedent) since the class had no way to redirect off the real system bus for testing.

- [x] T-008: GTest SystemInfoService D-Bus timeout bounding (never-responding Accounts, 2s-delayed Accounts)
  - REQs: REQ-F-009, REQ-F-010
  - Check: Hand-written fake Accounts D-Bus service (on a dedicated thread with its own private session-bus connection); constructor executes within timeout window (confirmed via `std::chrono` wall-clock timer: ~1.1s for never-responding, ~1.1s for 2s-delayed — both bounded by the client's own timeout rather than the server's behavior), with fallback (empty) values in `avatarPath`/`userName`/`realName` properties.

## LibsecretCredentialStorage Async Constructor Probe (Item 5, independent)

- [x] T-009: Convert service_available_ to shared_ptr<std::atomic<bool>> and offload probe to QtConcurrent::run
  - REQs: REQ-F-011, REQ-F-012, REQ-F-013
  - Check: `LibsecretCredentialStorage.h` declares `service_available_` as `std::shared_ptr<std::atomic<bool>>`; constructor body contains only `QtConcurrent::run([flag]() { ... secret_service_get_sync(...) })` lambda (no blocking call on constructor's thread); `isServiceAvailable()` returns `service_available_->load(std::memory_order_relaxed)`.

- [x] T-010: GTest async probe dispatch overhead (measure constructor wall-clock time, not injected delay)
  - REQs: REQ-F-011
  - Check: Constructor execution time (via `std::chrono::high_resolution_clock`) is <5ms (demonstrating only dispatch overhead, not actual D-Bus delay); no fake-injection seam added to constructor signature.

- [x] T-011: GTest accepted race window self-healing (sync initiated before and after probe completion)
  - REQs: REQ-F-012
  - Check: Intentional race scenario: `lookupPassword()` called before probe completes returns false/nullopt; same call after probe finishes returns true/password (assuming valid credentials); no blocking/gating code added; inline comments document the accepted race window.
  - **Implementation note:** no real libsecret backend is registered in the offscreen CI sandbox, so this environment cannot assert a genuine false→true transition; instead the test asserts the read is non-blocking/non-crashing immediately after construction and that the settled value is stable 500ms later (two consecutive reads agree), which is the property REQ-F-012 actually needs verified without depending on host secrets-daemon availability.

- [x] T-012: Verify lookupPassword() signature and behavior unchanged
  - REQs: REQ-F-013
  - Check: All existing GTests for `lookupPassword()` pass without source modification; method signature, `const`-ness, and test fakes remain identical; verify only the internal boolean test changes from plain read to atomic load.
  - **Implementation note:** no pre-existing GTests call `lookupPassword()` directly (only `CalDavProvider` calls it in production code, and it isn't currently unit-tested against a real `LibsecretCredentialStorage`), so this check is satisfied by inspection: `LibsecretCredentialStorage.h`'s `lookupPassword()` declaration is byte-identical to before this phase (same signature, same `const`), and `.cpp`'s only change is `!service_available_` → `!service_available_->load(std::memory_order_relaxed)`.

## LauncherService Cache for scanForDefaultApps (Item 6, independent)

- [x] T-013: Implement default_apps_cache using dirty-flag pattern (mirror category_counts_cache)
  - REQs: REQ-F-014, REQ-F-015, REQ-NF-004
  - Check: `LauncherService.h` adds `mutable QVector<DesktopEntry> default_apps_cache_` and `mutable bool default_apps_cache_dirty_{true}`; `.cpp` adds `cachedDefaultApps()` and `invalidateDefaultAppsCache()` methods; `rebuildDesktopFileIndex()` calls `invalidateDefaultAppsCache()` alongside existing `invalidateCategoryCache()` call; three `defaultAppEntriesFor*` methods call `cachedDefaultApps()` instead of `scanner_.scanForDefaultApps()`.

- [x] T-014: GTest LauncherService cache hit/miss and invalidation
  - REQs: REQ-F-014, REQ-F-015
  - Check: Hand-written fake scanner with call-count spy; verify scan invoked exactly once on first `defaultAppEntriesForMimeTypes()` call, not invoked on second call (cache hit), re-invoked after `rebuildDesktopFileIndex()` (cache invalidation).
  - **Implementation note:** `DesktopEntryScanner` is a concrete class with no virtual seam, and DESIGN.md Item 6 explicitly rejects adding one (caching belongs in the caller, not the scanner) — so no fake-scanner-with-call-count-spy is possible without an unwanted production change. Instead the test (`DefaultAppEntriesCachesScanResultUntilIndexRebuild`) proves the same hit/miss/invalidation lifecycle via an observable filesystem side effect: delete the backing `.desktop` file between calls — a cache hit returns stale (pre-deletion) data, and only after `reload()`'s `modelReset` → `rebuildDesktopFileIndex()` cycle does a subsequent call reflect the deletion.

## SessionIntegrationService Parallelization (Item 2, requires Item 1 helper, special infrastructure)

- [x] T-015: Refactor add*Diagnostics methods from void-mutating to pure methods returning QVariantList
  - REQs: (structural foundation for REQ-F-004, REQ-F-005, REQ-F-006)
  - Check: All seven `add*Diagnostics()` private methods converted to `[[nodiscard]] QVariantList add*Diagnostics() const`; each returns local QVariantList built from calls to refactored `addDiagnostic()` (now a pure row-builder, not appending to `diagnostics_` member); compiles with no clang-tidy warnings.
  - **Implementation note:** `addDiagnostic()` itself became `[[nodiscard]] static QVariantMap addDiagnostic(...)` (returns one row instead of appending). `collectDiagnostics()` now does `diagnostics_.append(addXDiagnostics())` for each of the seven, preserving identical row order/content — confirmed by all 14 pre-existing `SessionIntegrationServiceTest` cases passing unchanged. `addLastRebuildDiagnostics()` (the 8th, non-parallelized method reading `last_rebuild_steps_`) stays void/mutating per DESIGN.md, updated only to call the new `addDiagnostic()` signature via `diagnostics_.append(...)`.

- [x] T-016: Set up test infrastructure for async SessionIntegrationService tests (event-loop pumping)
  - REQs: (infrastructure for REQ-F-004, REQ-F-005, REQ-F-006 tests)
  - Check: Existing `tests/test_session_integration_service.cpp` augmented with `QSignalSpy` and `QTest::qWait()`-style fixtures capable of pumping the Qt event loop and waiting for `QFutureWatcher::finished` signals; existing plain `TEST()` fixtures for non-async behavior remain intact.
  - **Implementation note:** added `refreshAndWait()`/`rebuildAndWait()` helpers that construct a `QSignalSpy` on `diagnosticsChanged` *before* triggering the action, then only call `spy.wait(2000)` if the spy is still empty afterward. This is correct both today (refresh() is synchronous, so the spy is already non-empty and no event-loop wait is needed) and once T-017 makes `refresh()` dispatch async futures (spy starts empty, so `wait()` pumps the event loop until `QFutureWatcher::finished`'s queued slot fires). All 14 pre-existing `SessionIntegrationServiceTest` call sites (plus `RebuildApplicationCaches*`) were switched from raw `service.refresh()`/`service.rebuildApplicationCaches()` to these helpers so they stay green across the T-017 sync→async transition instead of only being fixed for new REQ-F-004/005/006 tests; TEST() bodies themselves (gtest fixtures) were left untouched — only the call sites for the async-triggering methods changed. Full 493/493 regression suite still passes.

- [x] T-017: Implement parallel refresh() using QtConcurrent::run and QtFuture::whenAll (dispatch, not sequential)
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-NF-002
  - Check: `SessionIntegrationService::refresh()` launches all seven diagnostic futures via `QtConcurrent::run([this, fn]() { return (this->*fn)(); })` into a `QVector<QFuture<QVariantList>>`; waits on `QtFuture::whenAll(futures.begin(), futures.end())` via `QFutureWatcher::finished` signal (not `QFutureSynchronizer::waitForFinished()`); aggregates results into `diagnostics_` only after all seven complete; `ISessionIntegrationCommandRunner` interface unchanged.
  - **Implementation note:** implemented verbatim per DESIGN.md's Item 2 code sample. `collectDiagnostics()` (now unused — its whole body was the sequential dispatch this task replaces) was deleted from both `.h` and `.cpp` rather than left as dead code. Loop variables renamed `fn`→`diag_fn` and `f`→`future` to satisfy `.clang-tidy`'s 3-character minimum identifier length. Added `#include <QFuture>`, `#include <QFutureWatcher>`, `#include <QtConcurrent/QtConcurrentRun>` to the `.cpp`. All 15 `SessionIntegrationServiceTest`/`ApplicationCacheRebuilderTest` cases pass through the T-016 `refreshAndWait()`/`rebuildAndWait()` helpers (now genuinely exercising the async event-loop-wait path, not just the synchronous short-circuit), and the full 493/493 regression suite and `holonight-shell` binary both build/pass clean.

- [x] T-018: GTest SessionIntegrationService parallelization and timing (REQ-F-004, REQ-F-005)
  - REQs: REQ-F-004, REQ-F-005
  - Check: Hand-written fake runner simulating 7 diagnostics at 200ms each; measure total `refresh()` execution time; verify parallel behavior (<300ms including jitter) not sequential (~1400ms); confirm all 7 diagnostics present in aggregated output; verify overlapping start times of at least two diagnostics (second starts before first completes).
  - **Implementation note:** a literal "7 diagnostics at 200ms each" fake isn't achievable against the real methods — only 5 of the 7 `add*Diagnostics()` methods make any external call at all (`addProcessEnvironmentDiagnostics`/`addDbusActivationDiagnostics` read only in-process environment variables), and `addMimeDiagnostics`/`addPortalAndDesktopServiceDiagnostics` each make 6+ external calls internally (sequential within their own worker thread), so a naive per-call sleep would make those two individually far slower than 200ms. Implemented `DelayTimeline`/`DelayingCommandRunner`/`DelayingBusProbe` in the test file: a shared, mutex-guarded timeline whose `delayOncePerThread()` sleeps 200ms only on the *first* external call a given QtConcurrent worker thread makes (to either fake), recording one `[start, end]` interval per delayed diagnostic and no-op'ing on subsequent calls from that same thread — this gives each of the 5 externally-calling diagnostics exactly one comparable ~200ms interval without penalizing multi-call methods. Also added a `ThreadPoolCapacityGuard` that temporarily raises `QThreadPool::globalInstance()->maxThreadCount()` to at least 7 for the test's duration (restored via RAII destructor, safe across `ASSERT_*` early returns) so a low-core CI host can't force some of the 7 dispatched futures to queue and run sequentially. New test `RefreshDispatchesDiagnosticsInParallelNotSequentially`: asserts wall-clock `refresh()` completion in <500ms (vs. an estimated ~1000ms if the 5 delayed diagnostics ran sequentially), asserts all 7 diagnostic categories are present in the aggregated output (one representative id per category), and asserts at least 2 recorded intervals overlap. Verified stable across 5 repeated runs (~201-202ms each, no flakiness observed). Full 846/846 regression suite, `task format-check`, and `task tidy` all clean for the touched file (one pre-existing unrelated `task tidy` warning remains in `test_hyprland_ipc_client.cpp`, per CLAUDE.md's documented gotcha).

- [x] T-019: GTest SessionIntegrationService rebuildApplicationCaches redundancy (REQ-F-006)
  - REQs: REQ-F-006
  - Check: Hand-written spy counts total `command_runner_->run()` invocations during single `rebuildApplicationCaches()` call; expect exactly 7 (one diagnostic pass), not 14 (two passes); document expected call sequence in test.
  - **Implementation note:** SPEC.md's REQ-F-006 acceptance criterion frames "7, not 14" as an illustrative example ("if a single refresh() invokes 7 distinct commands..."), not a literal requirement — the real number depends on which fake executables/results are configured. Added `ThreadSafeSpyCommandRunner` (mutex-guarded `calls`/`mutex` as public fields, matching `FakeCommandRunner`'s all-public-data convention — `cppcoreguidelines-non-private-member-variables-in-classes` flags a class that mixes public data with a private member, so the spy state stays public rather than adding a private mutex alongside public `executables`/`results`) since `rebuildApplicationCaches()`'s trailing `refresh()` now dispatches diagnostics onto concurrent worker threads that call `run()` at the same time, and the pre-existing `FakeCommandRunner.calls` is unguarded. New test `RebuildApplicationCachesIssuesExactlyOneRebuildAndOneDiagnosticsPass` configures the fake so exactly 7 distinct diagnostic commands are actually invoked (1 `systemctl` + 6 mime-role commands from `addSystemdEnvironmentDiagnostics`/`addMimeDiagnostics` — the only two `add*Diagnostics()` methods that call `run()`), asserts the first 2 calls (from the synchronous, sequential `ApplicationCacheRebuilder::rebuild()` step) are in fixed order, asserts the remaining 7 calls match the expected diagnostic-command set exactly (order-independent, since they race across worker threads), and asserts the grand total is exactly 9 — not 18, which is what a regression reintroducing a second rebuild-and-refresh pass would produce. Verified stable across 5 repeated runs. Full 847/847 regression suite, `task format-check`, and `task tidy` all clean for the touched file.

- [x] T-020: Audit QML call sites for stale-read assumptions (risk mitigation for async behavior change)
  - REQs: (risk mitigation, cross-cutting)
  - Check: Grep `SidebarSystem.qml` and diagnostics-screen QML for patterns like `refresh(); console.log(diagnostics)` (sync assumption); verify all call sites either (a) gate on `refreshInProgress` before reading, or (b) are not affected by stale data; document findings and any required QML changes.
  - **Implementation note:** `grep -rn "SessionIntegrationService" apps/shell/qml/` confirms `apps/shell/qml/RightSidebar/Tabs/System/SidebarSystem.qml` is the *only* QML file referencing the singleton (no other diagnostics-screen QML exists); `ShellApplication.cpp` only constructs/registers it and wires `setPostRebuildRefreshCallbacks` (both unaffected by T-017 — the callbacks fire synchronously inside `rebuildApplicationCaches()`, before the trailing async `refresh()` dispatch). Every `SessionIntegrationService` reference in `SidebarSystem.qml` was checked: (1) `failingDiagnostics` (line 12) and the two `overallStatus` bindings (lines 150-151) are plain QML property bindings on `diagnostics`/`overallStatus`, both `NOTIFY diagnosticsChanged` — QML bindings re-evaluate automatically whenever the NOTIFY signal fires, so they are never "stale-read after a synchronous call" in the way `refresh(); console.log(diagnostics)` would be, regardless of whether `refresh()` completes synchronously or asynchronously; (2) `Component.onCompleted: SessionIntegrationService.refresh()` (line 51) has no synchronous follow-up read at all — it only triggers the async dispatch, and the bindings above pick up the eventual result on their own; (3) both action buttons (Refresh line 218-244, Rebuild line 246-268) already gate their `enabled`/`text` state on `refreshInProgress`/`rebuildInProgress` (lines 220-222, 248-249) and neither `onClicked` handler (line 226, 253) reads `diagnostics` synchronously afterward. Conclusion: no QML call site assumes synchronous completion; the sync→async behavior change from T-017 requires **no QML changes**.

---

## Summary

**19 tasks covering all 18 requirements:**

| Category | Count | Task IDs |
|----------|-------|----------|
| Helper & foundation tests | 2 | T-001, T-002 |
| MimeService dedup | 2 | T-003, T-004 |
| KdeCompatService timeout | 2 | T-005, T-006 |
| SystemInfoService D-Bus | 2 | T-007, T-008 |
| LibsecretCredentialStorage async | 4 | T-009, T-010, T-011, T-012 |
| LauncherService cache | 2 | T-013, T-014 |
| SessionIntegrationService parallel | 6 | T-015, T-016, T-017, T-018, T-019, T-020 |
| **Total** | **20** | |

**Dependency order:**
- T-001, T-002 (GuardedProcessRunner foundation)
- T-003, T-004, T-005, T-006 (MimeService, KdeCompatService — depend on T-001)
- T-007, T-008, T-009–T-014 (SystemInfoService, LibsecretCredentialStorage, LauncherService — independent)
- T-015–T-020 (SessionIntegrationService parallelization — standalone structural work, then async implementation)

All requirements map to at least one task; each task is independently verifiable via GTest assertions, hand-written fakes, and wall-clock timing measurements. No live-compositor smoke tests required.
