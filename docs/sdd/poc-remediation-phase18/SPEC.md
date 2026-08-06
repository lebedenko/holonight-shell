# Phase 18 — Power Profiles Signal Lifecycle

**Status**: Complete — implementation and live acceptance passed.

## Objective

Remediate the remaining U-03 Power Profiles reconnect finding: ensure a
`PropertiesChanged` D-Bus subscription is removed before its daemon service
state is discarded or replaced. This prevents redundant signal deliveries
after a power-profiles-daemon restart.

| Source | Phase 18 item | Impact |
|---|---|---|
| U-03 I-07 | Disconnect `PropertiesChanged` on daemon restart | One active subscription tracks the currently selected daemon endpoint. |

## Functional Requirements

### REQ-F-01 — Subscription ownership is explicit

`PowerProfilesService` shall keep enough state to identify whether its active
`PropertiesChanged` subscription was successfully established.

- When the watched daemon disappears, the service shall disconnect that active
  subscription before clearing its endpoint state.
- Before a newly selected service endpoint is connected, any prior active
  subscription shall be disconnected.
- Failed initial property reads or failed signal connections shall not create a
  disconnect attempt for a subscription that was never established.
- The freedesktop-preferred / hadess-fallback selection policy and all exposed
  availability/profile state remain unchanged.

**Acceptance**: deterministic tests demonstrate balanced connect/disconnect
calls across a simulated reinitialization and no disconnect for an unconnected
failure path.

## Constraints and Verification

- Keep this to U-03 I-07; do not redesign `DbusPropertyClient`, change daemon
  selection, or introduce D-Bus retries.
- Use the existing `disconnectSignal()` wrapper and the existing
  `onPropertiesChanged` slot signature.
- No QML API, UI behavior, or process/sysfs changes.
- Run focused Power Profiles tests, `task test`, `task architecture-check`,
  `task format-check`, and `git diff --check`.

## Out of Scope

- The other 52 queued Low-severity candidates after this planned tranche.
- Unrelated U-03 work already accepted in Phases 8, 16, and 17.
- Live power-profile policy changes or daemon restart orchestration.
