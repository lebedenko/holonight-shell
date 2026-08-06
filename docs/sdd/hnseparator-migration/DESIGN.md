# HnSeparator Migration — Design

Stage 2 (Design) artifact for the SDD cycle defined in `docs/sdd/hnseparator-migration/SPEC.md`.
This document is architecture/documentation only — no source files are modified as part of
producing it. Implementation happens in Stage 4.

## Components Affected

| Component | Path | Change |
|---|---|---|
| Launcher | `apps/shell/qml/Launcher/Launcher.qml` | 2 `ContentSeparator` → `HnSeparator` instances; drop `import Holonight.Components` |
| Launcher browse panel | `apps/shell/qml/Launcher/LauncherRightPanelBrowse.qml` | 1 instance; **keep** `import Holonight.Components` (still uses `ExternalIcon`) |
| Network popup content | `apps/shell/qml/Popups/Network/NetworkPopupContent.qml` | 2 instances; swap `import Holonight.Components` → `import Holonight.Controls` |
| Weather current section | `apps/shell/qml/Popups/Weather/WeatherCurrentSection.qml` | 1 instance; swap import |
| Weather daily cards | `apps/shell/qml/Popups/Weather/WeatherDailyCards.qml` | 1 instance; swap import |
| Weather popup content | `apps/shell/qml/Popups/Weather/WeatherPopupContent.qml` | 8 instances (highest count/risk); swap import |
| Topbar workspace section | `apps/shell/qml/Topbar/WorkspaceSection.qml` | 1 instance (inside a `Component {}`, animated); swap import |
| Components module build registration | `qml/HoloNight/CMakeLists.txt` | Remove `Components/ContentSeparator.qml` from `HOLONIGHT_COMPONENT_QML_FILES`; `Components/ExternalIcon.qml` and the module definition stay |
| Component source being deleted | `qml/HoloNight/Components/ContentSeparator.qml` | Deleted entirely |
| Unit test being deleted | `tests/qml/tst_ContentSeparator.qml` | Deleted entirely |
| QML smoke test harness | `tests/test_qml_smoke.cpp` (`componentsQmldir()`, ~line 77) | Remove the `"ContentSeparator 1.0 %1ContentSeparator.qml\n"` line; keep `module Holonight.Components`, keep `ExternalIcon 1.0 ...` |

No other files are in scope (REQ-C-003).

## Data Flow / Migration Mechanics

`ContentSeparator` is a `Rectangle` subclass; `HnSeparator` is an `Item` wrapping an internal
`Rectangle` (`id: line`) plus an `HnSeparatorGeometry` helper for hairline snapping. The two
components are API-similar but not drop-in-compatible — every call site needs the 5 mechanical
rewrites below applied together, in this order, to reach a syntactically and semantically correct
`HnSeparator` block.

### Rule 1 — `direction` → `orientation` (REQ-F-001)

`ContentSeparator.Direction.*` is a local nested enum; `HnSeparator.orientation` takes the builtin
`Qt.Horizontal` / `Qt.Vertical` values directly (no component-qualified enum).

```qml
// before
ContentSeparator {
    direction: ContentSeparator.Direction.Horizontal
    ...
}
// after
HnSeparator {
    orientation: Qt.Horizontal
    ...
}
```
```qml
// before
direction: ContentSeparator.Direction.Vertical
// after
orientation: Qt.Vertical
```

If no `direction` was set at all, the implicit default was `Horizontal` (`ContentSeparator`'s own
default); the migrated instance may likewise omit `orientation` only if `Qt.Horizontal` is in fact
the correct value for that call site — every call site in the inventory sets `direction` explicitly
today, so in practice every migrated instance sets `orientation` explicitly too.

### Rule 2 — `type` → `fadeMode` (REQ-F-002)

Exact enum value mapping; a bare literal substitution, but note the **default flips direction of
meaning** — where a call site had no `type:` line at all (implicit `Center`... actually verify per
instance, see caveat below), it must gain an explicit `fadeMode:` line, because `HnSeparator`'s own
default differs from `ContentSeparator`'s.

| `ContentSeparator.Type` | `HnSeparator.fadeMode` |
|---|---|
| `Center` | `HnSeparator.FadeBoth` |
| `Start` | `HnSeparator.FadeStart` |
| `End` | `HnSeparator.FadeEnd` |
| `None` | `HnSeparator.Solid` |

```qml
// before (explicit)
type: ContentSeparator.Type.End
// after
fadeMode: HnSeparator.FadeEnd
```
```qml
// before (no type: line present → implicit ContentSeparator.Type.Center default)
ContentSeparator {
    direction: ContentSeparator.Direction.Vertical
    size: parent.height
    color: HoloniightPalette.borderPassive
    opacity: 0.5
}
// after (must add explicit fadeMode — implicit Center becomes explicit FadeBoth)
HnSeparator {
    orientation: Qt.Vertical
    height: parent.height
    opacity: 0.5
}
```

**Caveat / correction to SPEC wording:** SPEC.md's REQ-F-002 body text says "Instances with no
explicit `type` property currently default to `None` (Solid)" — this is the acceptance-criterion
framing for the general rule, but `ContentSeparator.qml` line 20 shows the component's actual
default is `type: ContentSeparator.Type.Center`, not `None`. Per-instance, the implementer must
read what `ContentSeparator`'s own default resolves to (`Center` → `FadeBoth`) for any call site
that omits `type:`, not assume `Solid`. Cross-checking the inventory: every one of the 16 call
sites in this codebase sets `type:` explicitly *or* omits it and relies on the `Center` default —
none rely on a `None` default. The implementer should verify this per-instance from the actual
source rather than trust the summary table, and set `fadeMode: HnSeparator.FadeBoth` (not `Solid`)
for any instance that omitted `type:`.

### Rule 3 — Color override removal (REQ-F-003)

Every instance currently sets `color: HoloniightPalette.borderPassive`. Delete that line entirely;
`HnSeparator`'s own default (`HoloniightPalette.borderSubtle`, declared as its own property default)
takes over.

```qml
// before
ContentSeparator {
    direction: ContentSeparator.Direction.Horizontal
    size: parent.width
    color: HoloniightPalette.borderPassive
    opacity: 0.5
}
// after
HnSeparator {
    orientation: Qt.Horizontal
    width: parent.width
    opacity: 0.5
}
```

### Rule 4 — Thickness selective override (REQ-F-004)

Two-way split by file, not by any property already on the instance — this is a hardcoded location
rule, not something derivable from the QML itself.

- **Case A — `NetworkPopupContent.qml`'s 2 instances only:** keep `thickness: root.sectionSeparatorThickness` verbatim.
- **Case B — all other 14 instances:** none of them set `thickness:` today anyway (verified: no
  `thickness:` line appears outside `NetworkPopupContent.qml` in the 7 files), so this rule is a
  no-op confirmation for those 14, not an active deletion.

```qml
// NetworkPopupContent.qml — thickness preserved
HnSeparator {
    Layout.fillWidth: true
    Layout.minimumHeight: root.sectionSeparatorThickness
    Layout.preferredHeight: root.sectionSeparatorThickness
    thickness: root.sectionSeparatorThickness
    centerOpacity: root.sectionSeparatorOpacity
    fadeMode: HnSeparator.Solid
}
```

### Rule 5 — `size` → explicit dimension (REQ-F-005), split A/B by parent type

**Rule 5A (Layout parent — `NetworkPopupContent.qml`'s 2 instances only):** the instance already
carries `Layout.fillWidth: true` + `Layout.minimumHeight`/`Layout.preferredHeight` bound to
`root.sectionSeparatorThickness`. Drop `size: root.width` entirely — the Layout attached properties
already fully determine geometry.

```qml
// before
ContentSeparator {
  Layout.fillWidth: true
  Layout.minimumHeight: root.sectionSeparatorThickness
  Layout.preferredHeight: root.sectionSeparatorThickness
  size: root.width
  thickness: root.sectionSeparatorThickness
  color: HoloniightPalette.borderPassive
  centerOpacity: root.sectionSeparatorOpacity
  type: ContentSeparator.Type.None
}
// after
HnSeparator {
  Layout.fillWidth: true
  Layout.minimumHeight: root.sectionSeparatorThickness
  Layout.preferredHeight: root.sectionSeparatorThickness
  thickness: root.sectionSeparatorThickness
  centerOpacity: root.sectionSeparatorOpacity
  fadeMode: HnSeparator.Solid
}
```

**Rule 5B (non-Layout parent — the other 14 instances):** convert `size: X` into an explicit
dimension matching the *new* `orientation` value — `Vertical` → `height: X`, `Horizontal` →
`width: X`. Note the conversion target is the **opposite** axis name from what might be assumed at
a glance: a *vertical* separator's `size` was its extent along the vertical axis, i.e. its
`height`.

```qml
// before (Vertical orientation)
ContentSeparator {
    direction: ContentSeparator.Direction.Vertical
    size: parent.height
    color: HoloniightPalette.borderPassive
    opacity: 0.5
}
// after
HnSeparator {
    orientation: Qt.Vertical
    height: parent.height
    opacity: 0.5
}
```
```qml
// before (Horizontal orientation)
ContentSeparator {
    direction: ContentSeparator.Direction.Horizontal
    size: parent.width
    color: HoloniightPalette.borderPassive
    opacity: 0.5
}
// after
HnSeparator {
    orientation: Qt.Horizontal
    width: parent.width
    opacity: 0.5
}
```

### Unaffected properties (carried over verbatim)

- `centerOpacity`, `edgeOpacity` (REQ-F-006) — identical names/semantics on both components, copy unchanged.
- Outer `opacity`, `Component.onCompleted: opacity = 1`, `Behavior on opacity { NumberAnimation {...} }`, `visible: <expr>`, `objectName` (REQ-F-007) — builtin `Item`/`Rectangle` properties, unaffected by the component swap, copy unchanged.

## Interfaces/APIs — Property Surface Mapping

| `ContentSeparator` property | `HnSeparator` property | Migration action |
|---|---|---|
| `direction` (enum `Direction.Horizontal`\|`Vertical`) | `orientation` (int, `Qt.Horizontal`\|`Qt.Vertical`) | Rename + remap values (Rule 1) |
| `type` (enum `Type.Center`\|`Start`\|`End`\|`None`) | `fadeMode` (enum `HnSeparator.FadeBoth`\|`FadeStart`\|`FadeEnd`\|`Solid`) | Rename + remap values (Rule 2) |
| `color` (color, default `HoloniightPalette.borderPassive`* not actually — see note) | `color` (color, default `HoloniightPalette.borderSubtle`) | Delete override; adopt new default (Rule 3) |
| `thickness` (real, default `1.0`) | `thickness` (real, default `HoloniightPalette.separatorWidth`) | Keep only in `NetworkPopupContent.qml` (Rule 4) |
| `size` (required int, no `HnSeparator` equivalent) | *(none — use `width`/`height` directly)* | Convert to explicit dimension, or drop under Layout parent (Rule 5) |
| `centerOpacity` (real, default `0.45`) | `centerOpacity` (real, default `1.0`) | Copy verbatim — every call site sets this explicitly or accepts the (differing) default per REQ-F-006 scope |
| `edgeOpacity` (real, default `0.0`) | `edgeOpacity` (real, default `0.0`) | Copy verbatim |
| *(none)* | `HnSeparatorGeometry`-backed hairline snapping (internal, not a public property) | N/A — internal implementation detail of `HnSeparator`, not consumer-facing |

Note on `color`: `ContentSeparator.qml` does not declare a `color` property of its own — it relies
on `Rectangle.color` (default black, alpha via gradient stops), and every one of the 16 call sites
sets `color: HoloniightPalette.borderPassive` explicitly to compensate. `HnSeparator` declares its
own `property color color: HoloniightPalette.borderSubtle`, so simply deleting the override line
is sufficient and correct — no call site relies on `Rectangle`'s bare default.

## Per-File Migration Plan

### `apps/shell/qml/Launcher/Launcher.qml` — 2 instances
- **~L326** (vertical divider between search results and right panel): `direction: Vertical` → `orientation: Qt.Vertical`; `size: parent.height` → `height: parent.height`; drop `color:`; no `type:` present → add `fadeMode: HnSeparator.FadeBoth` (Center default, see Rule 2 caveat); keep `opacity: 0.5`.
- **~L357** (footer divider above keyboard-hint row): `direction: Horizontal` → `orientation: Qt.Horizontal`; `size: parent.width` → `width: parent.width`; drop `color:`; add `fadeMode: HnSeparator.FadeBoth` (same Center-default caveat); keep `opacity: 0.5`.
- Import: remove `import Holonight.Components` (file already has `import Holonight.Controls` per REQ-F-008 table — verify at implementation time and add it only if actually absent).

### `apps/shell/qml/Launcher/LauncherRightPanelBrowse.qml` — 1 instance
- **~L90** (separator below "recent apps" empty state): `direction: Horizontal` → `orientation: Qt.Horizontal`; `size: parent.width` → `width: parent.width`; `type: ContentSeparator.Type.End` → `fadeMode: HnSeparator.FadeEnd`; drop `color:`; keep `opacity: 0.5`.
- Import: **keep** `import Holonight.Components` (file also uses `ExternalIcon` from that module); ensure `import Holonight.Controls` is present (already is, per inventory).

### `apps/shell/qml/Popups/Network/NetworkPopupContent.qml` — 2 instances
Both instances are Rule-5A (Layout parent) + Rule-4-Case-A (thickness preserved):
- **~L26** (separator after `NetworkToggleRow`): drop `size: root.width`; `type: ContentSeparator.Type.None` → `fadeMode: HnSeparator.Solid`; keep `Layout.fillWidth`, `Layout.minimumHeight`, `Layout.preferredHeight`, `thickness: root.sectionSeparatorThickness`, `centerOpacity: root.sectionSeparatorOpacity`; drop `color:`.
- **~L134** (separator after `WifiNetworkList`): identical shape/treatment to the above.
- Note: `currentConnectionSeparator` (a hand-rolled `Rectangle`, ~L48) is explicitly out of scope (REQ-C-002) — do not touch it, do not confuse it with the two `ContentSeparator` instances.
- Import: swap `import Holonight.Components` → `import Holonight.Controls`.

### `apps/shell/qml/Popups/Weather/WeatherCurrentSection.qml` — 1 instance
- **~L96** (separator inside a 12px-tall spacer `Item`, below condition text): `direction: Horizontal` → `orientation: Qt.Horizontal`; `size: parent.width` → `width: parent.width`; `type: ContentSeparator.Type.End` → `fadeMode: HnSeparator.FadeEnd`; drop `color:`; keep `opacity: 0.5`.
- Import: swap `import Holonight.Components` → `import Holonight.Controls`.

### `apps/shell/qml/Popups/Weather/WeatherDailyCards.qml` — 1 instance
- **~L61** (per-day-card leading vertical divider inside a `Repeater` delegate): `direction: Vertical` → `orientation: Qt.Vertical`; `size: parent.height` → `height: parent.height`; drop `color:`; no `type:` → add `fadeMode: HnSeparator.FadeBoth`; keep `opacity: 0.5` and `visible: dayCard.index > 0` unchanged.
- Import: swap `import Holonight.Components` → `import Holonight.Controls`.

### `apps/shell/qml/Popups/Weather/WeatherPopupContent.qml` — 8 instances (highest risk — see Known Risks)
All 8 are Rule-5B (plain `Item`/`RowLayout`-as-plain-sizing-context, not `Layout.*`-driven), none override `thickness`. Enumerated by actual line number (confirmed by direct read, matches SPEC.md inventory):
- **L86** vertical, no `type:` → `orientation: Qt.Vertical`, `height: parent.height`, `fadeMode: FadeBoth`, `opacity: 0.5`.
- **L100** horizontal, no `type:` → `orientation: Qt.Horizontal`, `width: parent.width`, `fadeMode: FadeBoth`, `opacity: 0.5`.
- **L117** horizontal, no `type:` → same shape as L100.
- **L145** vertical, no `type:` → `orientation: Qt.Vertical`, `height: parent.height`, `fadeMode: FadeBoth`, `opacity: 0.5`.
- **L232** horizontal, `type: End` → `orientation: Qt.Horizontal`, `width: parent.width`, `fadeMode: FadeEnd`, `opacity: 0.5`.
- **L283** horizontal, `type: End` → same shape as L232.
- **L331** horizontal, no `type:` → same shape as L100.
- **L358** vertical, no `type:`, **`opacity: 0.32`** (not 0.5 — the one outlier in this file) → `orientation: Qt.Vertical`, `height: parent.height`, `fadeMode: FadeBoth`, `opacity: 0.32`.
- Import: swap `import Holonight.Components` → `import Holonight.Controls`.

### `apps/shell/qml/Topbar/WorkspaceSection.qml` — 1 instance
- **~L121** (inside `Component { id: separatorComponent }`, dynamically instantiated for the "special workspace" divider — not a static child): `objectName: "workspaceSpecialSeparator"` stays; `direction: Vertical` → `orientation: Qt.Vertical`; `size: 48` → `height: 48` (a literal, not a binding — unlike every other instance in scope); `type: Center` → `fadeMode: HnSeparator.FadeBoth`; drop `color:`; **preserve verbatim**: `opacity: 0`, `Component.onCompleted: opacity = 1`, `Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }`.
- Import: swap `import Holonight.Components` → `import Holonight.Controls`.

## Key Decisions with Rationale

1. **Color and thickness defaults are allowed to drift, not pinned pixel-for-pixel.** `borderPassive` → `borderSubtle` and the no-override thickness default are both explicitly approved visual deltas (REQ-F-003, REQ-F-004, REQ-NF-002) — this is a token-adoption migration, not a byte-for-byte behavioral port. Preserving the old look exactly via per-instance overrides would silently defeat the entire point of routing every consumer onto the shared control's canonical defaults, and would leave 16 needless override lines as migration debt.
2. **No new `HnSeparator` unit test is written.** Same policy already applied to `HnIcon` and other adopted shared controls in this codebase: the control ships pre-tested from the shared design-system repo (`holonight-qt`), and this repo's regression gate for a pure swap-in is the existing suite of consumer-level tests that already exercise Launcher, Weather popup, Network popup, and WorkspaceSection behavior end-to-end. Adding a redundant local test for someone else's already-tested component would just be maintenance surface with no signal.
3. **Rule 5A/5B split exists because `size` has no direct `HnSeparator` analog and the correct replacement depends entirely on what already governs the parent's layout.** Under a `ColumnLayout`/`RowLayout` parent, an explicit `width`/`height` on a `Layout.fillWidth: true` child is redundant/fights the layout — the correct action is deletion, letting `Layout.*` attached properties own sizing (5A). Under a plain `Item`/`Row`/`Column` parent, there is no attached-property sizing mechanism at all, so the `size` value must land somewhere concrete or the separator collapses to implicit size — hence explicit `width`/`height` (5B). Treating all 16 instances the same way would either break the two Layout-driven `NetworkPopupContent.qml` instances (double-sizing conflict) or silently un-size the other 14 (if the 5A "delete it" rule were misapplied there).
4. **The `type`-omitted-instances get `fadeMode: HnSeparator.FadeBoth`, not `Solid`**, correcting a discrepancy between SPEC.md's REQ-F-002 acceptance-criterion prose (which says the implicit default is `None`/`Solid`) and `ContentSeparator.qml`'s actual source (`property int type: ContentSeparator.Type.Center`, i.e., default is `Center`/`FadeBoth`). This design doc treats the component source as ground truth over the spec's summary sentence; the acceptance criterion itself ("explicit `fadeMode` set, either derived from existing `type` or mapping the implicit default") is still satisfied — only the *specific value* used for the implicit-default case is corrected.
5. **`LauncherRightPanelBrowse.qml` keeps `import Holonight.Components`** (uniquely among the 7 files) because it is the one file in scope that has a second, unrelated consumer (`ExternalIcon`) of that module — removing the import would break an unrelated component in the same file, which is out of scope for this migration (REQ-C-003 in spirit: don't collaterally touch adjacent adoptions).

## Alternatives Considered

- **Preserve literal `color`/`thickness` via explicit overrides on every one of the 16 instances**, keeping the pre-migration rendering byte-identical. Rejected: this was explicitly discussed and rejected during Stage 0 grilling — the whole motivation for adopting `HnSeparator` is to route every separator consumer onto the shared token defaults so future palette/token changes propagate automatically; re-pinning old literals on every call site would immediately reintroduce the exact fragmentation this migration exists to remove, for zero benefit (the visual delta is minor and pre-approved).
- **Write a fresh `tst_HnSeparator.qml` in this repo** duplicating or extending the removed `tst_ContentSeparator.qml` coverage. Rejected: `HnSeparator` is upstream, shared-repo-owned, and already tested there; this repo's precedent (HnIcon, HnButton, and other prior shared-control adoptions per `project_holonight_components.md` memory) is to rely on consumer-level regression tests instead of re-testing someone else's component locally.
- **Introduce a compatibility shim** (e.g., a thin `ContentSeparator`-named wrapper around `HnSeparator` translating the old property names) to avoid touching 16 call sites at once. Rejected: the spec's explicit end-state is full deletion of `ContentSeparator` (REQ-F-009 through REQ-F-013) with a live-codebase grep verification step (REQ-F-013) — a shim would leave the exact symbol this migration is meant to eliminate, just renamed underneath, and would need its own migration later anyway.
- **Migrate `currentConnectionSeparator` (the hand-rolled `Rectangle` in `NetworkPopupContent.qml`) to `HnSeparator` too**, since it visually serves the same purpose as the two adjacent `ContentSeparator` instances in that file. Rejected: explicitly out of scope per REQ-C-002 — it was never a `ContentSeparator` instance, so it is not part of "the 16," and folding it in would silently expand scope beyond what Stage 0/1 approved.

## Known Risks

1. **`WeatherPopupContent.qml`'s 8 mixed-configuration instances are the highest-risk transcription surface.** Unlike the other 6 files (each with 1-2 uniform instances), this file mixes orientations (5 horizontal / 3 vertical), fade modes (5 `FadeBoth`-via-omitted-`type` / 2 `FadeEnd`... actually 2 explicit `End` at L232/L283 with rest omitted), and one outlier opacity (`0.32` at L358 vs. `0.5` everywhere else in the file). A careless find-and-replace across this file risks silently normalizing the L358 opacity to `0.5` or assigning the wrong `fadeMode` to one of the two `End`-type instances. Mitigate by migrating this file's 8 instances one at a time against the line-by-line table in the Per-File Migration Plan above, not via a bulk regex.
2. **FadeStart/FadeEnd middle-gradient-stop cosmetic difference is a documented Non-Goal, not a regression to chase.** `HnSeparator`'s `middleColor` for `FadeStart`/`FadeEnd` averages `centerOpacity`/`edgeOpacity` at the 0.5 gradient stop (`ContentSeparator.qml`'s middle stop for the same modes always used bare `centerOpacity`, no averaging — compare `ContentSeparator.qml` lines 42-45 to `HnSeparator.qml` lines 35-42). The visual difference is subtle and pre-approved (SPEC.md Non-Goal #1); the implementer should not attempt to "fix" this by adding compensating opacity overrides, since doing so would fight the shared control's intended behavior and reintroduce per-instance override debt (see Key Decision #1).
3. **Missing or stray import risk across the 7 files.** Six files need `import Holonight.Components` removed and `import Holonight.Controls` added; one file (`LauncherRightPanelBrowse.qml`) needs `Holonight.Components` kept; `Launcher.qml` needs only the removal (Controls import already present). Cross-referencing the wrong row of the REQ-F-008 table for any file leaves either a dangling now-unused `Holonight.Components` import (harmless but untidy, and would trip the REQ-F-013 grep if `ContentSeparator` also lingers) or a missing `Holonight.Controls` import (hard failure — `HnSeparator` unresolved at parse time). Verify each file's final import block against the REQ-F-008 table individually rather than applying one blanket swap-import rule to all 7.
4. **`WorkspaceSection.qml`'s instance lives inside a `Component { id: separatorComponent }`, not a static child** — it's dynamically instantiated (presumably via a `Loader` or `createObject` call elsewhere in the file, or in `WorkspacePillStrip`/a sibling). A search for `ContentSeparator {` that only scans top-level/static QML children could miss this instance if tooling assumes plain children rather than `Component` bodies; the migration must specifically account for this instance living inside a `Component` wrapper, including verifying the `Behavior on opacity` and `Component.onCompleted` survive unmodified inside that same wrapper.
5. **Build/registration ordering risk:** `ContentSeparator.qml` must not be deleted (REQ-F-009) or de-registered from CMake (REQ-F-010) until all 16 call sites across all 7 files are migrated — deleting it first would break the build for every not-yet-migrated file referencing the old component name. Similarly, `tst_ContentSeparator.qml` and the `test_qml_smoke.cpp` qmldir entry (REQ-F-011/012) should be removed only after the component source deletion, to avoid a dangling test referencing an already-gone type. Stage 4 should sequence: migrate all 16 call sites → verify zero remaining references → delete `ContentSeparator.qml` + CMake entry → delete test file + smoke-test qmldir line → full grep verification (REQ-F-013) → build/test/lint gates.
