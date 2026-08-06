# U-05 — Desktop Integration, Appearance & Portal — Deep Review Findings

**Task**: T-005 · **Skill**: `qt-cpp-review` (Phase 1 deterministic lint + Phase 2 six-agent deep analysis) · **Scope**: 18 files, read-only

## Scope

- `libs/holonight-services/src/` root-level: `AppearanceService.{h,cpp}`, `ThemeService.{h,cpp}` (4 files)
- `libs/holonight-services/src/portal/` — `NullPortalBackend`, `PortalService`, `SettingsPortalBackend` (6 files)
- `libs/holonight-services/src/kde-compat/` — `KdeCompatService` (2 files)
- `libs/holonight-services/src/mime/` — `MimeService` (2 files)
- `libs/holonight-services/src/session-integration/` — `ApplicationCacheRebuilder`, `SessionIntegrationService` (4 files)

## Prior Context

Consulted per T-005 instructions: `docs/sdd/portal-service/SPEC.md`+`DESIGN.md` (per SPEC.md §12, `portal-service` has 12 pre-existing unchecked `TASKS.md` items — not in scope, cataloged separately in T-013), `docs/sdd/system-appearance-portal/SPEC.md`+`DESIGN.md`, `docs/sdd/theme-variant-catalog/SPEC.md`+`DESIGN.md`.

**Mandatory check — ThemeService "trivial constant accessors" doc-drift**: **confirmed stale, 93/100 confidence.** DESIGN.md §5 observation #5 flagged that a 2026-05-28 audit described `ThemeService` as "trivial constant accessors" but current code includes a `SettingsPortalBackend.h` include. Verified: `ThemeService` is no longer trivial. It owns a live `QFileSystemWatcher` (`theme_config_watcher_`) that dynamically re-arms on every directory/file change, and it owns a heap-allocated `SettingsPortalBackend*` that registers a real D-Bus service (`org.freedesktop.impl.portal.desktop.holonight`, implementing `org.freedesktop.impl.portal.Settings`) on the session bus. Filesystem events trigger `reloadFromThemeConfig()` (mutating the backend's `Values` and emitting `SettingChanged` over D-Bus) before `ThemeService` emits its own `paletteReloadRequested()`. This is active, event-driven, stateful orchestration — the opposite of "trivial constant accessors." Filed as [F-01] below. Note: `AppearanceService` (a genuinely separate, `ConfigService`-backed `Q_PROPERTY` accessor class for fonts/sizes) does **not** overlap with `ThemeService`'s responsibility — no doc-drift there, only in the `ThemeService` framing itself.

## Tool Sign-off — Phase 1 Deterministic Lint

45 raw lint hits. Excluded as noise:

- **VAR-3** (38 hits, brace-init style) — not reported.
- **HDR-3** (1 hit, Windows-only) — not reported.

One flagged hit was investigated and refuted:

- **ERR-4** (`NullPortalBackend.cpp:83`, "hardcoded `http://` URL"): **false positive.** The string is a D-Bus introspection XML DOCTYPE identifier (`http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd`) embedded per the D-Bus introspection spec — a static string literal, never fetched over the network at runtime. Manually verified against the source; no action needed.

`PAT-7` (`SettingsPortalBackend.h:53`, `.cpp:109-110`, "QMap usage — verify copying needed") was deepened by the Performance agent and found to be a non-issue: the flagged `QMap` is a freshly-constructed, ≤1-entry stack local in the rarely-called `ReadAll()` D-Bus dispatch handler, not a hot-path copy. No action needed. Remaining category (`PAT-2` ×1, `DEP-5` ×1 `QPair`) not elevated individually.

## Confirmed Findings (confidence ≥ 80/100)

### [F-01] `ThemeService` "trivial constant accessors" documentation is stale — the service now owns live D-Bus state and filesystem watching
- **Severity**: Low (doc-drift, not a code defect)
- **Effort**: S
- **Location**: `ThemeService.h:1-39`, `ThemeService.cpp:1-51`
- **Rationale**: See "Mandatory check" above — this is the required T-005 verification and its confirmed verdict. `ThemeService` has grown from whatever the 2026-05-28 audit observed into an active orchestrator: a self-rearming `QFileSystemWatcher` plus an embedded, D-Bus-registered `SettingsPortalBackend`. Confidence 93/100.
- **Suggested Direction**: Update the audit/doc-comment referencing "trivial constant accessors" to reflect the current live-state design, so future readers don't assume the class is side-effect-free.

### [F-02] Theme-config path resolution logic duplicated verbatim between `ThemeService` and the `SettingsPortalBackend` it directly owns
- **Severity**: Medium
- **Effort**: S
- **Location**: `ThemeService.cpp:17-24` vs `portal/SettingsPortalBackend.cpp:23-29`
- **Rationale**: `ThemeService::resolveThemeConfigPath()` and `SettingsPortalBackend.cpp`'s anonymous-namespace `themeConfigPath()` independently compute the identical `$XDG_CONFIG_HOME`-or-`~/.config` + `/holonight/theme.conf` path — despite `ThemeService` directly owning and constructing the very `SettingsPortalBackend` instance containing the second copy. A future change to the resolution rule risks landing in only one copy, silently desynchronizing the watcher's target from the file the backend actually reads. A third, looser variant of the same XDG-base-dir pattern exists independently in `mime/MimeService.cpp:126-129`. Confidence 82/100.
- **Suggested Direction**: Extract one shared `themeConfigPath()` helper, or have `ThemeService` ask its owned `SettingsPortalBackend` for its config path instead of recomputing it — guaranteeing watcher and reader can never diverge.

### [F-03] `MimeService` role-mime-list reassignment can change a `Q_PROPERTY`'s resolved value with no `NOTIFY` signal ever emitted
- **Severity**: Medium
- **Effort**: M
- **Location**: `mime/MimeService.cpp:516-535`
- **Rationale**: `setDefaultTextEditor`/`setDefaultTerminal`/etc. synchronously reassign the role's mime list *before* the async `setDefaultAsync()` call, which can instantly change what the corresponding `Q_PROPERTY` accessor resolves to (against the still-old `mime_cache_`) — but `emitChangedSignals()` only runs later, inside the async success callback, and diffs using the *already-swapped* role list on both sides, so it can never detect a change caused by the swap itself. If the async operation subsequently fails, `emitChangedSignals()` never runs at all, leaving the synchronous value change permanently unnotified — QML bound to e.g. `defaultTextEditor` silently goes stale. Confidence 82/100.
- **Suggested Direction**: Capture the resolved value under the old role list before reassignment, compare against the new resolution immediately after, and emit the corresponding `*Changed()` signal right away — independent of the subsequent async call's outcome.

### [F-04] `KdeCompatService`'s `QProcess` calls have no timeout guard, unlike the identical pattern used elsewhere in the same review unit
- **Severity**: Medium
- **Effort**: S
- **Location**: `kde-compat/KdeCompatService.cpp:36-83` (`runUpdateDesktopDatabase`, `runKbuildsycoca`)
- **Rationale**: Neither function arms a kill-timer, relying solely on `finished`/`errorOccurred` signals to clear `rebuild_in_progress_`. This is the exact same "spawn cache-rebuild tool" operation that both `SessionIntegrationService::ProcessCommandRunner::run()` (explicit `waitForStarted(1000)`/`waitForFinished(5000)`) and `MimeService`'s `ProcessMimeResolver`/`runXdgSettings` (explicit 5s `QTimer` + `proc->kill()`) guard in this same unit. If `update-desktop-database`/`kbuildsycoca6` hangs (stalled disk, deadlocked lock file), `rebuild_in_progress_` stays `true` permanently with no user recovery path short of restarting the shell. **Independently corroborated by three separate agents** (Error Handling 85/100 confirmed, Performance 65/100, and Ownership's related double-fire finding at 72/100) — strong cross-agent agreement on this exact code path having weaker error-handling discipline than its siblings in the same unit.
- **Suggested Direction**: Add the same singleshot kill-timer + `proc->kill()` pattern already established in `MimeService.cpp` to both `KdeCompatService` process helpers, and surface a timeout as `rebuildFinished(false)`.

### [F-05] `SessionIntegrationService` runs a chain of fully synchronous, blocking `QProcess` calls directly on the UI thread — every diagnostics refresh or cache rebuild can freeze the shell
- **Severity**: High
- **Effort**: M
- **Location**: `session-integration/SessionIntegrationService.cpp:42-56` (`ProcessCommandRunner::run`), `:146-155` (`refresh`), `:163-184` (`rebuildApplicationCaches`); `ApplicationCacheRebuilder.cpp:32-62`
- **Rationale**: `ProcessCommandRunner::run()` blocks on `waitForStarted(1000)`/`waitForFinished(5000)` — and this is a `QML_SINGLETON` invoked directly from QML, so the block lands on the UI thread. `collectDiagnostics()` fans out to up to ~7 sequential blocking calls (`systemctl`, up to 6× `xdg-settings`/`xdg-mime`), worst-case ~35s of UI freeze; `rebuildApplicationCaches()` compounds this further by running the full rebuild chain and then unconditionally calling `refresh()` again — doubling the blocking work in a single button click. Confirmed reachable directly from `SidebarSystem.qml`'s `Component.onCompleted` (fires the instant the System tab opens) and its Refresh/Rebuild buttons; the `refreshInProgress`/`rebuildInProgress` QML properties never get a chance to repaint before the freeze, since no event-loop iteration occurs between setting the flag and running the blocking work. Confidence 90/100.
- **Suggested Direction**: Move `ProcessCommandRunner::run()` and its orchestration onto a worker thread (`QtConcurrent::run` + `QFutureWatcher`) or convert to the async `QProcess` signal + timeout-timer pattern already established in `MimeService.cpp`, mirroring the async pattern already proven elsewhere in this same unit. At minimum, drop the redundant second `refresh()` call at the end of `rebuildApplicationCaches()`.

### [F-06] Duplicated subprocess-timeout boilerplate (and a triplicated magic-number `5000`) across three call sites in `MimeService.cpp`
- **Severity**: Low
- **Effort**: S
- **Location**: `mime/MimeService.cpp:243-284` (`runXdgSettings`), `:289-333`, `:360-406` (`ProcessMimeResolver::queryDefault`/`setDefault`)
- **Rationale**: All three independently allocate a `QProcess`, construct a singleshot 5000ms kill-timer, wire near-identical `finished`/`errorOccurred` handlers with a `shared_ptr<bool> completed` re-entrancy guard, and manage `deleteLater()` — ~40 lines of copy-pasted boilerplate per site, with the timeout literal duplicated three times. Confidence 80/100.
- **Suggested Direction**: Extract a single `runGuardedProcess(program, args, timeoutMs, callback)` helper parameterized by a named `kSubprocessTimeoutMs` constant, used by all three call sites.

## Investigation Targets (confidence 60-79 — human verification needed)

#### [I-01] `KdeCompatService` lacks the "completed" re-entrancy guard `MimeService` uses for the structurally identical `QProcess` pattern — risk of a double-advanced rebuild chain
- **Severity**: Medium · **Effort**: S · **Confidence**: 72/100 (independently corroborated by a second agent at 66/100)
- **Location**: `kde-compat/KdeCompatService.cpp:36-83`
- **Rationale**: Both `finished` and `errorOccurred` are connected on the same `proc`, guarded only by `if (error != QProcess::Crashed)` — correct for the `Crashed`/`FailedToStart` cases, but `ReadError`/`WriteError`/`UnknownError` can fire `errorOccurred` on a still-running process that later also emits `finished`, each independently calling `deleteLater()` and re-advancing the "sequential rebuild chain" (potentially spawning a duplicate `kbuildsycoca6` process or double-emitting `rebuildFinished`). `MimeService.cpp`'s structurally identical process helpers already solve this exact problem with a shared `completed` bool guard — that fix was never ported to `KdeCompatService`.
- **Suggested Direction**: Port the same `completed`-flag idiom from `MimeService.cpp` to both `runUpdateDesktopDatabase()`/`runKbuildsycoca()` handlers.

#### [I-02] `KdeCompatService`'s entire QML-facing API (`kdeWarningActive`/`rebuildCaches`/`warningEmitted`) has no consumer anywhere in QML or tests
- **Severity**: Low · **Effort**: M · **Confidence**: 72/100
- **Location**: `kde-compat/KdeCompatService.h:13-14,31-35`, registered as a context property in `ShellApplication.cpp:128,151`
- **Rationale**: Instantiated unconditionally at startup (running its `kbuildsycoca6`-presence probe and `XDG_MENU_PREFIX` check every session), but `grep` across `apps/shell/qml/` and `tests/` finds zero references to `KdeCompatService`, `kdeWarningActive`, `rebuildCaches`, or `warningEmitted`. Looks like a backend built but never wired to a UI surface — unlike `SessionIntegrationService`, which *is* fully wired into `SidebarSystem.qml` and appears to cover overlapping diagnostic ground (`addKdeCacheDiagnostics()`).
- **Suggested Direction**: Either wire `KdeCompatService` into a UI surface (warning banner or System sidebar tab), or remove it if `SessionIntegrationService` has superseded its purpose.

#### [I-03] `containsDesktopFiles`/`hasDesktopFiles` — byte-for-byte identical helper duplicated across two session-integration files
- **Severity**: Low · **Effort**: S · **Confidence**: 68/100
- **Location**: `ApplicationCacheRebuilder.cpp:9-12` vs `SessionIntegrationService.cpp:123-126`
- **Rationale**: Both are private, anonymous-namespace `QDirIterator`-based recursive `.desktop`-file scans, doing the identical filesystem walk independently in the same feature area (one during diagnostics, one during rebuild).
- **Suggested Direction**: Hoist into one shared free function used by both files.

#### [I-04] `KdeCompatService::warningEmitted()` re-fires on every diagnostics recheck while the warning condition persists, not only on activation
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `kde-compat/KdeCompatService.cpp:16-26`
- **Rationale**: `recheckDiagnostics()` emits `warningEmitted()` unconditionally whenever `missing_prefix` is true, with no comparison against the previous `kde_warning_active_` state — unlike the paired `kdeWarningActiveChanged()`, which is correctly gated on an actual value change. Called from the constructor and after every cache rebuild, so this could re-fire repeatedly while the misconfiguration remains unresolved.
- **Suggested Direction**: If meant as a one-shot "just became active" transition (implied by the naming), gate it the same way as `kdeWarningActiveChanged()`; if repeated re-emission is intentional, document the asymmetry.

#### [I-05] "PortalBackend"/"PortalDBus" naming used for two unrelated, opposite-direction abstractions in the same directory
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `portal/NullPortalBackend.h:14-88` vs `portal/SettingsPortalBackend.h:23-72`
- **Rationale**: `IPortalDBus`/`SystemPortalDBus`/`NullPortalDBus` is a *client-side* abstraction `PortalService` uses to probe the external portal broker. `SettingsPortalBackend` is an unrelated *server-side* class registering holonight itself as a portal implementation — no shared interface exists between them despite the adjacent naming, making it easy to wrongly assume `SettingsPortalBackend` is a production `IPortalDBus` implementation.
- **Suggested Direction**: Rename one side (e.g. `SettingsPortalBackend` → `HolonightSettingsPortalService`) to make the client/server direction unambiguous from the type name alone.

#### [I-06] `MimeService` performs a full `QHash` copy-on-write detach on every single-key cache update
- **Severity**: Low · **Effort**: S · **Confidence**: 63/100
- **Location**: `mime/MimeService.cpp:575-585`, `:596-615`
- **Rationale**: Both `onQueryResult()` and the per-mime `setDefaultAsync()` callback snapshot `mime_cache_` into `old_cache` immediately before mutating a single key — because `QHash` is implicitly shared, this forces a full deep-copy of the entire (currently small, ≤~15-entry) hash purely so `emitChangedSignals()` can diff. Not urgent at current scale.
- **Suggested Direction**: If the tracked-role set grows, snapshot only the affected role's specific keys rather than the whole map.

#### [I-07] `PortalService`'s async D-Bus probe calls rely on the ~25s libdbus default timeout instead of the unit's own established 5s convention
- **Severity**: Low · **Effort**: S · **Confidence**: 60/100
- **Location**: `portal/PortalService.cpp:59-70,271-285,199-217`
- **Rationale**: `nameHasOwner()`/`introspectPortal()`/`listNames()`/`readSetting()` are all fired without a per-call timeout override, unlike the explicit 5s kill-guards this same unit uses for `QProcess` calls in `MimeService.cpp`. A portal broker that accepts the connection but stalls leaves `available`/`colorScheme`/`accentColor` stale for up to ~25s with no diagnostic.
- **Suggested Direction**: Pass an explicit shorter timeout on the `asyncCall()`s in `SystemPortalDBus`, matching the unit's established 5s convention.

#### [I-08] Un-named magic numbers for freedesktop portal `color-scheme` enum values (1/2)
- **Severity**: Low · **Effort**: S · **Confidence**: 60/100
- **Location**: `portal/SettingsPortalBackend.h:29`, `.cpp:39-45`
- **Rationale**: The `org.freedesktop.appearance` `color-scheme` spec values (0=no-preference, 1=prefer-dark, 2=prefer-light) are encoded as bare integer literals in both `SettingsPortalBackend` and (inverted) `PortalService::setColorScheme`, unlike the rest of the file's named-constant convention for strings.
- **Suggested Direction**: Introduce shared named constants (`kColorSchemeDark`, `kColorSchemeLight`, `kColorSchemeNoPreference`) used by both files.

## Summary

| Category | Lint (reported) | Deep (confirmed ≥80) | Investigation (60-79) | Total |
|---|---|---|---|---|
| Model Contracts | 0 | 1 (F-03) | 1 (I-04) | 2 |
| Ownership & Lifecycle | 0 | 0 | 1 (merged into I-01) | 1 |
| Thread Safety | 0 | 0 (verified clean — single-threaded, event-loop-based) | 0 | 0 |
| API & C++ Correctness | 0 | 2 (F-01, F-02) | 1 (I-05) | 3 |
| Error Handling & Validation | 1 (ERR-4, refuted) | 1 (F-04, corroborated) | 1 (merged into I-01) | 1 |
| Performance & Code Quality | PAT-7 (1, refuted) | 2 (F-05, F-06) | 3 (I-03, I-06, I-08) | 5 |
| **Total** | **45 raw / 2 refuted / rest low-value** | **6** | **8** | **14 actionable** |

14 actionable items (6 confirmed + 8 investigation targets). This unit's headline result is the **mandatory doc-drift check**: `ThemeService` has genuinely outgrown its "trivial constant accessors" characterization and now embeds a full D-Bus portal-implementation service plus a self-rearming filesystem watcher — worth flagging to whoever maintains the audit trail, since a future contributor relying on that stale framing could easily miss the live-state surface. Substantively, the two highest-value findings are **[F-05]** (a confirmed, UI-thread-freezing synchronous `QProcess` chain reachable directly from the sidebar's System tab) and the three-way-corroborated **[F-04]**/[I-01] (`KdeCompatService`'s process-handling lacks both the timeout guard *and* the double-fire guard that its sibling `MimeService` already solved in the same unit — a clear "fix landed in one place, never ported to its twin" pattern worth fixing together in one pass.
