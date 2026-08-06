# Architecture & Design Gaps — POC Readiness Review

**Task**: T-012 · **Input**: U-01 through U-11 unit findings + DESIGN.md §5 preliminary observations (#1–7)

This section synthesizes systemic weaknesses that recur across multiple independently-reviewed units — patterns invisible from any single unit's findings list but clear once all eleven are read together. Per-unit nitpicks are not repeated here; only issues with a common root cause spanning ≥2 units are included. Suggested directions assume REQ-C-2 (aggressive refactoring acceptable, no backward-compat constraint) since this is a pre-production POC.

---

## 1. Silent failure paths are the dominant defect class across the entire codebase

**Affected units**: U-01, U-02, U-03, U-04, U-05, U-06, U-07, U-11 (8 of 11 units)
**Severity**: High · **Effort**: L (systemic, not a single fix)

Every single unit's summary independently converges on the same observation: a failure occurs, and nothing downstream — no signal, no log, no user-visible state — reflects it. U-01 flagged silent config-parse failure and unlogged D-Bus service-registration failures. U-03 found `LowBatteryMonitor::sendNotification` discarding its D-Bus reply. U-04 produced the largest cluster: PulseAudio never reconnects after context failure (F-01), every PulseAudio mutation call passes null callbacks so volume/mute changes fail invisibly (F-02), session lock/logout/suspend commands discard their `bool` success result end-to-end (F-03, security-relevant), and Wi-Fi authentication failures surface only as a generic "disconnected" after 2.5s (F-04). U-05 found `MimeService`'s `Q_PROPERTY` can silently desync from D-Bus reality (F-03). U-06 found notification-rule persistence failures are silent (F-01) and `NotificationServer`'s process-spawn failures produce zero diagnostics anywhere in the unit (F-04). U-07's Critical finding ([F-01]) is a dead-code timeout guard that turns a hung CalDAV server into a silently "successful" empty sync. U-11 found weather coordinates are silently deleted on every settings save (D-C1) and config-load failures are invisible while save failures are not (D-C5).

**Root cause**: there is no shared convention for surfacing degraded state — some subsystems built one (D-Bus `saveError`/`syncError` signals exist in isolated spots) but it was never generalized, so each new feature reinvents (or omits) error propagation independently.

**Suggested direction**: establish one project-wide convention — e.g., every service-layer class that can fail exposes a `lastError`/`healthState`-style property and emits a `*Error(QString)` signal — and retrofit the highest-severity instances first (U-04's F-03 session-lock silence and U-07's F-01 CalDAV timeout, both of which mask total feature failure as success).

## 2. Copy-pasted logic diverges silently, and has already caused real bugs

**Affected units**: U-01, U-02, U-03, U-04, U-05, U-06, U-07, U-11 (8 of 11 units)
**Severity**: High · **Effort**: M

This is not abstract duplication-aversion — cross-unit evidence shows duplication is an active bug source here, not just style debt. U-07's Critical CalDAV timeout bug ([F-01]) exists *because* `CalDavProvider` and `IcsProvider` independently hand-rolled the same synchronous-HTTP-with-timeout idiom ([F-10]) and one copy silently dropped the `abort()` call the other has. U-04's [F-05] (broken `setDefaultOutput`/`setDefaultInput` overloads) stems from the same duplicated-D-Bus-parsing root as [F-13]. U-03's [F-01] found the exact same subtle, previously-crash-fixing logind-session workaround independently re-implemented in `IdleService` and `SysfsBackend` — a future fix to one would silently leave the other vulnerable to the original bug. U-05 found the theme-config path computed independently in three places ([F-02], plus a third looser copy in `MimeService`), and subprocess-timeout boilerplate triplicated ([F-06]). U-11's D-C4 found the same mode→scheme mapping ternary hand-written across three classes at up to six call sites.

**Suggested direction**: this is a stronger argument for shared helpers than typical DRY concerns, because divergence here has already produced a Critical-severity bug (U-07 F-01/F-10). Prioritize extracting the HTTP-sync-with-timeout helper (U-07) and the logind-session-resolution helper (U-03) first, since both wrap previously-debugged, non-obvious correctness logic where a second silent fork is the likeliest failure mode.

## 3. Every custom `QAbstractListModel` rebuilds `roleNames()` from scratch on every call

**Affected units**: U-01, U-02, U-04, U-06, U-07, U-11 (6 of 11 units, ~10 distinct model classes)
**Severity**: Low (individually) / Medium (systemically) · **Effort**: S per site, but recurs everywhere

`WorkspaceModel` (U-01), `TrayModel`/`DbusMenuModel` (U-02), `WifiNetworkModel`/`AudioDeviceModel`/`AudioStreamModel` (U-04), `LauncherModel`/`NotificationRuleModel`/`NotificationService` (U-06), `CalendarEventModel` (U-07), and `FontListModel` (U-11) all independently construct a fresh `QHash<int, QByteArray>` literal on every single `roleNames()` invocation instead of caching it once. No unit found a shared base class or convention that would have prevented this — each model was written independently and each author made the identical omission.

**Suggested direction**: this is the clearest case in the whole review for a small shared abstraction: a `CachedRoleNamesMixin` or a one-line `static const QHash` convention documented in CLAUDE.md, applied in one pass across all ~10 sites. Low individual risk but worth fixing in bulk precisely because it's mechanical and the pattern will otherwise keep recurring in every future model.

## 4. Untrusted local-IPC input is insufficiently bounded — a real, unauthenticated attack surface

**Affected units**: U-02, U-06
**Severity**: Critical · **Effort**: S–M

U-02's [F-01] is this review's single highest-severity finding: any process registering as a `StatusNotifierItem` on the session bus (no privilege required) can trigger a multi-gigabyte allocation via integer overflow in tray-icon pixmap decoding, crashing the shell. U-02's [F-02] lets any local process with control-socket access blindly close the user's sidebar via a malformed monitor name, and [I-08] notes the control socket itself has no message framing or size bound. U-06's [F-02] found `NotificationServer::Notify()` (also callable by any session-bus process) performs no length validation, and unconditionally logs the full unbounded payload to disk — a second unauthenticated local DoS vector against the same log file U-01 depends on for diagnostics.

**Suggested direction**: treat this cluster as a single security pass rather than three unrelated bugs — every local-IPC-facing entry point (tray D-Bus, control socket, notification D-Bus) needs the same discipline: reject non-positive/oversized dimensions before allocating, cap string/payload lengths before logging or storing, and bound socket reads. Fix U-02 [F-01] first given its Critical severity and trivial reproduction (register a fake `StatusNotifierItem`).

## 5. Synchronous blocking I/O on the UI thread, with fixes proven but never propagated to siblings

**Affected units**: U-04, U-05, U-07, U-09
**Severity**: High · **Effort**: M

U-05's [F-05] is the sharpest instance: `SessionIntegrationService`'s diagnostics/cache-rebuild chain runs up to ~7 sequential blocking `QProcess` calls directly on the UI thread (worst case ~35s freeze), reachable simply by opening the sidebar's System tab — confirmed by U-09's [I-001] as re-triggered on *every* tab (re)creation, since the sidebar is create-on-open. U-05's [F-04] found `KdeCompatService`'s process calls have no timeout guard at all, unlike three sibling call sites in the very same unit (`SessionIntegrationService`, `MimeService`) that already solved this. U-04's [F-09] found `SystemInfoService` blocks shell startup itself on an unbounded D-Bus call. U-07's [F-04] found `LibsecretCredentialStorage`'s constructor makes a blocking secrets-daemon D-Bus call on the main thread, called from calendar config changes.

**Root cause**: the async-with-timeout pattern exists and works correctly in this codebase (`MimeService`'s guarded `QProcess` helper) — it simply was never generalized or ported to the sibling classes solving the identical problem.

**Suggested direction**: extract `MimeService`'s guarded-subprocess pattern into a shared helper and migrate `KdeCompatService` and `SessionIntegrationService` onto it (`QtConcurrent::run` + `QFutureWatcher`, matching the idiom already proven elsewhere in this codebase per DESIGN.md §1.3.1's abstract-backend pattern).

## 6. Backend APIs built to completion, then never wired to their intended caller

**Affected units**: U-02, U-05, U-06, U-07, U-09
**Severity**: Medium · **Effort**: S–M to wire, trivial to delete

A recurring shape: a class is fully implemented, documented, and unit-tested, but has zero production callers. U-02 found `PopupGeometry` (F-05) and `sidebarSurfaceWidth()` (F-07) both dead. U-05 found `KdeCompatService`'s entire QML-facing API has no consumer anywhere, its diagnostic purpose apparently superseded by `SessionIntegrationService` without anyone removing the superseded class (I-02). U-06 found `LauncherService::desktop_file_index_` is built and maintained on every keystroke but never read (F-06). U-07 found the most consequential instance: `CalendarCache`'s account-removal cleanup API (`removeStaleAccounts`, `clearAccountEvents`) is fully implemented with doc comments describing the intended flow, but has zero production callers — a user who removes a CalDAV account sees its events persist indefinitely (F-03). U-09 found `SidebarSessionBar.qml` orphaned entirely (D-005) and six sidebar tabs' `preferredWidth` properties dead, with the shadow copy that actually drives width already 20px out of sync (D-001).

**Suggested direction**: audit each of these for intent before deleting — several (U-07's F-03 especially) read as "integration step forgotten," not "deliberately abandoned," and should be wired in rather than removed. Where a class has been genuinely superseded (U-05's KdeCompatService/SessionIntegrationService overlap), remove the superseded one rather than maintaining two parallel implementations of the same diagnostic.

## 7. Composition-root and manager invariants are enforced only by assertions that vanish in Release builds

**Affected units**: U-01, U-02 (directly substantiates DESIGN.md §5 observation #1)
**Severity**: High · **Effort**: M

DESIGN.md §5 flagged `ShellApplication`'s 40+ member constructor as a "concentration-of-risk, not yet a proven defect" before unit review began. U-02's [F-03] confirmed it is a proven defect: the three-phase `registerQmlTypes()` → `startServices()` → `startShell()` startup sequence is enforced only by `Q_ASSERT(registered_ && services_started_)`, which compiles to nothing in Release builds — a future entry point calling phases out of order would silently produce a half-initialized shell with no crash and no diagnostic. U-01's [I-02] found the identical shape one layer down: `ExtWorkspaceManager`'s constructor uses `Q_ASSERT` as its sole null-guard for a dependency it then unconditionally dereferences.

**Suggested direction**: replace both `Q_ASSERT`s with loud `qCritical` + early-return (or make the dependent phases self-enforcing, since both are already idempotent) so a Release-build ordering mistake fails visibly. This is a small, mechanical fix but a high-value one given DESIGN.md independently flagged the composition root as this codebase's single largest concentration of correctness risk.

---

## Cross-Reference Summary

| Systemic Issue | Units Touched | Also Substantiates |
|---|---|---|
| #1 Silent failure paths | U-01,02,03,04,05,06,07,11 | — |
| #2 Diverged duplication | U-01,02,03,04,05,06,07,11 | Caused U-07's Critical [F-01] |
| #3 `roleNames()` rebuild | U-01,02,04,06,07,11 | — |
| #4 Untrusted local IPC input | U-02,06 | This review's only Critical security finding |
| #5 UI-thread blocking I/O | U-04,05,07,09 | U-09 [I-001] confirms U-05 [F-05] fires on every sidebar open |
| #6 Built-but-unwired APIs | U-02,05,06,07,09 | — |
| #7 Release-mode-silent assertions | U-01,02 | DESIGN.md §5 observation #1 |

No single unit's findings alone would have surfaced these seven patterns as *systemic* — each looked like an isolated one-off until cross-referenced against the other ten units. Issues #1 and #2 in particular span nearly three-quarters of all reviewed units and represent the two highest-leverage remediation targets: fixing either would prevent an entire class of future findings, not just the ones already caught.
