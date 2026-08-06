# Phase 20 — NetworkManager Operation Error Detail

**Status**: Complete — implementation and live acceptance passed.

## Objective

Remediate U-04 I-08: retain NetworkManager's D-Bus error detail when an
asynchronous Wi-Fi operation fails, so the user and logs explain why the
requested action could not be completed.

| Source | Phase 20 item | Impact |
|---|---|---|
| U-04 I-08 | Preserve D-Bus error messages for failed NetworkManager operations | Wi-Fi scan, radio-state, activation, and disconnect failures retain their operation-specific context and expose useful D-Bus detail. |

## Functional Requirements

### REQ-F-01 — Preserve D-Bus operation detail

For a failed NetworkManager D-Bus call in `QtNetworkManagerBackend`, the
emitted `operationError` shall retain the existing operation-specific message
and include the non-empty error detail returned by D-Bus.

- Cover failed scan requests, Wi-Fi radio state changes, activation of saved
  networks, active-connection deactivation, and add-and-activate requests.
- Retain the existing generic context if D-Bus provides no detail.
- Do not add D-Bus details to local validation errors, such as an empty
  password or an absent active-connection path.
- Keep the public backend API and the operation scheduling behavior unchanged.

**Acceptance**: a session-bus fixture induces a deterministic operation
failure and a focused backend test observes the existing operation context
plus non-empty returned D-Bus detail in `operationError`.

## Constraints and Verification

- Keep the change scoped to U-04 I-08; do not alter polling, connection
  settings parsing, or NetworkManager endpoint selection.
- Reuse `tests/test_network_service.cpp` and its session-bus fake; do not add
  a live NetworkManager dependency.
- Run focused NetworkManager backend tests, `task test`, `task format-check`,
  and `git diff --check`.

## Out of Scope

- The other 50 queued Low-severity candidates after this planned tranche.
- U-04 I-01/I-07 destructor safety and I-09 polling consolidation.
- Retrying failed operations, changing error presentation in QML, or exposing
  raw D-Bus error names as a new public API.
