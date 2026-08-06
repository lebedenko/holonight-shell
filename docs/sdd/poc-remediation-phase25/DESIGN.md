# Phase 25 — D-Bus Service Registration Diagnostics: Design

**Input**: `poc-remediation-phase25/SPEC.md`
**Baseline**: Phase 24 accepted in `cbcefee`.
**Status**: Complete — implementation, automated validation, and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `libs/holonight-platform/src/DbusPropertyClient.cpp` | `tests/test_dbus_property_client.cpp` |

## 2. Design Decisions

### 2.1 Preserve negative availability as a normal result

A valid `false` reply means the checked service is absent, which is expected
for service discovery and fallback selection. Only transport/interface failures
produce warnings; this avoids noisy logs for ordinary absence checks.

### 2.2 Match established client diagnostics

Use `qCWarning(lcDbusPropertyClient)` and include the service name. Invalid
replies also include `QDBusError::message()`, matching `property()`,
`allProperties()`, and `setProperty()`.

### 2.3 Assert the externally observable diagnostic

Use QtTest's message expectation around an intentionally disconnected client
connection in the existing fixture. This validates the unavailable-interface
diagnostic without coupling the test to environment-specific D-Bus error text.

## 3. Risks and Boundaries

- The warning deliberately does not alter caller fallback behavior.
- The unavailable-interface branch is retained for disconnected/custom buses;
  it is logged but needs no separate integration fixture to establish the
  method's normal invalid-reply behavior.
