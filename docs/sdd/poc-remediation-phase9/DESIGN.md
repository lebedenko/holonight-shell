# Phase 9 — Top-Bar Section Exit Transitions: Design

**Input**: `poc-remediation-phase9/SPEC.md`
**Baseline**: implementation shall revalidate current `HEAD` and account for
the uncommitted top-bar animation draft before editing.

## 1. Change Map

| Requirement | Primary implementation | Tests |
|---|---|---|
| F-01 | `apps/shell/qml/Topbar/WeatherSection.qml` | focused top-bar QML transition test with a mutable weather fake |
| F-02 | `AudioWidget.qml`, `BatteryWidget.qml`, `KeyboardLayoutWidget.qml` | focused top-bar QML transition test |
| F-03 | no production change expected in `NetworkWidget.qml` | focused top-bar QML transition test |
| Test fixture | `tests/FakeQmlServices.h`, `tests/qml/tst_TopbarSectionTransitions.qml` | QML harness |

## 2. Design Decisions

### 2.1 Width owns visual lifetime

For a dynamic section, the availability expression continues to select its
target `implicitWidth` (normal width or zero). A `Behavior on implicitWidth`
uses the existing 200 ms `Easing.OutCubic` transition. `visible` is derived
from the *animated* width (`implicitWidth > 0`), rather than directly from the
availability expression.

This follows the repository's documented QML rule: `visible: false` removes an
item from the scene graph immediately and prevents a concurrent animation from
rendering. It also makes the `RowLayout` release space continuously.

### 2.2 Weather stays instantiated

`WeatherSection` shall directly instantiate `WeatherWidget`, rather than gate
it behind a `Loader`. The existing `WeatherWidget` `implicitWidth` behavior can
therefore animate from its last width to zero. Once the width reaches zero, the
section becomes invisible. This is a small resource trade-off for a single
lightweight top-bar widget and avoids timers or a second state machine.

### 2.3 Exit is non-interactive

Audio, battery, and keyboard-layout roots retain their animated visual lifetime
but bind `enabled` to their current availability condition. This preserves the
previous behavior—an unavailable service cannot open a popup, show a tooltip,
or receive wheel input—while the section is briefly visible to complete its
exit animation.

Weather uses the same availability/interaction gate. Its loader remains
instantiated, but an unconfigured or data-less widget cannot be interacted
with.

### 2.4 Network is intentionally not dynamic

`NetworkWidget` already maps unavailable/offline states to `wifi_offline` and
an explanatory tooltip. It shall not receive a visibility, width, or enabled
binding derived from `NetworkService.available`; this is intentional product
behavior, not an inconsistency.

### 2.5 Test-controlled singleton states

Extend only the QML test fakes with narrow `Q_INVOKABLE` state setters for the
weather, audio, battery, keyboard-layout, and network availability inputs. The
production services and their public APIs remain unchanged. The transition test drives those fakes,
checks an intermediate non-zero width after an availability loss, waits for
completion without arbitrary sleeps, and checks final visibility/interaction.

## 3. Test Strategy

| Scenario | Observable assertion |
|---|---|
| Weather becomes unavailable | width is initially retained, then reaches zero and becomes invisible |
| Audio, battery, keyboard become unavailable | each stays visible while shrinking, becomes disabled immediately, then invisible |
| Mid-exit reappearance | width grows from its current animated value without jumping to zero/full width |
| Network becomes unavailable | widget remains visible, enabled, and non-zero width with offline icon state |

Use `tryVerify`/signal-driven QML test progression for animation completion;
do not use fixed sleeps. Automated tests validate lifecycle and geometry only.
The live compositor check validates perceived smoothness and actual pointer
routing.

## 4. Risks

- `implicitWidth` animations are layout-driven, so the focused test must check
  the actual component width/implicit width rather than an implementation-only
  helper.
- Keeping weather loaded slightly increases idle object count; the component is
  already small and does no per-frame work when no data is present.
- A rapid service restart may interrupt an exit; the normal QML behavior must
  interpolate from the current value rather than snap.
