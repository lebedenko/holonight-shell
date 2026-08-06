# Phase 12 — Popup Resilience and Settings Defaults: Design

**Input**: `poc-remediation-phase12/SPEC.md`
**Baseline**: Phase 11 accepted in `af78194`.

## 1. Scope Map

| Finding | Location | Change boundary |
|---|---|---|
| U-09 I-003 | tray menu, tooltip, calendar | sibling declaration order only |
| U-09 I-004 | `BrightnessSlider.qml` | slider write timer only |
| U-09 I-005 | `WeatherPopupContent.qml` | weather content viewport only |
| U-11 I-C4 | `SettingsEditModel.h` | model member initializers only |

## 2. Design Decisions

### 2.1 Use declaration order for glow layering

Qt Quick renders later sibling declarations above earlier ones. Move each
`MultiEffect` immediately before its source `Rectangle`, keeping the same
anchors and source binding. This follows `StatusPopup.qml`, avoids an
unnecessary `z` property, and leaves source geometry unchanged.

### 2.2 Keep one repeating throttle timer

`Timer.restart()` on each `valueChanging` event is debounce behavior: a
continuous drag postpones every write until release. Store the latest value in
`pending`, start a repeating 100 ms timer only if it is not already running,
and write `pending` on each trigger. On commit, stop the timer and write the
committed value directly. This bounds write frequency while preserving the
latest value and existing release behavior.

### 2.3 Put the weather stack in a bounded Flickable

Keep the existing manually-sized `Column` content unchanged, but place it in a
`Flickable` filling the root. The Flickable's content size follows the stack's
natural height plus its top and bottom padding; it clips the viewport and uses
normal bounded vertical scrolling only when content exceeds the available
height. This preserves the visual layout when it fits while making lower rows
reachable under tight height budgets.

### 2.4 Delegate the accent default to ThemeConfigFile

`SettingsEditModel` already includes `ThemeConfigFile.h`. Initialize both
accent fields with `ThemeConfigFile::defaultAccent()` rather than a hard-coded
ID. The model then shares the same source of truth as configuration creation,
normalization, and the theme catalog.

## 3. Test Strategy

| Layer | Scenario | Assertion |
|---|---|---|
| QML | affected popup/calendar components instantiate | reordered glows retain valid source components |
| QML | continuous brightness drag | writes occur at timer cadence before release; pending value is latest |
| QML | brightness release | timer stops and the release value is written once immediately |
| QML | weather viewport overflow | bounded viewport clips and exposes scrollable content beyond its height |
| C++ | fresh settings model | accent equals `ThemeConfigFile::defaultAccent()` and is not dirty |
| Live shell | tray/tooltip/calendar/weather/brightness | glow sits behind source; weather can reach final row; brightness drag and release remain responsive |

## 4. Risks

- A repeating timer must be stopped on release so it cannot submit a stale value
  after the final direct write.
- A Flickable needs explicit content dimensions; deriving them from the existing
  stack avoids a binding loop and preserves its fixed-width layout.
- Reordering an effect must not move it out of the source's visual parent;
  declaration order is the only intended difference.
