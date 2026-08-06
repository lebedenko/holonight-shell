# Phase 9 — Top-Bar Section Exit Transitions

**Status**: Specification phase (Stage 1 of SDD)

## Objective

Fix the abrupt removal of dynamic top-bar sections. Phase 7 confirmed that
`WeatherSection` unloads `WeatherWidget` before its existing width transition
can render. The same `visible: false`-before-width-collapse pattern also
affects the audio, battery, and keyboard-layout sections.

This phase restores a consistent 200 ms exit transition for those four
sections. The network section is explicitly excluded from removal: it remains
visible as the offline-state indicator when NetworkManager is unavailable.

| Source | Phase 9 item | Rationale |
|---|---|---|
| U-08 I-03 | Weather section exit transition | Confirmed Phase 7 finding. |
| Follow-up | Audio, battery, keyboard-layout exit transitions | Same observable lifecycle defect in sibling status sections. |
| Product decision | Network remains visible offline | Offline status is useful shell feedback, not an absent section. |

## Functional Requirements

### REQ-F-01 — Animated weather removal

When weather becomes unavailable, the top bar shall retain `WeatherWidget`
until its existing 200 ms `implicitWidth` transition has reached zero.

- The section shall occupy and release horizontal layout space continuously;
  it shall not disappear in a single frame.
- Loading weather data shall retain the existing expansion behavior.
- A weather section whose width is zero shall not be visible or interactive.

**Acceptance**: deterministic QML coverage observes a non-zero width during
the exit transition and zero width/non-visible state after it completes.

### REQ-F-02 — Animated status-section removal

Audio, battery, and keyboard-layout sections shall use the same exit contract
when their availability conditions become false:

| Section | Availability condition |
|---|---|
| Audio | `AudioService.available` |
| Battery | `BatteryService.present` |
| Keyboard layout | `KeyboardLayoutService.layoutCode.length > 0` |

- Each section shall animate `implicitWidth` to zero over 200 ms with the
  established `Easing.OutCubic` curve.
- A section shall remain visible only while its animated width is positive.
- An unavailable section shall not accept hover, click, wheel, tooltip, or
  popup interaction during its exit transition.
- Reappearance before an exit finishes shall smoothly reverse toward the
  current non-zero width without a layout jump.

**Acceptance**: QML coverage verifies every availability condition, both
directions of the transition, and disabled interaction while exiting.

### REQ-F-03 — Persistent network offline state

`NetworkWidget` shall remain present for all `NetworkService.available` and
`NetworkService.online` states. It shall continue to show its existing offline
icon and tooltip when the service is unavailable.

**Acceptance**: QML coverage confirms unavailable network state does not
collapse or hide the network section.

## Constraints and Verification

- Keep the change within the affected top-bar QML components and QML test
  fixtures; do not add dependencies or alter service APIs used by production.
- Preserve the existing 200 ms duration and `Easing.OutCubic` easing.
- Do not animate the network section away.
- Run `task qml-lint`, `task qmltypes-check`, and the focused QML harness.
- Perform a live Hyprland check for each removal/reappearance transition after
  automated validation passes.

## Out of Scope

- Reworking unrelated top-bar layout or styling.
- Changing the meaning of offline network status.
- The separate Phase 7 battery critical-pulse snap (U-08 I-05).
- Other Phase 7 remediation candidates.
