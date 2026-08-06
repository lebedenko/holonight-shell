# U-09 — RightSidebar + Popups QML

**Task**: T-009 · **Skill**: qt-qml-review · **Scope**: 47 files
- `apps/shell/qml/RightSidebar/` (19 files: root sidebar, content, tab bar, buttons, Calendar/Media/Notifications/Overview/QuickSettings/System tabs)
- `apps/shell/qml/Popups/` (28 files: Audio, Battery, Network, Status, Tooltip, Tray, Weather popups and subcomponents)

## Prior Context / Mandatory-check verdicts

1. **Sidebar create-on-open / destroy-on-close lifecycle** — PASS. `SidebarManager::createSurface`/`destroySurface` confirmed genuine create-on-open/destroy-after-close-animation semantics. Every tab's `Component.onCompleted` correctly re-runs on each open (verified live: opened/closed sidebar via control socket, no stray warnings, tab content re-populated each time). No stale-caching bug found.
2. **`configured()` SingleShotConnection race guard** — N/A to this unit. Grepped all `LayerSurface::configured()` consumers — only `WidgetManager.cpp` connects to it (desktop widgets, out of scope). `SidebarManager`, `StatusPopupSurface`, `TooltipSurface`, and `TrayMenuSurface` all sidestep the race by a different, equally-defensive pattern: they unconditionally destroy and recreate the surface on every `show()` ("Always remap so the compositor sends a fresh configure cycle" — comment in `StatusPopupSurface.cpp:82`), rather than reusing a hidden resident surface.
3. **Dismiss-overlay input-region gotcha** (CLAUDE.md "Full-screen overlay input region") — PASS, not applicable. `StatusPopupDismissOverlay.qml`'s surface is fully destroyed (not hidden) on every dismiss (`StatusPopupSurface::hide()` → `destroySurface()`). `TrayMenuPopup.qml`'s dismiss path (`TrayMenuSurface.close()`) is deferred via `QTimer::singleShot(0, ...)` and also destroys, not hides. Neither is a keep-alive resident surface, so the stale-input-region trap doesn't apply.
4. **Repeater/Loader delegate memory leaks and state isolation** — PASS with one real finding. No `reuseItems: true` anywhere in scope, so Qt Quick does not recycle delegate instances (each model row keeps its own delegate for its lifetime) — no state-leak-across-reuse risk. All dynamic-composition surfaces (`WeatherPopupContent.qml`, `AudioPopupContent.qml`) compose subcomponents as static QML children — zero `createObject`/`Qt.createComponent`/`sourceComponent` usage anywhere in the 47 files, so no dynamic-object-lifecycle risk exists. See D-002 for the one real Repeater/Component-lifecycle defect found (redundant filesystem scans on recreation, not a leak).
5. **Layout nesting / MultiEffect z-order / nested-layout-centering** (CLAUDE.md gotchas) — mostly PASS, with drift. `BatteryPopupContent.qml` is the reference-correct example (explicitly documents and applies the nested-layout-centering rule). All 10 `MultiEffect` instances that provide glow/blur *behind* their source shape follow correct z-order except 3 — see I-003.

## Tool Sign-off

Phase 1 lint produced 396 hits. 152× ORD-1 (attribute ordering) + 59× STY-3 (dot-notation grouping) excluded as pure style noise, per the established pattern from prior units. 34 substantive hits itemized below (L-001–L-010); the remainder (JS-1 var, 89 hits) is grouped into one lint finding due to volume, with its worst-offender file elevated to a structural deep finding (D-006).

## Confirmed Findings

### D-001 — Dead `preferredWidth` properties + real width drift (87/100)
- **Severity**: Medium · **Effort**: Small
- **Location**: `SidebarOverview.qml:10` and 5 sibling tab roots (`SidebarCalendar.qml`, `SidebarNotifications.qml`, `SidebarSystem.qml`, `SidebarQuickSettings.qml`, `SidebarMedia.qml`); `SidebarContent.qml:22-28`
- **Rationale**: Every sidebar tab root declares `property int preferredWidth: N`, mirroring the genuinely-consumed `preferredHeight` pattern — but panel width is actually driven by a second, independently hardcoded `tabDefinitions` array in `SidebarContent.qml`. Repo-wide grep confirms zero consumers of the per-tab `preferredWidth` property outside its own declaration. The two sources of truth have already drifted: `SidebarOverview.qml` declares `preferredWidth: 380` but `tabDefinitions` actually uses `width: 360` — a 20px inconsistency invisible today only because nothing reads the dead property.
- **Suggested Direction**: Either delete the six dead `preferredWidth` declarations, or make `SidebarContent.qml` source tab width from `loader.item.preferredWidth` the same way it already does for height (eliminating the duplicate array). Fix the 380-vs-360 mismatch as part of whichever direction is chosen.

### D-002 — Six independent uncached filesystem scans on every System-tab load (85/100)
- **Severity**: High (performance) · **Effort**: Medium
- **Location**: `RightSidebar/Tabs/System/DefaultAppRow.qml:40-53`, instantiated 6× in `SidebarSystem.qml:70-122`
- **Rationale**: Every `DefaultAppRow` runs `Component.onCompleted: { refreshCandidates(); syncComboIndex(); }`, calling `LauncherService::defaultAppEntriesForMimeTypes[AndCategories]` → `DesktopEntryScanner::scanForDefaultApps()` (`libs/holonight-services/src/launcher/DesktopEntryScanner.cpp:293`), an uncached, recursive `QDirIterator` walk of every XDG application directory with no memoization — distinct from the cached `model_` path used elsewhere in the same service. Because `SidebarContent.qml`'s Loader destroys/recreates the tab component on every sidebar open and every tab switch, a single System-tab load performs 6 (the Terminal row can trigger a 7th via `categoryFallback`) redundant full filesystem scans + `.desktop` parses synchronously on the UI thread, and this re-fires on every `LauncherService.resultCountChanged` while the tab stays open.
- **Suggested Direction**: Cache `scanner_.scanForDefaultApps()`'s result in `LauncherService`/`DesktopEntryScanner`, keyed by the same invalidation signal as `model_`, so repeated calls in one "generation" reuse a single scan. Alternatively, have `SidebarSystem.qml` request the list once and hand filtered slices to each row.

### D-003 — Tab-switch animation race with no mutual-exclusivity guard (85/100)
- **Severity**: Medium (visible jitter) · **Effort**: Small
- **Location**: `RightSidebar.qml:85-92` (`onCurrentTabChanged`), `164-232` (`openAnimation`/`closeAnimation`/`tabSwitchAnimation`)
- **Rationale**: `openAnimation` (phase 2 animates `contentArea.width`) and `tabSwitchAnimation` (a bare `NumberAnimation` on the same property) are not mutually exclusive. `onActiveChanged` guards `openAnimation` against `tabSwitchAnimation`/`closeAnimation` — but `onCurrentTabChanged` only guards on `!root.active`, never on `openAnimation.running`. Sidebar tab buttons are always hit-testable during the open animation (no `enabled` gate tied to animation state), so clicking a different tab while the sidebar is still opening starts `tabSwitchAnimation` concurrently with `openAnimation`'s in-flight width animation — both writing `contentArea.width` every frame. This is the structural root cause underlying the lint-flagged `tabSwitchAnimation.to` imperative assignment (L-002).
- **Suggested Direction**: Add an explicit `if (openAnimation.running || closeAnimation.running) return` (or stop the conflicting animation first) before starting `tabSwitchAnimation`, mirroring the existing symmetric guard in `onActiveChanged`. Longer-term, this open/close/tab-switch flow is a strong candidate for a proper QML `State`/`Transition` design — Qt's state engine natively interrupts an in-flight `Transition` cleanly on a state change, eliminating this whole class of bug.

### D-004 — WiFi lock-icon Canvas has no repaint trigger for its color dependency (84/100)
- **Severity**: Low (visual staleness) · **Effort**: Trivial
- **Location**: `Popups/Network/WifiNetworkDelegate.qml:109-142` (Canvas `lockIcon`, `onPaint` at line 120)
- **Rationale**: The lock-icon Canvas reads the dynamic model role `connected` inside its imperative `onPaint` handler, but declares no `onConnectedChanged: lockIcon.requestPaint()`. `Canvas.onPaint` is imperative, not a binding — QML won't auto-repaint when a value it merely reads inside `onPaint` changes. `WifiNetworkModel::setNetworks()` performs in-place row updates via `dataChanged` (not model reset) when a network's `connected` state changes with identity preserved (the normal connect/disconnect/rescan path) — in that path the delegate survives and the lock glyph can go stale. Every other dynamic-dependency Canvas in this unit (`SidebarTabButton.qml`, `WeatherAqiGauge.qml`, `WeatherDetailsGrid.qml`, `WeatherWindWidget.qml`) wires an explicit repaint trigger; this is the one outlier.
- **Suggested Direction**: Add `onConnectedChanged: lockIcon.requestPaint()` to match the repaint-trigger convention already used elsewhere in the codebase.

### D-005 — `SidebarSessionBar.qml` is unreferenced dead code (85/100)
- **Severity**: Low (maintenance debt) · **Effort**: Trivial
- **Location**: `RightSidebar/Tabs/System/SidebarSessionBar.qml` (entire file)
- **Rationale**: Flagged by the Layout & Anchoring agent as a non-scored observation; independently verified by direct repo-wide grep (`grep -rln "SidebarSessionBar" --include="*.qml" --include="CMakeLists.txt"`) — zero references anywhere, including in `SidebarSystem.qml` (which implements its own session-action row inline rather than instantiating this component). The file is orphaned.
- **Suggested Direction**: Delete the file, or wire it into `SidebarSystem.qml` if the inline session-action row was meant to be replaced by it (check git history for intent before deleting — it may be a work-in-progress extraction).

### D-006 — `SidebarOverviewUpcoming.qml` structurally overloaded (80/100)
- **Severity**: Medium (maintainability) · **Effort**: Large
- **Location**: `RightSidebar/Tabs/Overview/SidebarOverviewUpcoming.qml` (whole file, 523 lines)
- **Rationale**: This file concentrates the unit's heaviest code-quality debt: ~45 of the 89 unit-wide JS-1 `var` hits, all 8 DEL-1 missing-required-property hits (see I-002), and five non-trivial JS functions at root scope — `getEventState()` alone is ~110 lines of nested date-arithmetic branching ("ending"/"soon"/"upcoming today"/"upcoming tomorrow") returning ad-hoc object literals with no shared type, invoked inline from a delegate property binding, with no corresponding QML unit test. This mixes presentation, event-state derivation, and time formatting in one file with no separation, making the date-branching logic (the kind that regresses silently) untestable in isolation.
- **Suggested Direction**: Extract `getEventState`, `formatDuration`, `formatAllDaySecondary`, and `getLocationIcon` into a dedicated, independently-testable helper — either a `pragma Singleton` JS module or a small C++ `QML_SINGLETON` (consistent with the existing `WeatherIconBridge`-style extraction) — so date-state derivation can be unit-tested without instantiating the sidebar, and the QML file shrinks to presentation-only bindings.

## Investigation Targets (human verification needed)

### I-001 — Synchronous blocking diagnostics probe on every System-tab load (75/100)
- **Location**: `SidebarSystem.qml:51` → `SessionIntegrationService::refresh()` (`libs/holonight-services/src/session-integration/SessionIntegrationService.cpp:146`)
- **Finding**: Re-runs `collectDiagnostics()` synchronously on every (re)creation (same lifecycle as D-002) — up to 7 blocking `QProcess` calls (`waitForStarted(1000)`/`waitForFinished(5000)`) plus filesystem mtime scans, all on the UI thread that also drives the topbar and other popups.
- **How to verify**: Confirm with a profiler/frame-timer whether opening the sidebar or switching to System tab produces a measurable UI stall; if so, cache with a short TTL or move probing to a worker thread.

### I-002 — Inconsistent `required property` usage across three delegates (70/100)
- **Location**: `Popups/Network/WifiNetworkDelegate.qml` (whole file), `SidebarOverviewNotifications.qml:86-92,176`
- **Finding**: These delegates rely on legacy implicit context-property injection (bare `ssid`/`connected`/`index`, or a non-required `property var notif`) instead of `required property` + `pragma ComponentBehavior: Bound`, unlike every other delegate in the unit (`AudioDeviceDelegate.qml`, `TrayMenuPopup.qml`, `WeatherHourlyStrip.qml`, etc.). All currently-accessed roles were verified to exist on their backing models, so this is not a live bug, but it forfeits compile-time role-name validation — a future rename/typo would silently resolve to `undefined` rather than fail at build time. (`SidebarSessionBar.qml` was also originally flagged here by the agent that produced this finding, but per D-005 that file is dead code and not a live risk.)
- **How to verify**: Add `required property`/`pragma ComponentBehavior: Bound` to these two files, run `task qml-lint`, confirm no regressions.

### I-003 — MultiEffect glow z-order inverted vs. the codebase's own documented convention (68/100)
- **Location**: `Popups/Tray/TrayMenuPopup.qml:86-109`, `Popups/Tooltip/TooltipPopup.qml:56-79`, `RightSidebar/Tabs/Overview/SidebarOverviewCalendar.qml:198-218`
- **Finding**: `StatusPopup.qml`, `ProfileButton.qml`, and `KeepAwakeAction.qml` correctly declare their ambient-glow `MultiEffect` *before* the shape it glows (with `StatusPopup.qml` carrying an explicit CLAUDE.md-citing comment confirming this is deliberate). These 3 files invert the order — the glow renders on top of the panel edge instead of behind it. Practical visual impact is likely small (opaque solid-color sources), but it's a real, repeated deviation from a documented in-repo rule.
- **How to verify**: Visually diff the tray menu, tooltip, and calendar "today" ring glow before/after reordering the `MultiEffect` block above its source Rectangle.

### I-004 — `BrightnessSlider` implements debounce, not the documented "throttle" (68/100)
- **Location**: `RightSidebar/Tabs/QuickSettings/BrightnessSlider.qml:9` (doc comment), `37-55` (`onValueChanging` + `Timer`)
- **Finding**: The header comment claims "throttles D-Bus writes to 100ms during drag," but `writeThrottle.restart()` on every `valueChanging` signal is a debounce (resets the countdown each event) — during a fast continuous drag, the timer may never fire until pointer release, so anything reading `BrightnessService.brightnessPercent` live (an OSD, another indicator) appears frozen mid-drag rather than updating progressively.
- **How to verify**: Drag the slider slowly across ~2 seconds while watching a second brightness-reading surface; confirm it updates only once, on release, rather than periodically.

### I-005 — `WeatherPopupContent.qml` has near-zero height margin with no overflow safety net (65/100)
- **Location**: `Popups/Weather/WeatherPopupContent.qml:36-378`
- **Finding**: Unlike its sibling popups (Audio/Battery/Network, which all use `ColumnLayout { anchors.fill: parent }`), this file positions a plain `Column` with manual `x`/`y`/`width`, no `clip: true`, no `Flickable` fallback. Summing its fixed row heights against the popup's actual available height (derived from `StatusPopupGeometry.cpp`'s `kWeatherHeight=920` minus panel chrome) leaves roughly 0-8px of slack. A larger configured title font size or a longer localized string could push content past the panel border with nothing to contain it.
- **How to verify**: Temporarily bump `AppearanceService.titleFontSize` or switch to a verbose locale and check whether Weather popup content visibly overflows its panel.

### I-006 — Notification-preview delegate lacks the discipline of its "rules" sibling (65/100)
- **Location**: `RightSidebar/Tabs/Notifications/SidebarNotifications.qml` vs `RightSidebar/Tabs/Overview/SidebarOverviewNotifications.qml`
- **Finding**: `SidebarNotifications.qml`'s per-app-rules `Repeater` (over `NotificationRuleModel`) correctly uses `required property`, verified against `NotificationRuleModel::roleNames()`. `SidebarOverviewNotifications.qml`'s live notification-preview `Repeater` (the actual notification-history rendering, which only exists in the Overview tab, not the Notifications tab) is the undisciplined one flagged in I-002 — the two nominally-related "notification list" surfaces have diverged in code quality.
- **How to verify**: Apply the I-002 fix to `SidebarOverviewNotifications.qml` and confirm the pattern now matches `SidebarNotifications.qml`'s rules Repeater.

### I-007 — `pragma ComponentBehavior: Bound` missing on several Overview/System delegates (68/100)
- **Location**: `SidebarOverviewCalendar.qml` (Repeaters at lines 160, 180), `SidebarOverviewNotifications.qml:86`, `SidebarOverviewUpcoming.qml:342`, `SidebarTabBar.qml:57`
- **Finding**: These files' Repeater delegates reach outer-scope ids (`root.*`, `dayHeaderRow`, `dayGrid`) without the pragma, unlike every file under `Popups/Weather/`, `Popups/Tray/`, and `Popups/Tooltip/` in this same unit, which all declare it. Without the pragma, outer-scope id references resolve through the runtime `QQmlContext` chain rather than a compiled direct binding — a small per-delegate lookup cost that compounds across the Overview tab's repeaters (35+ day-grid cells, N notification/event rows), all on the sidebar's most-frequently-opened tab.
- **How to verify**: Add the pragma and convert outer-id captures to `required property` params (as already done correctly in `WeatherHourlyStrip.qml`/`TrayMenuPopup.qml`'s delegate); confirm `task qml-lint` passes and no behavior regresses.

## Ruled Out (cross-agent contradictions and false positives, resolved)

- **`on_ActiveChanged`/`on_InactiveChanged` in `SidebarTabButton.qml:114-115`** — one agent (States & Structure) flagged these as misnamed dead handlers (naive expectation: `on_activeChanged`, lowercase). A second, independently-run agent (Bindings & Properties) empirically verified via a synthetic `qmllint` test that QML capitalizes the first *alphabetic* character after a leading underscore, making `on_ActiveChanged` correct — and confirmed live via screenshot that all six sidebar tab buttons render correctly. This matches the identical empirically-verified convention already established in U-08 (`AppWindowIcon.qml`'s `on_StrokeChanged`, confirmed via live `qml6` runtime test and consistent across 7 other files). **No finding recorded** — trusted the twice-independently-verified convention.
- **Unbounded `Repeater` in `SidebarOverviewUpcoming.qml`** — one agent (Performance & Quality) flagged the upcoming-events Repeater as unbounded, citing `CalendarEventModel::setEvents()` having no row cap. A second agent (ListView & Delegates) traced further upstream and found `CalendarSyncManager::refreshModel()` truncates events to `kModelEventLimit` *before* calling `setEvents()`. Independently verified directly against source: `CalendarSyncManager.h:118` declares `static constexpr int kModelEventLimit{3}`, and `CalendarSyncManager.cpp:200-203` applies `events.first(kModelEventLimit)` before `model_.setEvents(events)`. **The model is capped at 3 server-side; no finding recorded.** The QML's `visible: index < 3` guard is redundant-but-harmless, not evidence of a real unbounded-Repeater risk.
- **Phase-1 BND-2 cluster in `SidebarOverviewUpcoming.qml`** (9 lint hits, lines 107-169) — traced by the Bindings & Properties agent: every flagged line is a local JS `var` reassignment inside a plain helper function (`getEventState()`), not a QML property binding being destroyed. The function's return value feeds a genuinely live, correctly-recomputing `property var eventState` binding. Lint pattern-match limitation, not a bug.
- **`RightSidebar.qml:137` `root.currentTab` imperative assignment** (BND-2) — traced: `currentTab` was never a declarative binding to begin with (`property int currentTab: 0`, a literal default), so nothing is "destroyed." Correct-as-designed interactive state, analogous to `SwipeView.currentIndex`.
- **LDR-1** (`SidebarContent.qml:15,39`, `Loader.item` without status guard) — traced: line 15 uses optional chaining with a `?? 0` fallback (inert on null); line 39's `Connections { target: loader.item }` tolerates a null target and uses `ignoreUnknownSignals: true`. Both are already neutralized by QML's null-tolerant semantics; not a runtime risk.
- **DEL-1** (8 hits, `SidebarOverviewUpcoming.qml` `model.roleName`) — traced against `CalendarEventModel::roleNames()`; all 5 accessed roles (`startTime`, `endTime`, `isAllDay`, `title`, `location`) exist. Style-only (missing `required property` for qmlsc), not a functional defect — folded into I-002/D-006 context rather than reported separately.

## Lint Findings

- **L-001** BND-1 `property var` (17 hits) — `AudioDeviceDelegate.qml:12`, `AudioStreamDelegate.qml:12`, `TrayMenuPopup.qml:15/16/24`, `WeatherHourlyStrip.qml:51`, `SidebarTabBar.qml:61`, `SidebarOverviewCalendar.qml:23/24`, `SidebarOverviewNotifications.qml:10/92`, `SidebarOverviewUpcoming.qml:349`, `DefaultAppRow.qml:13/16/28`, `SidebarSessionBar.qml:38` (dead code, see D-005), `SidebarSystem.qml:170`. Mitigation: use typed properties for qmlsc compilation and type safety.
- **L-002** BND-2 imperative `=` destroying bindings (16 hits) — most were traced and resolved (see Ruled Out); remaining un-investigated instances are `WifiPasswordDialog.qml:61`, `TooltipPopup.qml:35/36`, `BrightnessSlider.qml:61` (ties to I-004). These look like standard, likely-intentional drag/show-hide patterns (consistent with the confirmed-intentional `AudioVolumeSlider.qml` pattern), but were not individually deep-traced — verify each is deliberate before treating as clean.
- **L-003** PRF-1 transparent `Rectangle` instead of `Item` (15 hits) — `AudioDeviceDelegate.qml:43`, `AudioMasterBar.qml:18`, `AudioTabSidebar.qml:54`, `StatusPopup.qml:58`, `TooltipPopup.qml:49`, `TrayMenuPopup.qml:79`, `RightSidebar.qml:37`, `SidebarTabBar.qml` (4×), `SidebarOverviewCalendar.qml` (3×).
- **L-004** JS-1 `var` instead of `let`/`const` (89 hits) — heavily concentrated in `SidebarOverviewUpcoming.qml` (~45, see D-006), `SidebarOverviewCalendar.qml`, `SidebarOverviewNotifications.qml`, and Weather popup files. Weather files' `var` usage was checked against the mandatory hot-path concern (T-009 item 3) and found to only run on periodic `WeatherService` refresh, not per-frame — cosmetic there. `SidebarOverviewUpcoming.qml`'s concentration is addressed by D-006.
- **L-005** JS-2 loose equality (17 hits) — `SidebarTabBar.qml` (5×), `SidebarOverviewUpcoming.qml` (5×), `AudioStreamDelegate.qml:65`, `DefaultAppRow.qml` (3×), `SidebarSystem.qml:204`, `SidebarNotifications.qml:143`.
- **L-006** IMG-1 missing `sourceSize` (7 hits) — `TooltipPopup.qml:110`, `SidebarTabBar.qml:36/170`, `SidebarOverviewUpcoming.qml:209/437/470`, `SidebarSessionBar.qml:48` (dead code, see D-005).
- **L-007** PRF-3 `clip: true` (10 hits) — mostly on `ListView`, acceptable per lint's own note; not individually re-verified.
- **L-008** PRF-6 `layer.enabled` (2 hits) — `SidebarTabBar.qml:177,189` — forces offscreen FBO rendering; not verified whether toggled on/off dynamically around effects/animations only.
- **L-009** LAY-3 four separate anchor edges instead of `anchors.fill: parent` (1 hit) — `SidebarOverview.qml:24`.
- **L-010** IMP-1 redundant `QtQuick.Window` import (1 hit) — `SidebarTabBar.qml:2`.

## Summary

| Category | Lint | Deep | Investigate | Total |
|----------|------|------|-------------|-------|
| Bindings & Properties | 33 | 1 | 0 | 34 |
| Layout & Anchoring | 1 | 0 | 1 | 2 |
| Component Loading & Lifecycle | 2 | 1 | 1 | 4 |
| ListView & Delegates | 8 | 0 | 2 | 10 |
| States & Structure | 0 | 1 | 2 | 3 |
| Performance & Quality | 106 | 3 | 1 | 110 |
| **Total** | **150** | **6** | **7** | **163** |

(152 ORD-1 + 59 STY-3 style-noise hits excluded from this table per established convention; 396 raw lint hits − 211 excluded − ~35 folded into Ruled Out/D-006 context = the above.)

Findings below confidence 60 are suppressed entirely. Two cross-agent contradictions were resolved via direct source verification (see Ruled Out).
