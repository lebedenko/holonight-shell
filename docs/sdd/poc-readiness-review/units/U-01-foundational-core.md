# U-01 — Foundational Core, Platform & Config — Deep Review Findings

**Task**: T-001 · **Skill**: `qt-cpp-review` (Phase 1 deterministic lint + Phase 2 six-agent deep analysis) · **Scope**: 30 files, read-only

## Scope

- `libs/holonight-platform/src/` — `LayerShell.h`, `LayerSurface.{h,cpp}`, `HyprlandIpc.{h,cpp}`, `HyprlandIpcClient.{h,cpp}`, `DbusPropertyClient.{h,cpp}` (9 files)
- `libs/holonight-core/src/` — `ConfigService.{h,cpp}`, `WorkspaceModel.{h,cpp}`, `ExtWorkspaceManager.{h,cpp}`, `HyprlandWorkspaceService.{h,cpp}`, `KeyboardLayoutService.{h,cpp}`, `BatteryState.{h,cpp}`, `AudioState.{h,cpp}`, `SystemInfo.{h,cpp}`, `Logger.{h,cpp}` (18 files)
- `libs/holonight-config/include/holonight_config/` + `libs/holonight-config/src/` — `config_parsers.h`, `config_structs.h`, `config_writer.h`, `ConfigParsers.cpp`, `ConfigWriter.cpp` (5 files, `config_writer.h` verified present alongside the 5 counted)

## Prior Context

Consulted before review, per T-001 instructions:

- `docs/sdd/arch-restructure-roadmap/DESIGN.md` §2.2 (dependency graph: `holonight_platform`/`holonight_config` sit at the bottom of the 5-layer static-lib stack, no upward dependencies) and its C1/C3/M1/M9 decisions (layered architecture, `LayerShellManager` ownership boundary, manual constructor-injection over a DI container, module split rationale).
- `docs/sdd/arch-restructure-roadmap/AUDIT.md` (coverage baseline for `holonight_core`/`holonight_platform` — used as a pre-restructure reference point, not as a target to re-litigate).

No doc-drift found: the current file layout and layering match §2.2 exactly — `holonight-platform` and `holonight-config` have no includes from higher layers, consistent with the roadmap's completed state. No further action needed against these prior docs for this unit.

## Tool Sign-off — Phase 1 Deterministic Lint

121 raw lint hits across 8 rule categories. Two categories are noise for this project and are excluded from the findings below with justification:

- **VAR-3** (82 hits, "prefer copy-init over direct brace-init") — pure style preference, not a correctness or safety issue; not reported.
- **HDR-3** (5 hits, "unprotected `std::min`/`std::max` — Windows macro safety") — this project is Linux-only (CLAUDE.md, Taskfile, CMake all Linux/Wayland-specific, no Windows target); the rule guards against a `<windows.h>` macro collision that cannot occur here. Not reported.

Remaining lint categories (PAT-2 ×18, TMO-1 ×4, DEP-13 ×4, DEP-11 ×3, LCY-6 ×2, PAT-9 ×1, MDL-7 ×1, ERR-6 ×1) were each independently verified by the Phase 2 agents. Two were confirmed as false positives (see **Refuted Lint Findings** below); the rest are low-severity style/modernization items not elevated to standalone findings (`PAT-2` `std::optional` default-construction is a real GCC-warning-bug pattern but is cosmetic; `TMO-1`/`DEP-13`/`DEP-11`/`PAT-9` are modernization suggestions with no behavioral impact).

## Confirmed Findings (confidence > 80/100)

### [F-01] Disabled `[[widget]]` config entries silently lose title/deadline/position/monitors on every parse
- **Severity**: High
- **Effort**: S
- **Location**: `libs/holonight-config/src/ConfigParsers.cpp:416-424` (`parseWidgetEntry`)
- **Rationale**: When `entry["enabled"] == false`, `parseWidgetEntry()` returns a `WidgetDefinition` with only `.enabled`/`.type` set — it skips `parseTimeToEventFields`, `parseClockFields`, `parseWidgetPositionField`, and `parseWidgetMonitors` entirely. `ConfigService::parseFile()` runs this on every load, and `apps/settings/src/ConfigFileService.cpp` round-trips the same `ParsedConfig` through `ConfigWriter::write()` on any Settings-app save. Net effect: disabling a widget in `config.toml`, then saving *any* unrelated change from the Settings app (e.g. an appearance tweak), permanently erases that widget's title, deadline, position, and monitor scoping — a real, silent data-loss path a user could hit today. Confidence 85/100.
- **Suggested Direction**: Parse title/deadline/position/monitors/clock fields unconditionally; use `enabled` only to skip the *validation-rejection* branches for invalid entries, not to skip parsing altogether.

### [F-02] `HyprlandIpcClient::connectEventStream` leaves the previous `connect_timeout_` timer armed on reconnect, can abort a freshly-opened socket
- **Severity**: Medium
- **Effort**: S
- **Location**: `libs/holonight-platform/src/HyprlandIpcClient.cpp:21-58` (compare correct pattern at `:219-223` in `finishCommand()`)
- **Rationale**: On reconnect, the old `connect_timeout_` is never `stop()`'d before being replaced; it stays parented to the old (deferred-`deleteLater`) socket and its `timeout` lambda inspects the *current* `event_socket_` member rather than checking `sender()` identity. Because the failure path (`onEventSocketError`/`onEventSocketDisconnected` → `scheduleReconnect()`) also never stops this timer, and both the connect-timeout and the initial reconnect backoff default to ~1000ms, a stale timeout can fire against a socket that was already replaced — logging a misleading "connection timed out" and aborting a connection attempt that just started. Self-heals via the same reconnect loop, so impact is transient reconnect churn rather than a permanent outage. Confidence 84/100.
- **Suggested Direction**: Call `connect_timeout_->stop()` before reassigning, mirroring the existing `command_timeout_->stop()` pattern in `finishCommand()`; additionally guard the lambda with a `sender() == connect_timeout_` identity check, as already done for the socket-signal handlers in this class.

### [F-03] `ConfigService::calendarConfig()` breaks the `const&` accessor convention, copies a list-holding struct on every call
- **Severity**: Medium
- **Effort**: S
- **Location**: `libs/holonight-core/src/ConfigService.h:36`
- **Rationale**: Every sibling getter (`appearance()`, `barWorkspaces()`, `weather()`, `notifications()`, …) returns `const T&` bound to the stored member. `calendarConfig()` alone returns `CalendarConfig` by value — a struct holding two `QList<...AccountConfig>`. `CalendarService.cpp` calls it three times across two functions (`:68`, `:124`, `:130`), each paying a full deep copy; `:124` copies the entire struct just to read one enum field (`week_start_day`). Independently flagged by two agents (Performance & Quality at 82/100, API & Correctness at 77/100 as a convention-consistency issue) — cross-agent agreement raises confidence.
- **Suggested Direction**: Change the signature to `[[nodiscard]] const CalendarConfig& calendarConfig() const { return calendar_config_; }`. No caller changes required — existing `const auto&` bindings at the call sites will then bind directly to the member instead of a temporary.

### [F-04] Inconsistent error logging across `HyprlandIpc.cpp` JSON response parsers
- **Severity**: Medium
- **Effort**: S
- **Location**: `libs/holonight-platform/src/HyprlandIpc.cpp:33-44, 64-94, 186-197` (silent) vs. `:255-275, 277-295, 297-327, 344-368` (logged)
- **Rationale**: `parseHyprlandActiveWindowJson`, `parseHyprlandKeyboardLayoutDevicesJson`, and `parseHyprlandActiveWorkspaceJson` return `std::nullopt`/defaults on malformed/unexpected-shape Hyprland IPC responses with zero logging, while `parseHyprlandMonitorsJson`, `parseHyprlandFocusedMonitorNameJson`, `parseHyprlandClientsJson`, and `workspaceIdForHyprlandClientAddressJson` all `qCWarning` on the identical failure class. A malformed/truncated `hyprctl` reply for active-window or active-workspace queries is diagnostically invisible while the same failure on a sibling query is logged — this is exactly the kind of gap that turns a compositor-side regression into an unexplained "workspace indicator stopped updating" bug report.
- **Suggested Direction**: Add `qCWarning(lcHyprlandIpc)` on the invalid-doc branches of the three silent parsers, mirroring the existing pattern in the other four.

### [F-05] `HyprlandIpcClient::runCommand` silently no-ops when the command socket path can't be resolved
- **Severity**: Medium
- **Effort**: S
- **Location**: `libs/holonight-platform/src/HyprlandIpcClient.cpp:60-68` (contrast with `connectEventStream` at `:21-34`)
- **Rationale**: `connectEventStream()` logs a warning and schedules a backoff retry when `HYPRLAND_INSTANCE_SIGNATURE` is unresolved. `runCommand()` hits the identical condition (`resolvedCommandSocketPath().isEmpty()`) but returns `false` with no log and no retry; callers (`HyprlandWorkspaceService::startCommand`, `KeyboardLayoutService::queryCurrentLayout`) either discard the bool or silently reset pending-command state. If a command fires before the environment variable is available, there is no log entry anywhere explaining why workspace/keyboard-layout queries never complete. Confidence 81/100.
- **Suggested Direction**: Log a `qCWarning` on the empty-path branch, mirroring `connectEventStream`; consider whether callers should retry/queue rather than silently drop the command.

### [F-06] `WorkspaceModel::roleNames()` rebuilds a `QHash` on every call
- **Severity**: Low
- **Effort**: S
- **Location**: `libs/holonight-core/src/WorkspaceModel.cpp:37-44`
- **Rationale**: Constructs a fresh 4-entry `QHash<int, QByteArray>` from an initializer list on every invocation instead of caching it. QML's delegate-model machinery can re-query `roleNames()` on resets/proxy-attach/re-binding, paying avoidable allocation each time. Confidence 85/100.
- **Suggested Direction**: Hoist into a `static const QHash<int, QByteArray>`, matching the caching pattern already used for other lookup tables in this codebase (e.g. `kKnownCodes` in `HyprlandIpc.cpp`).

### [F-07] `keyboardLayoutCode()` compiles a `QRegularExpression` on every call
- **Severity**: Low
- **Effort**: S
- **Location**: `libs/holonight-platform/src/HyprlandIpc.cpp:123`
- **Rationale**: Builds `QRegularExpression(QStringLiteral("[\\s(]"))` as a temporary on every call rather than a function-local `static const`. Runs on every `activelayout>>` Hyprland IPC event, so each keyboard-layout change pays full regex compilation. Confidence 82/100.
- **Suggested Direction**: Hoist to `static const QRegularExpression kWordSplitter(...)` — Qt regex objects are implicitly shared and safe to reuse this way — matching the existing `static const QHash kKnownCodes` pattern a few lines above in the same function.

### [F-08] Unnamed magic numbers for timeouts/backoff/buffer size in `HyprlandIpcClient`
- **Severity**: Low
- **Effort**: S
- **Location**: `libs/holonight-platform/src/HyprlandIpcClient.cpp:57,89,123,267,272` and `HyprlandIpcClient.h:100,105`
- **Rationale**: Connect timeout (`1000`), event-stream read chunk (`4096`), default command timeout (`2000`), initial reconnect backoff (`1000`, duplicated at two sites), and backoff cap (`30000`) are all bare literals. This is the transport underlying every Hyprland-backed service (workspace, keyboard layout) reconnect path, so these values are load-bearing for perceived responsiveness after a compositor restart but are invisible/unsearchable as tuning knobs. Confidence 80/100.
- **Suggested Direction**: Introduce named `static constexpr` constants (`kEventConnectTimeoutMs`, `kEventReadChunkBytes`, `kDefaultCommandTimeoutMs`, `kInitialReconnectDelayMs`, `kMaxReconnectDelayMs`) at class/anonymous-namespace scope.

## Refuted Lint Findings (false positives — verified and dismissed)

- **ERR-6** (`SystemInfo.cpp:248`, "`.arg()` has `%2` but only 1 `.arg()` call"): False positive. The call is the two-argument overload `QString::arg(candidate, QString::fromLatin1(extension))`, which binds both `%1` and `%2` in a single call — the lint heuristic miscounts multi-argument `.arg(a, b, …)` overloads as single-placeholder calls. Verified confidence 92/100. No action needed; lint rule should be adjusted to recognize multi-arg `.arg()` overloads.
- **MDL-7** (`WorkspaceModel.cpp:32`, "`data()` switch has `default:` — may hide unhandled roles"): False positive. `roleNames()` (lines 37-44) and the `data()` switch (lines 23-34) list exactly the same four roles, fully in sync — no unhandled role exists today. No action needed.
- **LCY-6** (`ExtWorkspaceManager.cpp:117-118`, "`qDeleteAll` — verify grandchildren also cleaned, non-recursive"): False positive for the flagged calls specifically. `ExtWorkspaceGroup::outputs_`/`::workspaces_` hold only non-owning, borrowed raw pointers used for identity comparison (never `delete`d); `ExtWorkspaceHandle::entry_` is a plain value member, not a pointer. Each object's own destructor correctly calls `destroy()` on its own Wayland proxy per the `ext-workspace-v1` protocol contract (verified against `protocols/ext-workspace-v1.xml:199-210,362-374`). Confidence 88/100. See investigation target below for one *adjacent*, genuinely missing cleanup this check surfaced.

## Investigation Targets (confidence 60-79 — human verification needed)

Capped at 10 per skill protocol; three items at the 60/100 floor (`WidgetPosition` enum's duplicated string mapping, `BatteryState::percent` unclamped, `ExtWorkspaceManager` handle lookup via linear scan) were dropped to stay within the cap and are noted here for completeness but not detailed.

#### [I-01] `WorkspaceModel::emitRowsChanged()` always emits `dataChanged` with no roles over the full row range
- **Severity**: Low · **Effort**: S · **Confidence**: 72/100
- **Location**: `libs/holonight-core/src/WorkspaceModel.cpp:313-318`, called from `applyBatchUpdate`, `setFocusedWorkspaceId`, `setOccupiedWorkspaceIds`, `addUrgentWorkspaceId`
- **Rationale**: Every focus/occupancy/urgency change (driven by frequent Hyprland IPC events) triggers a full-range, all-roles `dataChanged`, even though typically only `wsState` changes for at most one or two rows. Forces every bound QML delegate to re-evaluate all four roles on every event.
- **Suggested Direction**: Track which workspace ids actually changed and emit `dataChanged(index(row), index(row), {WorkspaceStateRole})` per affected row.

#### [I-02] `ExtWorkspaceManager` constructor uses `Q_ASSERT` as the sole null guard for `config`, then unconditionally dereferences it and `model_`
- **Severity**: Low · **Effort**: S · **Confidence**: 70/100
- **Location**: `libs/holonight-core/src/ExtWorkspaceManager.cpp:100-106`
- **Rationale**: `Q_ASSERT` compiles out in release builds; a null `config` would null-deref instead of asserting. Not currently reachable — the sole call site (`ShellApplication.cpp:99`) always passes valid, freshly-constructed objects — hence investigation-target rather than confirmed.
- **Suggested Direction**: Replace/augment with a runtime null check that fails loudly in both debug and release.

#### [I-03] `DbusPropertyClient::serviceRegistered` swallows failures without logging
- **Severity**: Low · **Effort**: S · **Confidence**: 68/100
- **Location**: `libs/holonight-platform/src/DbusPropertyClient.cpp:48-57`
- **Rationale**: Every sibling method in the same file (`property`, `allProperties`, `setProperty`, `upowerDevices`) logs on failure; `serviceRegistered` has two failure paths (`bus_iface == nullptr`, invalid D-Bus reply) and logs neither, despite gating entire feature availability (e.g. "is UPower present?") on this boolean.
- **Suggested Direction**: Add `qCWarning` logging the service name and error message, consistent with sibling methods.

#### [I-04] Config TOML parse failure produces no application-visible signal beyond a log line
- **Severity**: Low · **Effort**: M · **Confidence**: 66/100
- **Location**: `libs/holonight-core/src/ConfigService.cpp:78-84`
- **Rationale**: On a malformed hand-edited `config.toml`, only a `qCWarning` is emitted; no Qt signal informs QML/Settings UI that the config is broken. A typical user editing `config.toml` by hand is unlikely to check `holonight.log`. Deliberate simplicity tradeoff rather than a crash risk, hence sub-80 confidence as a "finding" vs. a UX gap worth a design decision.
- **Suggested Direction**: Consider a dedicated `configParseError(QString message)` signal so a settings UI or notification path can surface the failure.

#### [I-05] `BatteryState` property parsing does a double hash lookup per field
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `libs/holonight-core/src/BatteryState.cpp:24-51`
- **Rationale**: `contains(key)` followed by `value(key)` for each of 8 fields — two `QVariantMap` lookups where one would do.
- **Suggested Direction**: Use `constFind`/check `isValid()` on the single lookup result instead of a separate `contains()` call.

#### [I-06] Copy-pasted address-matching parse logic within `HyprlandIpc.cpp`
- **Severity**: Low · **Effort**: S · **Confidence**: 65/100
- **Location**: `libs/holonight-platform/src/HyprlandIpc.cpp:329-368`
- **Rationale**: `workspaceIdForHyprlandClientAddress()` and `workspaceIdForHyprlandClientAddressJson()` independently duplicate the same trim/empty-check/`endsWith` matching logic instead of the JSON variant delegating to the already-parsed-list variant.
- **Suggested Direction**: Implement the JSON variant in terms of `parseHyprlandClientsJson()` + `workspaceIdForHyprlandClientAddress()`.

#### [I-07] Logger console-output path is unsynchronized while the file-write path is explicitly mutex-protected
- **Severity**: Low · **Effort**: S · **Confidence**: 62/100
- **Location**: `libs/holonight-core/src/Logger.cpp:114-135` (contrast with the mutex-protected file-write block at `:88-95`)
- **Rationale**: `messageHandler()` is invoked from whatever thread calls `qCDebug`/`qCWarning`/etc. — confirmed elsewhere in the tree, multiple services log from `QtConcurrent` worker threads. The file-write block is correctly guarded by `ctx.log_mutex`; the console-print block that follows is not, so concurrent loggers can interleave console output relative to each other and relative to file order, relying on unstated libc stream-locking rather than an explicit guarantee in this code.
- **Suggested Direction**: Extend `ctx.log_mutex` (or a second dedicated mutex) to cover the console-print block, or explicitly document that console-interleaving-safety is an accepted tradeoff deferred to libc.

#### [I-08] `ExtWorkspaceManager` never sends the destroy/stop request for its own `ext_workspace_manager_v1` protocol object
- **Severity**: Low · **Effort**: S · **Confidence**: 62/100
- **Location**: `libs/holonight-core/src/ExtWorkspaceManager.cpp:116-119` (destructor)
- **Rationale**: Surfaced while verifying the LCY-6 lint hit: groups/handles are torn down protocol-correctly, but the manager's own base `ext_workspace_manager_v1` object is never explicitly destroyed (the generated base destructor is an empty no-op). Leaks one `wl_proxy`, reclaimed only at process-connection teardown — low real-world impact given the manager is constructed once for the process lifetime.
- **Suggested Direction**: Low priority; only worth fixing if construction/destruction of `ExtWorkspaceManager` within a running process (not just at exit) is ever expected.

#### [I-09] `HyprlandWorkspacePendingQuery` enum missing trailing comma on last enumerator
- **Severity**: Low · **Effort**: S · **Confidence**: 62/100
- **Location**: `libs/holonight-core/src/HyprlandWorkspaceService.h:16-22`
- **Rationale**: Has an explicit underlying type (good) but no trailing comma, unlike the other multi-line enums in scope (`config_structs.h`).
- **Suggested Direction**: Add trailing comma for diff-hygiene consistency.

#### [I-10] Weather latitude/longitude accepted from config with no range validation
- **Severity**: Low · **Effort**: S · **Confidence**: 62/100
- **Location**: `libs/holonight-config/src/ConfigParsers.cpp:267-280`
- **Rationale**: Only a type check (`double`) is applied to `weather.latitude`/`weather.longitude`; no bounds check against the physically valid `[-90,90]`/`[-180,180]` ranges before the value is stored and presumably forwarded to a weather API.
- **Suggested Direction**: Add a range check alongside the existing type check, falling back to `std::nullopt` (triggering IP geolocation) on out-of-range values.

## Summary

| Category | Lint (reported) | Deep (confirmed >80) | Investigation (60-79) | Total |
|---|---|---|---|---|
| Model Contracts | 1 (MDL-7, refuted) | 0 | 1 | 1 |
| Ownership & Lifecycle | 1 (LCY-6, refuted) | 1 | 2 | 3 |
| Thread Safety | 0 | 0 | 1 | 1 |
| API & C++ Correctness | 1 (PAT-9, noted) | 1 | 2 | 3 |
| Error Handling & Validation | 1 (ERR-6, refuted) | 2 | 3 | 5 |
| Performance & Code Quality | remaining lint (PAT-2/TMO-1/DEP-13/DEP-11) | 4 | 2 | 6 |
| **Total** | **121 raw / 2 refuted / rest low-value** | **8** | **10** | **18 actionable** |

18 actionable items (8 confirmed findings + 10 capped investigation targets) plus 3 verified-false-positive lint hits dismissed with rationale. No Critical or blocking findings in this unit — the foundational layer (`holonight_platform`/`holonight_core`/`holonight_config`) is structurally sound and matches the `arch-restructure-roadmap` design intent, with no doc-drift. The highest-severity item ([F-01], disabled-widget field loss) is a real silent-data-loss bug worth prioritizing early since it affects the config round-trip every other unit's Settings-app work (U-11) depends on.
