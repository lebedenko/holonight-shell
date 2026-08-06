# POC Readiness Review — Comprehensive Audit Report

| Field | Value |
|---|---|
| Document ID | POC-Readiness-Review/REPORT.md |
| Task | T-014 (final synthesis) |
| Inputs | DESIGN.md, 11 unit findings documents (U-01–U-11), ARCHITECTURE-GAPS.md (T-012), STALE-BACKLOG.md (T-013) |
| Review method | `qt-cpp-review` / `qt-qml-review` skills, sequential per-unit execution with human checkpoints, read-only (REQ-C-1) |
| Confidence floor | >80/100 for "Confirmed Findings"; 60–79/100 reported separately as "Investigation Targets" (REQ-C-4) |
| Status | Final — ready for handoff to a remediation SDD pipeline |

---

## 1. Executive Summary

This report closes out an 11-unit, sequential, read-only architecture and code-quality review of the `holonight-shell` codebase, commissioned to establish whether the project — self-described as a "pre-production POC" — is ready to graduate toward production hardening, and if not, what stands in the way.

**Scope.** The review covers 100% of `libs/` and `apps/` — 361 C++/QML source files, split into 11 review units of 18–55 files each (average 32.8 files/unit), matching DESIGN.md's independently-verified file-count reconciliation. Seven units are pure C++ (`qt-cpp-review`), three are pure QML (`qt-qml-review`), and one (Settings) is mixed and received both. `tests/` (75 files) and the shared `qml/HoloNight/` component plugin (2 files) were explicitly out of REQ-F-1's scope and are not covered here. Every unit was reviewed against the intent recorded in its relevant prior SDD cycle documents, with explicit doc-drift checks built into several tasks' mandatory verification steps.

**Headline result: the architecture is sound; the defects are almost all in error-handling discipline, not structure.** DESIGN.md's pre-review architecture pass (§1) found a genuinely well-layered, CMake-enforced five-library dependency graph with a single, narrow, tooled exception — and every one of the 11 units confirmed that assessment holds under close reading. No unit found a structural layering violation, an undocumented circular dependency, or evidence that the completed `arch-restructure-roadmap` cycle's decisions have eroded. What the unit-by-unit review *did* find, repeated independently across 8 of 11 units, is a single dominant defect class: **a failure occurs and nothing downstream reflects it** — a discarded D-Bus reply, a silently-skipped config field, a write that fails without a signal. This is documented in full in §5 (Architecture & Design Gaps, gap #1) and is this review's single most consequential finding, because it means the codebase's real defect count is likely *higher* than what a review can observe — silent failures by definition don't announce themselves during ordinary testing.

**Finding counts.** Across all 11 units: **89 confirmed findings** (>80/100 confidence) and **102 investigation targets** (60–79/100, requiring human verification before action) — 191 actionable items total, plus several hundred low-value lint hits (brace-init style, attribute ordering) explicitly excluded as noise per REQ-C-4's discipline. Of the 89 confirmed findings: **2 are Critical**, **15 are High**, and the remainder split between Medium and Low. Three refuted lint false-positives and one significant project-memory correction (§4, U-07) were also produced as review byproducts.

The two **Critical** findings are both genuine, unauthenticated local-attack-surface bugs, not theoretical: **U-02 [F-01]** — any process registering as a `StatusNotifierItem` on the session D-Bus (no privilege required) can trigger a multi-gigabyte allocation via integer overflow in tray-icon pixmap decoding, crashing the entire shell across all monitors; and **U-07 [F-01]** — a dead-code timeout guard in the CalDAV sync path means a hung calendar server silently produces a "successful" empty sync with zero diagnostic, masking total feature failure as success indefinitely. Both are cheap to fix (Effort: S) and should be prioritized ahead of everything else in this report.

**Recommended remediation priority** (detailed in §7): (1) the two Critical findings and a small cluster of adjacent local-IPC-hardening fixes; (2) the seven cross-cutting architecture gaps in §5, starting with the silent-failure-path convention and the Release-mode-silent composition-root assertions, since fixing these prevents entire *future* classes of the same bug rather than just today's instances; (3) the diverged-duplication cluster (HTTP timeout handling, logind session resolution, theme-path computation), because divergence here has *already* produced a Critical bug (U-07 F-01/F-10) and is the highest-leverage prevention target; (4) remaining unit-level High/Medium findings; (5) the large, low-risk, purely-mechanical `roleNames()` caching sweep (~10 sites) and other Low-severity items as background cleanup. Section 6 catalogs 34 pre-existing unchecked tasks from 10 prior SDD cycles, explicitly out of this review's scope, for the same remediation pipeline's later awareness.

**What this review is not.** It made no code changes (REQ-C-1) and did not re-litigate architectural decisions the completed `arch-restructure-roadmap` cycle already settled. It is a snapshot as of 2026-07-05; DESIGN.md §5 observation #7 already flags that formal test-coverage auditing is roughly five weeks and ~35 SDD feature cycles stale, and this review does not attempt to refresh that separately-scoped concern.

---

## 2. Architecture & Component Review

*(Pulled and condensed from DESIGN.md §1 — see that document for full detail.)*

### 2.1 Layering

The codebase is a clean, strictly-layered dependency graph enforced at the CMake level:

```
holonight_platform   (Wayland/Hyprland/D-Bus transport primitives)
       ^
holonight_config     (toml++ parsing/writing of on-disk config)
       ^
holonight_core       (shared models/state — PUBLIC-links config + platform)
       ^
holonight_services   (14 subsystem subdirectories + 16 root-level classes — PUBLIC-links core + platform)
       ^
holonight_surfaces   (layer-shell surface orchestration — PUBLIC-links core + platform + services)
       ^
holonight_app        (ShellApplication composition root + ControlServer)
       ^
holonight-shell       (executable; owns the qt6_add_qml_module for all shell QML)
```

`apps/settings` is a separate, parallel executable (`holonight-settings`) linking only `holonight_config` plus an external sibling package (`HolonightQt`) and the shared `holonight_componentsplugin` — architecturally isolated from the shell's runtime service graph. U-11 independently re-confirmed this isolation holds with zero shell-service includes.

Every static-lib `CMakeLists.txt` declares dependency edges explicitly and consistently as `PUBLIC`. The one intentional, narrow exception — `holonight_surfaces` linking `holonight_services` for "narrow presentation orchestration" — is enforced at commit time by `scripts/check-architecture-boundaries.sh`, which allow-lists exactly two headers (`MonitorOccupancyService.h`, `NotificationService.h`) and fails the build on any other cross-include. This passed cleanly during the review period. A prior internal inconsistency between the `arch-restructure-roadmap`'s terse v1.1 revision-history bullet ("dropped the edge entirely") and its more careful §2.2/§3.4 text (a narrowed, tooled dependency) was resolved in favor of the detailed text, which matches current code exactly — this is not a regression, and Stage 4 review correctly declined to re-flag it.

### 2.2 Technology Stack

Qt6/C++23 throughout. Wayland via `wlr-layer-shell-unstable-v1`, `ext-workspace-v1`, `ext-idle-notify-v1`, `xdg-shell`. D-Bus is the dominant integration mechanism — UPower, NetworkManager, power-profiles-daemon (dual bus-name support), `org.freedesktop.Notifications` (the shell **is** the notification daemon), tray protocols, XDG portal, logind, libsecret — and CLAUDE.md documents numerous hard-won, previously-hit D-Bus landmines that the review verified are still correctly worked around (see §2.4). Audio uses direct libpulse C API via a `pa_threaded_mainloop`, manually verified in U-04 to marshal correctly to the Qt main thread via `Qt::QueuedConnection` on every single callback path — a genuine strength, not just an absence-of-bugs finding. SQLite (via Qt6::Sql) backs the launcher desktop-entry cache and calendar cache; tomlplusplus backs on-disk config; libsecret backs CalDAV credentials; GTest + QtQuickTest/CTest with a custom `FakeQmlServices` harness backs testing.

### 2.3 Cross-Cutting Patterns Actually Observed

Seven patterns recur consistently across independently-developed subsystems, strongly suggesting a coherent internal convention rather than coincidence:

1. **Abstract backend + concrete/Null implementation, injected via constructor** — `SessionBackend`, `BrightnessBackend`, `IdleBackend`, `IPortalDBus`, `ICalendarProvider`, each with a `Null*`/fake sibling serving as both graceful runtime fallback and test-injection seam.
2. **QML singleton facade over a service** — 29+ services registered via a generic `reg(obj, name)` lambda in `ShellApplication::registerQmlTypes()`, scaled cleanly from the `arch-restructure-roadmap`'s original 13 with no drift.
3. **D-Bus protocol adapter classes that own no state** — `NotificationServer`/`TrayWatcher`, thin `QObject`+`QDBusContext` classes delegating all logic to a paired `*Service`/`*Model`.
4. **Composition root owns everything; DI is manual constructor injection** — `ShellApplication`'s ~40-member composition root, no `ServiceRegistry`, matching the roadmap's explicit "why no DI container" decision.
5. **Layer-shell surface lifecycle as a shared, reused concern** — `PerMonitorLayerManager` underlies three independent presentation features (topbar, background, widgets); a bug here is a bug in all three.
6. **Activity-gate fan-out for power-sensitive work** — `IActivityGate` implemented by `CalendarService`/`WeatherService`/`SuspendInhibitorService`, fanned out via `ActivityGateManager`, one of the few places using an explicit interface + registry instead of ad hoc pairwise wiring.
7. **`ConfigService` as the shared mutable read/write surface** — config plumbing centralized, not duplicated per subsystem, unlike (per §5 gap #2) other cross-cutting concerns such as HTTP-timeout handling or session resolution, which were *not* centralized and have already diverged.

### 2.4 Known Strengths Worth Preserving

- The CMake target boundaries are real and tooled, not aspirational — uncommon rigor for a self-described pre-production POC.
- The abstract-backend/test-seam pattern is applied with genuine consistency across independently-scoped SDD feature cycles (session, brightness, idle, portal, calendar), not just in one flagship subsystem.
- CLAUDE.md itself is a real architectural asset: dozens of narrow, empirically-verified gotchas are captured centrally. Every unit's reviewers cross-checked candidate findings against it before reporting, and several plausible-looking "bugs" (e.g., the `loginctl show-seat` shell-out instead of a "proper" D-Bus call) were correctly recognized as documented, deliberate workarounds for real Qt/D-Bus/libdbus defects — not oversights, and not re-flagged.
- The libpulse thread-boundary marshalling pattern (U-04) and the `IActivityGate` semantic consistency (U-07) were both independently verified end-to-end and are explicitly called out as strengths, not merely absence of findings.

---

## 3. Review Unit Inventory

11 units, 361 files, 100% coverage of `libs/`+`apps/` (verified by DESIGN.md's independent arithmetic reconciliation: 32+55+35+36+18+30+31+27+47+30+20 = 361).

| ID | Name | Files | Language | Tooling | Confirmed Findings | Investigation Targets |
|---|---|---|---|---|---|---|
| U-01 | Foundational Core, Platform & Config | 32 | C++ | `qt-cpp-review` | 8 | 10 |
| U-02 | Presentation Surfaces & Shell Composition Root | 55 | C++ | `qt-cpp-review` | 7 | 10 |
| U-03 | Power, Idle & Brightness Management | 35 | C++ | `qt-cpp-review` | 5 | 10 |
| U-04 | Session, Window Identity, Network & Audio Services | 36 | C++ | `qt-cpp-review` | 13 | 10 |
| U-05 | Desktop Integration, Appearance & Portal | 18 | C++ | `qt-cpp-review` | 6 | 8 |
| U-06 | Launcher & Notifications Services | 30 | C++ | `qt-cpp-review` | 9 | 10 |
| U-07 | Calendar & Weather Services | 31 | C++ | `qt-cpp-review` | 13 | 10 |
| U-08 | Topbar QML | 27 | QML | `qt-qml-review` | 7 | 8 |
| U-09 | RightSidebar + Popups QML | 47 | QML | `qt-qml-review` | 6 | 7 |
| U-10 | Overlay Content & Shared QML Building Blocks | 30 | QML | `qt-qml-review` | 7 | 7 |
| U-11 | Settings Application | 20 | C++ & QML | Both | 8 | 12 |
| | **Total** | **361** | | | **89** | **102** |

---

## 4. Per-Unit Deep Findings

### U-01 — Foundational Core, Platform & Config (32 files)

**Scope**: `libs/holonight-platform/src/` (9 files — `LayerShell`, `LayerSurface`, `HyprlandIpc`, `HyprlandIpcClient`, `DbusPropertyClient`), `libs/holonight-core/src/` (18 files — `ConfigService`, `WorkspaceModel`, `ExtWorkspaceManager`, `HyprlandWorkspaceService`, `KeyboardLayoutService`, `BatteryState`, `AudioState`, `SystemInfo`, `Logger`), `libs/holonight-config/` (5 files — parsers/writer/structs).

**Prior context consulted**: `arch-restructure-roadmap/DESIGN.md` §2.2 and `AUDIT.md`. No doc-drift found — current layering matches the roadmap's target exactly.

**Tool sign-off**: 121 raw lint hits, 2 refuted false positives (an `.arg()` multi-placeholder miscounting bug in the lint rule itself, and a `qDeleteAll` ownership false alarm correctly ruled out against the `ext-workspace-v1` protocol contract).

**Key findings by category**:
- *Correctness*: **[F-01, High]** disabled `[[widget]]` config entries silently lose their title/deadline/position/monitors on every parse-and-resave round-trip through the Settings app — a real, reachable data-loss bug affecting every downstream config consumer, including U-11's Settings work.
- *Error handling*: **[F-04, Medium]** three of seven Hyprland IPC JSON parsers silently swallow malformed responses while four sibling parsers correctly log — an inconsistency that turns a compositor regression into an unexplained "workspace indicator stopped updating" report. **[F-05, Medium]** `HyprlandIpcClient::runCommand` silently no-ops with no retry when the Hyprland socket path can't be resolved.
- *Reliability*: **[F-02, Medium]** a stale reconnect timer in `HyprlandIpcClient` can abort a freshly-opened socket, causing transient reconnect churn.
- *Performance/maintainability*: **[F-06, F-07, F-08, Low]** the first three instances (of an eventual ~10 across the whole review) of the systemic `roleNames()`-rebuilt-every-call pattern (§5 gap #3), an uncached regex compilation, and unnamed magic-number timeouts.
- *API consistency*: **[F-03, Medium]** `ConfigService::calendarConfig()` uniquely breaks the codebase's `const&` accessor convention, copying a list-holding struct on every call.

**Verdict**: no Critical or blocking findings — this is the review's structurally soundest unit, confirming the foundational layer matches the `arch-restructure-roadmap`'s design intent with zero drift. [F-01] deserves early priority since every other unit's Settings-app work depends on config round-trip correctness.

### U-02 — Presentation Surfaces & Shell Composition Root (55 files)

**Scope**: `libs/holonight-surfaces/src/` (all 50 files — tray, popups, sidebar, launcher-surface, background, widgets, notification-toast surfaces), `apps/shell/app/` (`ControlServer`, `ShellApplication`), `apps/shell/main.cpp`.

**Prior context consulted**: `arch-restructure-roadmap/DESIGN.md` §3.1–3.4, this review's own DESIGN.md §5 preliminary flag on `ShellApplication`'s composition-root risk, CLAUDE.md's layer-shell-hiding and `SingleShotConnection`-race gotchas. The dual-surface-lifecycle split (create-on-open sidebar/launcher vs. always-mapped topbar/background/widgets) was confirmed deliberate, not an inconsistency, with zero cross-contamination found.

**Tool sign-off**: 107 raw lint hits, 1 refuted false positive (`LCY-5`, a lint identifier mismatch against nonexistent code).

**Key findings by category**:
- *Security (the review's highest-severity finding)*: **[F-01, Critical]** integer overflow in tray-icon pixmap decoding bypasses the size guard, allowing any unprivileged local process registering as a `StatusNotifierItem` to trigger a multi-gigabyte allocation and crash the shell. **[F-02, High]** the control-socket's `sidebar:toggle:<monitor>` command tears down the open sidebar before validating the target monitor exists — any local process with socket access can blindly close a user's sidebar with a malformed monitor name.
- *Architecture-substantiating*: **[F-03, High]** confirms DESIGN.md's pre-review "concentration-of-risk" flag on `ShellApplication` as a proven defect — the three-phase startup ordering is enforced only by a `Q_ASSERT` that compiles to nothing in Release builds (directly substantiates §5 gap #7).
- *Maintainability*: **[F-04, F-06, Low]** duplicated shadow-padding and screen-edge-margin constants across three files despite an existing shared-constants home. **[F-05, F-07, Low]** two dead-code APIs (`PopupGeometry`, `sidebarSurfaceWidth()`) with zero production callers.
- *Investigation-tier*: an unsynchronized `TrayImageProvider` read against main-thread-mutated containers ([I-01], currently masked by a QML property not yet set) and a systemic lack of `QQuickView::status()` error-checking across every surface class in the unit ([I-04]).

**Verdict**: the highest-severity unit in the review. [F-01] should be fixed first given trivial reproduction and Critical severity; [F-02] and [F-03] both confirm DESIGN.md's preliminary architecture-level risk flags were well-founded, not speculative.

### U-03 — Power, Idle & Brightness Management (35 files)

**Scope**: root-level power/idle/inhibitor classes (13 files), `idle/` (12 files), `brightness/` (8 files).

**Prior context consulted**: `idle-management`, `power-extensions`, `brightness-service` SPEC/DESIGN docs. The CLAUDE.md-documented "discharging LAST" signal-order invariant in `BatteryService::applyStateUpdate()` was explicitly re-verified and confirmed intact, with no drift.

**Tool sign-off**: 56 raw lint hits, 2 refuted/clarified (a `.arg()` multi-placeholder false positive matching U-01's precedent, and a `static const` regex inside a loop that magic-statics guarantee is one-time-only).

**Key findings by category**:
- *Duplication with real risk*: **[F-01, Medium]** the identical, subtle, previously-crash-fixing logind-session-resolution workaround (documented in CLAUDE.md as fixing a real libdbus assertion abort) is independently re-implemented in both `IdleService` and `SysfsBackend` — a future fix to one copy would silently leave the other vulnerable to the original crash. This is the first of three cross-unit instances of the same root-cause pattern (§5 gap #2).
- *Silent failure*: **[F-04, Medium]** `LowBatteryMonitor::sendNotification` discards its D-Bus reply — a failed low-battery warning is completely silent, the one exception among otherwise-consistent sibling D-Bus call sites in the same unit.
- *Reliability*: **[F-05, Medium]** `SysfsBackend` has no device-removal detection and reports a permanent, misleading 0% after a backlight device disappears; `hasBacklight`'s `CONSTANT` QML binding structurally can't be corrected even if the C++ were fixed.
- *Code quality*: **[F-02, Low]** `InhibitorModel::setEntries()` performs a full model reset every 5-second poll regardless of whether anything changed. **[F-03, Low]** two non-const `Q_PROPERTY READ` accessors, the only exception in an otherwise fully-const-correct unit.

**Verdict**: architecturally sound overall — thread safety verified clean across all 33 files, the documented invariant holds exactly, and the abstract-backend pattern is correctly implemented throughout. [F-01]'s cross-file duplication of a previously-debugged crash workaround is the standout consolidation candidate.

### U-04 — Session, Window Identity, Network & Audio Services (36 files)

**Scope**: `SessionService`+`session/` (14 files), `ActiveWindowService`/`MonitorOccupancyService` (4 files), `SystemInfoService` (2 files), `NetworkService`+`network/` (6 files), `audio/` (8 files).

**Prior context consulted**: `session-lock-backend/DESIGN.md`, CLAUDE.md's per-monitor active-window and NetworkManager gotchas. No `docs/sdd/audio-service/` document exists — the audio subsystem was reviewed with no prior-cycle doc to check drift against. The unit's mandatory top-priority check — libpulse thread-boundary marshalling — was manually traced end-to-end across every registered PulseAudio callback and found **exemplary**: every path correctly converts C structs to POD values on the PulseAudio thread and marshals via `Qt::QueuedConnection` before touching any `QObject`, a genuine strength worth preserving.

**Tool sign-off**: 71 raw lint hits, 3 refuted `MDL-7` false positives (all three list models' `roleNames()`/`data()` role sets verified in sync).

**Key findings by category** — this is the review's largest confirmed-finding set (13), proportionate to its breadth spanning security-adjacent session control, a direct C-API audio backend, and D-Bus networking:
- *Security-adjacent silent failure*: **[F-03, High]** session lock/logout/suspend/reboot/shutdown command failures are discarded end-to-end — `CommandRunner::run()`'s `bool` result is checked nowhere, so if the locker binary is missing, the user presses "Lock" and the screen simply stays unlocked with no indication. Flagged as this unit's top remediation priority given real-world security consequences.
- *Silent failure, audio*: **[F-01, High]** PulseAudio context failure/termination never triggers a reconnect — audio goes permanently stale until shell restart. **[F-02, Medium]** every PulseAudio mutation call passes null callbacks, so volume/mute changes fail invisibly.
- *Index-namespace bugs*: **[F-06, High]** sink and source indices are independent PulseAudio counters, but `AudioService`'s removal handlers collapse them to one untyped signal — removing a sink can silently evict an unrelated, still-connected source (e.g. a working microphone). **[F-05, Medium]** `setDefaultOutput(uint32_t)` passes a stringified index where PulseAudio expects a device name — permanently non-functional, independently flagged by three separate review agents. **[F-07, Medium]** the same index-conflation bug affects stream moves.
- *Network*: **[F-04, Medium]** Wi-Fi activation only checks the immediate D-Bus reply, not the actual WPA handshake outcome — a wrong password surfaces only as a generic "disconnected" after 2.5s. **[F-13, Medium]** D-Bus unwrapping/SSID-decoding logic duplicated verbatim between `NetworkService` and `NetworkManagerBackend`.
- *Startup blocking*: **[F-09, Medium]** `SystemInfoService` blocks shell startup on an unbounded, default-timeout blocking D-Bus call.

**Verdict**: the libpulse thread-marshalling strength is real and should be explicitly preserved during any audio remediation. Confirmed findings cluster around two risk areas — silent failure paths and PulseAudio index/namespace conflation — with [F-03] deserving particular priority given its security-adjacent nature.

### U-05 — Desktop Integration, Appearance & Portal (18 files)

**Scope**: `AppearanceService`/`ThemeService` (4 files), `portal/` (6 files), `kde-compat/` (2 files), `mime/` (2 files), `session-integration/` (4 files). Below the 20-file soft floor, justified in DESIGN.md by the four related SDD cycles this cluster corresponds to one-to-one.

**Prior context consulted**: `portal-service`, `system-appearance-portal`, `theme-variant-catalog` SPEC/DESIGN docs. The unit's mandatory doc-drift check — whether `ThemeService` still matches a 2026-05-28 audit's "trivial constant accessors" framing — was **confirmed stale at 93/100 confidence**: `ThemeService` now owns a live, self-rearming `QFileSystemWatcher` and a heap-allocated `SettingsPortalBackend` registering a real D-Bus service. Filed as [F-01] below, a documentation-currency finding rather than a code defect.

**Tool sign-off**: 45 raw lint hits, 2 refuted (a D-Bus introspection DTD URL misidentified as a live network fetch, and a rarely-called single-entry `QMap` misidentified as a hot-path copy).

**Key findings by category**:
- *UI-thread blocking (the unit's headline substantive finding)*: **[F-05, High]** `SessionIntegrationService` runs a chain of up to ~7 sequential blocking `QProcess` calls directly on the UI thread — worst case ~35s freeze — reachable simply by opening the sidebar's System tab, and confirmed by U-09 to re-fire on every tab (re)creation. **[F-04, Medium]** `KdeCompatService`'s process calls have no timeout guard at all, unlike three sibling call sites in the same unit that already solved this — independently corroborated by three review agents.
- *Doc drift*: **[F-01, Low]** the `ThemeService` staleness described above.
- *Duplication*: **[F-02, Medium]** theme-config-path resolution computed independently in three places despite one class directly owning the other. **[F-06, Low]** subprocess-timeout boilerplate triplicated with a magic-number `5000` repeated three times.
- *Silent state desync*: **[F-03, Medium]** `MimeService` can change a `Q_PROPERTY`'s resolved value with no `NOTIFY` ever emitted if the paired async operation fails.
- *Dead/unwired code*: **[I-02]** `KdeCompatService`'s entire QML-facing API has zero consumers, apparently superseded by `SessionIntegrationService` without removal.

**Verdict**: the mandatory doc-drift check is this unit's headline result — worth flagging to whoever maintains the audit trail. Substantively, [F-05] (a confirmed UI-freezing blocking chain) and the three-way-corroborated [F-04] are the highest-value fixes, both fittable into one pass since they share the exact same "solved in one sibling, never ported to its twin" root cause.

### U-06 — Launcher & Notifications Services (30 files)

**Scope**: `launcher/` (13 files — cache, scanner, serializer, model, command execution), `notifications/` (15 files — server, service, model, rules, storage, filter, policy).

**Prior context consulted**: `launcher-desktop-cache`, `notification-hardening` SPEC/DESIGN docs. Two mandatory checks: `NotificationServer`'s D-Bus protocol adapter correctness against CLAUDE.md's gotchas — **verified correct** (`Q_CLASSINFO` present and commented as mandatory); and the launcher cache-invalidation strategy's known-incomplete state — characterized with two concrete blind spots sharper than SPEC.md's existing prose (filed as [F-03]).

**Tool sign-off**: 77 raw lint hits, three `MDL-7` false positives for the "hidden role gap" concern (though the underlying `default:` pattern is separately, validly flagged as a `-Wswitch`-suppression risk in [I-03]).

**Key findings by category**:
- *Unauthenticated local-IPC input (the review's second Critical-adjacent security cluster, §5 gap #4)*: **[F-02, High]** `NotificationServer::Notify()` performs no length validation on untrusted D-Bus input and unconditionally logs the full unbounded payload to disk — any session-bus process can force arbitrarily large, repeated writes into the persistent log file.
- *Silent failure*: **[F-01, High]** notification-rule persistence combines silent write failures with a synchronous, blocking disk write triggered on every incoming notification (even DND-suppressed ones) — the one persistence path in this subsystem that never adopted the async-write pattern its own sibling store already uses. **[F-04, Medium]** launcher process-spawn failures produce zero diagnostics anywhere — the only such silent path in the entire unit. **[F-05, Medium]** notification-history write failures never propagate back to the caller.
- *Cache correctness*: **[F-03, Medium]** the launcher cache's watcher only observes directory-level changes (missing in-place content edits) and its mtime comparison truncates to 1-second resolution — both sharper than the SPEC's already-accepted Stage-1 limitation.
- *Dead code on a hot path*: **[F-06, Low]** `LauncherService::desktop_file_index_` is rebuilt from scratch on every launcher keystroke but never read by anything.
- *Duplication*: **[F-09, Medium]** two `DesktopEntryScanner` methods duplicate the same directory-walk/dedup/sort pipeline and have already silently diverged in behavior.

**Verdict**: the mandatory `NotificationServer` protocol check passed clean. [F-02] (unauthenticated DoS via unbounded log growth) and [F-01] (the one persistence path that never got the async treatment) are the standout findings and touch the same D-Bus-facing code path, worth fixing together.

### U-07 — Calendar & Weather Services (31 files)

**Scope**: `CalendarService`+`calendar/` (18 files — CalDAV/ICS providers, cache, sync manager, credential storage, iCal parser), `weather/` (5 files), `weather-icon/` (8 files).

**Prior context consulted**: `weather-icon-compositor/SPEC.md`+`DESIGN.md` (no `docs/sdd/calendar*` directory exists — no prior-cycle calendar doc was available). Two mandatory checks: `IActivityGate` semantic consistency with U-03 — **verified consistent, no inversion bug**; and the WeatherIconCompositor wiring claim — **project memory was found stale and is corrected here**: `WeatherIconCompositor.qml`/`WeatherIconBridge` *are* wired into the weather popup screens (`WeatherCurrentSection`, `WeatherHourlyStrip`, `WeatherDailyCards`, `WeatherPopupContent`), and `WeatherIconMapper`'s condition-family coverage is now comprehensive, not partial. Only the topbar's compact icon still uses the SVG path.

**Tool sign-off**: 128 raw lint hits — the largest and most substantively important of any unit, dominated by real network-error-handling gaps in the HTTP-based calendar/weather providers; only 1 refuted false positive.

**Key findings by category** — tied with U-04 for the largest confirmed-finding set (13):
- *This review's second Critical finding*: **[F-01, Critical]** `CalDavProvider::sendSync()`'s documented "returns nullptr on timeout" contract is dead code — `QNetworkAccessManager::sendCustomRequest()` never returns nullptr, and the function never checks whether the reply actually finished before returning it. A hung CalDAV server silently produces a "successful" empty sync with zero diagnostic; existing cached events simply age out with no signal that sync is broken. Confirmed independently by two agents at matching 90/100 confidence, directly caused by [F-10]'s duplicated-then-diverged HTTP scaffolding — concrete proof that the §5 gap #2 duplication pattern is an active bug source, not style debt.
- *Data lifecycle gaps (a matched pair)*: **[F-02, High]** `CalendarCache` never deletes events absent from a current fetch — cancelled meetings linger indefinitely and can keep firing pre-event notifications. **[F-03, High]** the cache's account-removal cleanup API is fully implemented with doc comments describing the intended flow, but has zero production callers — a removed CalDAV account's events persist forever, and compounding this, config-change signals after the first are silently ignored entirely.
- *Thread safety*: **[F-04, Medium]** `LibsecretCredentialStorage`'s constructor makes a blocking secrets-daemon D-Bus call on the main thread, triggered by ordinary calendar config changes.
- *Diagnostics*: **[F-09, Medium]** SSL handshake failures on CalDAV surface as an opaque "HTTP 0" with zero certificate diagnostic — notable given self-hosted CalDAV (common self-signed certs) is this project's explicit target audience.
- *Correctness*: **[F-06, Medium]** `CalDavProvider` declares intent to deduplicate events across calendars but the dedup logic is dead code, never executed.

**Verdict**: this unit's real network I/O produced the densest and highest-severity lint-to-finding conversion rate of the review, as expected for the surface most exposed to external failure modes. [F-01] is Critical and should be fixed alongside [F-10] (its root cause) in one pass; [F-02]/[F-03] point to the same underlying gap (sync only adds/updates, never reconciles/deletes) and should also be fixed together. Both mandatory checks resolved cleanly, and the WeatherIconCompositor memory correction should be propagated to the project's persistent memory system.

### U-08 — Topbar QML (27 files)

**Scope**: `apps/shell/qml/Topbar/` — the always-on top bar (workspace pills, active-window title, clock, weather/battery/audio/network widgets, tray section, logo).

**Prior context consulted**: `workspace-indicator-redesign/SPEC.md`+`DESIGN.md`. Four mandatory checks all passed or were resolved cleanly: the `barMonitorName` required-property propagation chain was traced end-to-end with zero unsatisfied `required property`; `MultiEffect` z-order was checked on all 7 instances (6/7 correct, 1 violation — [D-003]); widget state-machine correctness found no loop risk in any of 6 widgets (one genuine staleness bug — [D-001]); and the weather-icon image-loading path was confirmed defect-free.

**Tool sign-off**: 46 raw lint hits, 33 of which (ORD-1 attribute ordering, STY-3 dot-notation) are pure style noise excluded per convention; 6 substantive lint hits itemized. One cross-agent naming contradiction (`on_StrokeChanged`) was resolved by live `qml6` runtime verification confirming the codebase's underscore-property capitalization convention is correct, not a bug.

**Key findings**:
- **[D-001, Medium]** `WeatherWidget.qml`'s day/night icon goes stale between weather refreshes — its `isDay` binding calls plain JS `Date.now()`, which registers no QML dependency, so it only re-evaluates on the next weather-data refresh rather than as wall-clock time actually crosses sunrise/sunset. `ClockSection.qml` in the same directory shows the correct fix pattern (a periodic `Timer` forcing re-evaluation).
- **[D-002, Low]** competing `anchors`+explicit `width` on the same `Column` in `ClockSection.qml` — empirically verified live that the anchor silently wins, a currently-harmless landmine.
- **[D-003, Low-Medium]** one `MultiEffect` glow declared *after* its source (breaking the file's own established z-order convention, followed correctly by the other 6 instances in the directory).
- **[D-004]/[D-005], Low**: reusable `StatusPopupTriggerArea`/`BarSection` components use legacy `MouseArea` where a sibling widget in the same directory already demonstrates the modern `TapHandler`/`WheelHandler` replacement — broad reach given both are shared base components.
- **[D-007], Low**: ~15 files carry unreferenced `id:` assignments, likely copy-paste boilerplate residue.
- *Investigation-tier*: an unbounded `Repeater` sizing formula tied to live Hyprland workspace IDs ([I-001], requires a live-session test to confirm actual cost) and a `Loader` that skips a widget's shrink animation on teardown ([I-003]).

**Verdict**: the mandatory `barMonitorName` propagation check — this unit's core architectural responsibility — passed cleanly. All findings are Low-to-Medium and QML-idiom-level; no structural defects.

### U-09 — RightSidebar + Popups QML (47 files)

**Scope**: `apps/shell/qml/RightSidebar/` (19 files, six tabs) + `apps/shell/qml/Popups/` (28 files, seven popup families).

**Prior context consulted**: `right-sidebar`, `sidebar-upcoming-calendar` SPEC/DESIGN docs (both with pre-existing stale TASKS, not in scope). Five mandatory checks: create-on-open/destroy-on-close sidebar lifecycle — **PASS**, verified live via control-socket open/close cycling; the `configured()` race guard — **N/A**, this unit's surfaces sidestep the race entirely via an unconditional destroy-and-recreate-on-every-`show()` pattern; dismiss-overlay input-region handling — **PASS, not applicable** for the same reason; Repeater/Loader delegate memory-leak risk — **PASS**, zero dynamic object creation found anywhere in the 47-file scope; layout-nesting/z-order conventions — **mostly PASS, with drift** (3 of 10 glow instances inverted — [I-003]).

**Tool sign-off**: 396 raw lint hits; 211 excluded as pure style noise (152 ORD-1 + 59 STY-3); 34 substantive hits itemized, with one high-volume category (89 JS-1 `var` hits) folded into a structural finding ([D-006]) rather than listed individually.

**Key findings**:
- **[D-002, High/performance]** every `DefaultAppRow` in the System tab runs an uncached, recursive filesystem scan on `Component.onCompleted` — 6-7 redundant full XDG-directory walks synchronously on the UI thread every single time the System tab is (re)created, which happens on every sidebar open given the create-on-open lifecycle. Directly corroborates U-05's [F-05] finding on the same underlying `SessionIntegrationService`/`LauncherService` call paths — confirmed by this unit to fire on *every* tab (re)creation, not just once.
- **[D-001, Medium]** six sidebar tab roots each declare a `preferredWidth` property with zero consumers — actual width comes from a second, independently-hardcoded array that has already drifted 20px out of sync with one tab's dead property.
- **[D-003, Medium]** a tab-switch animation race: clicking a different tab while the sidebar is still opening starts two animations writing the same property concurrently, with no mutual-exclusivity guard (an existing guard covers only one of the three relevant animation pairs).
- **[D-005, Low]** `SidebarSessionBar.qml` is entirely orphaned dead code, verified via repo-wide grep.
- **[D-006, Medium/maintainability]** `SidebarOverviewUpcoming.qml` concentrates the unit's heaviest code-quality debt in one 523-line file — a 110-line date-arithmetic function with no unit test, mixing presentation and business logic.
- *Investigation-tier*: a synchronous blocking diagnostics probe re-run on every System-tab load ([I-001], same root cause as U-05 [F-05] and this unit's [D-002]).
- Two cross-agent contradictions were resolved via direct source verification and recorded as **no finding**: an underscore-property naming pattern (consistent with U-08's identical resolution) and an "unbounded Repeater" claim disproven by tracing a server-side cap (`kModelEventLimit{3}`) the first agent missed.

**Verdict**: [D-002] and U-05's [F-05] are the same class of bug hitting the same feature area from two different review passes — strong independent corroboration that the sidebar's System tab has a real, repeatable UI-freeze risk worth fixing as one unit of work.

### U-10 — Overlay Content & Shared QML Building Blocks (30 files)

**Scope**: Launcher (7 files), Notifications toast stack (4), Tray context-menu content (4), shared Controls icons (8), Widgets (4), Background (1), Utility (1), WeatherIcon compositor (1) — a deliberate DESIGN.md-flagged merge of eight otherwise-unrelated small regions into one pass.

**Prior context consulted**: `launcher-redesign/SPEC.md`+`DESIGN.md`, `weather-icon-compositor` docs. Six mandatory checks, all resolved cleanly: the deliberate multi-region merge rationale confirmed no issues; WeatherIconCompositor's production-wiring status confirmed matching the corrected U-07 finding (wired into weather popups, not the topbar); toast animation state verified structurally sound; all 8 `Controls/*.qml` icon files confirmed Canvas-based with direct palette colors, so CLAUDE.md's MultiEffect-recoloring gotcha simply does not apply to any of them; Clock/TimeToEvent widgets confirmed to take only pre-computed C++ text properties with zero direct service coupling; Background/ThemeReloadBridge state management confirmed correctly guards the documented `setInitialProperties` race.

**Tool sign-off**: 140 raw lint hits (77 excluded as style noise), 63 substantive; `qmllint` ran clean with 0 warnings across 30 files.

**Key findings**:
- **[D-001, High]** the launcher's most consequential finding: keyboard-selected action rows have no visual highlight *and* pressing Enter launches the wrong command entirely — the keyboard-selection path always dispatches to the parent app's default launch, never the action's actual command, with zero visual cue that the wrong thing is about to happen. This is a silent wrong-app-launch bug in the shell's most latency-/attention-sensitive UI surface.
- **[D-002, Low-Medium]** `pragma ComponentBehavior: Bound` inconsistently applied — 5 files miss it while 6 structurally similar sibling files in the same unit already have it, independently corroborated by two review agents.
- **[D-006, Medium/performance]** the search-mode launcher delegate unconditionally instantiates both possible row-type component trees for every single row, doubling construction/binding/icon-request cost on the shell's most latency-sensitive list.
- **[D-005, Low]** the shared `BarIcon` building block hardcodes explicit `width`/`height` instead of `implicitWidth`/`implicitHeight`, already forcing one of its 8 call sites to work around the gap.
- **[D-007, Low]** a tray-icon opacity fade-out is structurally unreachable — `visible` and the fade condition are gated by the same boolean, so the item disappears in the same frame the fade would have started.
- *Investigation-tier*: three `image://icon/` consumers with no fallback-glyph handling, unlike two other components in the same codebase that already solved this ([I-001]); a docs-vs-code drift flag on `WeatherIconCompositor.qml`'s properties vs. the project memory file describing it ([I-007]).

**Verdict**: [D-001] is this unit's standout — a real, silent, wrong-command-execution bug with no visual warning, deserving priority attention despite Medium effort given the launcher's central role in the shell's UX.

### U-11 — Settings Application (20 files)

**Scope**: `apps/settings/src/` (13 C++ files) + `apps/settings/qml/` (7 QML files) — the standalone `holonight-settings` binary, reviewed with both `qt-cpp-review` and `qt-qml-review` given its mixed nature.

**Prior context consulted**: `holonight-settings/SPEC.md`+`DESIGN.md`. Four mandatory checks: architectural isolation from the shell's service graph — **CONFIRMED clean**, zero shell-service includes found by grep; the `current_`/`snapshot_` dirty-tracking pattern — **CONFIRMED correct**, independently verified by three agents via C++20 defaulted `operator==` deep comparison; TOML read/write safety — traced the documented SQLite null-vs-empty-string gotcha and confirmed it does **not** reproduce here, but surfaced a separate, serious bug instead ([D-C1]); `FontListModel`/`ShellStatusService` correctness — confirmed both services are entirely synchronous/single-threaded with no cross-thread risk to check.

**Tool sign-off**: 14 C++ lint hits (all pure style), 29 QML lint hits (17 style-noise excluded); `qmllint` ran clean with 0 warnings across 7 files. One cross-agent contradiction (a 4-finding "ColumnLayout margin" cluster) was resolved by reading the actual project memory file verbatim rather than relying on a paraphrased briefing — the paraphrase had inverted the documented gotcha, producing a false-positive cluster that was caught and excluded before entering this report.

**Key findings** — two High-severity, silent, user-facing defects, both warranting prompt follow-up given this app's role as the shell's own config editor:
- **[D-C1, High]** `ConfigWriter` silently deletes `weather.latitude`/`longitude`/`city` on every single save — any user with a pinned weather location loses it silently, converted to a commented-out placeholder, the instant they open Settings and click Apply, even if they touched nothing weather-related. Traced end-to-end from parse to write.
- **[D-C2, High]** the `HolonightTheme` QML singleton referenced by the app's own default landing page is never registered anywhere in the codebase — every user hits a silent `ReferenceError` on first launch, and the Theme/Accent picker sections of the landing page render permanently empty with no visible error.
- **[D-C3, Medium]** six ComboBox/Slider controls on the Appearance/Bar pages permanently desync from the underlying edit model after first user interaction, because Qt Controls' internal property writes destroy the prior declarative binding — a "Discard Changes" action silently fails to visually reset these six controls.
- **[D-C5, Medium]** asymmetric error reporting: config save failures surface a modal dialog, but load failures (e.g. a corrupt `config.toml`) are completely invisible — a user could believe "Discard Changes" restored their settings when it actually silently reset everything to hardcoded defaults.
- **[D-Q1, Medium]** the page-switching `Loader` has no error handling at all, compounding [D-C2]'s silent failure with zero console-level diagnostic path for the app's primary page-dispatch mechanism.
- **[D-Q2, Medium]** the exact `parent.modelData` delegate-fragility class already recorded in this project's own memory system from a prior incident recurs across two files in this unit — currently harmless, but the project has been burned by this exact pattern before.
- **[D-C4, Low-Medium]** the mode→scheme mapping ternary is hand-duplicated 5-6 times across three classes.

**Verdict**: the standalone Settings app's architectural isolation and dirty-tracking pattern are both genuinely sound, matching their documented design. But two High-severity, completely silent defects — one destroying user config data on every save, one breaking the app's own default page on first launch — make this unit's remediation priority disproportionate to its small (20-file) size.

---

## 5. Architecture & Design Gaps

*(Full text of T-012's output, `ARCHITECTURE-GAPS.md`, reproduced below.)*

**Input**: U-01 through U-11 unit findings + DESIGN.md §5 preliminary observations (#1–7)

This section synthesizes systemic weaknesses that recur across multiple independently-reviewed units — patterns invisible from any single unit's findings list but clear once all eleven are read together. Per-unit nitpicks are not repeated here; only issues with a common root cause spanning ≥2 units are included. Suggested directions assume REQ-C-2 (aggressive refactoring acceptable, no backward-compat constraint) since this is a pre-production POC.

### 5.1 Silent failure paths are the dominant defect class across the entire codebase

**Affected units**: U-01, U-02, U-03, U-04, U-05, U-06, U-07, U-11 (8 of 11 units)
**Severity**: High · **Effort**: L (systemic, not a single fix)

Every single unit's summary independently converges on the same observation: a failure occurs, and nothing downstream — no signal, no log, no user-visible state — reflects it. U-01 flagged silent config-parse failure and unlogged D-Bus service-registration failures. U-03 found `LowBatteryMonitor::sendNotification` discarding its D-Bus reply. U-04 produced the largest cluster: PulseAudio never reconnects after context failure (F-01), every PulseAudio mutation call passes null callbacks so volume/mute changes fail invisibly (F-02), session lock/logout/suspend commands discard their `bool` success result end-to-end (F-03, security-relevant), and Wi-Fi authentication failures surface only as a generic "disconnected" after 2.5s (F-04). U-05 found `MimeService`'s `Q_PROPERTY` can silently desync from D-Bus reality (F-03). U-06 found notification-rule persistence failures are silent (F-01) and `NotificationServer`'s process-spawn failures produce zero diagnostics anywhere in the unit (F-04). U-07's Critical finding ([F-01]) is a dead-code timeout guard that turns a hung CalDAV server into a silently "successful" empty sync. U-11 found weather coordinates are silently deleted on every settings save (D-C1) and config-load failures are invisible while save failures are not (D-C5).

**Root cause**: there is no shared convention for surfacing degraded state — some subsystems built one (D-Bus `saveError`/`syncError` signals exist in isolated spots) but it was never generalized, so each new feature reinvents (or omits) error propagation independently.

**Suggested direction**: establish one project-wide convention — e.g., every service-layer class that can fail exposes a `lastError`/`healthState`-style property and emits a `*Error(QString)` signal — and retrofit the highest-severity instances first (U-04's F-03 session-lock silence and U-07's F-01 CalDAV timeout, both of which mask total feature failure as success).

### 5.2 Copy-pasted logic diverges silently, and has already caused real bugs

**Affected units**: U-01, U-02, U-03, U-04, U-05, U-06, U-07, U-11 (8 of 11 units)
**Severity**: High · **Effort**: M

This is not abstract duplication-aversion — cross-unit evidence shows duplication is an active bug source here, not just style debt. U-07's Critical CalDAV timeout bug ([F-01]) exists *because* `CalDavProvider` and `IcsProvider` independently hand-rolled the same synchronous-HTTP-with-timeout idiom ([F-10]) and one copy silently dropped the `abort()` call the other has. U-04's [F-05] (broken `setDefaultOutput`/`setDefaultInput` overloads) stems from the same duplicated-D-Bus-parsing root as [F-13]. U-03's [F-01] found the exact same subtle, previously-crash-fixing logind-session workaround independently re-implemented in `IdleService` and `SysfsBackend` — a future fix to one would silently leave the other vulnerable to the original bug. U-05 found the theme-config path computed independently in three places ([F-02], plus a third looser copy in `MimeService`), and subprocess-timeout boilerplate triplicated ([F-06]). U-11's D-C4 found the same mode→scheme mapping ternary hand-written across three classes at up to six call sites.

**Suggested direction**: this is a stronger argument for shared helpers than typical DRY concerns, because divergence here has already produced a Critical-severity bug (U-07 F-01/F-10). Prioritize extracting the HTTP-sync-with-timeout helper (U-07) and the logind-session-resolution helper (U-03) first, since both wrap previously-debugged, non-obvious correctness logic where a second silent fork is the likeliest failure mode.

### 5.3 Every custom `QAbstractListModel` rebuilds `roleNames()` from scratch on every call

**Affected units**: U-01, U-02, U-04, U-06, U-07, U-11 (6 of 11 units, ~10 distinct model classes)
**Severity**: Low (individually) / Medium (systemically) · **Effort**: S per site, but recurs everywhere

`WorkspaceModel` (U-01), `TrayModel`/`DbusMenuModel` (U-02), `WifiNetworkModel`/`AudioDeviceModel`/`AudioStreamModel` (U-04), `LauncherModel`/`NotificationRuleModel`/`NotificationService` (U-06), `CalendarEventModel` (U-07), and `FontListModel` (U-11) all independently construct a fresh `QHash<int, QByteArray>` literal on every single `roleNames()` invocation instead of caching it once. No unit found a shared base class or convention that would have prevented this — each model was written independently and each author made the identical omission.

**Suggested direction**: this is the clearest case in the whole review for a small shared abstraction: a `CachedRoleNamesMixin` or a one-line `static const QHash` convention documented in CLAUDE.md, applied in one pass across all ~10 sites. Low individual risk but worth fixing in bulk precisely because it's mechanical and the pattern will otherwise keep recurring in every future model.

### 5.4 Untrusted local-IPC input is insufficiently bounded — a real, unauthenticated attack surface

**Affected units**: U-02, U-06
**Severity**: Critical · **Effort**: S–M

U-02's [F-01] is this review's single highest-severity finding: any process registering as a `StatusNotifierItem` on the session bus (no privilege required) can trigger a multi-gigabyte allocation via integer overflow in tray-icon pixmap decoding, crashing the shell. U-02's [F-02] lets any local process with control-socket access blindly close the user's sidebar via a malformed monitor name, and [I-08] notes the control socket itself has no message framing or size bound. U-06's [F-02] found `NotificationServer::Notify()` (also callable by any session-bus process) performs no length validation, and unconditionally logs the full unbounded payload to disk — a second unauthenticated local DoS vector against the same log file U-01 depends on for diagnostics.

**Suggested direction**: treat this cluster as a single security pass rather than three unrelated bugs — every local-IPC-facing entry point (tray D-Bus, control socket, notification D-Bus) needs the same discipline: reject non-positive/oversized dimensions before allocating, cap string/payload lengths before logging or storing, and bound socket reads. Fix U-02 [F-01] first given its Critical severity and trivial reproduction (register a fake `StatusNotifierItem`).

### 5.5 Synchronous blocking I/O on the UI thread, with fixes proven but never propagated to siblings

**Affected units**: U-04, U-05, U-07, U-09
**Severity**: High · **Effort**: M

U-05's [F-05] is the sharpest instance: `SessionIntegrationService`'s diagnostics/cache-rebuild chain runs up to ~7 sequential blocking `QProcess` calls directly on the UI thread (worst case ~35s freeze), reachable simply by opening the sidebar's System tab — confirmed by U-09's [I-001] as re-triggered on *every* tab (re)creation, since the sidebar is create-on-open. U-05's [F-04] found `KdeCompatService`'s process calls have no timeout guard at all, unlike three sibling call sites in the very same unit (`SessionIntegrationService`, `MimeService`) that already solved this. U-04's [F-09] found `SystemInfoService` blocks shell startup itself on an unbounded D-Bus call. U-07's [F-04] found `LibsecretCredentialStorage`'s constructor makes a blocking secrets-daemon D-Bus call on the main thread, called from calendar config changes.

**Root cause**: the async-with-timeout pattern exists and works correctly in this codebase (`MimeService`'s guarded `QProcess` helper) — it simply was never generalized or ported to the sibling classes solving the identical problem.

**Suggested direction**: extract `MimeService`'s guarded-subprocess pattern into a shared helper and migrate `KdeCompatService` and `SessionIntegrationService` onto it (`QtConcurrent::run` + `QFutureWatcher`, matching the idiom already proven elsewhere in this codebase per DESIGN.md §1.3.1's abstract-backend pattern).

### 5.6 Backend APIs built to completion, then never wired to their intended caller

**Affected units**: U-02, U-05, U-06, U-07, U-09
**Severity**: Medium · **Effort**: S–M to wire, trivial to delete

A recurring shape: a class is fully implemented, documented, and unit-tested, but has zero production callers. U-02 found `PopupGeometry` (F-05) and `sidebarSurfaceWidth()` (F-07) both dead. U-05 found `KdeCompatService`'s entire QML-facing API has no consumer anywhere, its diagnostic purpose apparently superseded by `SessionIntegrationService` without anyone removing the superseded class (I-02). U-06 found `LauncherService::desktop_file_index_` is built and maintained on every keystroke but never read (F-06). U-07 found the most consequential instance: `CalendarCache`'s account-removal cleanup API (`removeStaleAccounts`, `clearAccountEvents`) is fully implemented with doc comments describing the intended flow, but has zero production callers — a user who removes a CalDAV account sees its events persist indefinitely (F-03). U-09 found `SidebarSessionBar.qml` orphaned entirely (D-005) and six sidebar tabs' `preferredWidth` properties dead, with the shadow copy that actually drives width already 20px out of sync (D-001).

**Suggested direction**: audit each of these for intent before deleting — several (U-07's F-03 especially) read as "integration step forgotten," not "deliberately abandoned," and should be wired in rather than removed. Where a class has been genuinely superseded (U-05's KdeCompatService/SessionIntegrationService overlap), remove the superseded one rather than maintaining two parallel implementations of the same diagnostic.

### 5.7 Composition-root and manager invariants are enforced only by assertions that vanish in Release builds

**Affected units**: U-01, U-02 (directly substantiates DESIGN.md §5 observation #1)
**Severity**: High · **Effort**: M

DESIGN.md §5 flagged `ShellApplication`'s 40+ member constructor as a "concentration-of-risk, not yet a proven defect" before unit review began. U-02's [F-03] confirmed it is a proven defect: the three-phase `registerQmlTypes()` → `startServices()` → `startShell()` startup sequence is enforced only by `Q_ASSERT(registered_ && services_started_)`, which compiles to nothing in Release/NDEBUG builds — a future entry point calling phases out of order would silently produce a half-initialized shell with no crash and no diagnostic. U-01's [I-02] found the identical shape one layer down: `ExtWorkspaceManager`'s constructor uses `Q_ASSERT` as its sole null-guard for a dependency it then unconditionally dereferences.

**Suggested direction**: replace both `Q_ASSERT`s with loud `qCritical` + early-return (or make the dependent phases self-enforcing, since both are already idempotent) so a Release-build ordering mistake fails visibly. This is a small, mechanical fix but a high-value one given DESIGN.md independently flagged the composition root as this codebase's single largest concentration of correctness risk.

### Cross-Reference Summary

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

---

## 6. Stale Backlog Context

*(Full text of T-013's output, `STALE-BACKLOG.md`, reproduced below.)*

**These items are pre-existing incomplete work from prior SDD cycles and are NOT addressed by this review cycle.**

This section catalogs unchecked `TASKS.md` items from prior SDD feature cycles, as of the start of the poc-readiness-review cycle. It exists for transparency and context only — per REQ-C-3, no stale task listed here was analyzed, resumed, or executed as part of this review. Where a unit review's prior-context reading touched on one of these features (e.g., U-03 touching `idle-management`, U-05 touching `portal-service`), that review assessed the *current code*, not the outstanding stale tasks.

| Feature Directory | Unchecked Count | Sample Task Titles |
|---|---|---|
| portal-service | 12 | T-059: Verify PortalService.available is true on live session with xdg-desktop-portal; T-060: Verify interfaces QStringList contains expected portal interface names |
| idle-management | 7 | T-032: Verify ext-idle-notify-v1 backend activation and idle threshold crossing; T-035: Verify sidebar Keep Awake toggle holds logind inhibitor |
| launcher-desktop-cache | 4 | T-010: Verify cache load latency performance; T-011: Verify validator does not block UI during filesystem scan |
| holonight-settings | 2 | T-059: Verify shell ConfigService detects written changes; T-065: Final acceptance smoke test |
| sidebar-upcoming-calendar | 2 | T-039: Implement CalendarSyncManager::onConfigReloaded() to rebuild providers and re-run testConnection(); T-061: End-to-end verification: build, run shell, open sidebar, verify calendar widget displays cached events |
| theme-variant-catalog | 2 | Apply scheme-first resolution in `holonight-qt` when that repository is edited; Add resolver/platform-theme token coverage in `holonight-qt` |
| workspace-indicator-redesign | 2 | T-023: Live verification of special workspace protocol support (manual); T-024: Live verification of per-monitor active workspace distinction (manual) |
| launcher-redesign | 1 | T-024: Manual smoke test — Browse mode, Search mode, Recent tracking, action launching |
| right-sidebar | 1 | T-017: Add keyboard navigation |
| system-appearance-portal | 1 | Restart/start `xdg-desktop-portal` after installing portal files during manual validation |

**Total unchecked**: 34 (12 + 7 + 4 + 2 + 2 + 2 + 2 + 1 + 1 + 1).

**Note on count discrepancy**: TASKS.md's T-013 acceptance check states the ten per-feature counts "sum = 32." All ten individual per-feature figures above match SPEC.md §12 exactly (re-verified 2026-07-05, zero drift from the SPEC.md-recorded values). However, the arithmetic sum of those ten figures is 34, not 32 — the "32" figure in the TASKS.md check text is an addition error in that check's own wording, not a discrepancy in the underlying data. Flagged here for visibility rather than silently corrected upstream.

**Explicit scope statement**: None of the above stale tasks were resumed, executed, validated, or otherwise acted upon during the poc-readiness-review cycle. They are listed here solely so a future remediation effort has visibility into pre-existing incomplete work alongside the findings produced by this review.

---

## 7. Remediation Roadmap

This roadmap sequences all findings from §4 and all gaps from §5 by severity, effort, architectural dependency, and blast radius. It deliberately does **not** interleave the two — cross-cutting gaps come first because several unit-level findings are *instances* of them, and fixing the gap prevents entire classes of future findings rather than just today's known instances (per REQ-F-8's dependency-order requirement: cross-cutting architecture gaps should be resolved before unit-level fixes that assume the old architecture, and foundational-layer findings should precede surface-layer findings that build on top of them).

### Phase 0 — Immediate: the two Critical findings and their direct neighbors (Effort: S–M total)

These are unauthenticated-attacker-triggerable or silent-data-loss bugs with trivial reproduction. Fix before anything else in this report.

1. **U-02 [F-01]** — tray-icon pixmap integer overflow (Critical, Effort S). Reject non-positive/oversized dimensions, use 64-bit pixel-count arithmetic, clamp to a sane max before allocating.
2. **U-07 [F-01]** + **[F-10]** (its root cause) — CalDAV timeout dead-code + the duplicated HTTP-sync scaffolding that caused it (Critical + Medium, Effort S+M). Fix together: mirror `IcsProvider`'s correct abort-on-timeout pattern, then extract the shared `HttpSyncClient` helper so the two providers can't diverge again.
3. **U-11 [D-C1]** — `ConfigWriter` silently deleting weather coordinates on every save (High, Effort Low). A one-line conditional-emit fix in `ConfigWriter::write`.
4. **U-11 [D-C2]** — unregistered `HolonightTheme` singleton breaking the Settings app's default landing page (High, Effort Medium).
5. **U-02 [F-02]** — control-socket sidebar-close DoS via unvalidated monitor name (High, Effort S).
6. **U-06 [F-02]** — unbounded `Notify()` D-Bus payload logging (High, Effort S). Bundle with #5 and the tray fix as one "local-IPC input hardening" pass per gap #4's suggested direction (§5.4).

### Phase 1 — Cross-cutting gaps with systemic leverage (Effort: M–L, ongoing)

Fixing these prevents future instances of the defect classes already found, not just the ones cataloged in this report.

1. **Gap #7 (§5.7)** — replace both Release-mode-silent `Q_ASSERT`s (`ShellApplication`, `ExtWorkspaceManager`) with loud `qCritical` + early-return. Small, mechanical, high-value given DESIGN.md independently flagged the composition root as the codebase's largest concentration of correctness risk. Do this **before** any other `ShellApplication`-touching work, since it hardens the exact code every other surface-layer fix in Phase 3+ depends on initializing correctly.
2. **Gap #1 (§5.1)** — establish the project-wide `lastError`/`healthState` + `*Error(QString)` signal convention, and retrofit the two highest-severity instances first: U-04 [F-03] (silent session-lock failures, security-relevant) and U-07 [F-01] (already fixed in Phase 0, but the *convention* work generalizes it). This is Effort: L and will span multiple follow-up SDD cycles — treat this phase as establishing the pattern and fixing the worst 2-3 instances, not clearing all 8 units' silent-failure findings in one pass.
3. **Gap #2 (§5.2)** — extract the logind-session-resolution helper (consolidating U-03 [F-01]'s two independently-maintained copies of a previously-crash-fixing workaround) and the theme-config-path helper (U-05 [F-02]). Both wrap subtle, non-obvious correctness logic where a second silent fork is the likeliest next failure mode.

### Phase 2 — Foundation-layer findings (U-01), before surface-layer work depending on them

1. **U-01 [F-01]** — disabled-widget config field loss (High, Effort S). Every Settings-app save round-trips through this parser; fix before any further U-11 Settings-app remediation work, since U-11's own findings assume config round-tripping is otherwise reliable.
2. **U-01 [F-04]/[F-05]** — inconsistent Hyprland IPC error logging (Medium, Effort S each).

### Phase 3 — UI-thread blocking I/O cluster (Gap #5, §5.5)

Fix as one consolidated pass since all four instances share the same root cause and the same proven fix pattern already exists in the codebase (`MimeService`'s guarded-subprocess helper):

1. Extract `MimeService`'s guarded-`QProcess` pattern into a shared helper.
2. Migrate **U-05 [F-05]** (`SessionIntegrationService`, ~35s worst-case UI freeze, confirmed by **U-09 [I-001]** to refire on every System-tab open) and **U-05 [F-04]** (`KdeCompatService`, no timeout guard at all) onto it.
3. Fix **U-04 [F-09]** (`SystemInfoService` blocking shell startup) and **U-07 [F-04]** (`LibsecretCredentialStorage` blocking on main-thread construction) using the same async-with-timeout idiom.
4. **U-09 [D-002]** (six redundant filesystem scans per System-tab load) is architecturally adjacent to this same cluster (same UI surface, same root service) — fix in the same pass by caching `DesktopEntryScanner::scanForDefaultApps()`'s result.

### Phase 4 — Remaining unit-level High-severity findings

Fix independently, roughly in unit order since each is self-contained: **U-04** [F-01] (PulseAudio no-reconnect), [F-03] (silent session-lock failures — the concrete instance of Phase 1's convention work), [F-06] (sink/source index conflation); **U-06** [F-01] (notification-rule silent persistence failures); **U-07** [F-02]/[F-03] (calendar cache never reconciles/deletes — fix together, same root gap); **U-09** [D-002] (if not already folded into Phase 3); **U-10** [D-001] (launcher keyboard-selection wrong-command bug — high user-visibility despite Medium effort).

### Phase 5 — Built-but-unwired API audit (Gap #6, §5.6)

A one-time audit pass, not a code-writing pass: for each of `PopupGeometry`, `sidebarSurfaceWidth()`, `KdeCompatService`'s QML API, `LauncherService::desktop_file_index_`, `CalendarCache`'s account-removal API, and `SidebarSessionBar.qml`, decide wire-in vs. delete per the gap's suggested direction (checking git history/intent before deleting, since several read as "integration step forgotten" rather than "deliberately abandoned"). **U-07 [F-03]** (calendar account-removal cleanup) is the highest-priority item in this phase given real user-visible data-persistence impact.

### Phase 6 — Mechanical/background cleanup (Low risk, batchable)

- **Gap #3 (§5.3)** — the `roleNames()` caching sweep across ~10 model classes. Genuinely mechanical and low-risk; a good "fill-in" task between higher-priority phases, or a single dedicated cleanup pass.
- Remaining unit-level Low-severity findings (magic-number naming, dead ids, duplicated small constants, `MultiEffect` z-order fixes in U-08/U-09/U-10, `const`-correctness gaps) — no urgency, fold into ordinary maintenance.
- All Investigation Targets (60–79/100 confidence) not already pulled into an earlier phase — each needs the specific human verification step its unit document describes before any code change; none should be actioned on review confidence alone per REQ-C-4.

### Sequencing rationale

The order above follows three explicit rules pulled from the review itself: (1) foundation-layer fixes (U-01, the composition-root assertions) precede surface-layer fixes that assume correct initialization/config round-tripping (U-02 and above) — a config-loss bug fixed after a Settings-app UI fix would just resurface the same symptom; (2) cross-cutting architecture gaps (§5) are resolved via representative high-value instances before the long tail of unit-level findings that are themselves instances of those same gaps, since fixing the pattern generalizes to defects this review didn't catch; (3) findings that two or more units independently corroborate (U-05↔U-09's blocking-I/O cluster, U-01↔U-02's Release-mode assertions, U-03↔U-07's IActivityGate-adjacent parenting-order bug) are treated as one unit of remediation work regardless of which unit's document they're filed under, since a partial fix addressing only one unit's copy would leave the corroborating unit's instance of the identical bug in place.

---

*End of REPORT.md — this closes the poc-readiness-review SDD cycle's synthesis stage (T-014). Findings and this roadmap are ready for handoff to a dedicated remediation SDD pipeline; per REQ-C-1, no code changes were made during this review.*
