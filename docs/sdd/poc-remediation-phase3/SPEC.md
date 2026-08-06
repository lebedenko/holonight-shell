# POC Remediation Phase 3 SPEC

## Overview

Phase 3 addresses the "UI-thread blocking I/O cluster" identified in the POC Readiness Review (docs/sdd/poc-readiness-review/REPORT.md §7, Gap #5). The codebase contains six distinct I/O bottlenecks — QProcess invocations, D-Bus calls, and secret-service probes — that block the main thread during shell initialization and runtime operations. This phase eliminates the worst cases via helper consolidation, parallelization, timeout bounding, and caching.

**Key goals:**
- Reduce worst-case startup blocking from ~35 seconds to ~2–3 seconds (worst single I/O, not sum).
- Guarantee no indefinite main-thread hangs due to unresponsive system services.
- Consolidate duplicated timeout-guard logic into one reusable helper.
- Cache redundant filesystem scans without adding new infrastructure.

Verification is GTest-only; acceptance criteria are falsifiable and map to GTest assertions (call-count spies, timing measurements, state-flag checks). No live-compositor smoke tests are required.

---

## 1. Shared Guarded QProcess Helper and MimeService Deduplication

### Context

`MimeService.cpp` contains three independent call sites (`runXdgSettings`, `ProcessMimeResolver::queryDefault`, `ProcessMimeResolver::setDefault`), each manually implementing the same pattern:
- Spawn a `QProcess` for an external program.
- Arm a `QTimer` with a 5-second timeout that calls `proc->kill()`.
- Connect `finished` and `errorOccurred` signal handlers.
- Guard against double-firing (timeout-kill vs. normal completion) using a `shared_ptr<bool>` re-entrancy flag.

This pattern is error-prone (easy to miss the re-entrancy guard), duplicates ~40–50 lines per site, and uses an inconsistent timeout value across sites.

### Requirements

**REQ-F-001: Shared guarded-QProcess helper function**

*When* a caller needs to execute an external program with guaranteed timeout protection, *the system* shall provide a single helper function that encapsulates QProcess execution, timeout-kill logic, and callback delivery.

- **Acceptance:** A unit test constructs the helper with program="sleep", args=["1"], timeout=100ms, and a callback. The callback fires exactly once (not zero, not multiple) indicating timeout-kill status. Verify via `EXPECT_CALL` mock spy with `Times(Exactly(1))`.

---

**REQ-F-002: Exactly-once callback guarantee under all completion paths**

*If* a process completes normally, encounters an error, or is killed by timeout, *the system* shall guarantee the completion callback fires exactly once regardless of the path taken.

- **Acceptance:** Three separate test cases:
  1. Process completes after 50ms (fast) → callback fires 1 time.
  2. Process encounters startup error → callback fires 1 time.
  3. Process hangs, timeout fires at 100ms → callback fires 1 time.

  Use `EXPECT_CALL(..., Times(Exactly(1)))` in each case. If any test shows 0 or 2+ calls, the requirement fails.

---

**REQ-F-003: MimeService migration to shared helper**

*When* MimeService's three existing QProcess call sites (runXdgSettings, ProcessMimeResolver::queryDefault, ProcessMimeResolver::setDefault) are refactored to use the shared helper, *the system* shall preserve identical timeout values (5000ms) and success/failure semantics as the original code.

- **Acceptance:** A regression test that exercises MimeService with the same mock scenarios as before the refactor (successful query, xdg-settings unavailable, timeout-kill) produces identical output/error categorization before and after. Optionally compare captured error strings or return-value enums to ensure no semantic change.

---

**REQ-NF-001: Helper interface stability**

*The system* shall provide the shared helper with a stable, general-purpose C++ interface suitable for both MimeService and future call sites; the interface shall not introduce Qt private APIs or undocumented behavior.

- **Acceptance:** The helper function signature uses only public Qt6 (QProcess, QTimer, QObject, std::function) and standard C++ types (std::string, int, callback). No Q_PRIVATE_SLOT or moc-generated methods. Compile successfully in the project's normal build with no warnings from clang-tidy related to API stability.

---

## 2. SessionIntegrationService Parallelization and Redundancy Removal

### Context

`SessionIntegrationService::refresh()` sequentially calls up to ~7 diagnostic-collection methods:
- `addProcessEnvironmentDiagnostics`
- `addSystemdEnvironmentDiagnostics`
- `addDbusActivationDiagnostics`
- `addXdgMenuDiagnostics`
- `addKdeCacheDiagnostics`
- `addMimeDiagnostics`
- `addPortalAndDesktopServiceDiagnostics`

Many of these invoke `ISessionIntegrationCommandRunner::run()`, a synchronous QProcess-based call with ~1s start timeout + 5s finish timeout per invocation. Sequential execution chains to ~35 seconds worst-case. Additionally, `rebuildApplicationCaches()` unconditionally calls `refresh()` again at its end, causing redundant re-execution of all diagnostics.

### Requirements

**REQ-F-004: Parallel diagnostic collection**

*When* `SessionIntegrationService::refresh()` is invoked, *the system* shall launch all seven diagnostic-collection methods in parallel using `QtConcurrent::run()` rather than executing them sequentially.

- **Acceptance:** A unit test injects a spy on `ISessionIntegrationCommandRunner::run()` to log method entry/exit timestamps for each diagnostic. Measure the start times of at least two diagnostic methods; verify that the second method starts *before* the first completes (demonstrating parallelism), not strictly sequential start-wait-complete-next-start. A 50+ ms overlap is sufficient proof.

---

**REQ-F-005: Parallel completion and result aggregation**

*While* multiple diagnostic futures are executing in parallel, *the system* shall not return from `refresh()` until all futures complete; results shall be aggregated into the shared diagnostics state.

- **Acceptance:** A test injects fakes that simulate 7 diagnostics at 200ms each (total parallel time ~200ms if truly concurrent, ~1400ms if sequential). Measure total `refresh()` execution time; if parallel, time is ~200–250ms; if sequential, time is ~1400+ ms. Expect parallel behavior (time ≤ 300ms with 100ms jitter allowance). No partial-result leaking or race conditions; verify all 7 diagnostics appear in the aggregated output.

---

**REQ-F-006: Remove redundant refresh in rebuildApplicationCaches**

*When* `SessionIntegrationService::rebuildApplicationCaches()` completes, *the system* shall invoke refresh diagnostics exactly once (not twice) during the rebuild cycle.

- **Acceptance:** A test injects a spy on the command-runner counting invocations of `run()`; calls `rebuildApplicationCaches()` once; verifies that the total command count is consistent with a single refresh pass. For example, if a single refresh() invokes 7 distinct commands, expect 7 total calls, not 14. Document the exact expected call sequence in the test.

---

**REQ-NF-002: Synchronous command-runner interface stability**

*The system* shall not modify the `ISessionIntegrationCommandRunner` interface signature; all existing test fakes and production implementations shall compile and execute without change.

- **Acceptance:** All existing C++ GTests that use `ISessionIntegrationCommandRunner` fakes or mocks pass without any modification to those fakes' source code (constructors, method signatures, return types, slots). A check for "all tests in `tests/test_session_integration_service.cpp` pass" is sufficient.

---

## 3. KdeCompatService Timeout Guard

### Context

`KdeCompatService::runUpdateDesktopDatabase()` and `KdeCompatService::runKbuildsycoca()` spawn `QProcess` instances asynchronously with `finished` and `errorOccurred` signal handlers, but lack explicit kill-timers or timeout guards. If a child process hangs indefinitely, the `rebuild_in_progress_` state flag remains `true` permanently, potentially wedging UI code that depends on this flag.

### Requirements

**REQ-F-007: runUpdateDesktopDatabase timeout guard**

*While* `KdeCompatService::runUpdateDesktopDatabase()` is executing, *the system* shall enforce an explicit bounded timeout; if the process does not complete within the timeout, the process shall be killed and the `rebuild_in_progress_` state flag shall be reset to `false`.

- **Acceptance:** A test injects a fake process that never signals `finished` (simulating a hang); calls `runUpdateDesktopDatabase()`; measures elapsed time; after the timeout expires (e.g., 5000ms), verifies that:
  1. The process is in killed/terminated state.
  2. The `rebuild_in_progress_` flag reads as `false` (via property accessor or spy).
  3. The total execution time is within timeout + 100ms (not indefinite).

---

**REQ-F-008: runKbuildsycoca timeout guard**

*While* `KdeCompatService::runKbuildsycoca()` is executing, *the system* shall enforce an explicit bounded timeout identical to `runUpdateDesktopDatabase()`; if the process hangs, it shall be killed and `rebuild_in_progress_` shall reset to `false`.

- **Acceptance:** Identical test scenario to REQ-F-007, but invoking `runKbuildsycoca()`. Verify process kill, state-flag reset, and timeout completion within the same bounded window.

---

## 4. SystemInfoService Bounded D-Bus Timeout

### Context

`SystemInfoService::readAccountsService()` makes blocking synchronous `QDBusInterface` calls (`FindUserById`, then property reads for `IconFile`/`UserName`) against `org.freedesktop.Accounts` using the OS-default D-Bus timeout (~25 seconds). The constructor is called during shell startup on the main thread; if Accounts is slow or unresponsive, the shell blocks for tens of seconds.

Properties (`avatarPath`, `userName`, `realName`, `name`, `displayName`, `logoIconName`, `logoSource`) are declared as `CONSTANT` to avoid breaking `SidebarTabBar.qml` bindings; a full async rewrite is explicitly out of scope for this phase.

### Requirements

**REQ-F-009: D-Bus call timeout bounding**

*When* `SystemInfoService::readAccountsService()` invokes QDBusInterface methods against `org.freedesktop.Accounts`, *the system* shall set an explicit bounded timeout (order of a few hundred milliseconds to low seconds, exact value determined in Design stage) on all QDBusPendingReply/`QDBusInterface::call()` operations.

- **Acceptance:** A test mocks `org.freedesktop.Accounts` with a fake service that never responds to any call; constructs `SystemInfoService` and measures the time spent in `readAccountsService()` (e.g., via a spy on `QDBusPendingReply` or by elapsed-time assertion on constructor). Verify that the method returns within the timeout window (e.g., <500ms) rather than blocking for the OS-default timeout (>20s).

---

**REQ-F-010: Prompt fallback on Accounts unavailability**

*If* the Accounts service is unresponsive or unavailable, *the system* shall return promptly from the constructor using default/empty property values (as the class already does when Accounts data is unavailable), rather than blocking for extended periods.

- **Acceptance:** A test injects a slow-responding Accounts backend (e.g., 2s before timeout); constructs `SystemInfoService`; verifies:
  1. Total constructor time is <3s (timeout + small jitter), not >20s.
  2. Properties `avatarPath`, `userName`, `realName` contain their fallback values (empty string or default avatar path).
  3. No error-log spam or fatal crashes due to timeout.

---

**REQ-NF-003: Constant properties unchanged**

*The system* shall not modify the `CONSTANT` property declarations on `avatarPath`, `userName`, `realName`, `name`, `displayName`, `logoIconName`, `logoSource`, and shall not introduce `NOTIFY` signals or alter `SidebarTabBar.qml` bindings.

- **Acceptance:** `SidebarTabBar.qml` is not modified; property declarations in `SystemInfoService.h` remain `CONSTANT`; all existing QML tests binding to these properties continue to pass.

---

## 5. LibsecretCredentialStorage Async Constructor Probe

### Context

`LibsecretCredentialStorage::LibsecretCredentialStorage()` calls blocking `secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &error)` directly on the constructing thread. The class is instantiated in `CalendarService::initSyncManager()`, which runs on the main thread (triggered by `onCalendarConfigChanged()`, a main-thread slot). Every calendar config change causes a main-thread block on this D-Bus/secret-service round trip.

The `lookupPassword()` method on the same class is already thread-safe and runs exclusively via `QtConcurrent::run()` contexts in production; this requirement scopes only to the constructor probe, not `lookupPassword()`.

### Requirements

**REQ-F-011: Async constructor probe**

*When* `LibsecretCredentialStorage` is constructed, *the system* shall offload the `secret_service_get_sync()` probe to a background thread (via `QtConcurrent::run` or equivalent) rather than blocking the calling thread.

- **Acceptance:** A test constructs `LibsecretCredentialStorage` on the main thread while injecting a slow fake secret-service backend (e.g., 500ms response delay). Measure the constructor's return time; verify it is <50ms (demonstrating async execution off the main thread), not blocking for the full 500ms. Use a wall-clock timer (`std::chrono`) to measure elapsed time between construction start and return.

---

**REQ-F-012: Accepted race window on early sync**

*While* the async constructor probe is in flight, the system shall accept an intentional self-healing race window where a calendar sync initiated before probe completion may fail or skip due to `service_available_` reading as uninitialized; subsequent retry cycles after probe completion shall succeed.

- **Acceptance:** No gating, blocking, or synchronization mechanism shall be added to close this race. Document the accepted race in inline code comments. A test that intentionally triggers a sync before probe completion demonstrates the failure mode and verifies that a subsequent retry after probe completion succeeds (demonstrating self-healing). No "wait for probe" blocking calls are permitted.

---

**REQ-F-013: lookupPassword unchanged**

*The system* shall not modify the `lookupPassword()` method's thread-safety guarantees, calling convention, or test fakes; it shall remain callable exclusively from `QtConcurrent::run` contexts as today.

- **Acceptance:** All existing GTests for `lookupPassword()` pass without source modification; the method's signature and behavioral contract remain identical before and after this phase. Verify that production code calls `lookupPassword()` exclusively from `CalDavProvider::testConnection()` and `CalDavProvider::fetchEvents()` (both invoked via `QtConcurrent::run()`).

---

## 6. LauncherService Cache for scanForDefaultApps

### Context

`SidebarSystem.qml` instantiates `DefaultAppRow.qml` six times (lines ~70–122), and each instantiation independently triggers a fresh uncached filesystem scan via `LauncherService::DesktopEntryScanner scanner_` through calls to `defaultAppEntriesForMimeTypes()`, `defaultAppEntriesForMimeTypesAndCategories()`, or `defaultAppEntriesForCategory()`. Every System-tab open incurs 6 redundant identical scans. `LauncherService` already implements a category-count cache using a `category_counts_dirty_` mutable bool flag with invalidation in `rebuildDesktopFileIndex()`; the pattern is proven and minimal.

### Requirements

**REQ-F-014: Cache for scanForDefaultApps result**

*When* `LauncherService` methods (`defaultAppEntriesForMimeTypes()`, etc.) invoke `scanForDefaultApps()`, *the system* shall return a cached result on repeated calls (within a single tab-open cycle) rather than re-invoking the filesystem scan each time.

- **Acceptance:** A test calls `defaultAppEntriesForMimeTypes({"application/json"})` three times in succession without invalidation; spies on the underlying filesystem scan (e.g., via `DesktopEntryScanner::scan()` call count). Verify the scan is invoked exactly once, with the result reused on the second and third calls. Use `EXPECT_CALL(..., Times(Exactly(1)))` on the filesystem-scan entry point.

---

**REQ-F-015: Cache invalidation in rebuildDesktopFileIndex**

*When* `LauncherService::rebuildDesktopFileIndex()` executes, *the system* shall invalidate the `scanForDefaultApps()` cache using the same dirty-flag mechanism as `category_counts_dirty_`, ensuring the next call re-invokes the filesystem scan.

- **Acceptance:** A test:
  1. Calls `defaultAppEntriesForMimeTypes()` → filesystem scan invoked once (cache miss).
  2. Calls `defaultAppEntriesForMimeTypes()` again → filesystem scan NOT invoked (cache hit).
  3. Calls `rebuildDesktopFileIndex()` → cache is invalidated.
  4. Calls `defaultAppEntriesForMimeTypes()` again → filesystem scan IS invoked (cache miss after invalidation).

Use spy call-count assertions (`Times(1)` for first call, `Times(0)` for second, `Times(1)` for fourth) to verify this sequence.

---

**REQ-NF-004: No new filesystem-watching infrastructure**

*The system* shall not introduce new `QFileSystemWatcher` instances, new `QTimer` debounce timers, or new signal/slot connections beyond integration with the existing invalidation trigger in `rebuildDesktopFileIndex()`.

- **Acceptance:** The implementation adds at most one mutable bool flag (e.g., `scanForDefaultApps_cache_dirty_`) and hooks it at the existing invalidation point. No new `QFileSystemWatcher` constructor calls appear in `LauncherService.cpp` after this phase. Verify by counting added lines in the implementation vs. the architecture budget (typically <20 LOC for a cache-flag pattern).

---

## Non-Goals

The following items are **explicitly out of scope** for Phase 3 and are reserved for future phases:

1. **SystemInfoService property async conversion:** Converting `CONSTANT` properties on `SystemInfoService` to `NOTIFY` properties, or refactoring `readAccountsService()` into a fully asynchronous call path. This phase bounds the timeout only.

2. **LibsecretCredentialStorage race-window gating:** Building any blocking, polling, or synchronization mechanism to close the accepted race window where an early sync may fail before the async probe completes.

3. **ISessionIntegrationCommandRunner interface async migration:** Converting `ISessionIntegrationCommandRunner` to an asynchronous/callback-based interface. Parallelism is achieved by wrapping the synchronous interface in `QtConcurrent::run()`, not by changing the interface itself.

4. **Live-compositor smoke testing:** Phase 3 acceptance criteria are GTest-only. No live Hyprland environment testing, no manual popup/sidebar/widget behavior verification, no screenshot validation.

5. **Phase 4+ roadmap items:** Any remediation items from REPORT.md Phases 4, 5, or 6 are deferred to their respective SDD cycles.

---

## Verification Strategy

**GTest-Only Approach:**

All acceptance criteria in this SPEC are phrased to map directly onto GTest assertions and do not require a live compositor or manual testing:

- **Call-count verification:** `EXPECT_CALL(mock, Method()).Times(Exactly(N))` or via `ON_CALL` + explicit counter spy.
- **Timing bounds:** `EXPECT_LT(elapsed_ms, timeout_ms + jitter)` using `std::chrono::high_resolution_clock`.
- **State-flag assertions:** `EXPECT_FALSE(service_->rebuild_in_progress())` or direct member inspection in friends/test fixtures.
- **Thread identity:** `EXPECT_NE(QThread::currentThread(), QCoreApplication::instance()->thread())` to prove off-thread execution.
- **Cache hit/miss:** Spy on filesystem scan entry point; verify invocation count matches expected pattern.

**No Smoke Tests:**

This phase does NOT require:
- Launching Hyprland or any live Wayland compositor.
- Opening the shell sidebar, toggling System tab, or visually inspecting UI responsiveness.
- Measuring wall-clock shell-startup time in a live session.
- Manual interaction with calendar config dialogs or mime-type defaults.

**Test Infrastructure:**

Tests are written using Qt's `QTest` framework (for main-thread test fixtures) and Google Test (`gtest`) for off-thread and utility-function testing. All mocking uses Qt's `QTest::ignoreMessage()`, `QSignalSpy`, or `std::function` callbacks; no external mocking library is required.

---

## Summary

**6 scope items, 15 requirements:**

| Category | Count | IDs |
|----------|-------|-----|
| Functional | 14 | REQ-F-001 to REQ-F-015 |
| Non-Functional | 4 | REQ-NF-001 to REQ-NF-004 |
| **Total** | **18** | |

Each requirement has a distinct acceptance criterion phrased as a falsifiable GTest assertion. No implementation or design details appear in this SPEC (reserved for Stage 2, Design). The five non-goals are clearly listed to prevent scope creep.
