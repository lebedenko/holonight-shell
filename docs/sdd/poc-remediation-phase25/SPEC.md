# Phase 25 — D-Bus Service Registration Diagnostics

**Status**: Complete — implementation, automated validation, and user verification passed.

## Objective

Remediate U-01 I-03: make failed `QtDbusPropertyClient::serviceRegistered()`
probes observable through the existing D-Bus logging category.

| Source | Phase 25 item | Impact |
|---|---|---|
| U-01 I-03 | Service-registration failure diagnostics | NetworkManager availability checks and power-profile service selection retain `false` results while emitting actionable failures. |

## Functional Requirements

### REQ-F-01 — Log D-Bus lookup failures

`QtDbusPropertyClient::serviceRegistered()` shall emit a warning when it cannot
obtain the bus interface or when `isServiceRegistered()` returns an invalid
D-Bus reply.

- The warning shall identify the service being checked.
- Invalid replies shall include the D-Bus error message.
- A valid negative reply (the service is simply not registered) shall still
  return `false` without a warning.
- The method's interface and boolean result contract shall remain unchanged.

**Acceptance**: a service-registration transport failure returns `false` and
emits a warning identifying the queried service; a registered service remains
silent and returns `true`.

## Constraints and Verification

- Reuse `lcDbusPropertyClient`, matching the existing property-operation logs.
- Keep the change local to the platform D-Bus client and its unit test.
- Run focused platform tests, `task test`, `task format-check`, and
  `git diff --check`.

## Out of Scope

- Retrying, caching, or changing service-availability policy in callers.
- Redesigning the `DbusPropertyClient` interface or D-Bus connection seam.
- The remaining 46 queued Low-severity candidates.
