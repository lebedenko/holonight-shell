# U-08 — Topbar QML

**Scope**: `apps/shell/qml/Topbar/` (27 files) — bar shell, workspace strip, active-window
section, weather/status widgets, tooltip/popup trigger infrastructure.
**Skill**: `qt-qml-review`
**Task**: T-008 (`docs/sdd/poc-readiness-review/TASKS.md`)

## Prior Context

Consulted `docs/sdd/workspace-indicator-redesign/SPEC.md` + `DESIGN.md` (exists, relevant to
`WorkspaceSection.qml`/`WorkspacePillStrip.qml`/`WorkspacePill.qml`/`WorkspaceEdgeArrow.qml`).
`docs/sdd/topbar-redesign/` and `docs/sdd/notification-ui-redesign/` do not exist under those
names — no other SDD docs apply to this unit's scope.

### Mandatory-check verdicts

1. **`barMonitorName` required-property propagation chain** — **PASS**. Traced end-to-end from
   `TopBar.qml`'s `required property string barMonitorName` through every section
   (`LogoSection`, `WorkspaceSection` incl. both `Repeater` delegate chains, `ActiveWindowSection`,
   `WeatherSection`→`WeatherWidget`, `StatusesSection`→{`NetworkWidget`, `AudioWidget`,
   `BatteryWidget`, `NotificationsWidget`, `KeyboardLayoutWidget`}, `ClockSection`) to every leaf
   instantiating `BarTooltipArea`/`StatusPopupTriggerArea`. No leaf found with a missing or
   hardcoded value; no unsatisfied `required property`. One latent inconsistency surfaced — see
   D-006 below.
2. **`MultiEffect` z-order** — 7 instances found and checked individually against sibling elements.
   6 of 7 follow the codebase's "glow declared first" convention correctly; 1 violates it — see
   D-003.
3. **Widget state-machine correctness** (`AudioWidget`, `BatteryWidget`, `BatteryIndicator`,
   `NetworkWidget`, `KeyboardLayoutWidget`, `WeatherWidget`) — 4 of 6 have no `States` at all (pure
   computed `readonly property` chains, no loop risk). `BatteryIndicator`'s single `State` and its
   critical-pulse `SequentialAnimation` are time-disjoint (correct, not a loop) with a minor cosmetic
   snap on exit — see I-005. `WeatherWidget`'s `isDay` derivation has a genuine staleness bug — see
   D-001.
4. **Image-loading path** (`WeatherWidget.qml`'s icon) — no defect. Goes through the shared
   `HnIcon` component (external `holonight-qt` repo), correctly gated by `root.ready` (zero-width
   until `WeatherService.hasData`), `sourceSize` correctly set via `HnIcon`'s `size` prop. Missing
   `Image.status`/error handling exists but is inside `HnIcon`, outside this repo's ownership, and
   is a documented accepted trade-off per `docs/sdd/exact-hn-icon-adoption/DESIGN.md`.

## Tool Sign-off

Lint (`qt_qml_lint.py`) produced 46 hits across 27 files. Excluded from itemized reporting as pure
style/mechanical noise, not itemized individually:
- **25× ORD-1** (attribute declaration ordering) — zero runtime effect, would be pure line-shuffling
  churn to list one-by-one; a single pass with an ordering-aware formatter (if one exists for QML)
  would resolve all of these at once.
- **4× STY-3** (dot-notation instead of group notation for `font.*`/`anchors.*`).
- **3× STY-6** (id `_style` should be lowerCamelCase — same underscore-prefix convention verified
  legitimate elsewhere in this review; the linter's camelCase rule doesn't special-case it).
- **1× STY-1** (`WeatherIconRoles.qml` missing `id: root`) — this file is a pure data/enum
  singleton with no visual root; the rule doesn't apply.

Six substantive lint hits itemized below (L-001–L-006). No `qmllint` run (not confirmed available
on this system, consistent with prior units).

## Confirmed Findings (>80/100)

#### [D-001] `WeatherWidget.qml` day/night icon goes stale between weather refreshes
- **Severity**: Medium | **Effort**: Low
- **Location**: `apps/shell/qml/Topbar/WeatherWidget.qml:18-24` (also affects the shared
  `isDay` derivation consumed from `WeatherSection.qml:18-24`)
- **Confidence**: 88/100 — corroborated by two independently-spawned agents (States/Transitions
  and Bindings & Properties both flagged the same underlying issue from different angles; merged
  here at the higher score)
- **Rationale**: `isDay` is computed as `Date.now() / 1000 >= WeatherService.current.sunrise && ...
  < WeatherService.current.sunset` inside a property binding. `Date.now()` is a plain JS call, not
  a QML-reactive property read, so it registers no dependency — the binding only re-evaluates when
  `root.ready`/`WeatherService.current` change (i.e., on the next weather-data refresh), not as
  wall-clock time actually crosses sunrise/sunset. `ClockSection.qml` in the same directory shows
  the correct pattern: a 1s `Timer` reassigns a tracked property to force re-evaluation.
- **Suggested Direction**: add a low-frequency `Timer` (60s is plenty) that reassigns a trivial
  property to force `isDay` to re-evaluate, mirroring `ClockSection`'s approach.

#### [D-002] `ClockSection.qml` — anchors and explicit width compete on the same Column, width silently loses
- **Severity**: Low | **Effort**: Low
- **Location**: `apps/shell/qml/Topbar/ClockSection.qml:40-49` (`clockColumn`)
- **Confidence**: 83/100
- **Rationale**: `clockColumn` has both `anchors { left; right }` and a competing explicit `width:`
  binding. Empirically verified (live Qt 6.11.1 reproduction by the reviewing agent) that the
  anchor-derived width silently wins with the explicit width having zero runtime effect — no
  diagnostic is emitted. Currently harmless (~16px of dead space), but a landmine: if
  `Layout.fillWidth` or margins change later, the explicit width's author-intended sizing silently
  never applies.
- **Suggested Direction**: remove the dead `width:` binding, or remove the conflicting anchors and
  let the explicit width govern — pick one sizing mechanism, not both.

#### [D-003] `WorkspaceEdgeArrow.qml` — `MultiEffect` glow declared after its source, breaking the file's own z-order convention
- **Severity**: Low-Medium | **Effort**: Low
- **Location**: `apps/shell/qml/Topbar/WorkspaceEdgeArrow.qml:51` (`glowEffect`, source: `chevron`
  at line 29)
- **Confidence**: 82/100
- **Rationale**: Every other glow-on-hover/urgent `MultiEffect` in this directory (`AudioWidget`,
  `BatteryWidget`, `NetworkWidget`, `NotificationsWidget`, `WorkspacePill`, `SpecialWorkspaceDot` —
  6 of 7 instances) is declared *before* the item it glows, per CLAUDE.md's documented rule. Here
  it's declared *after* `chevron`, so the effect's texture-sampled copy paints on top of the
  natively-rendered `Shape`. Mostly self-overlapping today (same geometry), but inconsistent with
  the established convention and fragile — if `blurEnabled`/shadow offsets are ever tuned, the
  crisp chevron would end up visually buried under the blurred copy.
- **Suggested Direction**: move the `MultiEffect { id: glowEffect ... }` block above the
  `Shape { id: chevron ... }` declaration to match every sibling widget.

#### [D-004] `StatusPopupTriggerArea.qml` and 4 other widgets — click-only `MouseArea` should be `TapHandler`
- **Severity**: Low | **Effort**: Low
- **Location**: `apps/shell/qml/Topbar/StatusPopupTriggerArea.qml:20-28`
- **Confidence**: 83/100
- **Rationale**: The `MouseArea` only handles `onClicked` — no hover/drag/press-visual state is
  read from it. This component is reused by nearly every status widget (Network, Audio, Battery,
  Keyboard, Weather), so the pattern has broad reach. `NotificationsWidget.qml` in the same
  directory already demonstrates the idiomatic replacement (`TapHandler { onTapped: ... }`), making
  this both an inconsistency and a straightforward modernization — `TapHandler` avoids
  `MouseArea`'s implicit exclusive-grab semantics.
- **Suggested Direction**: replace with `TapHandler`, matching `NotificationsWidget.qml`'s existing
  pattern.

#### [D-005] `BarSection.qml` — wheel-only `MouseArea` should be `WheelHandler`
- **Severity**: Low | **Effort**: Low
- **Location**: `apps/shell/qml/Topbar/BarSection.qml:10-16`
- **Confidence**: 82/100
- **Rationale**: `MouseArea { acceptedButtons: Qt.NoButton; onWheel: ... }` is instantiated purely
  to catch wheel events (button acceptance explicitly disabled) — the textbook case `WheelHandler`
  exists for. `BarSection` is the base component reused by every scrollable bar widget, so fixing
  it here has broad payoff for a one-line change.
- **Suggested Direction**: replace with `WheelHandler { onWheel: root.wheeled(...) }`.

#### [D-006] `WorkspacePill.qml` — `label` and `active` should be `readonly`
- **Severity**: Low | **Effort**: Trivial
- **Location**: `apps/shell/qml/Topbar/WorkspacePill.qml:13-14`
- **Confidence**: 82/100
- **Rationale**: `label` and `active` are pure derivations of `wsId`
  (`wsId > 0 ? String(wsId) : ""` / `wsId > 0`). The only instantiation site
  (`WorkspacePillStrip.qml`'s `Repeater` delegate) never sets either, and neither is imperatively
  reassigned anywhere in the file. Contrast with `glowOpacity`/`urgentPulseOpacity` in the same
  file, which correctly stay non-`readonly` because animation value-sources need to write them.
- **Suggested Direction**: mark both `readonly property`.

#### [D-007] Widespread unreferenced `id` assignments across ~15 files
- **Severity**: Low | **Effort**: Trivial (bulk cleanup)
- **Location**: e.g. `id: frameCanvas` in `ActiveWindowSection.qml`, `ClockSection.qml`,
  `LogoSection.qml`, `StatusesSection.qml`, `WeatherWidget.qml`, `WorkspaceSection.qml`; all 7
  section ids in `TopBar.qml`; one-offs in `AudioWidget.qml`, `BatteryWidget.qml`,
  `KeyboardLayoutWidget.qml`, `NetworkWidget.qml`, `NotificationsWidget.qml`, `WeatherWidget.qml`
- **Confidence**: 81/100
- **Rationale**: Verified via a per-file reference count that each listed `id:` occurs exactly once
  (the declaration) with zero in-file references. Harmless at runtime but adds noise, and looks like
  accidental leftovers from copy-pasted `BarSection`-derived boilerplate rather than intentional
  future test hooks.
- **Suggested Direction**: prune unreferenced ids in a POC-cleanup pass; keep only where deliberately
  reserved for test/automation hooks (call this out explicitly with a comment if so).

## Investigation Targets (60-79, human verification needed)

#### [I-001] `WorkspacePillStrip.qml` — unbounded `Repeater` sizing driven by `WorkspaceModel.maxWorkspaceId()`
- **Category**: ListView & Delegates | **Confidence**: 68/100
- **Finding**: The `Repeater` is sized by
  `Math.max(windowStart + displayCount - 1 + stripPad, WorkspaceModel.maxWorkspaceId())` — an
  unbounded value from Hyprland's live workspace IDs. A workspace numbered far outside the visible
  range (e.g. via `hyprctl dispatch movetoworkspace 80`) would force instantiation of ~80 full
  nontrivial delegate items (Shape+CurveRenderer+MultiEffect+animation+MouseArea+BarTooltipArea)
  even though only `displayCount` are visible.
- **Unverified because**: requires a live Hyprland session driving workspace IDs far outside normal
  usage to observe the actual instantiation cost.
- **How to verify**: use `hyprctl dispatch movetoworkspacesilent 80,address:0x...` per CLAUDE.md's
  occupancy-gate testing pattern, then inspect frame time / QML profiler for a spike.

#### [I-002] `BarSection.qml` base `implicitWidth` formula is non-functional
- **Category**: Layout & Anchoring | **Confidence**: 65/100
- **Finding**: `implicitWidth: container.implicitWidth + 16` where `container` is a bare `Item` that
  never derives implicit size from its children — the formula effectively always adds 16 to 0.
  Currently masked because every subclass overrides `implicitWidth`.
- **Unverified because**: could not confirm whether any current or near-future subclass relies on
  the base formula rather than overriding it.
- **How to verify**: grep all `BarSection { ... }` usages for one that omits an `implicitWidth`
  override and inspect its rendered width.

#### [I-003] `WeatherSection.qml` Loader skips `WeatherWidget`'s shrink animation on `configured → false`
- **Category**: Layout & Anchoring | **Confidence**: 60/100
- **Finding**: `Loader { active: WeatherService.configured }` destroys the `WeatherWidget` instance
  immediately when `configured` flips false, rather than letting its 200ms `implicitWidth` shrink
  `Behavior` play out first — the widget disappears with a pop instead of animating away.
- **Unverified because**: this transition (weather becoming unconfigured while the shell is running)
  is rare and wasn't reproduced live.
- **How to verify**: toggle weather configuration off at runtime and observe whether the topbar
  widget animates out or pops.

#### [I-004] `WorkspaceSection.qml:57` STA-2 — positioner `move:` transition, not a state-machine transition
- **Category**: States & Structure | **Confidence**: 65/100
- **Finding**: The lint-flagged `Transition` is attached to `pillRow.move:` (a `Row` positioner
  slot), not to a `states`-driven `transitions: [...]` list — so the lint's literal "add from/to"
  remedy doesn't change current behavior; likely a category mismatch in the linter. Real risk is
  only forward-looking: if a future refactor adds named `States` to `WorkspaceSection` and someone
  copies this block into a top-level `transitions:` array, the missing `from`/`to` would make it
  fire for every state change, not just pill reflows.
- **Unverified because**: no such future refactor exists yet to test against.
- **How to verify**: re-check when/if `WorkspaceSection.qml` gains explicit named `States`.

#### [I-005] `BatteryIndicator.qml` cosmetic snap when exiting the critical-pulse state
- **Category**: States & Structure | **Confidence**: 60/100
- **Finding**: When `root.critical` flips true→false mid-pulse, `glyphScale` jumps discontinuously
  to `1.0` (no `Transition` softens the `PropertyChanges`) — purely cosmetic.
- **Unverified because**: only visible in the low-battery (<10%) code path, not reproduced live.
- **How to verify**: drain a test battery/mock below the critical threshold then recover it while
  watching the icon.

#### [I-006] `BarTooltipArea.qml`'s `barMonitorName` is not `required`, unlike `StatusPopupTriggerArea.qml`'s
- **Category**: Bindings & Properties | **Confidence**: 70/100
- **Finding**: `BarTooltipArea.qml:8` declares `property string barMonitorName: ""` (optional,
  silent empty-string fallback) while `StatusPopupTriggerArea.qml:11` declares the same property
  `required`. All 12 current call sites set it correctly, so there's no live bug, but the leaf
  doesn't enforce the contract the rest of the propagation chain relies on.
- **Unverified because**: no current call site actually omits it, so the failure mode is
  hypothetical until a future consumer forgets to wire it.
- **How to verify**: N/A — this is a preventive hardening suggestion, not an active bug; verification
  would just be code inspection of the proposed change.

#### [I-007] `WorkspaceEdgeArrow.qml` dead `hoverEnabled: true` on an unnamed `MouseArea`
- **Category**: Performance & Quality | **Confidence**: 72/100
- **Finding**: `hoverEnabled: true` is set but no property in the file reads `containsMouse` —
  visual state depends only on `root.urgent`/`root.active`. Looks like leftover scaffolding from a
  hover-glow feature that was removed or never wired up.
- **Unverified because**: could not confirm whether a hover effect was intentionally planned but
  not yet implemented, vs. genuinely vestigial.
- **How to verify**: check `docs/sdd/workspace-indicator-redesign/` history/git blame for a
  hover-glow feature that was scoped out.

#### [I-008] Missing `Text.PlainText` on frequently-updating labels
- **Category**: Performance & Quality | **Confidence**: 62/100
- **Finding**: No file in the directory sets `textFormat` explicitly, so every `Text`/`Label`
  defaults to `Text.AutoText` (re-scans for markup on every text change).
  `ClockSection.qml`'s `timeLabel`/`dateLabel` (updated every 1000ms) are the strongest candidate;
  `AudioWidget.qml`'s `volumeLabel` and `WeatherWidget.qml`'s `tempText`/`condText` are secondary.
- **Unverified because**: actual measured cost of `AutoText` detection on short, fixed-format
  strings on this Qt build is likely negligible — flagged as cheap hygiene, not a proven bottleneck.
- **How to verify**: profile `ClockSection.qml` repaint cost with/without `textFormat: Text.PlainText`
  under `QSG_RENDER_TIMING=1` or similar.

## Ruled Out (agent disagreement resolved by verification)

**`AppWindowIcon.qml:40` `on_StrokeChanged` handler naming** — one agent (States/Transitions)
initially flagged this as a signal-handler name mismatch for property `_stroke` (naive capitalization
would predict `on_strokeChanged`). A second agent (Bindings & Properties) empirically verified with a
live `qml6` runtime harness that QML capitalizes the first *alphabetic* character, skipping the
leading underscore — `on_StrokeChanged` is correct and is the codebase's established convention,
consistently used across 7 other files (`Controls/AudioIcon.qml`, `Controls/NetworkIcon.qml`,
`Controls/BatteryIcon.qml`, etc.). Trusted the empirical verification; **no finding**.

## Lint Findings

#### [L-001] `BarBackground.qml:9` — transparent `Rectangle`
- **Rule**: PRF-1
- **Finding**: `Rectangle` with `color: "transparent"` creates a geometry node even though nothing
  is drawn.
- **Mitigation**: replace with a plain `Item`, or toggle `visible` instead of using transparency.

#### [L-002] `BarSection.qml:6` — transparent `Rectangle`
- **Rule**: PRF-1
- **Finding**: same pattern as L-001.
- **Mitigation**: same as L-001.

#### [L-003] `SpecialWorkspaceDot.qml:14` — `property var`
- **Rule**: BND-1
- **Finding**: untyped property blocks qmlsc compilation and type safety.
- **Mitigation**: give it a concrete type (this holds a heterogeneous JS object per the agents'
  review — if truly heterogeneous, document why `var` is required; otherwise narrow the type).

#### [L-004] `WorkspaceSection.qml:63` — `property var`
- **Rule**: BND-1
- **Finding**: same pattern as L-003; verified by the Bindings agent to legitimately hold a
  heterogeneous JS array, not a scalar that should be typed narrower.
- **Mitigation**: leave as `var` with a short comment noting why, or refactor to a typed model role
  if the shape is actually fixed.

#### [L-005] `WorkspacePillStrip.qml:19` — `clip: true` disables scene graph batching
- **Rule**: PRF-3
- **Finding**: acceptable on a scrollable strip (this is effectively a `ListView`-like component),
  but worth confirming clipping is actually needed at this level vs. a parent boundary.
- **Mitigation**: verify visually that removing `clip` causes actual overflow before keeping it;
  otherwise no action needed — this is likely a legitimate use.

#### [L-006] `WorkspaceSection.qml:57` — `Transition` without `from`/`to`
- **Rule**: STA-2
- **Finding**: catch-all transition fires on every reflow. Deepened in [I-004] above — currently
  attached to a positioner `move:` slot (not a state machine), so today's behavior is fine; risk is
  forward-looking only.
- **Mitigation**: no action needed today; revisit if `WorkspaceSection` ever gains named `States`.

## Summary

| Category | Lint | Deep | Investigate | Total |
|----------|------|------|--------------|-------|
| Bindings & Properties | 2 | 1 | 2 | 5 |
| Layout & Anchoring | 0 | 1 | 2 | 3 |
| Component Loading & Lifecycle | 0 | 0 | 0 | 0 |
| ListView & Delegates | 0 | 0 | 1 | 1 |
| States, Transitions & Structure | 1 | 1 | 2 | 4 |
| Performance & Code Quality | 3 | 3 | 2 | 8 |
| **Total** | **6** | **7** | **8** | **21** |

25 ORD-1 + 8 STY-* lint hits suppressed as pure style noise (see Tool Sign-off). One cross-agent
contradiction encountered and resolved by empirical re-verification (see Ruled Out).
