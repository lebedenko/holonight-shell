# Phase 42 — NetworkManager Wi-Fi Activation Contracts

**Status**: Complete — automated checks and user verification passed.

## Objective

Fix the two linked NetworkManager D-Bus mapping defects found during Phase 41
live verification: saved Wi-Fi profiles are not reliably recognized after
disconnect, and password-based activation sends an incorrectly typed settings
payload.

## Functional Requirements

### REQ-F-01 — Saved Wi-Fi profiles remain known

The backend shall decode `Settings.Connection.GetSettings` using its nested
`a{sa{sv}}` D-Bus shape. A visible access point whose SSID matches a saved
wireless profile shall expose `known: true` and the saved connection object path
whether or not it is currently connected.

**Acceptance**: disconnecting a saved secured network and selecting it again
uses `ActivateConnection` without requesting a password.

### REQ-F-02 — New secured connections use the NetworkManager settings shape

`AddAndActivateConnection2` shall receive its settings argument as a nested
string-to-property-map value matching `a{sa{sv}}`. The payload shall retain the
SSID, connection type, IP auto-configuration, security key management, and
provided password.

**Acceptance**: entering the correct password for an unknown secured network
successfully creates and activates a persistent NetworkManager profile.

## Constraints and Verification

- Keep QML decision logic and public `NetworkService` APIs unchanged.
- Do not read, log, or persist passwords outside NetworkManager's existing
  activation call.
- Add deterministic fake-D-Bus coverage for both nested settings read and write
  paths.
- Run focused network tests, `task test`, `task architecture-check`, changed-file
  formatting checks, and `git diff --check`.
- Live-check saved-profile reconnect and first-time password activation before
  closing the phase.

## Out of Scope

- Network polling-loop consolidation, teardown behavior, Wi-Fi profile editing,
  enterprise authentication, and unrelated Phase 7 backlog candidates.
