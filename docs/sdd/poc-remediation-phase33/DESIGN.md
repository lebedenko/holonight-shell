# Phase 33 — Portal Color-Scheme Protocol Constants: Design

**Input**: `poc-remediation-phase33/SPEC.md`
**Baseline**: Phase 32 accepted in `9f9d232`.
**Status**: Complete — automated checks and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | Define dark/light constants with `SettingsPortalBackend` and use them for the default and theme mapping | Existing `SettingsPortalBackendTest` read and read-all coverage, with explicit dark/light assertions as needed |

## 2. Design Decisions

### 2.1 Keep the protocol mapping local and integer-backed

The backend is the sole producer of the implementation-side Settings portal
value. Define its two named protocol values adjacent to the existing portal
keys, retaining their integer representation so `QDBusVariant` serialization
does not change. The names document the external contract without leaking a
new enum into unrelated services.

### 2.2 Cover the default and both mapping branches

The bare values currently occur in `Values::color_scheme` and in the resolved
scheme and mode-fallback branches of `colorSchemeForThemeConfig()`. Route all
three uses through the named values; this prevents the default from silently
drifting away from the mapping contract.

### 2.3 Preserve consumers as pass-through readers

`PortalService` consumes the D-Bus value as an integer and deliberately has no
literal comparison to replace. Its API and all D-Bus wire values remain
unchanged; the cleanup is confined to the server-side producer.

## 3. Risks and Boundaries

- Accidentally reversing the names would invert light and dark behavior.
  Existing read tests establish light as `2` and dark as `1`; focused coverage
  will make both cases explicit.
- Changing the storage type or D-Bus serialization is unnecessary and could
  alter a freedesktop protocol boundary, so this phase keeps the existing
  integer representation.
- This is a source-clarity cleanup with no live-compositor behavior change;
  focused automated verification is sufficient.
