# SDD Tasks — poc-readiness-review

**Overview**: 14 tasks spanning 11 unit-by-unit deep reviews (T-001 through T-011) plus 3 synthesis tasks (T-012 through T-014). Execution is strictly sequential per REQ-NF-5, with human approval checkpoint after each unit before advancing to the next. All reviews are read-only (REQ-C-1); no code modifications occur.

---

## Unit Review Tasks (T-001 through T-011)

Each unit task instructs: (1) run the specified skill(s) on the exact source paths listed, (2) consult the prior context docs listed to establish architectural intent, (3) produce a findings write-up saved to `docs/sdd/poc-readiness-review/units/U-XX-{slug}.md` with every finding tagged Severity + Effort + Location + Rationale + Suggested Direction (REQ-F-5), (4) explicitly call out any doc-drift (current code vs. prior design intent per REQ-F-2 and REQ-NF-4).

---

- [x] T-001: Review U-01 — Foundational Core, Platform & Config
  - REQs: REQ-F-2, REQ-F-3, REQ-F-5, REQ-NF-4, REQ-C-1, REQ-C-4
  - Sources: `libs/holonight-platform/src/` (9 files: `LayerShell.h`, `LayerSurface.{h,cpp}`, `HyprlandIpc.{h,cpp}`, `HyprlandIpcClient.{h,cpp}`, `DbusPropertyClient.{h,cpp}`); `libs/holonight-core/src/` (18 files: `ConfigService.{h,cpp}`, `WorkspaceModel.{h,cpp}`, `ExtWorkspaceManager.{h,cpp}`, `HyprlandWorkspaceService.{h,cpp}`, `KeyboardLayoutService.{h,cpp}`, `BatteryState.{h,cpp}`, `AudioState.{h,cpp}`, `SystemInfo.{h,cpp}`, `Logger.{h,cpp}`); `libs/holonight-config/include/holonight_config/` + `libs/holonight-config/src/` (5 files: `config_parsers.h`, `config_structs.h`, `config_writer.h`, `ConfigParsers.cpp`, `ConfigWriter.cpp`)
  - Skill: `qt-cpp-review`
  - Prior context: consult `docs/sdd/arch-restructure-roadmap/DESIGN.md` (§2.2 dependency graph, C1/C3/M1/M9 decisions), `docs/sdd/arch-restructure-roadmap/AUDIT.md` (coverage baseline for `holonight_core`/`holonight_platform`)
  - Check: `docs/sdd/poc-readiness-review/units/U-01-foundational-core.md` exists, contains ≥1 finding (or explicit "no findings above confidence threshold" statement per REQ-C-4), includes a "Prior Context" subsection naming the consulted prior docs, and every reported finding has all five fields: Severity, Effort, Location, Rationale, Suggested Direction

- [x] T-002: Review U-02 — Presentation Surfaces & Shell Composition Root
  - REQs: REQ-F-2, REQ-F-3, REQ-F-5, REQ-NF-4, REQ-C-1, REQ-C-4
  - Sources: `libs/holonight-surfaces/src/` (50 files, flat, all core surface/manager classes); `apps/shell/app/` (4 files: `ControlServer.{h,cpp}`, `ShellApplication.{h,cpp}`); `apps/shell/main.cpp`
  - Skill: `qt-cpp-review`
  - Prior context: consult `docs/sdd/arch-restructure-roadmap/DESIGN.md` (§3.1–3.4 surfaces/app composition, C3 LayerShellManager ownership), DESIGN.md §5 (preliminary flag on `ShellApplication` 40+ member initialization concentration-of-risk), CLAUDE.md "Layer-Shell Surface Hiding" and "SingleShotConnection race on `configured()`" gotchas (verify flag #3 about dual-lifecycle—should be confirmed, not refactored as "simplification")
  - Check: `docs/sdd/poc-readiness-review/units/U-02-surfaces.md` exists with ≥1 finding or explicit "no findings" statement, documents the `ShellApplication` initialization-order concentration-of-risk assessment, acknowledges the dual-surface-lifecycle split (SidebarManager create-on-open vs. PerMonitorLayerManager keep-mapped) as *deliberately* different per CLAUDE.md gotchas, and every finding has all five metadata fields

- [x] T-003: Review U-03 — Power, Idle & Brightness Management
  - REQs: REQ-F-2, REQ-F-3, REQ-F-5, REQ-NF-4, REQ-C-1, REQ-C-4
  - Sources: `libs/holonight-services/src/` root-level files only: `ActivityGateManager.{h,cpp}`, `IActivityGate.h`, `LidStateMonitor.{h,cpp}`, `LowBatteryMonitor.{h,cpp}`, `SuspendInhibitorService.{h,cpp}`, `InhibitorModel.{h,cpp}`, `BatteryService.{h,cpp}`, `PowerProfilesService.{h,cpp}` (13 files); `libs/holonight-services/src/idle/` (12 files); `libs/holonight-services/src/brightness/` (8 files)
  - Skill: `qt-cpp-review`
  - Prior context: consult `docs/sdd/idle-management/SPEC.md` and `DESIGN.md`, `docs/sdd/power-extensions/SPEC.md` and `DESIGN.md`, `docs/sdd/brightness-service/SPEC.md` and `DESIGN.md`. Note: `idle-management` has 7 unchecked TASKS (pre-existing stale work per SPEC §12 appendix) — this review audits *current code*, not the stale tasks. Flag any ActivityGate/LidStateMonitor/LowBatteryMonitor state-update invariants (CLAUDE.md references "signal-order invariant: discharging LAST" on LowBatteryMonitor — verify it holds in current code)
  - Check: `docs/sdd/poc-readiness-review/units/U-03-power-idle-brightness.md` exists with findings or "no findings" statement, documents the state-update signal-order invariant check for LowBatteryMonitor, names the three prior SDD docs consulted, flags any doc-drift against those specs, and every finding has all five metadata fields

- [x] T-004: Review U-04 — Session, Window Identity, Network & Audio Services
  - REQs: REQ-F-2, REQ-F-3, REQ-F-5, REQ-NF-4, REQ-C-1, REQ-C-4
  - Sources: `libs/holonight-services/src/SessionService.{h,cpp}` + `libs/holonight-services/src/session/` (2 + 12 files: backends, CommandRunner, ProcessEnvironment, Locker); `libs/holonight-services/src/ActiveWindowService.{h,cpp}`, `MonitorOccupancyService.{h,cpp}` (4 files); `libs/holonight-services/src/SystemInfoService.{h,cpp}` (2 files); `libs/holonight-services/src/NetworkService.{h,cpp}` + `libs/holonight-services/src/network/` (2 + 4 files); `libs/holonight-services/src/audio/` (10 files: backends, models, PulseAudio wrappers)
  - Skill: `qt-cpp-review`
  - Prior context: consult `docs/sdd/session-lock-backend/DESIGN.md`, `docs/sdd/active-window-per-monitor/MEMORY.md` (per-monitor focus tracking pattern, CLAUDE.md "Per-Monitor Active Window" section), `docs/sdd/audio-service/SPEC.md`+`DESIGN.md`. AudioService uses `pa_threaded_mainloop` (direct libpulse C API) — verify thread-boundary marshalling to Qt main thread via `Qt::QueuedConnection` per the code comments. SessionService demonstrates the abstract-backend/test-seam pattern (DESIGN.md §1.3.1) — confirm it holds correctly
  - Check: `docs/sdd/poc-readiness-review/units/U-04-session-window-network-audio.md` exists with findings or "no findings" statement, documents the per-monitor focus tracking pattern and how it propagates (or diverges) from prior design, verifies libpulse thread-boundary safety, confirms SessionBackend abstraction quality, and every finding has all five metadata fields

- [x] T-005: Review U-05 — Desktop Integration, Appearance & Portal
  - REQs: REQ-F-2, REQ-F-3, REQ-F-5, REQ-NF-4, REQ-C-1, REQ-C-4
  - Sources: `libs/holonight-services/src/AppearanceService.{h,cpp}`, `ThemeService.{h,cpp}` (4 files); `libs/holonight-services/src/portal/` (6 files: NullPortalBackend, SettingsPortalBackend, PortalService); `libs/holonight-services/src/kde-compat/` (2 files); `libs/holonight-services/src/mime/` (2 files); `libs/holonight-services/src/session-integration/` (4 files)
  - Skill: `qt-cpp-review`
  - Prior context: consult `docs/sdd/portal-service/SPEC.md`+`DESIGN.md` (note: portal-service has 12 unchecked TASKS — stale, not in scope per REQ-C-3), `docs/sdd/system-appearance-portal/SPEC.md`+`DESIGN.md`, `docs/sdd/theme-variant-catalog/SPEC.md`+`DESIGN.md`. DESIGN.md §5 observation #5 flags that ThemeService was documented as "trivial constant accessors" in 2026-05-28 audit but now includes SettingsPortalBackend.h — verify whether ThemeService still matches the "trivial" framing or has grown live state (a doc-drift candidate). Confirm IPortalDBus abstraction is used consistently, not bypassed for convenience
  - Check: `docs/sdd/poc-readiness-review/units/U-05-desktop-integration.md` exists with findings or "no findings" statement, explicitly audits ThemeService's actual complexity vs. prior audit framing (trivial vs. live portal state), documents the IPortalDBus abstraction quality, flags any inconsistencies in portal backend usage, and every finding has all five metadata fields

- [x] T-006: Review U-06 — Launcher & Notifications Services
  - REQs: REQ-F-2, REQ-F-3, REQ-F-5, REQ-NF-4, REQ-C-1, REQ-C-4
  - Sources: `libs/holonight-services/src/launcher/` (14 files: cache, scanner, serializer, model, command execution, category mapper, recent apps); `libs/holonight-services/src/notifications/` (16 files: server, service, model, rules, types, storage, filter, policy)
  - Skill: `qt-cpp-review`
  - Prior context: consult `docs/sdd/launcher-desktop-cache/SPEC.md`+`DESIGN.md` (note: launcher-cache has 4 unchecked TASKS, stale), `docs/sdd/notification-hardening/SPEC.md`+`DESIGN.md`. Both services maintain persistent state (launcher SQLite cache, notification store) — verify cache invalidation strategies (launcher has known incomplete strategy per SPEC §12), confirm NotificationServer protocol adapter correctly implements `org.freedesktop.Notifications` D-Bus contract (CLAUDE.md has documented D-Bus gotchas — QDBusArgument read-mode traps, Q_CLASSINFO requirement, etc.; verify NotificationServer.cpp applies them)
  - Check: `docs/sdd/poc-readiness-review/units/U-06-launcher-notifications.md` exists with findings or "no findings" statement, documents the cache invalidation/persistence strategies for both services, verifies NotificationServer D-Bus protocol adapter correctness against CLAUDE.md gotchas and the spec, flags any findings about stale TASKS mentioned in prior DESIGN (note they are *not* in scope for this review per REQ-C-3, but may inform understanding of known gaps), and every finding has all five metadata fields

- [x] T-007: Review U-07 — Calendar & Weather Services
  - REQs: REQ-F-2, REQ-F-3, REQ-F-5, REQ-NF-4, REQ-C-1, REQ-C-4
  - Sources: `libs/holonight-services/src/CalendarService.{h,cpp}` + `libs/holonight-services/src/calendar/` (2 + 16 files: CalDAV/ICS providers, cache, sync manager, credential storage, parser, event model); `libs/holonight-services/src/weather/` (5 files: provider, data, service); `libs/holonight-services/src/weather-icon/` (8 files: mapper, moon phase calculator, compositor, bridge, layers)
  - Skill: `qt-cpp-review`
  - Prior context: consult `docs/sdd/calendar/SPEC.md`+`DESIGN.md`, `docs/sdd/weather-icon-compositor/SPEC.md`+`DESIGN.md`. Both CalendarService and WeatherService implement IActivityGate (reviewed in U-03) — cross-reference those findings for gate consistency. DESIGN.md §5 observation #6 notes that WeatherIconCompositor.qml is deliberately *not wired* into production topbar (which still uses SVG path) — when U-10's QML review encounters it, should treat as "intentional by design" (documented in project memory), not dead code. Verify calendar/weather sync scheduling, battery-gate interaction (per idle-management DESIGN), and CalDAV credential-storage via libsecret
  - Check: `docs/sdd/poc-readiness-review/units/U-07-calendar-weather.md` exists with findings or "no findings" statement, documents the IActivityGate integration pattern (consistent with U-03 findings), verifies sync scheduling and battery-gate interaction, confirms libsecret credential usage is safe, documents that WeatherIconCompositor non-wiring is deliberate-by-design (for U-10 QML reviewer awareness), and every finding has all five metadata fields

- [x] T-008: Review U-08 — Topbar QML
  - REQs: REQ-F-2, REQ-F-4, REQ-F-5, REQ-NF-4, REQ-C-1, REQ-C-4
  - Sources: `apps/shell/qml/Topbar/` (27 files: TopBar root, sections (workspace, status, active-window, clock, weather, audio, network, notifications, tray, logo), widgets, tooltips, indicators, strips, etc.)
  - Skill: `qt-qml-review`
  - Prior context: consult `docs/sdd/topbar-redesign/SPEC.md`+`DESIGN.md` (if exists), `docs/sdd/workspace-indicator-redesign/SPEC.md`+`DESIGN.md`, `docs/sdd/notification-ui-redesign/SPEC.md`+`DESIGN.md` (if exists). Per CLAUDE.md, every topbar component must propagate `barMonitorName` down to leaf `BarTooltipArea` components for per-monitor routing (this is the "Per-Monitor Active Window" pattern reviewed in detail in U-04 — topbar is the QML surface of that pattern). Verify binding chains don't have stale dependencies, check z-order of MultiEffect declarations (CLAUDE.md gotcha), verify state machine transitions for widgets, check image-loading paths
  - Check: `docs/sdd/poc-readiness-review/units/U-08-topbar.md` exists with findings or "no findings" statement, documents verification of per-monitor `barMonitorName` propagation chain (every tooltip and routing-sensitive component must have it), verifies MultiEffect/glow declarations are correct per CLAUDE.md, flags any binding loops or stale dependencies, and every finding has all five metadata fields

- [x] T-009: Review U-09 — RightSidebar + Popups QML
  - REQs: REQ-F-2, REQ-F-4, REQ-F-5, REQ-NF-4, REQ-C-1, REQ-C-4
  - Sources: `apps/shell/qml/RightSidebar/` including `Tabs/` (19 files: root sidebar, content, tab bar, buttons, Calendar/Media/Notifications/Overview/QuickSettings/System tabs); `apps/shell/qml/Popups/` including all seven subdirectories (28 files: Audio, Battery, Network, Status, Tooltip, Tray, Weather popups and subcomponents)
  - Skill: `qt-qml-review`
  - Prior context: consult `docs/sdd/right-sidebar/SPEC.md`+`DESIGN.md` (note: 1 unchecked TASK, stale), `docs/sdd/sidebar-upcoming-calendar/SPEC.md`+`DESIGN.md` (note: 2 unchecked TASKS, stale). Per CLAUDE.md and project memory (right-sidebar-architecture), sidebar surfaces are created-on-open and destroyed-on-close (different from topbar's always-mapped lifecycle); dismiss-overlay must set empty input region on hide (CLAUDE.md "Full-screen overlay input region" gotcha). Verify `configured()` race condition guard is in place (SingleShotConnection trap from CLAUDE.md). Check QML delegate behavior in Repeaters (potential memory leaks, state isolation), verify z-order and nested-layout centering (CLAUDE.md gotchas)
  - Check: `docs/sdd/poc-readiness-review/units/U-09-sidebar-popups.md` exists with findings or "no findings" statement, verifies sidebar create-on-open lifecycle matches prior design, confirms `configured()` race-condition guard exists, audits dismiss-overlay input-region logic, checks Repeater/Loader delegate state isolation, flags any layout nesting problems, and every finding has all five metadata fields

- [x] T-010: Review U-10 — Overlay Content & Shared QML Building Blocks
  - REQs: REQ-F-2, REQ-F-4, REQ-F-5, REQ-NF-4, REQ-C-1, REQ-C-4
  - Sources: `apps/shell/qml/Launcher/` (7 files); `apps/shell/qml/Notifications/` (4 files: toast stack, items, action bars); `apps/shell/qml/Tray/` (4 files: tray item, menu, dismiss overlay, section); `apps/shell/qml/Controls/` (8 files: icon components for audio, battery, network, session, signal-strength, bar, utility); `apps/shell/qml/Widgets/` (4 files: clock, time-to-event, stable digits, surface); `apps/shell/qml/Background/` (1 file); `apps/shell/qml/Utility/` (1 file: theme reload bridge); `apps/shell/qml/WeatherIcon/` (1 file: compositor)
  - Skill: `qt-qml-review`
  - Prior context: consult `docs/sdd/launcher-redesign/SPEC.md`+`DESIGN.md` (note: 1 unchecked TASK, stale), `docs/sdd/weather-icon-compositor/SPEC.md`+`DESIGN.md`. Per DESIGN.md §3.10 rationale, this unit intentionally merges several smaller QML regions (launcher, notifications, tray, controls, widgets, background, utility, weather-icon) into one 30-file pass rather than reviewing each 2–7-file fragment separately. WeatherIconCompositor.qml is deliberately *not wired* into production (topbar still uses SVG) — treat as intentional per design, not dead code. Verify Toast notification animations and state, check Control icon recoloring/symbolism patterns, verify Clock/TimeToEvent widgets consume correct services (per U-03/U-07), check Background and ThemeReloadBridge for any state-management gaps
  - Check: `docs/sdd/poc-readiness-review/units/U-10-overlay-controls-widgets.md` exists with findings or "no findings" statement, confirms WeatherIconCompositor non-wiring is deliberate (cross-referenced with U-07 docs), audits Toast animation/state correctness, verifies Control icon rendering patterns (including recoloring for symbolism per CLAUDE.md gotchas), checks widget data bindings to services, and every finding has all five metadata fields

- [x] T-011: Review U-11 — Settings Application
  - REQs: REQ-F-2, REQ-F-3, REQ-F-4, REQ-F-5, REQ-NF-4, REQ-C-1, REQ-C-4
  - Sources: `apps/settings/src/` (13 files: ConfigFileService, FontListModel, SettingsApplication, SettingsEditModel, ShellStatusService, ThemeConfigFile, main.cpp); `apps/settings/qml/` (7 files: SettingsWindow, NavPanel, ContentStack, PlaceholderPage, FooterBar, AppearancePage, BarPage)
  - Skill: Both `qt-cpp-review` and `qt-qml-review` (unit contains mixed C++/QML)
  - Prior context: consult `docs/sdd/holonight-settings/SPEC.md`+`DESIGN.md` (note: 2 unchecked TASKS, stale). Settings is architecturally isolated from shell's service graph (links only `holonight_config` + external `HolonightQt` packages + shared `holonight_componentsplugin`, per DESIGN.md §1.1) — verify this isolation holds (no accidental includes of shell services). Verify SettingsEditModel's `current_`/`snapshot_` isDirty tracking pattern (per project memory), ConfigFileService TOML read/write correctness, QML window/panel layout correctness
  - Check: `docs/sdd/poc-readiness-review/units/U-11-settings.md` exists with findings or "no findings" statement, verifies settings binary's architectural isolation (no shell-service includes), audits SettingsEditModel dirty-tracking pattern correctness, checks ConfigFileService TOML safety, verifies QML window layout against design, and every finding has all five metadata fields

---

## Synthesis Tasks (T-012, T-013, T-014)

After all 11 unit reviews are complete and approved, execute the synthesis tasks sequentially.

---

- [x] T-012: Synthesize "Architecture & Design Gaps" Section
  - REQs: REQ-F-6, REQ-C-1, REQ-C-4
  - Input: All 11 unit findings documents (U-01 through U-11) + DESIGN.md §5 "Preliminary Architecture-Level Observations" (observations #1–7)
  - Task: Synthesize cross-cutting architectural gaps and systemic weak decisions that span multiple units and are not reducible to a single unit's findings. Identify 3–8 systemic issues (not per-unit nitpicks). Each issue must include: affected units (by ID), severity/effort triage per REQ-F-5 schema, concrete rationale (failure scenario or weakness), and suggested architectural direction (per REQ-C-2: aggressive refactoring OK, no backward-compat constraint). Write to `docs/sdd/poc-readiness-review/ARCHITECTURE-GAPS.md`, ≥500 words, organized by systemic issue
  - Examples of cross-cutting patterns to look for: pervasive anti-patterns across multiple units, missing/weak abstraction layers, inconsistent error/retry strategies, circular or unclear dependency edges (especially within services), uneven testing coverage, lifecycle/ownership invariants that are fragile, threading model gaps
  - Check: `docs/sdd/poc-readiness-review/ARCHITECTURE-GAPS.md` exists, ≥500 words, documents ≥3 and ≤8 systemic issues, each with affected-unit IDs, Severity/Effort triage, Rationale, and Suggested Direction; sections are organized by issue (not by unit); cross-references to individual unit findings are explicit (e.g., "U-03 flagged X, U-07 flagged Y; common root: Z")

- [x] T-013: Assemble "Stale Backlog Context" Section
  - REQs: REQ-F-7, REQ-C-1, REQ-C-3
  - Input: SPEC.md §12 appendix table (pre-existing counts of unchecked TASKS.md items by feature directory)
  - Task: Assemble a "Stale Backlog Context" section documenting pre-existing incomplete work from prior SDD cycles. For each feature directory listed in SPEC.md §12 with >0 unchecked TASKS, extract: feature name, unchecked count, 1–2 sample task titles (from that feature's TASKS.md), and a clear statement: "These items are pre-existing incomplete work from prior SDD cycles and are NOT addressed by this review cycle." Do NOT analyze, attempt, or extend the stale tasks — merely catalog them. Write to `docs/sdd/poc-readiness-review/STALE-BACKLOG.md`
  - Check: `docs/sdd/poc-readiness-review/STALE-BACKLOG.md` exists, lists all features from SPEC.md §12 with unchecked count ≥1, includes ≥1 sample task title per feature, explicitly states they are pre-existing and out-of-scope per REQ-C-3, and total unchecked count matches SPEC.md §12 (sum = 32 per the table: portal-service 12 + idle-management 7 + launcher-cache 4 + holonight-settings 2 + launcher-redesign 1 + right-sidebar 1 + sidebar-upcoming-calendar 2 + system-appearance-portal 1 + theme-variant-catalog 2 + workspace-indicator-redesign 2)

- [x] T-014: Assemble Final "REPORT.md" Comprehensive Audit Report
  - REQs: REQ-F-8, REQ-C-1
  - Input: All 11 unit findings docs (U-01–U-11), ARCHITECTURE-GAPS.md (T-012 output), STALE-BACKLOG.md (T-013 output), DESIGN.md §1 (architecture & component review), DESIGN.md §3 (unit inventory)
  - Task: Produce the final comprehensive audit report, `docs/sdd/poc-readiness-review/REPORT.md`, integrating all prior work into a single, ready-for-remediation-pipeline artifact. The report must include all 7 required sections per REQ-F-8:
    1. **Executive Summary** (1–2 pages): High-level overview of review scope, unit count (11), file count (361), key findings categories, recommended remediation strategy priority (by severity/effort)
    2. **Architecture & Component Review** (2–3 pages): Pull from DESIGN.md §1 (layering, technology stack, cross-cutting patterns, strengths to preserve)
    3. **Review Unit Inventory** (1 page): Tabular summary of all 11 units (ID, Name, File Count, C++/QML, Purpose)
    4. **Per-Unit Deep Findings** (main body, 1–2 pages per unit): For each unit U-01–U-11, summarize: name, scope, prior context docs consulted, key findings (grouped by category: architecture, correctness, performance, maintainability), and tool sign-off (qt-cpp-review or qt-qml-review result)
    5. **Architecture & Design Gaps** (≥500 words): Pull entire ARCHITECTURE-GAPS.md output
    6. **Stale Backlog Context** (pull entire STALE-BACKLOG.md output)
    7. **Remediation Roadmap** (1–2 pages): Synthesize priority sequence across all findings + gaps, ordered by severity/effort matrix, architectural impact, and dependency order (e.g., "Foundation fixes (U-01 findings) must precede surface remediation (U-02); cross-cutting architecture gaps should be resolved before unit-level fixes that assume the old architecture")
  - Check: `docs/sdd/poc-readiness-review/REPORT.md` exists, is self-contained and ≥10 pages, includes all 7 required sections with clear hierarchy, explicitly names/summarizes every SDD unit (all 361 files attributed by unit), integrates all 11 unit findings docs by reference, synthesizes all cross-cutting gaps, documents all pre-existing stale work, proposes a defensible remediation sequence, and is formatted in Markdown suitable for handoff to a future remediation SDD pipeline

---

## Checkpoint Protocol

After each of T-001 through T-011 completes:
1. Human reviewer reads the unit findings document and approves, requests adjustments, or requests re-analysis
2. If approved or adjusted, advance to next unit
3. If re-analysis requested, return to same unit task and re-run the skill(s) with revised scope/criteria
4. Log decision in a running `REVIEW-CHECKPOINTS.md` file (one line per unit: "U-01 ✅ Approved", "U-02 ⚠️ Adjusted: re-scope platform docs", etc.) for final report audit trail

After T-011 approval, proceed sequentially to T-012, then T-013, then T-014 (these synthesis tasks have no inter-human checkpoint — they execute continuously once unit reviews are done).

---

## Execution Notes

- **REQ-NF-5 (Sequential)**: Units must be analyzed 1 at a time. No parallelization. Process logs (checkpoints) document sequence and any mid-pipeline adjustments.
- **REQ-C-1 (Read-Only)**: No code modifications. Only new .md documentation files created or checked into docs/sdd/poc-readiness-review/.
- **REQ-C-4 (Confidence Threshold)**: Only report findings with >80/100 confidence from qt-cpp-review / qt-qml-review. Lower-confidence findings (if included) must be explicitly marked with score and caveats.
- **Stale Work (REQ-C-3)**: Unchecked TASKS.md items from prior SDD cycles are noted in STALE-BACKLOG.md for context only; they are NOT analyzed or executed. Any prior doc in a unit's context is used to understand *intent*, not to validate *past work*.

---

*End of TASKS.md — Execute T-001 through T-014 sequentially per REQ-NF-5, with human approval checkpoint after each unit (T-001–T-011) before advancing.*
