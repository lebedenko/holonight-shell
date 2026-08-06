# Phase 19 — NetworkManager Settings Map Access

**Status**: Complete — implementation and live acceptance passed.

## Objective

Remediate U-04 I-05: simplify `QtNetworkManagerBackend::connectionId()` so
it reads the `connection` group from NetworkManager's `GetSettings` reply
without copying every settings group through an intermediate `QVariantMap`.

| Source | Phase 19 item | Impact |
|---|---|---|
| U-04 I-05 | Avoid the `QMap<QString, QVariantMap>` to `QVariantMap` settings copy | Active connection name extraction remains identical while avoiding an unnecessary full-map copy. |

## Functional Requirements

### REQ-F-01 — Extract only the requested settings group

`connectionId()` shall obtain only the `connection` settings group needed to
return its `id`, rather than materializing a second map containing every
NetworkManager settings group.

- It shall continue to return an empty string for D-Bus errors, missing
  arguments, missing `connection` groups, and missing IDs.
- It shall retain support for both reply representations accepted today: an
  already-converted `QVariantMap` and a `QDBusArgument` containing the
  NetworkManager settings map.
- The public backend API and emitted `NetworkBackendState` values shall remain
  unchanged.

**Acceptance**: focused tests demonstrate that the backend still reports the
active connection name from the existing D-Bus fake, including its
`QMap<QString, QVariantMap>` `GetSettings` reply.

## Constraints and Verification

- Keep the change scoped to U-04 I-05; do not alter NetworkManager polling,
  error-reporting policy, or D-Bus endpoint selection.
- Reuse the existing `tests/test_network_service.cpp` D-Bus fake; do not add
  a live NetworkManager dependency.
- Run focused NetworkManager backend tests, `task test`, `task format-check`,
  and `git diff --check`.

## Out of Scope

- The other 51 queued Low-severity candidates after this planned tranche.
- U-04 I-01/I-07 destructor safety, I-08 error detail, and I-09 polling work.
- Any change to NetworkManager's D-Bus wire format or connection settings
  schema.
