# Phase 7 Triage Synthesis Report

| Field | Value |
|---|---|
| Document ID | poc-remediation-phase7/REPORT.md |
| Inputs | 11 triage/U-XX.md documents |
| Method | Per-unit re-examination against current HEAD at `fe32997a7c2264d9b54efbf78bead03983c4f7f8`, cross-checked against Phases 0-6 commit history and re-checked for production changes after Phase 6. Zero code changes (REQ-C-01). |
| Status | Final — seeds Phase 8 remediation scope |

## 1. Executive Summary

All 101 Investigation Targets from the original readiness review have been re-triaged: **77 Confirmed, 14 Refuted, 10 Not-a-bug, 0 Needs-Manual-Verification**. Every Confirmed item's re-derived confidence landed at or above the 80/100 gate (range 80–92), up from the original 60–79% band, and — notably — none of the 77 Confirmed items carry High or Critical severity; they cluster entirely at Medium (15) and Low (62). This is consistent with how the original review scoped this track: the higher-severity findings were already captured in the review's separate "Confirmed Findings" track and fixed across Phases 0–6, leaving Investigation Targets as the lower-confidence, generally lower-severity residue. Likewise, no Confirmed item requires Large (L) remediation effort — 69 are Small and 8 are Medium.

A recurring and citable pattern across the Refuted verdicts: several trace directly to Phases 0–6 fixing the underlying code as a side effect of unrelated work, not because anyone re-verified these specific Investigation Targets at the time. Notable examples: U-02's I-02 (`roleNames()` rebuilding every call) was mooted by Phase 6's `static const QHash` caching fix; U-03's I-02 (`ActivityGateManager` dangling-pointer teardown) was mooted by Phase 6 reordering member destruction; U-05's I-01, I-02, and I-04 — three separate Investigation Targets — were all mooted by a single Phase 5 commit that deleted `KdeCompatService` in its entirety; U-07's I-04 and I-05 (cache-failure visibility, `roleNames()` caching) were mooted by Phase 4's account-removal rewrite and Phase 6's caching fix respectively; and U-09's I-001 (blocking diagnostics probe) was mooted by Phase 3's async `SessionIntegrationService::refresh()` rewrite. This is a real signal that Phases 0–6 delivered broader positive impact than their own task lists claimed credit for.

One unit stands out structurally: **U-06 (launcher-notifications) produced 10/10 Confirmed — zero Refuted, Not-a-bug, or Needs-Manual-Verification**, the only unit with that distribution. Per the triage doc's own note, this was treated as a rigor red flag (per SPEC.md's anti-dumping-ground guardrail) rather than a badge of thoroughness, and the reviewer explicitly re-stress-tested its three most stylistic-looking items before finalizing — each still held up with concrete in-repo counter-evidence.

## 2. Summary Tally

| Unit | Confirmed | Refuted | Not-a-bug | Needs-Manual-Verification | Total |
|---|---|---|---|---|---|
| U-01 | 4 | 2 | 4 | 0 | 10 |
| U-02 | 8 | 1 | 1 | 0 | 10 |
| U-03 | 7 | 3 | 0 | 0 | 10 |
| U-04 | 9 | 0 | 1 | 0 | 10 |
| U-05 | 5 | 3 | 0 | 0 | 8 |
| U-06 | 10 | 0 | 0 | 0 | 10 |
| U-07 | 7 | 2 | 1 | 0 | 10 |
| U-08 | 6 | 2 | 0 | 0 | 8 |
| U-09 | 6 | 1 | 0 | 0 | 7 |
| U-10 | 5 | 0 | 2 | 0 | 7 |
| U-11 | 10 | 0 | 1 | 0 | 11 |
| **Total** | **77** | **14** | **10** | **0** | **101** |

## 3. Confirmed Items — Ranked Remediation Candidates

Ranked by Severity (High → Medium → Low), sub-ranked by Effort (S before M before L). No High-severity Confirmed items exist in this cycle (see §1). Within an identical Severity×Effort tier, items are ordered by Unit then Item ID for traceability — **not** by confidence, per DESIGN.md §5.5 (confidence is an inclusion gate, not a sort key).

**Legend**: `*` = Severity/Effort not present in the source unit doc's Investigation Targets entry for U-08; value shown is the Phase 7 triage doc's documented judgment. `~` = Severity/Effort absent from both the triage doc and original unit doc for U-09/U-10/U-11; value is explicitly inferred and recorded by this synthesis step from the item's rationale, using the original High/Medium/Low/S-M-L criteria. See §6 for provenance and limitations.

### High severity

None. See Executive Summary for why.

### Medium severity

**Effort S**

1. **U-02 [I-08]** ControlServer has no framing/size bound, single readyRead chunk (Confidence 84/100, Severity Medium, Effort S) — malformed/oversized control-socket input is never bounded or reassembled; see triage/U-02.md.
2. **U-03 [I-01]** BatteryService/LowBatteryMonitor unclamped UPower percentages (Confidence 83/100, Severity Medium, Effort S) — negative UPower percent can produce a broken "-N% remaining" notification; see triage/U-03.md.
3. **U-07 [I-02]** CalendarSyncManager maps keyed by account name only (Confidence 85/100, Severity Medium, Effort S) — cross-provider-type collision risk, blast radius widened by Phase 4's new `removeAccount()`; see triage/U-07.md.
4. **U-08 [I-01]*** Unbounded Repeater sizing via maxWorkspaceId (Confidence 84/100, Severity Low-Medium per triage doc — ranked Medium, Effort S) — no delegate virtualization against nontrivial per-delegate cost; see triage/U-08.md.
5. **U-08 [I-03]*** WeatherSection Loader skips shrink animation (Confidence 83/100, Severity Medium, Effort S) — `Loader.active:false` destroys the item synchronously, skipping the 200ms shrink Behavior; see triage/U-08.md.
6. **U-09 [I-003]~** MultiEffect glow z-order inverted (Confidence 83/100, Severity Medium, Effort S) — three components still declare MultiEffect after their source shape, contradicting the StatusPopup.qml reference pattern; see triage/U-09.md.
7. **U-09 [I-004]~** BrightnessSlider debounce is not throttle (Confidence 88/100, Severity Medium, Effort S) — `Timer.restart()` on every `valueChanging` event is textbook debounce despite the doc comment claiming throttle; see triage/U-09.md.
8. **U-09 [I-005]~** WeatherPopupContent near-zero height margin (Confidence 81/100, Severity Medium, Effort S) — manual Column with no clip/Flickable sums to ~900–910px against a 920px budget; see triage/U-09.md.
9. **U-11 [I-C1]~** Integer setters lack range/positivity validation (Confidence 84/100, Severity Medium, Effort S) — unclamped values are also persisted verbatim by `ConfigWriter.cpp`, masked only by QML Slider bounds today; see triage/U-11.md.
10. **U-11 [I-C4]~** Hardcoded "cyan" default-accent diverges from catalog default (Confidence 92/100, Severity Medium, Effort S) — the real catalog default is `"default"`; currently masked because `load()` always runs before QML binds; see triage/U-11.md.
11. **U-11 [I-Q3]~** Apply/Save&Apply buttons identical, neither gated on isDirty (Confidence 88/100, Severity Medium, Effort S) — byte-identical `enabled`/`onClicked` bindings with no documented rationale for two buttons; see triage/U-11.md.

**Effort M**

12. **U-02 [I-01]** TrayImageProvider not synchronized with TrayModel rows (Confidence 82/100, Severity Medium, Effort M) — no locking primitive anywhere in TrayModel; dormant risk pending an `asynchronous:true` call site; see triage/U-02.md.
13. **U-02 [I-04]** No QQuickView status check after setSource() (Confidence 85/100, Severity Medium, Effort M) — exhaustive re-grep of all 7 cited files confirms zero status()/statusChanged checks anywhere, systemic; see triage/U-02.md.
14. **U-04 [I-01]** NetworkManagerBackend destructor stale-snapshot UAF risk (Confidence 84/100, Severity Medium, Effort M) — a stale copy of `operation_watchers_` is iterated while `processEvents()` lets a sibling watcher mutate and `deleteLater()` the list; see triage/U-04.md.
15. **U-06 [I-02]** rescan resets selection/scroll unconditionally (Confidence 85/100, Severity Medium, Effort M) — the reset lambda and `refreshSelectionAfterModelReset()` still unconditionally clamp selection on every modelReset; see triage/U-06.md.

### Low severity

**Effort S**

16. **U-01 [I-01]** WorkspaceModel::emitRowsChanged full-range no-roles emit (Confidence 82/100, Severity Low, Effort S) — see triage/U-01.md.
17. **U-01 [I-03]** DbusPropertyClient::serviceRegistered swallows failures silently (Confidence 83/100, Severity Low, Effort S) — real callers include NetworkService/PowerProfilesService's primary/fallback bus selection; see triage/U-01.md.
18. **U-01 [I-05]** BatteryState property parsing does a double lookup per field (Confidence 80/100, Severity Low, Effort S) — see triage/U-01.md.
19. **U-01 [I-10]** Weather lat/long accepted with no range validation (Confidence 84/100, Severity Low, Effort S) — zero range validation anywhere in the pipeline before the live OWM API call; see triage/U-01.md.
20. **U-02 [I-03]** Duplicated screen-edge-centering geometry (Confidence 82/100, Severity Low, Effort S) — TooltipSurface still inlines the same clamp formula StatusPopupGeometry factors out; see triage/U-02.md.
21. **U-02 [I-06]** decodePixmapList per-pixel setPixel + unconditional debug string (Confidence 84/100, Severity Low, Effort S) — Phase 0 fixed the adjacent overflow bug but left both complaints untouched; see triage/U-02.md.
22. **U-02 [I-07]** TooltipSurface anchor_x missing screen-origin subtract (Confidence 86/100, Severity Low, Effort S) — tray tooltips pass a genuine global coordinate that's clamped without subtracting screen origin, unlike its siblings; see triage/U-02.md.
23. **U-02 [I-09]** WidgetManager::viewForMonitor linear scan (Confidence 81/100, Severity Low, Effort S) — O(n) string-keyed scan on every occupancy change; see triage/U-02.md.
24. **U-02 [I-10]** TrayWatcher QStringList O(n) contains/removeOne (Confidence 81/100, Severity Low, Effort S) — see triage/U-02.md.
25. **U-03 [I-03]** Hardcoded 2000ms subprocess timeouts across three services (Confidence 82/100, Severity Low, Effort S) — Phase 1's dedup collapsed two copies into a shared helper but it still hardcodes the literal; see triage/U-03.md.
26. **U-03 [I-04]** Dead/duplicate branch in parseProfileNames (Confidence 83/100, Severity Low, Effort S) — the trailing branch is provably unreachable via QVariant::canConvert semantics; see triage/U-03.md.
27. **U-03 [I-06]** BrightnessService computePercent missing clamp (Confidence 84/100, Severity Low, Effort S) — a concrete externally-triggerable path via a writable sysfs file; see triage/U-03.md.
28. **U-03 [I-07]** PowerProfilesService never disconnects PropertiesChanged on restart (Confidence 82/100, Severity Low, Effort S) — no dedup/UniqueConnection layer exists, so bindings genuinely accumulate on each daemon restart; see triage/U-03.md.
29. **U-03 [I-09]** IdleService default idle threshold as inline magic number (Confidence 81/100, Severity Low, Effort S) — see triage/U-03.md.
30. **U-03 [I-10]** SysfsBackend ignores QTextStream extraction failure (Confidence 80/100, Severity Low, Effort S) — see triage/U-03.md.
31. **U-04 [I-03]** focusedMonitor()/focusedMonitorName() 100% duplicate getters (Confidence 90/100, Severity Low, Effort S) — character-for-character identical one-liners; see triage/U-04.md.
32. **U-04 [I-04]** Volume-set calls hardcode 2-channel pa_cvolume (Confidence 85/100, Severity Low, Effort S) — no channel-map/channel-count field exists anywhere in AudioTypes.h; see triage/U-04.md.
33. **U-04 [I-05]** connectionId avoidable QMap→QVariantMap copy (Confidence 82/100, Severity Low, Effort S) — sibling `savedWifiConnections()` proves the copy is unnecessary; see triage/U-04.md.
34. **U-04 [I-06]** Duplicated .desktop INI-parsing state machine (Confidence 85/100, Severity Low, Effort S) — identical scan-and-split logic differing only in extracted keys; see triage/U-04.md.
35. **U-04 [I-08]** NM D-Bus failures surface generic hardcoded message (Confidence 84/100, Severity Low, Effort S) — all four `emitError()` sites discard `reply.error().message()` while sibling methods log it; see triage/U-04.md.
36. **U-05 [I-03]** containsDesktopFiles/hasDesktopFiles duplicated (Confidence 85/100, Severity Low, Effort S) — byte-for-byte identical QDirIterator scan; see triage/U-05.md.
37. **U-05 [I-05]** PortalBackend/PortalDBus naming confusion (Confidence 83/100, Severity Low, Effort S) — NullPortalBackend.h defines no class named that; see triage/U-05.md.
38. **U-05 [I-06]** MimeService QHash detach on every cache update (Confidence 80/100, Severity Low, Effort S) — full detach every call confirmed, real-world impact stays minor; see triage/U-05.md.
39. **U-05 [I-07]** PortalService relies on ~25s libdbus default timeout (Confidence 88/100, Severity Low, Effort S) — zero timeout anywhere in the four D-Bus calls, contrasted with the unit's own 5s convention elsewhere; see triage/U-05.md.
40. **U-05 [I-08]** Un-named magic numbers for color-scheme (1/2) (Confidence 81/100, Severity Low, Effort S) — see triage/U-05.md.
41. **U-06 [I-01]** results() non-const Q_PROPERTY accessor (Confidence 82/100, Severity Low, Effort S) — NetworkService::wifiNetworks() const proves a const accessor is achievable here; see triage/U-06.md.
42. **U-06 [I-03]** switch default: suppresses -Wswitch protection (Confidence 80/100, Severity Low, Effort S) — defeats exhaustiveness checking on future Role additions; see triage/U-06.md.
43. **U-06 [I-05]** rules_ has no upper bound (Confidence 87/100, Severity Low, Effort S) — `ensureApp()` still appends/persists unconditionally with no cap; see triage/U-06.md.
44. **U-06 [I-06]** Roles enums lack explicit underlying type (Confidence 81/100, Severity Low, Effort S) — LauncherModel::Role's in-repo precedent forecloses a "QML constraint" reading; see triage/U-06.md.
45. **U-06 [I-07]** fresh prepared statement per entry (Confidence 82/100, Severity Low, Effort S) — runs on a background thread so not UI-blocking; see triage/U-06.md.
46. **U-06 [I-08]** no self-healing path for tampered schema (Confidence 81/100, Severity Low, Effort S) — version-pragma-only detection never distinguishes tamper from legitimate migration; see triage/U-06.md.
47. **U-06 [I-09]** DesktopEntry field list enumerated in 3+ places (Confidence 85/100, Severity Low, Effort S) — the new `startup_wm_class` field is a concrete post-review instance of the shotgun-surgery risk; see triage/U-06.md.
48. **U-06 [I-10]** destructor doesn't release QSqlDatabase connection (Confidence 84/100, Severity Low, Effort S) — destructor is still `= default`, never calls close(); see triage/U-06.md.
49. **U-07 [I-01]** WeatherIconMapper default: on last switch case (Confidence 84/100, Severity Low, Effort S) — sibling method already uses the safer no-default pattern; see triage/U-07.md.
50. **U-07 [I-03]** WeatherProvider::parseReplyBody duplicates lambda (Confidence 82/100, Severity Low, Effort S) — see triage/U-07.md.
51. **U-07 [I-07]** WeatherService retries indefinitely on bad API key (Confidence 82/100, Severity Low, Effort S) — the error payload is reduced to a display string before WeatherService sees it; needs structured status plumbing; see triage/U-07.md.
52. **U-07 [I-08]** hourlyVariant/dailyVariant rebuild list every read (Confidence 80/100, Severity Low, Effort S) — systemic pattern, usage-pattern-dependent cost; see triage/U-07.md.
53. **U-07 [I-09]** ICalParser ~26 sequential string comparisons (Confidence 80/100, Severity Low, Effort S) — runs on background thread at most every 15/60 min, not UI-blocking; see triage/U-07.md.
54. **U-07 [I-10]** No range/sanity validation on weather numeric fields (Confidence 81/100, Severity Low, Effort S) — contrasts with `condition_id`'s existing safe-fallback precedent; see triage/U-07.md.
55. **U-08 [I-02]*** BarSection base implicitWidth formula non-functional (Confidence 85/100, Severity Low, Effort S) — all 11 current BarSection subclasses override implicitWidth, proving the base formula is dead code; see triage/U-08.md.
56. **U-08 [I-05]*** BatteryIndicator cosmetic snap on critical-pulse exit (Confidence 81/100, Severity Low, Effort S) — animation-stop-mid-cycle plus a state change with no Transition compose to a visible jump; see triage/U-08.md.
57. **U-08 [I-06]*** BarTooltipArea barMonitorName not required (Confidence 80/100, Severity Low, Effort S) — no live bug (all 13 call sites set it correctly) but the required-property asymmetry has no architectural rationale; see triage/U-08.md.
58. **U-08 [I-07]*** WorkspaceEdgeArrow dead hoverEnabled:true (Confidence 85/100, Severity Low, Effort S) — whole-file grep for containsMouse returns zero matches; see triage/U-08.md.
59. **U-09 [I-002]~** Inconsistent required property across delegates (Confidence 85/100, Severity Low, Effort S) — zero required properties/pragma despite an unrelated Phase 6 fix landing in the same file; see triage/U-09.md.
60. **U-09 [I-006]~** Notification-preview lacks rules-sibling discipline (Confidence 82/100, Severity Low, Effort S) — the rules-sibling delegate got *more* disciplined via Phase 5/6 while this preview delegate was untouched, widening the gap; see triage/U-09.md.
61. **U-09 [I-007]~** Missing pragma ComponentBehavior:Bound on delegates (Confidence 84/100, Severity Low, Effort S) — all four cited files still lack the pragma; see triage/U-09.md.
62. **U-10 [I-001]~** Icon Image elements lack error handling (Confidence 85/100, Severity Low, Effort S) — three Image elements lack the usedFallback/onStatusChanged pattern established twice elsewhere; see triage/U-10.md.
63. **U-10 [I-004]~** StableDigitsText omits textFormat PlainText (Confidence 84/100, Severity Low, Effort S) — defaults to AutoText's markup scan on a hot per-second tick path across 8+ instances; see triage/U-10.md.
64. **U-10 [I-005]~** Stale width comment vs actual value (Confidence 90/100, Severity Low, Effort S) — comment still says "fixed 240px" above `Layout.preferredWidth: 256`; see triage/U-10.md.
65. **U-10 [I-006]~** Dead MultiEffect/Rectangle ids unreferenced (Confidence 88/100, Severity Low, Effort S) — five ids across three files declared but never read anywhere; see triage/U-10.md.
66. **U-10 [I-007]~** WeatherIconCompositor undocumented properties (Confidence 87/100, Severity Low, Effort S) — `conditionDescription`/`windSpeedKmh`/`moonPhase` plus a whole second invokable remain undocumented in project memory; see triage/U-10.md.
67. **U-11 [I-C2]~** setFromParsedConfig bypasses guarded-recompute convention (Confidence 80/100, Severity Low, Effort S) — `blockSignals` no-op, unconditional signal emission, forced `is_dirty_=false`; see triage/U-11.md.
68. **U-11 [I-C3]~** markSaved 2-arg overload dead in production (Confidence 90/100, Severity Low, Effort S) — repo-wide grep confirms zero production callers; see triage/U-11.md.
69. **U-11 [I-C5]~** FontListModel::rebuild() rescans full font list every toggle (Confidence 80/100, Severity Low, Effort S) — a sibling anti-pattern in the same file was fixed in Phase 6, this one wasn't; see triage/U-11.md.
70. **U-11 [I-Q1]~** Missing pragma ComponentBehavior: Bound on delegate-reaching files (Confidence 85/100, Severity Low, Effort S) — absent in all 7 cited files; see triage/U-11.md.
71. **U-11 [I-Q2]~** Duplicate, unbound "current page" state (NavPanel/ContentStack) (Confidence 82/100, Severity Low, Effort S) — two independently-declared properties synced only via one click-handler path; see triage/U-11.md.
72. **U-11 [I-Q5]~** Repeated per-delegate Qt.rgba recomputation in NavPanel (Confidence 84/100, Severity Low, Effort S) — pure function of palette/alpha re-evaluated on every nav click across 13 delegates; see triage/U-11.md.
73. **U-11 [I-Q6]~** Text elements lack explicit textFormat: Text.PlainText (Confidence 80/100, Severity Low, Effort S) — repo-wide grep confirms zero textFormat usage across ~17 Text elements; see triage/U-11.md.

**Effort M**

74. **U-04 [I-02]** ActiveWindowService per-monitor state never pruned (Confidence 85/100, Severity Low, Effort M) — zero `.remove()`/`.erase()` against monitor_windows/monitor_workspaces; hot-unplugged monitor entries orphaned for process lifetime; see triage/U-04.md.
75. **U-04 [I-07]** Destructor processEvents() can re-arm watcher_ mid-teardown (Confidence 83/100, Severity Low, Effort M) — a queued refresh() delivered during the teardown window re-arms `watcher_` with no further wait; see triage/U-04.md.
76. **U-04 [I-09]** Two overlapping NM polling loops re-query same state (Confidence 83/100, Severity Low, Effort M) — `onPollTimer()` fires both `queryAll()` and `backend_->refresh()` every 2s tick back-to-back; see triage/U-04.md.
77. **U-06 [I-04]** copies full DesktopEntry structs per keystroke (Confidence 83/100, Severity Low, Effort M) — `ScoredEntry::entry` remains value-typed despite `entries_` being stable per rebuild; see triage/U-06.md.

## 4. Needs-Manual-Verification Roster

Empty. All 101 items resolved to one of the other three verdicts (Confirmed, Refuted, Not-a-bug) — no item in this Phase 7 cycle required live-compositor interaction to adjudicate. Every unit's triage agent applied SPEC.md's decision rule (resolve statically whenever possible) and none reached for this verdict as a default; see each triage/U-XX.md's methodology note for confirmation.

## 5. Phase 8 Seed Statement

This list [Section 3] seeded Phase 8. The first bounded remediation tranche fixed U-02 I-07/I-08, U-03 I-01, U-07 I-02, and U-11 I-C1/I-Q3 in `daef305` (`fix: remediate phase 8 correctness findings`), with targeted and project validation recorded in `poc-remediation-phase8/TASKS.md` T-087/T-088. The live Hyprland verification for U-02 I-07 also passed (T-089).

Phase 9 fixed U-08 I-03 in `fdc7a23` (`fix: animate dynamic topbar section
exits`), with focused/full QML validation in T-095 and live Hyprland acceptance
in T-096. It also remedied matching audio, battery, and keyboard-layout exit
lifecycles while retaining the network offline state. Phase 10 implemented and
accepted U-08 I-01 in `af78194` (`fix: complete workspace pill remediation`),
with focused/full QML validation in T-101 and live Hyprland acceptance in
T-102.

Phase 12 fixed U-09 I-003/I-004/I-005 and U-11 I-C4 in `4da14e8` (`fix:
complete phase 12 popup remediation`). Phase 13 (`c7be4e6`, `fix: adapt status
popup sizing`) completed the adaptive Weather sizing follow-up to Phase 12's
bounded-overflow safeguard. Phase 14 accepted the final Medium-severity
candidates — U-02 I-01/I-04, U-04 I-01, and U-06 I-02 — in `af590d6` (`fix:
complete phase 14 reliability remediation`), with automated checks and user
live verification recorded in `poc-remediation-phase14/TASKS.md`. Phase 15
accepted U-04 I-02/I-03/I-06 in `e7c81cd` (`fix: complete phase 15 active
window remediation`), with automated checks and user live verification
recorded in `poc-remediation-phase15/TASKS.md`. Phase 16 accepted U-03
I-04/I-06/I-09 in `a93e26e` (`fix: complete phase 16 power remediation`),
with automated checks and user live verification recorded in
`poc-remediation-phase16/TASKS.md`. Phase 17 accepted U-03 I-03/I-10 in
`b0c5635` (`fix: complete phase 17 power hardening`), with automated checks
and user live verification recorded in `poc-remediation-phase17/TASKS.md`.
Phase 18 accepted U-03 I-07 in `6ec9872` (`fix: complete phase 18 power
profiles remediation`), with automated checks and user live verification
recorded in `poc-remediation-phase18/TASKS.md`. The other 52 Low-severity
candidates stayed queued for later tranches. Phase 19 accepted U-04 I-05 in
`2b0c8c7` (`fix: complete phase 19 network remediation`), with automated
checks and user live verification recorded in `poc-remediation-phase19/TASKS.md`.
Phase 20 accepted U-04 I-08 in `7ce92c0` (`fix: complete phase 20 network
remediation`), with automated checks and user live verification recorded in
`poc-remediation-phase20/TASKS.md`. The other 50 Low-severity candidates stay
queued for later tranches. Phase 21 accepted U-05 I-03 in `96fdc5f` (`fix:
share desktop file discovery`), with automated checks and user verification
recorded in `poc-remediation-phase21/TASKS.md`. The other 49 Low-severity
candidates stay queued for later tranches. Phase 22 accepted U-05 I-07 in
`4e8fa56` (`fix: bound portal D-Bus probes`), with automated checks and user
verification recorded in `poc-remediation-phase22/TASKS.md`. The other 48
Low-severity candidates stay queued for later tranches. Phase 23 accepted U-01
I-05 in `c12f9dc` (`perf: avoid duplicate battery property lookups`), with
automated checks and user verification recorded in
`poc-remediation-phase23/TASKS.md`. The other 47 Low-severity candidates stay
queued for later tranches. Phase 24 accepted U-01 I-01 in `cbcefee` (`perf:
narrow workspace model role notifications`), with automated checks and user
verification recorded in
`poc-remediation-phase24/TASKS.md`. The other 46 Low-severity candidates stay
queued for later tranches. Phase 25 accepted U-01 I-03 in `3c291a1` (`fix:
log D-Bus service registration failures`), with automated checks and user
verification recorded in `poc-remediation-phase25/TASKS.md`. The other 45
Low-severity candidates stay queued for later tranches.
Phase 26 accepted U-01 I-10 in `3d2c9fd` (`fix: validate configured weather
coordinates`), with automated checks and user verification recorded in
`poc-remediation-phase26/TASKS.md`. The other 44 Low-severity candidates stay
queued for later tranches. Phase 27 accepted U-02 I-06 in `471209e` (`perf:
streamline tray pixmap decoding`), with automated checks and user verification
recorded in `poc-remediation-phase27/TASKS.md`. This closeout also reconciles
U-02 I-03 as already accepted through Phase 8's `TooltipGeometry` extraction;
the other 42 Low-severity candidates stay queued for later tranches. Phase 28
accepted U-02 I-09 in `e230597` (`perf: index per-monitor views`), with
automated checks and user verification recorded in
`poc-remediation-phase28/TASKS.md`. The other 41 Low-severity candidates stay
queued for later tranches. Phase 29 accepted U-02 I-10 in `bee950b` (`perf:
index tray item membership`), with automated checks and user verification
recorded in `poc-remediation-phase29/TASKS.md`. The other 40 Low-severity
candidates stay queued for later tranches. Phase 30 accepted U-04 I-04 in
`54133c3` (`fix: preserve PulseAudio channel counts`), with automated checks
and user verification recorded in `poc-remediation-phase30/TASKS.md`. The
other 39 Low-severity candidates stay queued for later tranches.

## 6. Methodology Notes

Dispatch was parallel across all 11 units (see DESIGN.md §4); the Phase 0–6 manifest covers `0b28776..cf76ba0` (Phase 0 commit `d4f2310` through Phase 6 commit `cf76ba0`). Before publication, the review was revalidated against current HEAD `fe32997a7c2264d9b54efbf78bead03983c4f7f8`; the changed-file impact pass and affected-item re-triage are recorded in `.manifest/post-phase6-reverification.md`. Severity/Effort values used in the Section 3 ranking are sourced from the original readiness-review Investigation Target entries wherever that field exists; only Confidence is re-derived by the Phase 7 triage agents, per REQ-F-03.

This synthesis step cross-referenced each `triage/U-XX.md` against its corresponding original `docs/sdd/poc-readiness-review/units/U-XX-*.md` to obtain matching Severity/Effort. The original doc's **Investigation Targets** section carries an explicit `**Severity**: X · **Effort**: Y` field for U-01 through U-07 only. For **U-08**, that field is absent from the original Investigation Targets entries (it exists only in the separate, differently-numbered Confirmed Findings track); the Phase 7 triage doc supplies its documented Severity/Effort judgment per item, which this report uses as-is (marked `*` in Section 3). For **U-09, U-10, and U-11**, the field is absent from both documents. To satisfy REQ-F-10's complete-ranking requirement, this report explicitly infers Severity/Effort for those 21 items from each rationale, using the original High/Medium/Low and S/M/L criteria. These `~` classifications are lower-confidence than sourced values and are recorded here, adjacent to the ranked list, for Phase 8 review.

Within each identical Severity×Effort tier, Section 3 orders items by Unit then Item ID rather than by confidence, per DESIGN.md §5.5's explicit instruction that confidence functions as an inclusion gate (≥80 to appear as Confirmed at all), not a further sort key.
