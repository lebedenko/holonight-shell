# Phase 31 — Portal D-Bus Adapter Naming

**Status**: Complete — automated checks and user verification passed.

## Objective

Remediate U-05 I-05: make the portal broker client-side D-Bus adapter
unambiguous beside the unrelated server-side `SettingsPortalBackend`.

| Source | Phase 31 item | Impact |
|---|---|---|
| U-05 I-05 | Name portal broker D-Bus adapter accurately | Portal client and portal implementation responsibilities are distinguishable from filenames and includes. |

## Functional Requirements

### REQ-F-01 — Name the adapter for what it represents

The file pair defining `IPortalDBus`, `SystemPortalDBus`, and
`NullPortalDBus` shall use a portal-D-Bus-adapter name rather than the
misleading `NullPortalBackend` name.

- `PortalService` and its tests include the renamed file pair.
- The production adapter, injectable interface, and test stub retain their
  existing class names and behavior.
- `SettingsPortalBackend` remains the separately named server-side
  implementation of `org.freedesktop.impl.portal.Settings`.

**Acceptance**: source and test includes clearly separate the portal broker
adapter from the Settings portal backend; portal service behavior and focused
tests remain unchanged.

## Constraints and Verification

- This is a naming-only remediation: do not alter D-Bus methods, timeouts,
  service registration, ownership, or QML-facing APIs.
- Verify CMake's existing recursive source discovery and update current
  source/test includes as part of the atomic rename.
- Preserve historical SDD records; they describe the filenames that existed at
  the time of their completed phases.
- Run focused portal tests, `task test`, `task architecture-check`,
  `task format-check`, and `git diff --check`.

## Out of Scope

- Renaming `SettingsPortalBackend` or changing its D-Bus service contract.
- MIME-cache, portal timeout, or portal color-scheme work.
- The remaining queued Phase 7 Low-severity candidates.
