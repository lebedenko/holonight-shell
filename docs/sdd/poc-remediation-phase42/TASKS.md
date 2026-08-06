# Phase 42 — NetworkManager Wi-Fi Activation Contracts: Tasks

**Status**: Complete — automated checks and user verification passed.

## Pre-flight

- [x] T-282: Reproduce both live symptoms in the backend call graph.
  - Result: saved-profile reads and new-profile writes both used a flat
    `QVariantMap` for NetworkManager's nested `a{sa{sv}}` settings type.

## Implementation and tests

- [x] T-283: Use one registered nested settings type for D-Bus reads and writes.
  - REQs: REQ-F-01, REQ-F-02

- [x] T-284: Cover saved-profile recognition at the fake D-Bus boundary.
  - REQs: REQ-F-01

- [x] T-285: Cover password activation's nested settings payload.
  - REQs: REQ-F-02

## Validation and handoff

- [x] T-286: Run focused and project validation.
  - Result: all three focused service/fake-D-Bus cases passed. `task test`
    built successfully and ran 956 tests; the only failure was the known
    pre-existing `KeyboardLayoutService.EventSocketConnectedTriggersDevicesQuery`
    test, and the compositor-screen test retained its expected skip.
    `task architecture-check`, changed-file clang-format, and
    `git diff --check` passed.

- [x] T-287: Obtain live verification and close the phase.
  - Check: reconnect a disconnected saved secured network without a password;
    forget or choose a new secured network and connect with its correct
    password; confirm both paths activate successfully.
  - Result: user verified both saved-profile reconnect without a password and
    correct-password activation for a new secured network. `58cfde0` (`fix:
    restore networkmanager wifi activation`) implements both D-Bus contract
    repairs; the separate Phase 7 Low-severity backlog remains at 18 candidates
    because this phase fixes a newly reported functional defect.
