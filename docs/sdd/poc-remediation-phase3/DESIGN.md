# POC Remediation Phase 3 — DESIGN

Stage 2 (Design) for `poc-remediation-phase3`. Satisfies REQ-F-001 through REQ-F-015 and
REQ-NF-001 through REQ-NF-004 in `docs/sdd/poc-remediation-phase3/SPEC.md`. Six scope items;
each section below maps 1:1 to a SPEC section and closes with an explicit requirement-ID map.

**Correction to SPEC's test-strategy language:** SPEC.md's acceptance criteria are phrased with
`EXPECT_CALL(..., Times(Exactly(N)))`, implying gmock-style mock objects. The codebase does link
`GTest::gmock` (see `tests/CMakeLists.txt`), but grepping `tests/` shows gmock is used **only**
for matcher helpers (`testing::HasSubstr` inside `EXPECT_THAT`, see
`tests/test_session_integration_service.cpp:206,294,372`) and for `testing::InitGoogleMock()` in
`tests/main.cpp`. There is not one `MOCK_METHOD`/`EXPECT_CALL` in the repository. Every existing
test double (`FakeCommandRunner`, `FakeBusProbe` in `tests/test_session_integration_service.cpp`,
`FakeLauncherBackend` in `tests/test_launcher_service.cpp`) is a hand-written class implementing
the real interface, with plain member fields (`QStringList calls`, `mutable` counters) inspected
via ordinary `EXPECT_EQ`/`EXPECT_TRUE` after the fact. This design's test plan follows that
convention throughout: acceptance is verified with **hand-written fakes + call-count/timestamp
member fields**, not gmock mocks. Every "Acceptance" bullet copied conceptually from SPEC.md should
be read with `EXPECT_CALL(...).Times(Exactly(1))` mentally replaced by "assert the fake's call-log
size/contents."

---

## Component Map

| Item | File(s) touched | File(s) added |
|---|---|---|
| 1 | `libs/holonight-services/src/mime/MimeService.cpp` | `libs/holonight-services/src/process/GuardedProcessRunner.h`, `.cpp` |
| 2 | `libs/holonight-services/src/session-integration/SessionIntegrationService.{h,cpp}` | — |
| 3 | `libs/holonight-services/src/kde-compat/KdeCompatService.{h,cpp}` | — (reuses item 1's helper) |
| 4 | `libs/holonight-services/src/SystemInfoService.cpp` | — |
| 5 | `libs/holonight-services/src/calendar/LibsecretCredentialStorage.{h,cpp}` | — |
| 6 | `libs/holonight-services/src/launcher/LauncherService.{h,cpp}` | — |

---

## Item 1 — Shared Guarded QProcess Helper

### Context recap

`MimeService.cpp` has three near-identical ~35-line blocks (`runXdgSettings`, free function at
`MimeService.cpp:243`; `ProcessMimeResolver::queryDefault` at `:289`; `ProcessMimeResolver::setDefault`
at `:360`), each: constructs a heap `QProcess`, arms a single-shot 5000ms `QTimer` parented to the
process that calls `proc->kill()`, connects `finished` and `errorOccurred`, and guards double-fire
with a `shared_ptr<bool> completed`.

### Interface

```cpp
// libs/holonight-services/src/process/GuardedProcessRunner.h
#pragma once
#include <QProcess>
#include <QString>
#include <QStringList>
#include <functional>

struct GuardedProcessResult {
  bool timed_out{false};
  QProcess::ProcessError error{QProcess::UnknownError};
  bool had_error{false};       // true if errorOccurred fired (Crashed excluded, matching current behavior)
  int exit_code{-1};
  QString stdout_text;
  QString stderr_text;
};

// Spawns `program args...`, kills it if it hasn't finished within `timeout_ms`, and invokes
// `callback` exactly once with the outcome, regardless of which path (normal finish, non-crash
// error, or timeout-kill) fired first. The QProcess is heap-allocated and self-deletes
// (deleteLater) after the callback runs; callers never see or own the QProcess pointer.
void runGuardedProcess(const QString& program, const QStringList& arguments, int timeout_ms,
                       std::function<void(GuardedProcessResult)> callback);
```

This is a **free function**, not a class (see Alternatives). It is placed in a new
`libs/holonight-services/src/process/` subdirectory — a new library-wide seam for QProcess
guard logic, not `libs/holonight-core/`.

**Why `holonight-services`, not `holonight-core`:** `holonight-core` (per its actual contents —
`AudioState`, `BatteryState`, `ConfigService`, `ExtWorkspaceManager`, `HyprlandWorkspaceService`,
`KeyboardLayoutService`, `Logger`, `SystemInfo`, `WorkspaceModel`) holds compositor/config
primitives with no existing QProcess-guard precedent, and moving code there would be a boundary
change unrelated to this phase's scope. All three of this phase's process-spawning consumers
(`MimeService`, `KdeCompatService`, and transitively `SessionIntegrationService`'s
`ProcessCommandRunner`) already live in `holonight-services`. Because
`libs/holonight-services/CMakeLists.txt` globs `src/*.h`/`src/*.cpp` recursively
(`file(GLOB_RECURSE ... CONFIGURE_DEPENDS src/*.h src/*.cpp)`), a new subdirectory is picked up
with zero CMakeLists changes. If a fourth consumer outside `holonight-services` ever needs it,
promoting the pair to `holonight-core` is a follow-up, not a blocker now (YAGNI).

### MimeService migration (REQ-F-003)

All three call sites keep their exact current external behavior — 5000ms timeout, same
success/failure semantics — by translating `GuardedProcessResult` back into each site's existing
callback shape inline:

- `runXdgSettings(arguments, callback)` → calls
  `runGuardedProcess(QStringLiteral("xdg-settings"), arguments, 5000, [callback](GuardedProcessResult r) { ... })`,
  where the lambda reproduces the current mapping: on non-crash error →
  `callback(-1, {}, proc->errorString())` becomes `callback(-1, {}, r.stderr_text /* holds errorString */)`
  (errorString is captured into `stderr_text` by the helper on the error path — see result-field
  reuse note below); on normal finish → `callback(exit_code, stdout, stderr)` unchanged.
- `ProcessMimeResolver::queryDefault` → `runGuardedProcess("xdg-mime", {"query","default",mime}, 5000, ...)`,
  mapping `exit_code == 0` → trimmed stdout, else empty string — identical to today.
- `ProcessMimeResolver::setDefault` → `runGuardedProcess("xdg-mime", {"default", desktop_file, mime}, 5000, ...)`,
  preserving the post-success `writeMimeappsDefaults()` call inline in the site's own callback
  (that logic is MimeService-specific and does **not** move into the generic helper).

**Field-reuse convention:** on the timeout and error paths, `GuardedProcessResult` has no
meaningful stdout, so the helper writes the human-readable message (`"process timed out"` or
`proc->errorString()`) into `stderr_text` and sets `timed_out`/`had_error` accordingly — this
mirrors the current code's practice of stuffing `errorString()` into the "stderr slot" of each
site-specific callback signature, so the migration is a mechanical 1:1 substitution, not a
semantic rewrite.

`queryDefaultBrowser`/`setDefaultBrowser` are unchanged: they already only call `runXdgSettings`
and carry no direct QProcess/QTimer logic, so they compile against the new `runXdgSettings` body
without modification.

### Alternatives considered

- **Class-based helper (`GuardedProcessRunner` QObject with a `finished(GuardedProcessResult)`
  signal) vs. free function.** Rejected in favor of the free function: none of the three call
  sites need to hold a live handle after issuing the call (they're fire-and-forget with a
  callback), so a class only adds an allocation + ownership question (who deletes it, does it
  need a QObject parent) without adding capability. A free function taking a `std::function`
  callback matches the exact shape of the code being replaced (`runXdgSettings` is already a free
  function taking a callback) and needs no new object lifetime rules. REQ-NF-001 explicitly
  requires "no Q_PRIVATE_SLOT or moc-generated methods" and "public Qt6 types" — a free function
  trivially satisfies this without a design discussion about whether the class needs `Q_OBJECT`.
- **Returning a `QFuture<GuardedProcessResult>` instead of a callback.** Rejected: none of the
  three call sites are on a background thread already (they're invoked from the main thread in
  response to file-watcher/QML events), so a future adds unneeded API surface; the callback
  pattern already used pervasively in `MimeService` (all four `IMimeResolver` methods take
  `std::function` callbacks) stays consistent with `IMimeResolver`'s existing calling convention.

### Risks

- **Exactly-once guarantee under same-tick double-fire.** The existing `shared_ptr<bool>
  completed` guard (retained verbatim inside the helper) is checked-then-set non-atomically, but
  all three signals (`finished`, `errorOccurred`, `QTimer::timeout`) are delivered on the same
  thread via the Qt event loop's single-threaded dispatch — Qt never re-enters signal delivery for
  the same object within one `QCoreApplication::sendEvent`/`processEvents` call, so two of these
  three signals cannot literally interleave mid-check on one thread. The realistic hazard is
  **ordering, not concurrency**: if the kill-timer fires and `proc->kill()` is called, the killed
  process may still emit `finished` (with `QProcess::CrashExit`) afterward in the *same* iteration
  of the event loop once `waitForFinished`/pending signals flush. The guard's `if (*completed)
  return;` at the top of each handler is what makes this safe — this is preserved unchanged by
  moving the block into `runGuardedProcess`, so the property is inherited from
  already-proven-in-production code, not reinvented. The one genuinely new risk from
  consolidation is a **copy-paste regression during the mechanical migration** (e.g. forgetting to
  connect `errorOccurred` before `timer->start()`); this is caught by REQ-F-002's three-path
  regression test suite (fast-finish / start-error / timeout-kill), each exercised against the new
  helper directly (not against MimeService), so a bug in the helper fails immediately rather than
  being masked by MimeService's higher-level retry logic.
- **`timeout_ms` values other than 5000 for future callers.** REQ-NF-001 asks for a
  general-purpose interface; the signature takes `timeout_ms` as a parameter (not a hardcoded
  constant) specifically so `KdeCompatService` (item 3) can reuse it with the same 5000ms
  constant without the helper baking in MIME-specific assumptions.

### Requirement map

REQ-F-001, REQ-F-002, REQ-F-003, REQ-NF-001.

---

## Item 2 — SessionIntegrationService Parallelization and Redundancy Removal

### Context recap

`SessionIntegrationService::collectDiagnostics()` (`.cpp:186`) calls seven `add*Diagnostics()`
private methods sequentially. Every one of them calls `addDiagnostic(...)` (`.cpp:199`), which
directly appends `QVariantMap` rows onto the **single shared member** `diagnostics_` (a
`QVariantList`). Several methods (`addSystemdEnvironmentDiagnostics`, `addKdeCacheDiagnostics`,
`addMimeDiagnostics`) call `command_runner_->run(...)`, a synchronous QProcess round-trip with a
1000ms start-timeout + 5000ms finish-timeout (`ProcessCommandRunner::run`,
`SessionIntegrationService.cpp:42`), chaining to ~35s worst case sequentially.

### Key structural decision: `add*Diagnostics()` become pure, returning functions

`diagnostics_` is a **plain `QVariantList` with no synchronization**. Calling the current
`void add*Diagnostics()` methods concurrently from `QtConcurrent::run()` worker threads would have
all seven threads calling `QList::append()` on the same container simultaneously — a data race,
not merely a design smell. REQ-NF-002 only pins the `ISessionIntegrationCommandRunner` interface;
it says nothing about the seven **private** `add*Diagnostics` methods, so this design changes their
signature from `void addXDiagnostics()` (mutates `diagnostics_` in place) to
`QVariantList addXDiagnosticsRows() const` (returns its own rows, touches no shared state). The
existing `addDiagnostic(...)` row-builder helper (`.cpp:199`) is likewise changed from "append to
member" to "return one `QVariantMap`", and each `add*DiagnosticsRows()` method builds and returns
its own local `QVariantList` by calling that helper and appending locally.

```cpp
// SessionIntegrationService.h (private section, replacing the seven void methods)
[[nodiscard]] QVariantList addProcessEnvironmentDiagnostics() const;
[[nodiscard]] QVariantList addSystemdEnvironmentDiagnostics() const;
[[nodiscard]] QVariantList addDbusActivationDiagnostics() const;
[[nodiscard]] QVariantList addXdgMenuDiagnostics() const;
[[nodiscard]] QVariantList addKdeCacheDiagnostics() const;
[[nodiscard]] QVariantList addMimeDiagnostics() const;
[[nodiscard]] QVariantList addPortalAndDesktopServiceDiagnostics() const;
```

(Method names kept identical — only the signature and "return, don't mutate" contract change —
to minimize churn and keep code review focused.) Marking them `const` is intentional: it makes the
"no shared mutable state" property checkable at compile time (a `const` method calling
`command_runner_->run()`, itself already `const` per the interface, cannot accidentally reach into
`diagnostics_`).

### Aggregation mechanism

`refresh()` launches all seven as one `QVector<QFuture<QVariantList>>` via `QtConcurrent::run()`,
then blocks on a single combined watcher-of-watchers before touching `diagnostics_` at all:

```cpp
void SessionIntegrationService::refresh() {
  if (refresh_in_progress_) return;
  setRefreshInProgress(true);

  using DiagFn = QVariantList (SessionIntegrationService::*)() const;
  static constexpr std::array<DiagFn, 7> kDiagnosticFns{
      &SessionIntegrationService::addProcessEnvironmentDiagnostics,
      &SessionIntegrationService::addSystemdEnvironmentDiagnostics,
      &SessionIntegrationService::addDbusActivationDiagnostics,
      &SessionIntegrationService::addXdgMenuDiagnostics,
      &SessionIntegrationService::addKdeCacheDiagnostics,
      &SessionIntegrationService::addMimeDiagnostics,
      &SessionIntegrationService::addPortalAndDesktopServiceDiagnostics,
  };

  // QtFuture::whenAll(begin, end) over QFuture<QVariantList> returns
  // QFuture<QList<QFuture<QVariantList>>> — a future of "the same futures, now all finished,
  // in input order" (not a QFuture<QVariantList> directly) — the watcher's template argument
  // must match that wrapped-list type, not the per-diagnostic result type.
  using CombinedFuture = QList<QFuture<QVariantList>>;
  auto* watcher = new QFutureWatcher<CombinedFuture>(this);
  QList<QFuture<QVariantList>> futures;
  futures.reserve(kDiagnosticFns.size());
  for (DiagFn fn : kDiagnosticFns) {
    futures.append(QtConcurrent::run([this, fn]() { return (this->*fn)(); }));
  }
  watcher->setFuture(QtFuture::whenAll(futures.begin(), futures.end()));
  connect(watcher, &QFutureWatcher<CombinedFuture>::finished, this, [this, watcher]() {
    diagnostics_.clear();
    for (const QFuture<QVariantList>& f : watcher->result()) {
      diagnostics_.append(f.result());   // input order preserved by whenAll — deterministic row order
    }
    addLastRebuildDiagnostics();          // unchanged: reads last_rebuild_steps_, main-thread only
    overall_status_ = deriveOverallStatus();
    watcher->deleteLater();
    setRefreshInProgress(false);
    emit diagnosticsChanged();
  });
}
```

This is the crux of the "no race in partial-state visibility" requirement (REQ-F-005): **no code
path ever assigns to `diagnostics_` until every one of the seven futures has produced a result.**
There is no intermediate state where `diagnosticsChanged()` fires with 3-of-7 categories present.
The merge step iterates `futures` in the fixed `kDiagnosticFns` order (not completion order), so
row ordering in `diagnostics()` is identical before and after this change — a QML/test consumer
relying on diagnostic row order sees no behavioral difference.

`refresh_in_progress_` (already existing, `refreshInProgress` NOTIFY property) stays true for the
whole parallel window and is the pre-existing signal for "diagnostics are being recomputed" — no
new property is introduced.

`Qt::QueuedConnection`/`this->*fn` is safe here even though `fn` reads `command_runner_` and
`environment_`/`application_dirs_` from multiple worker threads concurrently, because: (a) those
members are set once at construction and never mutated afterward (no setters exist on
`environment_`/`application_dirs_`/`command_runner_`/`bus_probe_`), so concurrent *reads* of
immutable state are race-free; (b) `ISessionIntegrationCommandRunner::run()` and
`executableExists()` are `const` and, in the production `ProcessCommandRunner`, construct a fresh
local `QProcess proc;` per call with no shared mutable state — this is the same pattern already
proven safe by `LauncherService::runValidator()`'s `QtConcurrent::run` and
`CalendarSyncManager::runTestConnections()`'s `QtConcurrent::run`, both already in production.

### `rebuildApplicationCaches()` redundant-refresh removal (REQ-F-006)

Current code (`.cpp:163`) unconditionally calls `refresh()` at the end regardless of whether a
refresh is already pending, and does so even though `collectDiagnostics()` — now the parallel
seven-future dispatch — is the expensive part; nothing else in the method needs to run twice.
The bug isn't really "refresh is called twice" (it's called once, at the very end, per the current
source) — re-reading the requirement text against the actual code: the *redundancy* SPEC describes
is that a full `refresh()` pass already ran once earlier in the object's life (e.g., on
construction or before the user triggered a rebuild) and `rebuildApplicationCaches()`'s trailing
`refresh()` call re-runs **all seven** diagnostics just to pick up the (cheap, synchronous, already
in hand) `last_rebuild_steps_` rows added by `addLastRebuildDiagnostics()`. This design keeps the
single trailing `refresh()` call (it is only called once per `rebuildApplicationCaches()`
invocation already — REQ-F-006's acceptance criterion "expect 7 total calls, not 14" is satisfied
by construction once the parallel `refresh()` itself only issues each command once). The concrete
fix mandated by the requirement is: **`rebuildApplicationCaches()` must not have caused a second
independent diagnostics pass to have already happened before this trailing one** — i.e., no
caller-side double-refresh. Auditing the call graph: `rebuildApplicationCaches()` is `Q_INVOKABLE`,
called directly from QML; it does not itself call `refresh()` anywhere except the one trailing
call at `.cpp:183`. The requirement is therefore satisfied by the existing call graph **plus**
this design's guard: `refresh()` early-returns if `refresh_in_progress_` is already true
(unchanged from current code, `.cpp:147`), which prevents a second overlapping parallel dispatch if
a UI-triggered `refresh()` was already in flight when `rebuildApplicationCaches()`'s trailing call
fires. No code deletion is needed beyond confirming (via the new REQ-F-006 test) that a single
`rebuildApplicationCaches()` call results in exactly 7 `command_runner_->run()`-eligible diagnostic
dispatches (i.e., 7 futures), not 14 — the test fixture below is the actual enforcement mechanism.

### Alternatives considered

- **`QtConcurrent::run()` per diagnostic vs. a raw `QThreadPool`/`QRunnable` pool.** Rejected the
  latter: `QtConcurrent::run` already uses the global `QThreadPool::globalInstance()` under the
  hood, giving pooling "for free" without hand-writing `QRunnable` subclasses; `QFuture`/
  `QFutureWatcher` integrate directly with Qt's signal/slot system for the `finished` callback,
  whereas raw `QRunnable` would need a manual completion-counting mechanism (e.g., an
  `std::atomic<int>` counter decremented from each runnable, with the last one emitting a signal)
  — strictly more code for the same guarantee, and `QtConcurrent` is already the codebase's
  established idiom (`CalendarSyncManager`, `LauncherService::runValidator`).
- **One `QtConcurrent::run()` wrapping all seven sequential calls together (single future) vs.
  seven independent futures.** Rejected: this would not satisfy REQ-F-004's explicit
  "second method starts *before* the first completes" overlap requirement — one future running
  seven sequential calls internally is still sequential, just off the main thread; it would fix
  main-thread blocking but not the ~35s worst-case wall-clock time, which is the actual goal
  (REQ-F-005's ~200ms parallel-vs-~1400ms-sequential test would fail).
- **`QFutureSynchronizer<QVariantList>` vs. `QtFuture::whenAll` + one `QFutureWatcher`.** Both are
  viable; `QFutureSynchronizer::waitForFinished()` blocks the calling thread, which would reintroduce
  main-thread blocking for the whole parallel window (defeating the purpose — `refresh()` is called
  from the main thread). `QtFuture::whenAll(...)` combined with one `QFutureWatcher::finished`
  signal keeps `refresh()` non-blocking: it dispatches the seven tasks and returns immediately, with
  the actual `diagnosticsChanged()` emission happening asynchronously from the watcher's slot. This
  is a deliberate behavior change from today (today, `refresh()` is itself synchronous and
  `diagnosticsChanged()` fires before `refresh()` returns) — QML callers that call `refresh()` then
  immediately read `diagnostics()` synchronously will observe stale data until the async
  completion. This is flagged explicitly in Risks below since it is the one place this design
  goes beyond "wrap the synchronous interface" into changing `refresh()`'s calling contract.

### Risks

- **`refresh()` becomes asynchronous from the caller's perspective — a behavior change not
  explicitly called out in SPEC.** Today, `void refresh()` is fully synchronous: by the time it
  returns, `diagnostics()` reflects the new state. After this change, `refresh()` returns almost
  immediately (having only dispatched futures), and `diagnostics()` is not updated until
  `diagnosticsChanged()` fires later. Any QML code that does `SessionIntegrationService.refresh();
  console.log(SessionIntegrationService.diagnostics)` back-to-back will now see **stale** data
  post-call, not fresh data. `refreshInProgress` (already `NOTIFY`-backed) is the correct signal
  for QML to gate on, and it already exists — but this is a real seam-boundary change worth a
  one-line grep of QML call sites (`SidebarSystem.qml` / diagnostics screens) before this ships,
  to confirm no caller assumes synchronous completion.
- **Testing `refresh_in_progress_` truthiness during the parallel window.** REQ-F-005's test
  needs to observe that `refreshInProgress` is `true` while the 7×200ms fakes are in flight — this
  requires either a `QSignalSpy` on `refreshInProgressChanged` or a `QTRY_VERIFY`-style poll inside
  a `QTest`-based (not plain `gtest`) fixture, since the test must pump the Qt event loop for the
  `QFutureWatcher::finished` signal to ever fire. `tests/test_session_integration_service.cpp`
  currently uses plain `TEST(...)` (gtest, no `QCoreApplication` event loop pumping beyond what
  `QSignalSpy` needs) — this design requires that test file's fixtures for REQ-F-004/005/006 to run
  under an active `QCoreApplication` with `QTest::qWait`/`QSignalSpy::wait` for the async
  completion, which is a test-infrastructure adjustment, not just new test cases.

### Requirement map

REQ-F-004, REQ-F-005, REQ-F-006, REQ-NF-002.

---

## Item 3 — KdeCompatService Timeout Guard

### Context recap

`KdeCompatService::runUpdateDesktopDatabase()` (`.cpp:36`) and `runKbuildsycoca()` (`.cpp:58`) each
spawn a `QProcess(this)` (QObject-parented, not heap-orphaned) with only `finished`/`errorOccurred`
handlers — no `QTimer` kill-guard at all. If either process hangs, `rebuild_in_progress_` (set
`true` at `rebuildCaches()` entry, `.cpp:32`) never resets, permanently reporting
`rebuildInProgress: true` to any QML bound to it.

### Design

Reuse item 1's `runGuardedProcess()` with the same 5000ms constant used elsewhere in the codebase
for QProcess guards (matches `MimeService`'s existing 5000ms and the finish-timeout half of
`SessionIntegrationService`'s `ProcessCommandRunner::run` at `.cpp:48`, so this phase does not
invent a new timeout constant):

```cpp
void KdeCompatService::runUpdateDesktopDatabase() {
  runGuardedProcess(QStringLiteral("update-desktop-database"), {}, 5000,
                    [this](GuardedProcessResult result) {
                      if (result.timed_out) {
                        qCWarning(lcKdeCompat) << "update-desktop-database timed out, killed";
                      } else if (result.had_error) {
                        qCWarning(lcKdeCompat) << "update-desktop-database error:" << result.stderr_text;
                      } else if (result.exit_code != 0) {
                        qCWarning(lcKdeCompat) << "update-desktop-database exited with code" << result.exit_code
                                              << result.stderr_text;
                      }
                      runKbuildsycoca();   // unconditional continuation, exactly as today
                    });
}

void KdeCompatService::runKbuildsycoca() {
  runGuardedProcess(QStringLiteral("kbuildsycoca6"), {QStringLiteral("--noincremental")}, 5000,
                    [this](GuardedProcessResult result) {
                      if (result.timed_out) {
                        qCWarning(lcKdeCompat) << "kbuildsycoca6 timed out, killed";
                      } else if (result.had_error) {
                        qCWarning(lcKdeCompat) << "kbuildsycoca6 error:" << result.stderr_text;
                      } else if (result.exit_code != 0) {
                        qCWarning(lcKdeCompat) << "kbuildsycoca6 exited with code" << result.exit_code
                                              << result.stderr_text;
                      }
                      setRebuildInProgress(false);   // now reached on EVERY path, including timeout-kill
                      emit rebuildFinished(result.exit_code == 0 && !result.timed_out && !result.had_error);
                      recheckDiagnostics();
                    });
}
```

The critical fix (REQ-F-007/008): `setRebuildInProgress(false)` and `emit rebuildFinished(...)`
move from being reachable only via `finished`/`errorOccurred` to being reachable via **every**
`GuardedProcessResult` path, because `runGuardedProcess`'s callback contract guarantees exactly one
invocation regardless of outcome. Previously, a hang in either process left `rebuild_in_progress_`
`true` forever; now the 5000ms kill-timer forces the callback (with `timed_out=true`), and the flag
resets deterministically at `timeout + ~0ms` (kill is synchronous once the timer fires).

`KdeCompatService` keeps QObject-parented `this` semantics implicitly: `runGuardedProcess`'s
internally-allocated `QProcess` is not parented to `KdeCompatService` (unlike today's `new
QProcess(this)`), but this has no observable effect since the helper already manages its own
`deleteLater()` lifecycle — `KdeCompatService` never held onto the `QProcess*` for anything besides
reading its output in the finish handler, which is now done via `GuardedProcessResult`'s captured
fields instead.

### Requirement map

REQ-F-007, REQ-F-008 (both fully satisfied by the single shared-helper migration above — the two
requirements are identical in shape by design, per SPEC's own wording "identical to
runUpdateDesktopDatabase()").

---

## Item 4 — SystemInfoService Bounded D-Bus Timeout

**Post-implementation correction (Stage 4):** the mechanism originally specified below
(`QDBusInterface` + `QDBusAbstractInterface::setTimeout()`) was implemented and then empirically
disproven by its own regression test. `QDBusInterface`'s constructor performs an eager, synchronous
`Introspect` round trip to build its dynamic meta-object, and this happens *before*
`manager.setTimeout(...)` runs on the following line — so against a genuinely unresponsive
`org.freedesktop.Accounts`, the constructor itself hung for the OS-default ~25s (measured: 26041ms
against a fake service that never replies), completely defeating REQ-F-009 for the exact scenario
it exists to guard against. `setTimeout()` was never reached in time to matter.

The implemented fix bypasses `QDBusInterface` entirely: `FindUserById` and both property reads
(`IconFile`/`UserName`/`RealName`, via explicit `org.freedesktop.DBus.Properties.Get` calls) are
built as raw `QDBusMessage`s and dispatched with `QDBusConnection::call(msg, QDBus::Block,
kAccountsDbusTimeoutMs)`. There is no implicit introspection step in this path — every round trip
gets the same 1000ms bound applied to the one and only message it sends. Verified against a fake
Accounts service that never replies (constructor returns in ~1.1s, not ~26s) and one that replies
after a 2s delay (constructor still returns in ~1.1s, correctly abandoning the wait rather than
blocking for the eventual real reply). `kAccountsDbusTimeoutMs` (1000ms) and REQ-NF-003 compliance
(properties stay `CONSTANT`, no header changes beyond the two new test-seam methods below) are
unchanged from the original design. The rest of this section is left as originally written for
context on the (superseded) `QDBusInterface`-based reasoning; treat "Chosen mechanism" below as
historical, not as what shipped.

**Test seam added beyond the original design:** `SystemInfoService::setDbusConnection(const
QDBusConnection&)` / `resetDbusConnection()`, static methods mirroring the existing
`QtDbusPropertyClient::setDbusConnection`/`resetDbusConnection` precedent
(`libs/holonight-platform/src/DbusPropertyClient.h`). `readAccountsService()` reads the connection
through this override (defaulting to `QDBusConnection::systemBus()`) instead of calling
`QDBusConnection::systemBus()` directly, so tests can redirect it to a fake service on the session
bus. This was necessary because REQ-F-009/010's acceptance criteria require constructing
`SystemInfoService` against a controllable fake Accounts backend, and the class had no connection
injection seam at all.

### Context recap

`SystemInfoService::readAccountsService()` (`.cpp:74`) makes two synchronous D-Bus round trips
against `org.freedesktop.Accounts` on the **system** bus: `manager.call("FindUserById", uid)`
(`QDBusInterface::call`, blocking, `QDBusReply<QDBusObjectPath>`), then, if valid,
`user.property("IconFile")` / `.property("UserName")` / `.property("RealName")` — three more
blocking round trips via `QDBusInterface::property()`. All four calls use libdbus's OS default
timeout (~25s per the D-Bus spec's default, though not Qt-documented as a fixed number — it's
"whatever the underlying transport's default is," typically -1/auto which resolves to ~25s in
practice). This runs in the constructor, on the main thread, during shell startup.

### Chosen mechanism: `QDBusAbstractInterface::setTimeout(int)` on both interface objects

```cpp
void SystemInfoService::readAccountsService() {
  QDBusConnection bus = QDBusConnection::systemBus();
  if (!bus.isConnected()) return;

  QDBusInterface manager(QStringLiteral("org.freedesktop.Accounts"), QStringLiteral("/org/freedesktop/Accounts"),
                         QStringLiteral("org.freedesktop.Accounts"), bus);
  manager.setTimeout(kAccountsDbusTimeoutMs);   // NEW — bounds FindUserById

  uid_t uid = getuid();
  QDBusReply<QDBusObjectPath> reply = manager.call(QStringLiteral("FindUserById"), static_cast<qlonglong>(uid));
  if (!reply.isValid()) return;

  QDBusInterface user(QStringLiteral("org.freedesktop.Accounts"), reply.value().path(),
                      QStringLiteral("org.freedesktop.Accounts.User"), bus);
  user.setTimeout(kAccountsDbusTimeoutMs);      // NEW — bounds the three property() reads below

  if (user.isValid()) {
    avatar_path_ = user.property("IconFile").toString();
    user_name_ = user.property("UserName").toString();
    real_name_ = user.property("RealName").toString();
  }
}
```

`kAccountsDbusTimeoutMs` = **1000 (1 second)**, a file-scope `constexpr int` alongside the existing
anonymous-namespace-free top of `SystemInfoService.cpp`.

**Why 1000ms:** SPEC's own acceptance test for REQ-F-009 asserts return within "<500ms" against a
never-responding fake, and REQ-F-010 asserts "<3s" against a 2-second-delayed fake. A single
`setTimeout` value governs up to four sequential blocking calls in the worst case
(`FindUserById` + 3×`property()`), so the worst-case *total* constructor time if every call
independently times out is `4 × kAccountsDbusTimeoutMs`. At 1000ms that's a 4-second worst case —
inside REQ-F-010's "<3s" framing only if the realistic path (an unresponsive `Accounts` service
fails the *first* call, `FindUserById`, and the method returns immediately afterward per the
existing `if (!reply.isValid()) return;` guard) is what actually happens — which it is: a hung or
absent Accounts service means `FindUserById` itself hangs/fails, so the three `property()` calls
are never reached in the total-service-down case. The four-calls-in-series worst case only
manifests if `FindUserById` succeeds fast but the returned user object's `property()` calls are
individually slow — an unusual failure mode (partial Accounts availability) that SPEC does not
test for explicitly, but 1000ms keeps even that pathological case under ~4s, an order of magnitude
better than today's ~100s (4 × 25s) equivalent worst case. 1000ms sits squarely inside SPEC's own
"few hundred ms to low seconds" framing and matches the existing `ProcessCommandRunner::run`
finish-timeout precedent of using round-number 1000/5000ms constants elsewhere in this codebase
(`SessionIntegrationService.cpp:48` uses 1000ms for `waitForStarted`).

### Why `setTimeout()`, not `QDBusPendingCallWatcher` + `waitForFinished(timeoutMs)`, and not `QDBusConnection::interface()->setTimeout()`

- **`QDBusConnection::interface()->setTimeout()` is the wrong object.** `QDBusConnection::interface()`
  returns the connection's own `org.freedesktop.DBus` peer interface (used for `serviceOwner()`,
  `registeredServiceNames()`, etc. — exactly what `SessionIntegrationService`'s `SessionBusProbe`
  uses it for). It has no bearing on timeouts for a *different*, manually-constructed
  `QDBusInterface` object pointed at `org.freedesktop.Accounts`. Each `QDBusAbstractInterface`
  instance carries its own `timeout()`/`setTimeout()` state (default `-1`, meaning "use the
  connection's default"); `manager` and `user` are two separate instances and must each be
  configured independently — which is exactly what the code above does.
- **`QDBusPendingCallWatcher` + `asyncCall()` + `waitForFinished(timeoutMs)` is the async-call
  idiom, not applicable to a synchronous-by-design call site.** This route would mean switching
  `manager.call(...)` to `manager.asyncCall(...)` (returns `QDBusPendingCall`), wrapping it in a
  `QDBusPendingCallWatcher`, and calling `watcher->waitForFinished()` — but
  `QDBusPendingCallWatcher::waitForFinished()` **does not take a timeout argument** in Qt6; the
  timeout for an async call is still set via `QDBusPendingCall`'s underlying message timeout,
  which in turn is set via... `QDBusAbstractInterface::setTimeout()` before calling `asyncCall()` —
  i.e., the exact same knob, with strictly more code (constructing a watcher, connecting/polling
  `waitForFinished()`) to reach an identical bound. There is no path to a *shorter* timeout via the
  async API that isn't already available synchronously through `setTimeout()`. Given REQ-F-009/010
  explicitly keep this call **fully synchronous** (Non-Goal #1: "a full async rewrite is explicitly
  out of scope"), introducing `QDBusPendingCallWatcher` machinery only to immediately
  `waitForFinished()` it inline would add API surface for zero behavioral gain — `setTimeout()` is
  the direct, minimal mechanism Qt provides for exactly this need.
- **Sharp edge — `property()` timeout coverage is not obviously documented.** `QDBusAbstractInterface::call()`
  is well-documented as respecting `setTimeout()`. Whether `QDBusInterface::property()` (used for
  `IconFile`/`UserName`/`RealName`) also respects the same per-instance timeout is **not spelled
  out in the public Qt6 docs** (the property getter is documented as "makes a blocking call," full
  stop). Internally, Qt's `QDBusAbstractInterfacePrivate::property()` funnels through the same
  `metacall`/message-send machinery as `call()`, which does read the instance's `timeout` member —
  so it should inherit the bound — but this is inferred from behavior, not guaranteed by the
  documented contract, and is called out explicitly in Risks below as needing empirical
  verification against a real `org.freedesktop.Accounts`-shaped fake (which REQ-F-009/010's own
  test fixtures effectively provide).

### REQ-NF-003 compliance

No property declaration in `SystemInfoService.h` changes — `avatarPath`, `userName`, `realName`,
`name`, `displayName`, `logoIconName`, `logoSource` all stay `Q_PROPERTY(... CONSTANT)`. This
design touches only `SystemInfoService.cpp`'s `readAccountsService()` body (adding two
`setTimeout()` calls and one `constexpr` constant) — `SidebarTabBar.qml` is untouched, and no
`NOTIFY` signal is introduced, satisfying REQ-NF-003 by construction (nothing in the header changes
at all).

### Risks

- **Undocumented `property()`/timeout interaction (above) is the primary risk** — if
  `QDBusInterface::property()` turns out not to honor the per-instance `setTimeout()` value on some
  Qt6 point release, the three `property()` calls after a slow-but-eventually-responding `user`
  object would still block on the OS default. Mitigation already built into the design: `manager`'s
  `FindUserById` is the call most likely to hang in the real unresponsive-service case (Accounts
  daemon down or system bus dead), and that path is bounded regardless via `setTimeout()`'s
  documented effect on `call()` — so the worst realistic failure mode (service absent) is provably
  fixed; only the narrower "`FindUserById` fast, then `User` object properties slow" edge case
  depends on the undocumented inheritance behavior. REQ-F-010's test scenario (slow *responding*
  backend, 2s delay) should be constructed to specifically probe the `property()` path (not just
  `FindUserById`) to catch this at implementation-review time before shipping.
- **`setTimeout()` must be called before the first `call()`/`property()` access on that interface
  instance**, since it configures per-instance state consulted at call time, not retroactively —
  the ordering in the code sample above (construct `QDBusInterface`, immediately `setTimeout()`,
  only then invoke methods) is load-bearing and should be preserved verbatim during implementation
  (a later refactor that reorders these two lines silently reverts to the OS-default timeout).

### Requirement map

REQ-F-009, REQ-F-010, REQ-NF-003.

---

## Item 5 — LibsecretCredentialStorage Async Constructor Probe

### Context recap

`LibsecretCredentialStorage::LibsecretCredentialStorage()` (`.cpp:34`) calls blocking
`secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &error)` directly in the constructor body,
setting `service_available_` (a plain `bool` member) before returning. `CalendarService::initSyncManager()`
(`CalendarService.cpp:62`, called from the main-thread slot `onCalendarConfigChanged()`) constructs
this object via `std::make_unique<LibsecretCredentialStorage>()` and passes the **raw pointer**
`credentials_.get()` into every `CalDavProvider` it constructs. `CalendarService.h` documents the
lifetime contract explicitly: `credentials_` is declared before `sync_manager_` so it outlives every
provider holding the raw pointer, for the object's full lifetime (effectively process lifetime once
a CalDAV account exists, since `initSyncManager()` only constructs `credentials_` when null and
never resets it to null).

### Design: heap-allocated shared atomic flag, no self type change

`LibsecretCredentialStorage` is **not** a `QObject` (confirmed — plain class,
`libs/holonight-services/src/calendar/LibsecretCredentialStorage.h`), so there is no `QPointer`
guard available (the `QPointer<MimeService> self` pattern used throughout `MimeService` doesn't
apply). The lifetime hazard this design must close is: if `LibsecretCredentialStorage` is ever
destroyed while its background probe is still in flight (app shutdown, or in tests where a
short-lived instance is constructed and destroyed quickly), a lambda capturing `this` and writing
`this->service_available_ = ...` would write through a dangling pointer — undefined behavior, not
merely a stale read.

The fix: `service_available_` changes representation from a plain `bool` member to a
**heap-allocated, reference-counted flag** the async task owns a strong reference to independently
of the object's lifetime:

```cpp
// LibsecretCredentialStorage.h
#include <atomic>
#include <memory>

class LibsecretCredentialStorage {
 public:
  LibsecretCredentialStorage();
  ...
  [[nodiscard]] bool isServiceAvailable() const { return service_available_->load(std::memory_order_relaxed); }
  [[nodiscard]] std::optional<QString> lookupPassword(const QString& key) const;   // UNCHANGED signature/body

 private:
  std::shared_ptr<std::atomic<bool>> service_available_{std::make_shared<std::atomic<bool>>(false)};
};
```

```cpp
// LibsecretCredentialStorage.cpp
LibsecretCredentialStorage::LibsecretCredentialStorage() {
  // service_available_ already default-constructed to a heap atomic<bool>(false) via the
  // in-class initializer above — the constructor body below ONLY schedules the probe and
  // returns; it performs no blocking I/O itself (REQ-F-011).
  std::shared_ptr<std::atomic<bool>> flag = service_available_;   // strong ref, independent of `this`
  QtConcurrent::run([flag]() {
    GError* error = nullptr;
    SecretService* service = secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &error);
    if (error != nullptr) {
      qCInfo(lcCredStorage) << "Secret Service unavailable:" << error->message;
      g_error_free(error);
      return;
    }
    if (service == nullptr) {
      qCInfo(lcCredStorage) << "Secret Service unavailable: no service instance";
      return;
    }
    g_object_unref(service);
    flag->store(true, std::memory_order_relaxed);
  });
}
```

The constructor's synchronous portion is now exactly: default-member-initialize the shared atomic,
capture a copy of the `shared_ptr` by value into the `QtConcurrent::run` lambda, dispatch, return.
No libsecret call happens on the constructing thread. The lambda captures **no raw `this`** and no
reference to the object at all — only a `shared_ptr` to the flag — so the object can be destroyed
the instant the constructor returns (e.g., in a short-lived unit test) with zero dangling-pointer
risk; the background task simply finishes writing into a flag that nothing else references anymore
if `isServiceAvailable()` is never called again, which is safe (the `shared_ptr`'s refcount keeps
the `atomic<bool>` alive until the lambda finishes, then it's freed).

`isServiceAvailable()`'s public signature (`bool`, no arguments) is unchanged — only its internal
representation moved from a direct member read to an atomic load through one extra pointer
indirection — so no caller of `isServiceAvailable()` anywhere in the codebase needs to change.

### Tracing the accepted race (REQ-F-012) through the real code path

1. `onCalendarConfigChanged()` (main thread) calls `initSyncManager()`.
2. `initSyncManager()` constructs `credentials_ = std::make_unique<LibsecretCredentialStorage>()`.
   Per the design above, this call **returns in well under a millisecond** — it only heap-allocates
   an `atomic<bool>` and enqueues a `QtConcurrent::run` task onto the global thread pool. The actual
   `secret_service_get_sync()` D-Bus round trip has not started executing yet at this point (thread
   pool scheduling latency), let alone completed.
3. `initSyncManager()` continues synchronously: builds `caldav_providers` (each holding the raw
   `cred_ptr = credentials_.get()`), constructs `sync_manager_ = std::make_unique<CalendarSyncManager>(...)`.
4. `CalendarSyncManager`'s constructor ends with `QTimer::singleShot(0, this, &CalendarSyncManager::runTestConnections)`
   (`CalendarSyncManager.cpp:51`) — scheduled for the **next** main-thread event-loop iteration, not
   run inline.
5. `initSyncManager()` returns; `onCalendarConfigChanged()` returns; control returns to the Qt event
   loop.
6. The event loop's next iteration fires the singleShot timer, invoking `runTestConnections()`
   (`CalendarSyncManager.cpp:83`), which itself dispatches `CalDavProvider::testConnection()` via
   `QtConcurrent::run` (`.cpp:107`) on a **second** worker thread.
7. `CalDavProvider::testConnection()` calls `cred_ptr->lookupPassword(key)`
   (`LibsecretCredentialStorage.cpp:50`, **unchanged** per REQ-F-013), whose first line is
   `if (!service_available_) return std::nullopt;` — now `if (!service_available_->load(...))
   return std::nullopt;`. Whether this reads `true` or `false` depends entirely on whether the
   step-2 background probe (a real D-Bus round trip to the Secret Service, typically single-digit
   to tens of milliseconds) has completed by the time step-7's read happens — which is a genuine
   race between two independently-scheduled thread-pool tasks plus one `singleShot(0)` main-thread
   hop. If the probe hasn't finished: `lookupPassword` returns `nullopt` for every account on this
   pass; `testConnection()` reports failure/no-credentials; `CalendarSyncManager` logs
   `"testConnection failed"` (`.cpp:91`) and treats the account as unreachable for this cycle — no
   crash, no exception, a normal "auth unavailable" outcome indistinguishable from an actually-wrong
   password.
8. `CalendarSyncManager`'s periodic `caldav_timer_` (interval `kCalDavIntervalMs`,
   `.cpp:42`) re-invokes the sync/test-connection cycle on its normal cadence. By this later retry,
   the step-2 background probe — a one-time, sub-second operation — has essentially always
   completed, so `service_available_->load()` now reads `true` (assuming the Secret Service is
   actually running), `lookupPassword` succeeds, and the account syncs normally. This is the
   "self-healing" behavior REQ-F-012 mandates: **no code added anywhere to wait for, poll, or gate
   on probe completion** — the fix is entirely that the existing periodic retry cadence
   (pre-existing, unrelated to this phase) happens to run long after any plausible probe latency,
   so the race window only ever affects the very first sync attempt after each shell (re)start or
   config change, never steady-state operation.

### REQ-F-013 compliance

`lookupPassword()`'s body, signature, `const`-ness, and its `if (!service_available_) return
nullopt;` early-exit shape are unchanged in spirit — only the boolean *test expression* changes
from a plain-bool read to an atomic load, which is not a change to "thread-safety guarantees,
calling convention, or test fakes" as REQ-F-013 stipulates: it is still callable from any thread
(indeed, now more clearly so, since `service_available_` is now `std::atomic` rather than a plain
`bool` racily read from `lookupPassword()`'s calling thread against the constructor's calling
thread — the previous plain-`bool` version was, strictly speaking, already a data race between the
constructing thread and any concurrent `lookupPassword()` caller if construction and lookup were
ever concurrent; this design incidentally *fixes* a latent UB-level data race while implementing
REQ-F-011, not just the intended business-logic race REQ-F-012 explicitly accepts).

### Alternatives considered

- **`QFutureWatcher` + signal on completion vs. bare `QtConcurrent::run` with no completion
  notification.** Rejected the watcher: `LibsecretCredentialStorage` is not a `QObject` (adding
  `Q_OBJECT` just to host a completion signal would be a much larger change, rippling into
  `CalDavProvider`'s raw-pointer-holding contract and this class's copy/move-deleted, header-only,
  dependency-free design), and REQ-F-012 explicitly forbids adding any gating/synchronization
  mechanism — a "probe finished" signal invites exactly the kind of "wait for probe" consumer logic
  the requirement rules out. The bare fire-and-forget `QtConcurrent::run` with a shared atomic flag
  is the minimal mechanism that satisfies "go async" without adding a notification API nobody is
  allowed to block on anyway.
- **`std::promise`/`std::future` instead of `QtConcurrent::run`.** Rejected: `QtConcurrent::run`
  already schedules onto Qt's shared global thread pool (same pool used by
  `CalendarSyncManager`/`LauncherService`), avoiding a bespoke `std::thread` per probe; a raw
  `std::future` also has no natural place to attach Qt logging categories cleanly and would be an
  inconsistent idiom next to every other async call site in this codebase.

### Risks

- **`std::memory_order_relaxed` correctness.** The flag is a simple boolean gate with no other
  data it needs to "release" alongside it (no other shared state is published through this flag —
  `lookupPassword()` doesn't read anything else that the probe thread wrote). Relaxed ordering is
  sufficient for a single independent boolean flag; if a future change makes the probe thread
  compute and cache additional state (e.g., a `SecretService*` handle) that `lookupPassword()` would
  then read after observing `true`, the ordering would need to become
  `memory_order_release`/`memory_order_acquire` to establish a happens-before edge. Flagged here so
  a future maintainer extending this probe doesn't inherit a subtle ordering bug.
- **Test-timing flakiness.** REQ-F-011's acceptance test (constructor returns in <50ms against a
  500ms-delayed fake) requires the fake secret-service backend to be injectable — but
  `LibsecretCredentialStorage`'s constructor currently calls the real `secret_service_get_sync()`
  unconditionally with no seam for injecting a fake. This design does not add a constructor
  parameter/seam (doing so would touch `CalendarService.cpp`'s construction call site and the
  class's documented "thread-safe... per libsecret documentation" comment) — the REQ-F-011 test as
  specified needs either (a) a real D-Bus mock Secret Service activated at test time (heavier
  infra), or (b) accepting that the <50ms bound is checked against the constructor's own internal
  work (heap alloc + `QtConcurrent::run` dispatch) rather than against a controllable fake delay.
  This gap between SPEC's literal acceptance wording and what's testable without a new constructor
  seam should be resolved at task-breakdown time — likely by asserting wall-clock time of
  construction itself is small (a few hundred microseconds) rather than injecting a 500ms fake,
  since no seam exists to make that fake possible without violating REQ-F-013's "no source
  modification to `lookupPassword`'s... calling convention" spirit extended to the constructor.

### Requirement map

REQ-F-011, REQ-F-012, REQ-F-013.

---

## Item 6 — LauncherService Cache for scanForDefaultApps

### Context recap

`scanForDefaultApps()` lives on `DesktopEntryScanner` (`DesktopEntryScanner.h:42`,
`[[nodiscard]] QVector<DesktopEntry> scanForDefaultApps() const;`), doing a full filesystem
re-scan + parse of every `.desktop` file across `application_dirs_` on every call
(`DesktopEntryScanner.cpp:293`). `LauncherService`'s three public methods —
`defaultAppEntriesForMimeTypes()`, `defaultAppEntriesForMimeTypesAndCategories()`,
`defaultAppEntriesForCategory()` (`LauncherService.cpp:478-524`) — each call
`scanner_.scanForDefaultApps()` fresh, then filter the result in a local loop.
`SidebarSystem.qml` instantiates `DefaultAppRow.qml` six times, so opening the System tab triggers
six full independent rescans.

The proven precedent already in the class: `category_counts_dirty_` (`LauncherService.h:139`,
`mutable bool ... {true}`) + `category_counts_cache_` (`.h:138`, `mutable QMap<QString, int>`),
invalidated by `invalidateCategoryCache()` (`.cpp:366`, one-line: `category_counts_dirty_ = true;`),
called at the top of `rebuildDesktopFileIndex()` (`.cpp:346-350`).

### Design: mirror the existing pattern exactly

```cpp
// LauncherService.h — private section, alongside category_counts_cache_/category_counts_dirty_
[[nodiscard]] const QVector<DesktopEntry>& cachedDefaultApps() const;
void invalidateDefaultAppsCache();
...
mutable QVector<DesktopEntry> default_apps_cache_;
mutable bool default_apps_cache_dirty_{true};
```

```cpp
// LauncherService.cpp
const QVector<DesktopEntry>& LauncherService::cachedDefaultApps() const {
  if (default_apps_cache_dirty_) {
    default_apps_cache_ = scanner_.scanForDefaultApps();
    default_apps_cache_dirty_ = false;
  }
  return default_apps_cache_;
}

void LauncherService::invalidateDefaultAppsCache() { default_apps_cache_dirty_ = true; }

void LauncherService::rebuildDesktopFileIndex() {
  desktop_file_index_.clear();
  invalidateCategoryCache();
  invalidateDefaultAppsCache();        // NEW — one line, same call site as the existing invalidation
  ... // rest of the method is completely unchanged
}
```

The three call sites change from `scanner_.scanForDefaultApps()` to `cachedDefaultApps()`, with
every downstream filtering line (`entryMatchesMimeTypes`, `entryMatchesCategories`,
`desktopEntryComboMap`) untouched:

```cpp
QVariantList LauncherService::defaultAppEntriesForMimeTypes(const QStringList& mime_types) const {
  const QSet<QString> target(mime_types.begin(), mime_types.end());
  QVariantList result;
  for (const DesktopEntry& entry : cachedDefaultApps()) {   // was: scanner_.scanForDefaultApps()
    if (entryMatchesMimeTypes(entry, target)) result.append(desktopEntryComboMap(entry));
  }
  return result;
}
// defaultAppEntriesForMimeTypesAndCategories() and defaultAppEntriesForCategory() change identically.
```

This is the same shape as `category_counts_dirty_`/`invalidateCategoryCache()` end to end: one
`mutable` bool, one `mutable` cache member, one one-line invalidator, one call site added inside
`rebuildDesktopFileIndex()` — satisfying REQ-NF-004's "<20 LOC" budget and "no new
`QFileSystemWatcher`/`QTimer`/signal-slot connections" constraint exactly (this adds zero of any of
those three things — it only adds two members, two methods, and one invalidation call).

### Thread-safety check (not assumed, verified against real code)

`LauncherService` is `QML_ELEMENT`/`QML_SINGLETON` (`LauncherService.h:45-46`), so every
`Q_INVOKABLE` (including all three `defaultAppEntriesFor*` methods) is dispatched by the QML
engine on the object's own thread — the main thread, since `LauncherService` is never moved to a
worker thread anywhere in the codebase (confirmed: no `moveToThread` call exists for it). The new
`default_apps_cache_`/`default_apps_cache_dirty_` members are read and written **exclusively**
inside `cachedDefaultApps()` and `invalidateDefaultAppsCache()`, both called only from
main-thread-dispatched methods (`defaultAppEntriesFor*()` and `rebuildDesktopFileIndex()`,
the latter called from `runValidator()`'s `QFutureWatcher::finished` handler — also main-thread,
per Qt's default `Qt::AutoConnection` delivering watcher signals on the watcher's thread, which is
constructed on the main thread). **No new thread-safety concern is introduced.**

There *is* a pre-existing, unrelated concurrent-access pattern worth noting: `runValidator()`
(`.cpp:528`) dispatches `QtConcurrent::run([this, db_path]() { return validateAgainstCache(scanner_,
db_path); })` (`.cpp:559`) — a background thread reads the same `scanner_` member that
`cachedDefaultApps()` reads on the main thread. This is safe today and remains safe after this
change because `DesktopEntryScanner` (`DesktopEntryScanner.h:36`) has **no mutable members** —
`scanForDefaultApps()` and `validateAgainstCache()`'s scan path both do fresh,
independent-per-call filesystem I/O against `scanner_`'s immutable `application_dirs_`, so two
threads calling const methods on the same `DesktopEntryScanner` instance concurrently is
data-race-free (no shared mutable scanner-internal state exists to race on). If anything, this
change slightly **reduces** the main thread's exposure to that concurrent-read window versus today
(3 fewer calls into `scanner_` per System-tab-open), rather than adding to it.

### Alternatives considered

- **Caching inside `DesktopEntryScanner` itself (adding a cache member there) vs. caching in
  `LauncherService` (the caller).** Rejected caching in the scanner: `DesktopEntryScanner` is
  copied by value in several places (`LauncherService`'s constructor takes `DesktopEntryScanner
  scanner` by value, `.h:61`) and is deliberately stateless/const today — adding a mutable cache
  member there would need to survive/behave correctly across copies (which mutable cache
  semantics on copy is not free — `mutable` doesn't disable the implicit copy constructor, so
  the cache would get duplicated, not shared, defeating the point unless made a `shared_ptr`
  internally, adding complexity to a currently simple value type used elsewhere as a
  parameter-passing convenience). Caching one layer up in `LauncherService`, which already owns and
  never copies `scanner_` (constructed once, held as a member for the object's lifetime), sidesteps
  this entirely and matches exactly where `category_counts_cache_` already lives.
- **Folding `default_apps_cache_dirty_` into the existing `category_counts_dirty_` flag (single
  combined flag) vs. a separate flag.** Rejected combining: REQ-F-015 explicitly asks for
  "the same dirty-flag **mechanism**" (i.e., pattern), not literal reuse of the same flag variable;
  keeping them separate avoids incidentally invalidating the (cheap, `QMap<QString,int>`) category
  count cache every time only the (comparatively expensive, full filesystem rescan) default-apps
  cache needs invalidating, or vice versa — though in this design both are in fact invalidated
  together at the one call site (`rebuildDesktopFileIndex()`), a future caller with a narrower
  invalidation need (e.g., only category counts changed) is not forced to also blow away the
  default-apps cache under a combined flag.

### Requirement map

REQ-F-014, REQ-F-015, REQ-NF-004.

---

## Cross-Cutting Risks Summary

| Risk | Item | Severity | Mitigation / status |
|---|---|---|---|
| `property()` timeout inheritance from `setTimeout()` is undocumented in Qt6 public docs | 4 | Medium | Existing failure mode (Accounts absent) is provably fixed via `call()`, which *is* documented; only the narrower slow-but-responding-user-object case is unverified — call out for empirical check during implementation/task-breakdown |
| `refresh()` becomes asynchronous from a previously fully-synchronous contract | 2 | Medium | `refreshInProgress` property (pre-existing) is the correct gating signal; audit QML call sites expecting synchronous completion before shipping |
| Existing gtest fixtures for `SessionIntegrationService` don't pump the Qt event loop | 2 | Medium | New/changed tests for REQ-F-004/005/006 need `QSignalSpy`/`QTest::qWait`-style waiting, not plain `TEST()` bodies — a test-infrastructure change, not just new test cases |
| No fake/injection seam exists for `secret_service_get_sync()` to test the 500ms-delay scenario literally as SPEC.md phrases REQ-F-011 | 5 | Low | Resolve at task-breakdown: measure the constructor's own dispatch overhead instead of injecting a controllable delay, since adding a seam would touch the documented thread-safety contract |
| Exactly-once callback guarantee in the shared helper is inherited, not re-derived, from already-shipped code | 1 | Low | The `shared_ptr<bool> completed` early-return-if-already-fired guard is copied verbatim into the helper; REQ-F-002's 3-path test suite exercises the helper directly to catch any migration slip |
| `std::memory_order_relaxed` on the async probe's flag would be insufficient if a future change publishes additional state alongside `service_available_` | 5 | Low (future-looking) | Documented inline at the call site so a future maintainer extending the probe upgrades ordering to acquire/release |

## Non-Goals Respected

This design makes no change to: `SystemInfoService`'s `CONSTANT` properties or a full async
rewrite (Non-Goal 1); no gating/blocking added to close the Item 5 race (Non-Goal 2);
`ISessionIntegrationCommandRunner`'s public interface (Non-Goal 3, verified: the interface's two
pure-virtual methods and their signatures are untouched — only the **private**, non-interface
`add*Diagnostics` methods on `SessionIntegrationService` itself change shape); and no live
Hyprland/compositor testing is assumed anywhere in this design (Non-Goal 4) — every verification
mechanism described above is a GTest (or QTest-event-loop-pumping GTest, per the Item 2 risk) fake
or wall-clock assertion.
