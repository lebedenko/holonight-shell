# Phase 19 — NetworkManager Settings Map Access: Design

**Input**: `poc-remediation-phase19/SPEC.md`
**Baseline**: Phase 18 accepted in `6ec9872`.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `NetworkManagerBackend.cpp` | `test_network_service.cpp` existing fake `GetSettings` response and active-state assertion |

## 2. Design Decisions

### 2.1 Select the connection group at the decoding boundary

`connectionId()` needs one nested group and one `id` value. Decode the reply
only far enough to select `connection`, then decode that selected value into a
`QVariantMap` if necessary. This removes the intermediate full settings copy
without changing how nested D-Bus values are interpreted.

### 2.2 Preserve both established reply representations

The current code accepts a normal `QVariantMap` and the explicit
`QDBusArgument` map representation. Keep both branches because the project
fake and real D-Bus peers can exercise the latter; the cleanup must not rely
on one particular marshalling path.

### 2.3 Extend the existing integration-shaped test

The NetworkManager backend fixture already exports `GetSettings()` as
`QMap<QString, QVariantMap>` and asserts the resulting active connection
name. Keep it as the regression boundary, adding only a focused assertion or
test when needed to make the settings-decoding path explicit.

## 3. Risks and Boundaries

- Collapsing directly to `QDBusReply<QVariantMap>` could accidentally narrow
  the legacy `QDBusArgument` handling path, so it is intentionally not the
  only decoding path.
- Do not change the empty-string failure contract: callers use it as the
  absent-active-connection result.
- The expected benefit is small and bounded to avoiding one temporary copy per
  active connection-state query; this phase is a maintenance cleanup, not a
  polling redesign.
