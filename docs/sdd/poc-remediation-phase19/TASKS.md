# Phase 19 — NetworkManager Settings Map Access: Tasks

**Status**: Complete — automated checks and user live verification passed.

## Pre-flight

- [x] T-156: Revalidate U-04 I-05 against current HEAD.
  - Result: `NetworkManagerBackend.cpp:494-530` still manually demarshals the
    `GetSettings` result into `QMap<QString, QVariantMap>` and copies every
    entry into `QVariantMap settings` before reading only `connection`.
    `savedWifiConnections()` already reads the same group directly from a
    `QVariantMap`; the current test fake returns the map-form reply and the
    active-state test observes `FakeWiFi`.

## Implementation and Tests

- [x] T-157: Select the `connection` group without a full settings-map copy.
  - REQs: REQ-F-01
  - Files: `libs/holonight-services/src/network/NetworkManagerBackend.cpp`.
  - Check: preserve empty/error behavior and both normal-map and
    `QDBusArgument` decoding paths.
  - Result: `connectionId()` now selects only `connection` from either reply
    representation. It uses exact meta-types before deserializing a
    `QDBusArgument`, avoiding its read-mode conversion trap.

- [x] T-158: Make the existing D-Bus fake regression coverage explicit.
  - REQs: REQ-F-01
  - Files: `tests/test_network_service.cpp`.
  - Check: the `QMap<QString, QVariantMap>` `GetSettings` response still
    yields `FakeWiFi` as the active connection name.
  - Result: the fake accepts a fixture-selected connection ID and the new
    `ActiveConnectionNameReadsMapEncodedSettings` test verifies that a
    map-encoded response reaches the emitted backend state.

## Validation and Handoff

- [x] T-159: Run focused and project validation.
  - Check: focused NetworkManager backend tests, `task test`,
    `task format-check`, and `git diff --check`.
  - Record unrelated pre-existing failures separately.
  - Result: the focused NetworkManager selection passed 6 tests and `task
    test` passed all 935 tests. `task format-check` reports only the four
    pre-existing formatting violations in
    `libs/holonight-core/src/HyprlandWorkspaceService.cpp`; both Phase 19 C++
    files were formatted with `clang-format`. `git diff --check` passed.

- [x] T-160: Record live acceptance and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the unscheduled Low
    backlog from 52 to 51 only after all acceptance evidence is recorded.
  - Result: user verified the completed phase. `2b0c8c7` (`fix: complete
    phase 19 network remediation`) implements U-04 I-05; the 51 other
    Low-severity candidates remain queued.
