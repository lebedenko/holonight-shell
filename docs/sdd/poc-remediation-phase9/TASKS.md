# Phase 9 — Top-Bar Section Exit Transitions: Tasks

**Status**: Complete — automated and live Hyprland acceptance passed.

## Pre-flight

- [x] T-091: Revalidate Phase 7 U-08 I-03 and the four affected top-bar
  components against implementation `HEAD`.
  - Result: the draft matched the width-driven lifecycle. `Loader` sizing was
    not a valid layout-size owner, so WeatherSection now directly instantiates
    WeatherWidget and retains it through the existing width animation.

## Implementation and Tests

- [x] T-092: Keep `WeatherWidget` instantiated through its width-collapse
  transition.
  - REQs: REQ-F-01
  - Files: `apps/shell/qml/Topbar/WeatherSection.qml`
  - Result: WeatherSection directly hosts WeatherWidget; its existing 200 ms
    `implicitWidth` behavior controls the entire exit transition.

- [x] T-093: Add consistent animated removal to audio, battery, and
  keyboard-layout sections.
  - REQs: REQ-F-02
  - Files: `apps/shell/qml/Topbar/AudioWidget.qml`, `BatteryWidget.qml`,
    `KeyboardLayoutWidget.qml`
  - Result: roots are enabled only while available, visible only while their
    animated width is positive, and use a 200 ms OutCubic width behavior.

- [x] T-094: Add deterministic QML transition coverage and retain network's
  offline-state behavior.
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03
  - Files: `tests/FakeQmlServices.h`,
    `tests/qml/tst_TopbarSectionTransitions.qml`
  - Result: mutable QML-only service fakes drive weather, audio, battery,
    keyboard, and network states. The focused cases cover exit, reappearance,
    disabled interaction, and network's offline state.

## Validation and Handoff

- [x] T-095: Run focused and project QML validation.
  - Result: QML harness passed (131 tests); `task qml-lint` and
    `task qmltypes-check` passed. The harness retains pre-existing offscreen
    warnings for unavailable icon providers and missing tray-menu fake APIs.

- [x] T-096: Perform a live Hyprland top-bar animation check.
  - Steps:
    1. Toggle weather configuration/data and observe both removal and return.
    2. Stop/start audio availability, battery presence, and keyboard layout;
       verify each reflows smoothly and does not accept interaction while
       exiting.
    3. Disable network availability and confirm the offline icon remains in
       place on every monitor.
    4. Repeat a rapid removal/reappearance for each dynamic section and check
       for snaps, residual gaps, or wrong-monitor popups.
  - Result: user confirmed the implemented top-bar transitions work as
    expected in the live session.

- [x] T-097: Update the Phase 7/9 handoff after all acceptance checks pass.
  - Result: U-08 I-03 is implemented and accepted in `fdc7a23`
    (`fix: animate dynamic topbar section exits`). Phase 9 also
    covers audio, battery, and keyboard-layout exits; NetworkWidget remains the
    intentional offline-state indicator. Unrelated Phase 7 items remain queued.
