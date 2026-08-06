# U-06 — Launcher & Notifications Services — Deep Review Findings

**Task**: T-006 · **Skill**: `qt-cpp-review` (Phase 1 deterministic lint + Phase 2 six-agent deep analysis) · **Scope**: 28 files, read-only

## Scope

- `libs/holonight-services/src/launcher/` — `CategoryMapper`, `DesktopEntryCache`, `DesktopEntryScanner`, `DesktopEntrySerializer`, `LauncherCommand`, `LauncherModel`, `LauncherService`, `RecentAppsTracker` (13 files)
- `libs/holonight-services/src/notifications/` — `NotificationFilter`, `NotificationPolicy`, `NotificationRuleModel`, `NotificationRuleStore`, `NotificationServer`, `NotificationService`, `NotificationStore`, `NotificationTypes` (15 files)

## Prior Context

Consulted per T-006 instructions: `docs/sdd/launcher-desktop-cache/SPEC.md`+`DESIGN.md` (per SPEC.md §12, `launcher-desktop-cache` has 4 pre-existing unchecked `TASKS.md` items, not in scope — cataloged in T-013), `docs/sdd/notification-hardening/SPEC.md`+`DESIGN.md`.

**Mandatory check — `NotificationServer` D-Bus protocol adapter correctness against CLAUDE.md gotchas**: **verified correct.** `Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")` is present on `NotificationServer.h:18`, with an explicit code comment noting it's mandatory for correct introspection naming — the exact gotcha CLAUDE.md documents. The class also inherits `protected QDBusContext` (per the "D-Bus sender identity" pattern), though `calledFromDBus()`/`message().service()` are not actually invoked anywhere in `NotificationServer.cpp` — this is not a gap: the `org.freedesktop.Notifications` spec has callers self-report identity via the `app_name` argument rather than via D-Bus sender verification, so there's no protocol reason to use sender identity here.

**Mandatory check — launcher cache invalidation strategy (known incomplete per SPEC §12)**: characterized precisely in [F-03] below — two concrete blind spots beyond what SPEC.md's prose already documents.

## Tool Sign-off — Phase 1 Deterministic Lint

77 raw lint hits. Excluded as noise:

- **VAR-3** (51 hits, brace-init style) — not reported.
- **HDR-3** (3 hits, Windows-only) — not reported.

Three `MDL-7` hits (`LauncherModel.cpp:120`, `NotificationRuleModel.cpp:184`, `NotificationService.cpp:180`, "`data()` switch has `default:`") were independently investigated — all three models' `roleNames()`/`data()` role sets are in sync (false positives for the "hidden gap" concern), though the `default:` pattern itself is separately flagged as a `-Wswitch`-suppression risk in [I-03] below (a distinct, valid concern from the same code shape). Remaining categories (`TMO-1` ×9, `DEP-13` ×6, `ENM-2` ×2 deepened in [I-06], `DEP-7` ×2, `PAT-7` ×1 deepened and refuted — see Performance section) not elevated individually.

## Confirmed Findings (confidence ≥ 80/100)

### [F-01] Notification rule persistence: silent write failures, plus a synchronous, blocking disk write triggered unconditionally on every incoming notification — including DND-suppressed ones
- **Severity**: High
- **Effort**: M
- **Location**: `notifications/NotificationRuleModel.cpp:44-91,126-130`, `NotificationRuleStore.cpp:161-182`, `NotificationService.cpp:194-203`
- **Rationale**: `NotificationService::addOrReplace()` calls `rule_model_->ensureApp(data)` on **every** `Notify()` D-Bus call — before DND/rule filtering — and `ensureApp()` unconditionally calls `persist()` even when only `last_seen_ms` changed. `NotificationRuleStore::persist()` is a synchronous, blocking `QSaveFile` open/write/commit on the calling thread, with no async/debounce path (unlike the notification-history store in the same subsystem, which is deliberately `QtConcurrent`-async with a dirty-flag guard). On failure, only `qCWarning` is logged — no signal, no return value the QML layer can observe. A user disabling notifications for an app sees the UI toggle flip immediately, but if the write fails (disk full, read-only home), the change silently doesn't persist and reappears after restart with zero explanation. **Independently flagged by two agents from different angles** (Error Handling: silent-failure framing, 85/100; Performance: blocking-write-on-every-D-Bus-call framing, 88/100) — the same root cause, worth fixing together.
- **Suggested Direction**: Have `persist()` return `bool`/emit a failure signal so the discrepancy is diagnosable; separately, debounce or route the passive `last_seen_ms`-only touch through the same async `QtConcurrent` pattern already used by the notification-history store, reserving synchronous writes for explicit user actions (`setEnabled`/`setUrgencyFilter`).

### [F-02] `NotificationServer::Notify()` performs no length/size validation on untrusted D-Bus input; the full unbounded payload is logged to disk unconditionally
- **Severity**: High
- **Effort**: S
- **Location**: `notifications/NotificationServer.cpp:115-130` (`Notify`), `:29-69` (`buildNotificationData`)
- **Rationale**: `Notify()` is exported on the session bus, callable by any process. None of `app_name`/`app_icon`/`summary`/`body`/`actions`/`hints` are bounded before use. `qCInfo` unconditionally logs the entire payload — and per this project's Logger (CLAUDE.md), `qCInfo` writes to the on-disk log file regardless of console verbosity level — so any process on the bus can force arbitrarily large, repeated writes into the persistent log file simply by calling `Notify()` with huge strings. The same unbounded strings also flow into `history.json` via `itemToJson()` with no per-field size limit (`max_items` bounds item *count*, not per-item byte size). Confidence 82/100.
- **Suggested Direction**: Clamp `app_name`/`summary`/`body`/`app_icon` to a sane maximum (matching what dunst/mako-class daemons do) before constructing `NotificationData`, and truncate before logging regardless of the stored/rendered value.

### [F-03] Launcher cache invalidation has two concrete blind spots beyond what SPEC.md's prose documents
- **Severity**: Medium
- **Effort**: M (deliberately deferred per SPEC — see below)
- **Location**: `launcher/LauncherService.cpp:80-135`, `DesktopEntryCache.h:12-15`; `docs/sdd/launcher-desktop-cache/SPEC.md:242-246,270-274` (REQ-C-003/REQ-C-006)
- **Rationale**: SPEC.md already documents that in-place byte edits aren't watcher-detected and that invalidation uses mtime+size only (no hash/inode) — this is a known, deliberately accepted Stage-1 limitation, not a new discovery. Reading the implementation confirms two sharper failure modes than the SPEC's prose conveys: (1) `LauncherService::start()` only watches `QFileSystemWatcher::directoryChanged`, not per-file changes — an in-place content rewrite that doesn't trigger a directory-level add/remove/rename produces **zero** watcher signal. (2) Even when the validator does run, `persistValidationResult()` compares `lastModified().toSecsSinceEpoch()` (**1-second resolution**) and `size()` against the cached `FileMeta` — an edit within the same wall-clock second, or one that preserves the exact byte count, is structurally invisible to invalidation even on explicit `reload()`. Confidence 88/100.
- **Suggested Direction**: If moving beyond the accepted Stage-1 limitation: add per-file `QFileSystemWatcher::addPath()` watching (not just directories), and/or widen the mtime comparison to sub-second `QDateTime` precision instead of truncating via `toSecsSinceEpoch()`.

### [F-04] Launcher process-spawn failures produce zero diagnostics — the only silent failure path in the entire unit
- **Severity**: Medium
- **Effort**: S
- **Location**: `launcher/LauncherService.cpp:156-170` (`ProcessLauncherBackend::launch`/`launchExec`)
- **Rationale**: `QProcess::startDetached()`'s static overload discards all error information; on failure (missing executable, nonexistent working directory, permission error) there is no `qCWarning`, no logging category use at all — the only place in this whole unit where a failure path produces no log output whatsoever (everywhere else — `DesktopEntryCache`, `NotificationRuleStore`, `NotificationStore` — failures are at minimum logged). A user clicking a stale launcher tile (app uninstalled, cache not yet revalidated) gets no diagnosable trace of why nothing happened. Confidence 85/100.
- **Suggested Direction**: Add `qCWarning(lcLauncher)` on a `false` return from `startDetached`, matching the logging discipline used elsewhere in this unit.

### [F-05] Notification history persistence failures are silent — write failures never propagate back to the caller
- **Severity**: Medium
- **Effort**: S
- **Location**: `notifications/NotificationStore.cpp:181-202` (`writeHistoryToDisk`), `:275-310` (`persist`/`launchWrite`/`onWriteFinished`)
- **Rationale**: `writeHistoryToDisk()` (worker thread) only logs `qCWarning` on `QSaveFile` failure and returns — `NotificationStore::onWriteFinished()`/`writeCompleted()` fire unconditionally regardless of success, and `NotificationService`'s handler treats every completion as success. A disk-full or read-only-state scenario means `history.json` silently freezes at its last successful snapshot while in-memory history keeps evolving correctly — a crash/restart loses everything written since, with zero warning surfaced anywhere but the log file. Confidence 80/100.
- **Suggested Direction**: Propagate success/failure through `writeCompleted(bool ok)` and expose a `historyPersistenceHealthy`-style property so degraded persistence is diagnosable before data loss occurs.

### [F-06] `LauncherService::desktop_file_index_` is dead, write-only state rebuilt from scratch on every launcher keystroke
- **Severity**: Low (dead code, not a correctness bug — but a real, confirmed perf waste on a latency-sensitive hot path)
- **Effort**: S
- **Location**: `launcher/LauncherService.h:137`, `.cpp:348-364`
- **Rationale**: `desktop_file_index_` is fully rebuilt on every `LauncherModel::modelReset`, which fires from `setQuery()` — i.e. on every character typed in the launcher search box, at full app-list size (potentially 500-2000 entries). A repo-wide grep confirms zero `.value()`/`.contains()`/`.find()` call anywhere — the hash is written but never read. Meanwhile `LauncherModel::findEntryByDesktopFile()`, the method that would plausibly benefit from it, does its own independent O(n) linear scan instead. Confidence 92/100.
- **Suggested Direction**: Delete the unused hash, or wire `findEntryByDesktopFile` to consult it via a getter — and in either case, stop rebuilding on every query/category change (only entry-list changes actually invalidate a desktop-file→row mapping).

### [F-07] `roleNames()` rebuilds a fresh `QHash` on every call across all three list models in this unit
- **Severity**: Low
- **Effort**: S
- **Location**: `launcher/LauncherModel.cpp:125-143`, `notifications/NotificationRuleModel.cpp:189-194`, `notifications/NotificationService.cpp:185-192`
- **Rationale**: Same systemic pattern already seen in prior units — three independent occurrences here, not guaranteed single-call by Qt/QML (proxy models re-query it; `NotificationService` is itself wrapped by a `QSortFilterProxyModel` per monitor). Confidence 84/100.
- **Suggested Direction**: Cache each as a function-local `static const QHash`.

### [F-08] Launcher filesystem-watcher debounce interval (`500`ms) is an unnamed, unexplained magic number
- **Severity**: Low
- **Effort**: S
- **Location**: `launcher/LauncherService.cpp:252`
- **Rationale**: Hardcoded inline with no named constant and no rationale comment, unlike the notification subsystem's clearly-named constants (`kUrgencyMax`, `kMaxEntries`, `kHistoryVersion`) in the same review scope. This value directly controls launcher responsiveness to filesystem changes and is exactly the kind of tuning knob easy to miss when scanning for behavior-affecting constants. Confidence 82/100.
- **Suggested Direction**: Hoist to `constexpr int kFsWatchDebounceMs = 500;` with a one-line rationale comment.

### [F-09] `DesktopEntryScanner::scanForDefaultApps()` and `scanWithDirs()` duplicate the directory-walk/dedup/sort pipeline — and have already drifted
- **Severity**: Medium
- **Effort**: M
- **Location**: `launcher/DesktopEntryScanner.cpp:293-360`
- **Rationale**: Two near-identical ~30-line methods independently implement the same `QDirIterator` walk, dedup-by-`desktop_id`, and sort pipeline. They have already diverged: `scanForDefaultApps` passes `include_no_display=true` while `scanWithDirs` passes `false`, and only `scanWithDirs` tracks `seen_dirs` for `ScanResult::watched_dirs` — exactly the kind of silent behavioral drift unshared duplicated logic produces. Confidence 82/100.
- **Suggested Direction**: Extract a shared private helper parameterized by `include_no_display` and an optional `seen_dirs` output, called by both public methods.

## Investigation Targets (confidence 60-79 — human verification needed)

#### [I-01] `LauncherService::results()` is the sole non-const `Q_PROPERTY READ` accessor on the class
- **Severity**: Low · **Effort**: S · **Confidence**: 74/100
- **Location**: `launcher/LauncherService.h:57,85`
- **Rationale**: Every other `Q_PROPERTY READ` accessor on this class (`query()`, `selectedIndex()`, `resultCount()`, etc.) is `const`; `results()` alone is not. Latent rather than actively broken (no current call site holds a `const LauncherService&`), but silently blocks future const-correct usage and is inconsistent with the rest of the property surface.
- **Suggested Direction**: Mark `const` and return via `const_cast` with a comment explaining QML's non-const model requirement, matching the logical-constness pattern used elsewhere in the codebase.

#### [I-02] Launcher background rescan resets selection/scroll state on every filesystem event, even when the entry set is unchanged
- **Severity**: Medium · **Effort**: M · **Confidence**: 72/100
- **Location**: `launcher/LauncherService.cpp:190-195`, `LauncherModel.cpp:272-281`
- **Rationale**: `rebuildResults()` always does a full `beginResetModel()`/`endResetModel()`, including from async-scan completion triggered by any debounced filesystem event (e.g. a background package install touching an unrelated `.desktop` dir). The connected handler unconditionally clamps `selected_index_` to 0/-1 regardless of prior state. If the launcher is open mid-navigation when a background rescan lands, the user's selection silently jumps to the top row with no indication.
- **Suggested Direction**: Diff old vs. new entries and skip the reset when the desktop-file set/order is unchanged, or restore selection by desktop-file id across the reset instead of always clamping to row 0.

#### [I-03] `LauncherModel::data()` switches over an enum with a `default:` label, suppressing `-Wswitch` protection for future `Role` additions
- **Severity**: Low · **Effort**: S · **Confidence**: 72/100
- **Location**: `launcher/LauncherModel.cpp:67,120-121`
- **Rationale**: Covers all 15 current `Role` enumerators explicitly, but the trailing `default:` means the compiler won't warn if a new `Role` is added without a corresponding `case`. Some catch-all is needed (Qt-supplied roles like `Qt::DisplayRole` can arrive), but a pre-switch range check would preserve exhaustiveness checking that the current shape defeats.
- **Suggested Direction**: Validate the role range before the switch and return `{}` early, then switch on `Role` without a `default:` so `-Wswitch` protects future enumerator additions.

#### [I-04] `LauncherModel` copies full `DesktopEntry` structs into `ScoredEntry` on every keystroke
- **Severity**: Low · **Effort**: M · **Confidence**: 72/100
- **Location**: `launcher/LauncherModel.cpp:227-238,240-270`, `.h:52-57`
- **Rationale**: `ScoredEntry::entry` is value-typed, not a pointer/index, despite `entries_` being stable across each rebuild. Each matching entry (potentially hundreds per keystroke) triggers one full `DesktopEntry` copy (cheap COW refcount bumps, not deep copies, which tempers severity) plus one extra per matched action for search results.
- **Suggested Direction**: Store `const DesktopEntry*` or an index into `entries_` in `ScoredEntry` instead of a value copy.

#### [I-05] `NotificationRuleModel::rules_` has no upper bound, unlike `RecentAppsTracker`'s explicitly capped history
- **Severity**: Low · **Effort**: S · **Confidence**: 68/100
- **Location**: `notifications/NotificationRuleModel.cpp:44-91`, `.h:62`
- **Rationale**: `ensureApp()` appends a rule for every previously-unseen `app_name` from `Notify()` calls, persisting to disk on every insert. The only pruning path only drops rules with a non-empty `desktop_entry` that no longer matches an installed app — rules from ad-hoc `notify-send` scripts/CLI tools (empty `desktop_entry`) are never pruned. `RecentAppsTracker` explicitly caps at `kMaxEntries = 20` with LRU eviction for the same class of unbounded external input; `NotificationRuleModel` has no analogous cap.
- **Suggested Direction**: Cap `rules_` with LRU-by-`last_seen_ms` eviction (mirroring `RecentAppsTracker`), or periodically prune stale/no-desktop-entry rules past an age threshold.

#### [I-06] `NotificationRuleModel`/`NotificationService` `Roles` enums lack an explicit underlying type (deepens `ENM-2`)
- **Severity**: Low · **Effort**: S · **Confidence**: 68/100
- **Location**: `notifications/NotificationRuleModel.h:25`, `NotificationService.h:42`
- **Rationale**: Both carry a NOLINT comment justifying staying *unscoped* for `Q_ENUM`/role-int interop, but that doesn't address the separate missing-underlying-type concern lint flagged. Adding `: int` wouldn't change any Qt/QML behavior (both are already used exclusively as plain `int`), and the project's own `LauncherModel::Role` in the same subsystem already pins an explicit underlying type (`enum class Role : std::uint16_t`) — this is an inconsistency with the project's own established convention nearby.
- **Suggested Direction**: Add `: int` to both `enum Roles` declarations; extend the existing NOLINT comment to cover both the scoped/unscoped and underlying-type rationale in one place.

#### [I-07] `DesktopEntryCache` issues one fresh prepared statement per desktop entry during validation instead of reusing/rebinding
- **Severity**: Low · **Effort**: S · **Confidence**: 68/100
- **Location**: `launcher/LauncherService.cpp:80-108`, `DesktopEntryCache.cpp:135-188`
- **Rationale**: `persistValidationResult()` loops over every scanned entry (potentially 500-2000) calling `metadata()`, and both `metadata()`/`upsert()` construct+prepare a fresh `QSqlQuery` each call rather than preparing once and rebinding per row. Runs on a background thread (`QtConcurrent`), so not a UI-blocking concern — hence investigation-target rather than confirmed — but still N sequential prepare+bind+exec round-trips where one reusable statement would do.
- **Suggested Direction**: Hoist a single prepared `QSqlQuery` outside the loop, rebinding `:path` per iteration.

#### [I-08] `DesktopEntryCache` has no self-healing path if the schema is externally tampered with while `PRAGMA user_version` still matches
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `launcher/DesktopEntryCache.cpp:69-79`
- **Rationale**: Schema versioning is driven entirely by `PRAGMA user_version`; `CREATE TABLE IF NOT EXISTS` is a no-op if the version already matches but the actual column shape was altered by something outside this code path. Every subsequent query would then fail with only a warning logged — no automatic recovery short of manual `launcher.db` deletion.
- **Suggested Direction**: On any `desktop_entries` query failure (not just at `open()`), consider a one-shot drop+recreate fallback, or validate `PRAGMA table_info` against the expected column set at open time.

#### [I-09] `DesktopEntry` field list is independently enumerated in at least three places — a forward-looking shotgun-surgery risk
- **Severity**: Low · **Effort**: S · **Confidence**: 63/100
- **Location**: `launcher/DesktopEntryScanner.cpp:119-152`, `DesktopEntrySerializer.h:16-91`, `LauncherModel.cpp:59-123`
- **Rationale**: The `.desktop`-file parser, the JSON cache serializer, and `LauncherModel::data()`'s role switch each independently enumerate every `DesktopEntry` field. All four sites are currently in sync for existing fields (verified — no active bug), but a future field addition that misses one site would silently drop data rather than fail loudly, as already happened with the `mime_types` field requiring a coordinated update across all of them.
- **Suggested Direction**: No urgent action; consider a code comment on the `DesktopEntry` struct pointing at the other files that must be updated in lockstep.

#### [I-10] `DesktopEntryCache`'s destructor does not release its `QSqlDatabase` connection — relies entirely on caller discipline
- **Severity**: Low · **Effort**: S · **Confidence**: 62/100
- **Location**: `launcher/DesktopEntryCache.h:17-18`, `.cpp:92-98`
- **Rationale**: `open()` registers a uniquely-named connection, but `~DesktopEntryCache() = default` never calls `close()`. All current call sites correctly pair `open()`/`close()` manually, but nothing in the class enforces this — a future early-return, exception, or long-lived member usage would leak a named entry in Qt's global connection table for the process lifetime.
- **Suggested Direction**: Call `close()` from the destructor (guarded by the `open_` flag) so the class is safe by construction rather than by caller discipline.

## Summary

| Category | Lint (reported) | Deep (confirmed ≥80) | Investigation (60-79) | Total |
|---|---|---|---|---|
| Model Contracts | 3 (MDL-7, all refuted for the "hidden gap" concern) | 0 | 0 (2 dropped for cap) | 0 |
| Ownership & Lifecycle | 0 | 0 | 2 (1 merged into I-10, 1 dropped for cap) | 1 |
| Thread Safety | 0 | 0 (verified clean — SQLite thread-confinement and async marshalling both correct) | 0 | 0 |
| API & C++ Correctness | ENM-2 (2, deepened) | 1 (F-09) | 3 (I-01, I-03, I-06) | 4 |
| Error Handling & Validation | 0 | 4 (F-01 partial, F-02, F-03, F-04, F-05) | 2 (I-08, 1 dropped for cap) | 6 |
| Performance & Code Quality | PAT-7 (1, refuted) | 4 (F-01 partial, F-06, F-07, F-08) | 3 (I-04, I-07, I-09) | 7 |
| **Total** | **77 raw / 3 refuted / 2 deepened** | **9** | **10** | **19 actionable** |

19 actionable items (9 confirmed + 10 investigation targets). The mandatory `NotificationServer` D-Bus protocol check passed clean — `Q_CLASSINFO` is present and correctly commented as mandatory. Thread safety across both async subsystems (SQLite desktop-entry validation, notification-history writes) was traced end-to-end and found correctly isolated. The standout findings are **[F-02]** (a real, unauthenticated-caller DoS vector: any process on the session bus can force unbounded log/history growth via `Notify()`) and **[F-01]** (the notification-rules store is the one persistence path in this subsystem that never adopted the async-write pattern its sibling `NotificationStore` already uses, and its failures are completely silent) — both worth prioritizing together since they touch the same D-Bus-facing code path. **[F-06]** is a clean, low-risk perf win (dead code on the launcher's per-keystroke hot path) worth an easy early fix.
