# Phase 41 — QML Delegate Contract Hardening: Tasks

**Status**: Complete — automated checks and user verification passed.

## Pre-flight

- [x] T-276: Revalidate U-09 I-002, I-006, and I-007 against current `main`.
  - Result: both cited undisciplined delegates still used implicit context
    inputs, and all four cited sidebar files still lacked bound component
    behavior.

## Implementation and tests

- [x] T-277: Declare the Wi-Fi delegate's consumed model roles.
  - REQs: REQ-F-01
  - Check: row identity, visuals, actions, and lock repaint behavior retain
    their existing bindings.

- [x] T-278: Harden the Overview notification delegate contract.
  - REQs: REQ-F-02
  - Check: the delegate requires `modelData` and `index`, with its notification
    value exposed read-only.

- [x] T-279: Bind sidebar delegates and declare their model inputs.
  - REQs: REQ-F-03
  - Check: calendar cells, upcoming events, and tab buttons compile without
    implicit role or outer-context lookup.

## Validation and handoff

- [x] T-280: Run focused and project validation.
  - Check: focused sidebar/network QML tests, `task qml-lint`,
    `task qmltypes-check`, `task test`, `task architecture-check`, and
    `git diff --check`.
  - Result: all 12 focused component-instantiation QML cases passed with only
    their known fake-resource warnings. `task qml-lint`, `task qmltypes-check`,
    `task architecture-check`, and `git diff --check` passed. `task test` built
    successfully and ran 955 tests; the only failure was the known pre-existing
    `KeyboardLayoutService.EventSocketConnectedTriggersDevicesQuery` test, and
    the compositor-screen test retained its expected skip.

- [x] T-281: Obtain user verification and close the phase.
  - Check: user confirms Wi-Fi rows and sidebar Overview/tab interactions in a
    live Hyprland session; then record commits and reduce the queued
    Low-severity backlog from 18 to 15 after reconciling the three U-09 items
    already accepted in Phase 12.
  - Result: user verified that Wi-Fi information and sidebar Overview/tab
    behavior remain unchanged. Reconnect/password failures were confirmed as a
    pre-existing NetworkManager integration defect outside this delegate-only
    phase and are queued for separate remediation. `0b813fe` (`refactor: bind
    sidebar delegate contracts`) implements U-09 I-002, I-006, and I-007; 15
    Low-severity candidates remain queued.
