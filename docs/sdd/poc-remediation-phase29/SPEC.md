# Phase 29 — Tray Item Membership Index

**Status**: Complete — implementation, automated validation, and user verification passed.

## Objective

Remediate U-02 I-10: avoid linear membership checks for every asynchronous
tray-item property-fetch completion while retaining the ordered registration
list required by the StatusNotifierWatcher D-Bus interface.

| Source | Phase 29 item | Impact |
|---|---|---|
| U-02 I-10 | Indexed tray-item membership | Async D-Bus completions test whether their item is still live directly. |

## Functional Requirements

### REQ-F-01 — Maintain direct membership lookup

`TrayWatcher` shall maintain a key set synchronized with its ordered registered
items list.

- Duplicate registrations remain ignored.
- Registration and removal keep the key set, ordered D-Bus list, and
  per-service item counts synchronized.
- Property-fetch completion guards use the key set.
- `RegisteredStatusNotifierItems` retains its existing ordered `QStringList`
  contract.

**Acceptance**: tray watcher tests preserve duplicate, removal, service-count,
and registration-order behavior while completion guards no longer scan the
registered items list.

## Constraints and Verification

- Do not change StatusNotifierWatcher D-Bus names, signals, or item lifecycle.
- Keep ordered client-visible registration data separate from set-like internal
  membership data.
- Run focused tray watcher tests, `task test`, `task format-check`, and
  `git diff --check`.

## Out of Scope

- Tray item rendering, icon decoding, D-Bus timeout policy, and service
  discovery behavior.
- The remaining queued Phase 7 Low-severity candidates.
