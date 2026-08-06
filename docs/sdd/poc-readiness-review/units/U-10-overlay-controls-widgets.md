# U-10 — Overlay Content & Shared QML Building Blocks

**Task**: T-010 · **Skill**: qt-qml-review · **Scope**: 28 files
- `apps/shell/qml/Launcher/` (7 files)
- `apps/shell/qml/Notifications/` (4 files: toast stack, items, action bars)
- `apps/shell/qml/Tray/` (4 files: tray item, menu item, dismiss overlay, section)
- `apps/shell/qml/Controls/` (8 files: icon components for audio, battery, network, session, signal-strength, bar, utility)
- `apps/shell/qml/Widgets/` (4 files: clock, time-to-event, stable digits, surface)
- `apps/shell/qml/Background/` (1 file)
- `apps/shell/qml/Utility/` (1 file: theme reload bridge)
- `apps/shell/qml/WeatherIcon/` (1 file: compositor)

## Prior Context / Mandatory-check verdicts

1. **DESIGN.md §3.10 deliberate multi-region merge** — CONFIRMED. This unit intentionally bundles eight otherwise-unrelated small QML regions into one 28-file pass rather than reviewing each fragment separately, per DESIGN.md rationale. No issues arose from the merged scope itself.
2. **`WeatherIconCompositor.qml` non-wiring into production topbar** — CONFIRMED, intentional-by-design. `apps/shell/qml/Topbar/WeatherWidget.qml:68` still renders via `WeatherService.iconPath(...)` (the SVG path). `WeatherIconCompositor` is instantiated only in the weather **popup** screens — `Popups/Weather/WeatherCurrentSection.qml:35`, `WeatherHourlyStrip.qml:96`, `WeatherDailyCards.qml:84` — matching project memory ("now wired into weather popup screens; topbar still SVG-only"). Not dead code — verified live call sites.
3. **Toast notification animations/state** — verified structurally sound. Entry uses `ToastItem.enterProgress` (opacity + `Translate`), independent of the `y` property that `ToastStack.qml`'s `Column.move` transition animates — see the STA-2 Ruled-Out entry below for the full trace. No entry/reflow animation conflicts found. One structural observation: neither `ToastStack.qml` nor `TraySection.qml` defines a `remove:` transition, so dismissed items disappear instantly with no exit animation — only sibling reflow is animated. Given `ToastItem.qml`'s own header comment describes only an entry animation, this reads as a deliberate scope decision, not an oversight (flagged for awareness, not filed as a defect).
4. **Control icon recoloring/symbolism patterns** (CLAUDE.md's MultiEffect-colorization gotcha) — verified per-file. **All 8 `Controls/*.qml` files are Canvas-based and draw with `HoloniightPalette.*` stroke/fill colors directly** — none loads `image://icon/` or any themed/symbolic pixmap, so the MultiEffect-recolor pattern this gotcha describes does not apply anywhere in this directory:
   - `AudioIcon.qml` — Canvas, palette-direct. N/A.
   - `BarIcon.qml` — no own Canvas/Image; a `Loader` dispatching by `name` to the other Canvas icons, plus a `MultiEffect` *glow* (not a recolor). N/A.
   - `BatteryIcon.qml` — Canvas, palette-direct. N/A.
   - `HudFrame.qml` — Canvas, palette-direct. N/A.
   - `NetworkIcon.qml` — Canvas, palette-direct. N/A.
   - `SessionIcon.qml` — Canvas, palette-direct. N/A.
   - `SignalStrengthIcon.qml` — Canvas, palette-direct. N/A.
   - `UtilityIcon.qml` — Canvas, palette-direct. N/A.
   The `MultiEffect` z-order convention (glow declared before the item it shadows) is followed correctly everywhere it's used for actual glow (`BarIcon.qml`, `TrayItem.qml`, `ToastItem.qml`, `ClockWidget.qml`, `TimeToEventWidget.qml`).
5. **Clock/TimeToEvent widgets consume correct services** — verified, and the finding is stronger than "correct": **neither widget binds to any service directly at all.** `ClockWidget.qml` and `TimeToEventWidget.qml` take only `required property string` text fields (`timeText`/`secondsText`/`dateText`, `titleText`/`remainingText`/`deadlineLabel`) computed upstream in C++ (`WidgetManager`, per both files' header comments) and style them using `AppearanceService` (fonts/sizes) and `HoloniightPalette` (colors) only. This is a clean separation — no drift, no direct calendar/idle-service coupling to cross-check against U-03/U-07.
6. **Background/ThemeReloadBridge state-management gaps** — none found. `Background.qml`'s crossfade state machine (`ready`/`frontIsA` flags) correctly guards the documented `setInitialProperties`/`onXChanged` double-trigger race. `ThemeReloadBridge.qml` is a trivial, correctly-implemented singleton-reload bridge with a fixed `Connections { target: ThemeService }` — no dynamic-target null-handling concern.

## Tool Sign-off

**Phase 1 (Python lint)**: 140 raw hits across 28 files. Breakdown: ORD-1 (attribute ordering) 61, JS-2 (loose equality) 18, STY-3 (dot-notation grouping) 16, JS-1 (`var` vs `let`/`const`) 15, PRF-1 (transparent Rectangle) 9, BND-1 (`property var`) 7, PRF-3 (`clip: true`) 4, IMG-1 (missing `sourceSize`) 4, BND-2 (imperative `=` destroying binding) 3, STA-2 (Transition without from/to) 1, PRF-2 (`opacity: 0` without `visible: false`) 1, IMP-1 (redundant import) 1. ORD-1 (61) and STY-3 (16) — 77 hits total — are excluded as pure style noise per the established convention from prior units; 63 substantive hits itemized below.

**Phase 1b (qmllint)**: ran clean via the project's own import-path configuration (`tests/qmllint`, `build/apps/shell`, `build/apps/settings`, `build/`, Qt prefix paths) — **0 warnings across 30 files** (type resolution sound; `--unqualified`/`--unused-imports` checks disabled per project convention, consistent with prior units).

**Phase 2 (six parallel deep-analysis agents)**: all six completed; findings consolidated below with cross-agent corroboration noted where two agents independently reached the same finding.

## Confirmed Findings

### D-001 — Keyboard-selected launcher action rows have no visual highlight, and Enter launches the wrong command (90/100)
- **Severity**: High · **Effort**: Medium
- **Location**: `Launcher/LauncherActionRow.qml` (whole file); `Launcher/Launcher.qml:298-306`; `libs/holonight-services/src/launcher/LauncherService.cpp:376-390,393-406,421-436`
- **Rationale**: `LauncherModel` interleaves action rows with app rows in one flat result list (`LauncherModel::rebuildSearchResults`), and `LauncherService::moveSelection` walks `selected_index_` across the *entire* result count with no row-type filtering — keyboard Up/Down can land on an action row. `LauncherActionRow.qml` declares no `selected` property and has no highlight binding analogous to `LauncherResultRow`'s `selected: searchDelegate.index === LauncherService.selectedIndex` — so there is zero visual indication when an action is keyboard-selected (no border, no "↵ Enter" hint). Worse, pressing Enter routes through `LauncherService::launchSelected() → launch(selected_index_)`, which always launches the **parent app's default command** via `model_.entryAt(index)` — never the action's `exec`. The action-specific dispatch (`launchAction(entry_index, action_index)`) is wired only to `LauncherActionRow.onActivated` (mouse click), never to the keyboard/Enter path. This is a silent wrong-app-launch bug with no visual cue, in the shell's most latency-/attention-sensitive UI.
- **Suggested Direction**: Give `LauncherActionRow` the same `required property bool selected` highlight treatment as `LauncherResultRow`. On the C++ side, have `launchSelected()` (or the Enter-key path) check `LauncherModel`'s `IsActionRole`/`ActionIndexRole` at `selected_index_` and dispatch to `launchAction()` instead of `launch()` when the selected row is an action.

### D-002 — `pragma ComponentBehavior: Bound` inconsistently applied across delegate-bearing files (85/100)
- **Severity**: Low-Medium · **Effort**: Small
- **Location**: `Launcher/Launcher.qml`, `Launcher/LauncherRightPanelBrowse.qml`, `Launcher/LauncherRightPanelSearch.qml`, `Notifications/ToastActionBar.qml`, `Widgets/StableDigitsText.qml`
- **Rationale**: Independently flagged by two agents (Performance & Code Quality at 85/100; Bindings & Properties at 69/100 before cross-referencing). None of these five files declare the pragma even though each has a `Repeater`/`ListView` delegate reaching an outer-scope id (e.g. `LauncherRightPanelBrowse.qml`'s category delegate: `readonly property bool isActive: root.activeCategory === catDelegate.modelData`). Six sibling files in the same unit — `ToastItem.qml`, `ToastStack.qml`, `TrayItem.qml`, `TrayMenuItem.qml`, `TraySection.qml`, `WeatherIconCompositor.qml` — all declare it. This is a real, verified inconsistency against the codebase's own established convention, not a hypothetical: nothing is currently broken (creation context matches lexical scope everywhere), but the five outlier files miss the compile-time/qmllint verification the rest of the module already relies on.
- **Suggested Direction**: Add `pragma ComponentBehavior: Bound` as line 1 of each of the five files, then run `task qml-lint` to catch any newly-surfaced unqualified-access issues before committing, for consistency with the rest of the module.

### D-003 — Dead `topMargin` on a `verticalCenter`-anchored Text (no top anchor set) (85/100)
- **Severity**: Low · **Effort**: Trivial
- **Location**: `Launcher/Launcher.qml:249`
- **Rationale**: The search-mode `ListView`'s `section.delegate` sets `anchors { left; right; verticalCenter: parent.verticalCenter; topMargin: 8 }`. `topMargin` only takes effect with a `top` anchor present — here only `verticalCenter` is set, so the 8px offset is silently discarded. The sibling `header` block a few lines above (~line 231) uses the correct `top` + `topMargin` combination, suggesting the two were meant to look consistent but only one actually applies its margin.
- **Suggested Direction**: Anchor `top: parent.top` instead of `verticalCenter` if an 8px top inset was intended, or delete the inert `topMargin: 8` if centering is correct as-is.

### D-004 — Search-panel action delegate relies on implicit `parent` instead of a named id (82/100)
- **Severity**: Medium · **Effort**: Trivial
- **Location**: `Launcher/LauncherRightPanelSearch.qml:238,249`
- **Rationale**: The `Repeater { model: LauncherService.selectedEntryActions }` delegate (lines 218-251) declares `required property var modelData`/`required property int index` on the anonymous delegate root, but the root has no `id:`. Two descendants reach back into those required properties via implicit visual `parent`: `text: parent.modelData.name || ""` (line 238) and `onClicked: LauncherService.launchAction(LauncherService.selectedIndex, parent.index)` (line 249). This works only because `Text`/`MouseArea` happen to be direct children of the delegate root — any future refactor inserting a wrapper `Item`/`Row` would silently break both, producing `undefined`/`TypeError` or a bad action index. A second agent independently spotted the same pattern but scored it below the reporting threshold since it isn't broken *today* — flagged here at the higher-confidence assessment since the fragility is real and the fix is trivial.
- **Suggested Direction**: Give the delegate root an explicit id (e.g. `id: actionDelegate`) and change both references to `actionDelegate.modelData`/`actionDelegate.index`. Purely mechanical, no behavior change.

### D-005 — `BarIcon.qml` (reusable icon building block) hardcodes `width`/`height` instead of exposing implicit sizing (82/100)
- **Severity**: Low · **Effort**: Small
- **Location**: `Controls/BarIcon.qml:20-21`
- **Rationale**: `BarIcon` is the shared icon wrapper consumed across the topbar/popups (8 call sites: `AudioWidget.qml`, `NetworkWidget.qml`, `BatteryIndicator.qml`, `NotificationsWidget.qml`, `NetworkCurrentCard.qml`, `WifiNetworkDelegate.qml`, `NetworkToggleRow.qml`, `TooltipPopup.qml`). It declares plain `width: 24`/`height: 24` rather than `implicitWidth`/`implicitHeight`. Qt Quick Layouts fall back to implicit sizing (not explicit `width`/`height`) absent a `Layout.preferred*` override — a future `Layout`-based consumer that forgets to set explicit sizing would collapse toward 0. This is not hypothetical: `NetworkToggleRow.qml:15-16` already has to explicitly set `Layout.preferredWidth/Height: 32` to work around exactly this gap; every other call site also re-specifies size via anchors or explicit dimensions.
- **Suggested Direction**: Rename to `implicitWidth: 24`/`implicitHeight: 24` so the component is self-sizing for both anchor- and Layout-based consumers, matching its role as a reusable building block. `SignalStrengthIcon.qml` has the identical shape (`width: 18`/`height: 16`) but is lower priority since both its call sites already override via anchors.

### D-006 — Search-mode delegate always instantiates both row types, doubling per-row construction and icon-request cost (80/100)
- **Severity**: Medium (performance) · **Effort**: Medium
- **Location**: `Launcher/Launcher.qml:260-307`
- **Rationale**: The search-mode delegate unconditionally instantiates both `LauncherResultRow { id: appRow }` and `LauncherActionRow { id: actionRow }` as static children for every row, gating only `visible` on `searchDelegate.isAction`. Since `rebuildSearchResults()` appends every matched app's actions into the same flat list, every single row — app or action — pays full construction/layout/binding cost for *both* component trees. `LauncherResultRow`'s async `Image` (`source: "image://icon/" + ...`) fires regardless of the enclosing item's `visible` state, so every action row still triggers an icon-provider request it never displays. This runs on the shell's most latency-sensitive list.
- **Suggested Direction**: Restructure to a `Loader { active: ...; sourceComponent: ... }` pair (mirroring `Widgets/WidgetSurface.qml`'s existing per-type Loader-dispatch pattern), or migrate to `DelegateChooser`/`DelegateChoice` keyed on `isAction`, so only the active row type's bindings/Image are ever created.

### D-007 — `TrayItem.qml` opacity fade-out on the "Passive" status transition is structurally unreachable (80/100)
- **Severity**: Low (visual-polish gap) · **Effort**: Small
- **Location**: `Tray/TrayItem.qml:42,70-75`
- **Rationale**: `visible: root.status !== "Passive" && root.overflowVisible` and the "hide" branch of `onStatusChanged: root.opacity = root.status !== "Passive" ? 1.0 : 0.0` are gated by the *same* boolean condition. The instant `status` becomes `"Passive"`, `visible` flips `false` in the same tick the 100ms opacity fade-out begins — Qt Quick does not paint invisible items, so the `Behavior`-driven fade animation runs as a timer but produces zero visible frames; the icon pops off-screen instantly instead of fading. Only the entrance direction (already `visible: true` before the fade-in starts) is ever actually rendered.
- **Suggested Direction**: Decouple `visible` from the fade — e.g. drive `visible` from a separate flag set only once the fade-out's `NumberAnimation.onStopped` completes, or drop `status` from the `visible` condition and rely solely on `opacity` (with `overflowVisible` still gating hit-testing via `enabled`) so the fade is actually visible. Low risk since it only affects the rarely-hit "SNI item transitions to Passive without being removed from the model" case.

## Investigation Targets (human verification needed)

### I-001 — Icon `Image` elements outside the established fallback pattern have no error handling (72/100)
- **Location**: `Launcher/LauncherRightPanelBrowse.qml:64`, `Launcher/LauncherRightPanelSearch.qml:174`, `Tray/TrayMenuItem.qml:75`
- **Finding**: These three `image://icon/`-sourced `Image` elements have no `onStatusChanged`/`Image.status` handling — if the icon name doesn't resolve, the `Image` silently renders nothing with no fallback glyph. This is inconsistent with the pattern already established twice elsewhere in the codebase for the same provider: `LauncherResultRow.qml:53-73` tracks `usedFallback` and swaps to `application-x-executable` on `Image.Error`, and the shared `qml/HoloNight/Components/ExternalIcon.qml` (consumed by `ToastItem.qml`/`TrayItem.qml`) does the same via `fallbackIconName`.
- **How to verify**: Force a stale/missing icon name (e.g. temporarily rename a `.desktop` file's `Icon=` value) and confirm whether the affected row/menu-item shows a blank slot vs. a fallback glyph in the other two components.

### I-002 — `TraySection.qml`'s per-delegate `_roles` re-reads all 12 model roles on every `TrayModel.revision` bump (68/100)
- **Location**: `Tray/TraySection.qml:100-118`
- **Finding**: Each `TrayItem` delegate computes a `_roles` object via 12 `TrayModel.data()` calls, deliberately re-triggered on every `TrayModel.revision` change (a documented, intentional workaround for `Repeater`'s deep-equal early-exit). The tradeoff is explicit in the code's own comment, but the consequence is real: any single tray-item mutation forces every visible item to redo all 12 role reads, not just the changed one — O(items × 12) rather than O(1) per revision bump.
- **How to verify**: Profile tray-icon update latency with a larger StatusNotifierItem count (many concurrent tray apps); if it shows measurable jank, consider a per-row "generation" role from `TrayModel` instead of the global `revision` counter.

### I-003 — `MouseArea` used for hover+click where sibling components already use `HoverHandler`+`TapHandler` (65/100)
- **Location**: `Launcher/LauncherResultRow.qml:157-162`, `Launcher/LauncherActionRow.qml:68-73`, `Tray/TrayMenuItem.qml:129-139`
- **Finding**: These use a single `MouseArea { hoverEnabled: true }` for simple hover+tap, while `Notifications/ToastActionButton.qml` and `Tray/TrayItem.qml` (elsewhere in this same unit) use the modern `HoverHandler`+`TapHandler` pair instead — `TrayItem.qml` specifically keeps `MouseArea` only where structurally necessary (multi-button dispatch), suggesting the project's own convention favors pointer handlers for simple cases.
- **How to verify**: Swap the three flagged files to `HoverHandler`+`TapHandler` where only single-button tap+hover is needed; confirm no regression and improved multi-touch parity.

### I-004 — `StableDigitsText.qml`'s per-glyph delegate omits `textFormat: Text.PlainText` despite high update frequency (65/100)
- **Location**: `Widgets/StableDigitsText.qml:106-118`
- **Finding**: Consumed by `ClockWidget.qml` (seconds-resolution ticking) and `TimeToEventWidget.qml` (per-second countdown) to render each character as an individual `Text` item. None sets `textFormat: Text.PlainText`; the default `Text.AutoText` runs a markup-detection scan on every text change — a small but avoidable per-tick cost across 8+ delegate instances on a hot, frequently-ticking path.
- **How to verify**: Add `textFormat: Text.PlainText` (and consider `font.preferShaping: false`, since the character set is limited to digits/colon) and confirm no visual regression.

### I-005 — Stale width comment vs. actual `Layout.preferredWidth` on Launcher's right panel (65/100)
- **Location**: `Launcher/Launcher.qml:327-329`
- **Finding**: A comment reads `// Right column — mode-switched panel (fixed 240px)` but the `Item` below actually sets `Layout.preferredWidth: 256`. Not a rendering bug — the RowLayout consistently honors 256 — but a stale comment here could mislead a future edit that "fixes" the width back to 240 based on the comment rather than the design spec.
- **How to verify**: Reconcile against the HoloNight design reference (`assets/dont-commit/`) for the launcher's intended right-panel width; update whichever of comment/code is wrong.

### I-006 — Several `MultiEffect`/`Rectangle` ids declared but never referenced (dead ids) (78/100)
- **Location**: `Tray/TrayItem.qml:77,104,144`, `Tray/TraySection.qml:62,86`, `Notifications/ToastItem.qml:268`
- **Finding**: Each id appears exactly once (its own declaration) with no read from a binding, `Connections`, `Behavior`, or animation `target:` elsewhere in the file — harmless but likely refactor debris or speculative "just in case" naming.
- **How to verify**: Confirm via grep that none is consumed anywhere (already checked within-file; a repo-wide check would catch any external QML `Connections` targeting these by id, though cross-file id references are not legal QML so this is effectively conclusive already). Delete if truly unused, or add a comment if intentionally reserved for a planned animation target.

### I-007 — `WeatherIconCompositor.qml` carries undocumented properties vs. project memory (70/100)
- **Location**: `WeatherIcon/WeatherIconCompositor.qml:11-14`
- **Finding**: The component now declares `conditionDescription`, `windSpeedKmh`, and an explicit `moonPhase` property, none of which are mentioned in the project's `WeatherIconCompositor` memory doc — which specifically states "there is no separate `moonPhase` property; it's derived internally." The component itself is structurally sound (verified separately — see Ruled Out); this is purely a docs-vs-code drift flag for whoever next updates that memory file.
- **How to verify**: Compare against `docs/sdd/weather-icon-compositor/SPEC.md`/`DESIGN.md` and the C++ `WeatherIconBridge::layersFor` signature to determine whether the memory doc or the component is now stale, and update the memory file accordingly.

## Ruled Out (cross-agent verifications, resolved)

- **`ToastStack.qml:49` STA-2 lint hit ("Transition without from/to")** — traced and confirmed harmless (85/100 verification). The Transition is assigned to `Column.move` (a positioner attached-transition), which only ever fires for the `y` property of children repositioned by sibling add/remove/resize — it cannot be triggered by unrelated state changes, so the generic "fires on every state change" risk the lint rule warns about does not apply to positioner `move` transitions. `ToastItem.qml`'s own entry animation drives opacity + `Translate`, disjoint from `y`, so no double-animation conflict exists. **No finding recorded.**
- **`Controls/BarIcon.qml`'s 5 `Binding {}` elements with no explicit `restoreMode`** — traced: all 5 target `iconLoader.item`, gated by `when: iconLoader.item !== null` (plus an `_iconSource` guard for `charging`). Because `iconLoader.source` derives 1:1 from `_iconSource`, `item` is destroyed/recreated in lockstep with anything that would flip the `when` guard — there is no window where a live target survives a `when` true→false transition, so Qt6's `RestoreBindingOrValue` default has no observable effect. **Not a defect.**
- **Deprecated `Connections` handler syntax** — all `Connections` blocks in scope (`Launcher.qml:44`, `LauncherRightPanelBrowse.qml:223`, `ThemeReloadBridge.qml:14`) already use modern `function onFoo()` syntax. **No finding.**
- **`Qt5Compat.GraphicalEffects`/`QtGraphicalEffects` regression check** — zero occurrences anywhere in scope; all glow effects consistently use `QtQuick.Effects.MultiEffect`, confirming no regression of the documented Glow-migration fix. **No finding.**
- **`Component.createObject()`/`Qt.createComponent` dynamic-object leak risk** — zero occurrences anywhere in the 28-file scope. **N/A, not a risk in this unit.**
- **Pooled-delegate `onPooled: visible = false` pattern** — `reuseItems: true` does not appear anywhere in scope (zero grep matches across all `ListView`/`Repeater` usage). **N/A — no pooled delegates exist to check.**
- **`Loader` `source`/`sourceComponent` mixing anti-pattern** — `Widgets/WidgetSurface.qml` (the one file structurally resembling this pattern, with two sibling type-gated Loaders) uses `sourceComponent` exclusively on both instances, never mixing in `source`. Launcher's two-panel switch (`LauncherRightPanelBrowse`/`LauncherRightPanelSearch`) turned out to be two permanently-instantiated siblings toggled by `visible`, not Loader-dispatched at all, so the anti-pattern doesn't apply there by construction. **No finding.**
- **C++/QML object-ownership boundary** — traced `LauncherService`, `RecentAppsTracker`, `TrayModel`, `NotificationService` invokables consumed by this unit's QML: all return value types or stable `CppOwnership`-parented pointers; `ShellApplication::reg()` explicitly sets `CppOwnership` before every singleton registration. **No parentless-object leak found.**
- **Multi-cycle binding loops, `Qt.binding()` closures, property-alias chains** — none found anywhere in scope (zero `property alias` declarations; the one `for (var …)` loop in `TraySection.qml` builds a plain array with no captured closure).
- **`setInitialProperties`/`onXChanged` race guard** (CLAUDE.md gotcha) — `Background.qml`'s `imagePath` is the only true `setInitialProperties` root property in scope, and it already implements the documented `ready`-flag guard correctly.
- **Per-branch required-property Loader dispatch** (CLAUDE.md gotcha) — `WidgetSurface.qml` implements this pattern correctly per its own explanatory comment: type-specific fields are plain (non-`required`) properties, so the inactive Loader branch never fires "Required property … was not initialized."

## Lint Findings

- **BND-1** `property var` (7 hits) — `LauncherRightPanelBrowse.qml:39`, `LauncherRightPanelSearch.qml:219`, `ToastActionBar.qml:10,17`, `ToastItem.qml:16`, `TraySection.qml:89`, `WeatherIconCompositor.qml:14`. Mitigation: typed properties for qmlsc compilation and type safety.
- **BND-2** imperative `=` destroying bindings (3 hits) — `LauncherRightPanelBrowse.qml:226` (`recentRepeater.model`), `LauncherSearchField.qml:18` (`input.text`), `TrayItem.qml:58` (`root.opacity`). All three read as standard, likely-intentional recreate/show-hide patterns consistent with confirmed-intentional analogues in prior units, but were not individually deep-traced — verify each is deliberate before treating as clean.
- **PRF-1** transparent `Rectangle` instead of `Item` (9 hits) — `Launcher.qml:104`, `LauncherResultRow.qml:32,132`, `LauncherRightPanelBrowse.qml:50,152`, `LauncherRightPanelSearch.qml:53,109,228`, `ToastStack.qml:32`.
- **PRF-2** `opacity: 0` without animation (1 hit) — `Background.qml:60`; prefer `visible: false` to skip rendering/input-handling entirely. Note: this is the base `Rectangle` fallback layer behind the crossfading wallpaper images, always present regardless of opacity — verify this doesn't need to stay `opacity`-driven for some z-order/compositing reason before applying the fix.
- **PRF-3** `clip: true` (4 hits) — `Launcher.qml:181,219`, `LauncherRightPanelBrowse.qml:14`, `LauncherSearchField.qml:54`; all on `ListView`s, acceptable per the lint's own note.
- **IMG-1** missing `sourceSize` (4 hits) — `LauncherResultRow.qml:53`, `LauncherRightPanelBrowse.qml:64`, `LauncherRightPanelSearch.qml:174`, `TrayMenuItem.qml:75`. Three of these four overlap with I-001 (missing error handling) above — same locations, distinct concerns.
- **STA-2** `Transition` without from/to (1 hit) — `ToastStack.qml:49`; confirmed harmless, see Ruled Out.
- **IMP-1** redundant `QtQuick.Window` import (1 hit) — `Background.qml:2` (Qt6 auto-includes `QtQuick.Window` types when `QtQuick` is imported).
- **JS-1** `var` instead of `let`/`const` (15 hits) — concentrated in `Tray/TrayItem.qml` (3), `Tray/TraySection.qml` (9, mostly the documented `_roles` role-lookup block), `WeatherIcon/WeatherIconCompositor.qml` (1), plus 2 more scattered. Not individually deep-traced beyond the TraySection block already covered by I-002.
- **JS-2** loose equality (18 hits) — `Background.qml`, `Controls/BarIcon.qml` (5), `Launcher/LauncherSearchField.qml`, `Tray/TrayItem.qml` (4), `Tray/TrayMenuItem.qml` (6). Not individually deep-traced; standard style cleanup.
- **ORD-1** (61 hits) / **STY-3** (16 hits) — excluded as pure style noise per established convention.

## Summary

| Category | Lint | Deep | Investigate | Total |
|----------|------|------|-------------|-------|
| Bindings & Properties | 10 | 2 | 0 | 12 |
| Layout & Anchoring | 0 | 1 | 1 | 2 |
| Component Loading & Lifecycle | 4 | 0 | 1 | 5 |
| ListView & Delegates | 0 | 2 | 0 | 2 |
| States & Structure | 1 | 1 | 0 | 2 |
| Performance & Quality | 48 | 1 | 4 | 53 |
| **Total** | **63** | **7** | **7** | **77** |

(61 ORD-1 + 16 STY-3 style-noise hits excluded from this table per established convention; 140 raw lint hits − 77 excluded = 63 substantive lint hits shown above.)

Findings below confidence 60 are suppressed entirely. Two cross-agent corroborations strengthened the confidence of D-002 (pragma) and D-004 (parent.modelData); one cross-agent verification (STA-2) resolved a lint flag as a non-issue.
