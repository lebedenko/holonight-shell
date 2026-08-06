# Phase 23 — Single-Lookup Battery Property Parsing

**Status**: Complete — implementation, automated validation, and user verification passed.

## Objective

Remediate U-01 I-05: parse each optional UPower battery property with one
ordered-map lookup, rather than a presence lookup followed by a second value
lookup.

| Source | Phase 23 item | Impact |
|---|---|---|
| U-01 I-05 | Single property lookup | Battery state refreshes avoid redundant `QVariantMap` tree searches while retaining the exact state-update contract. |

## Functional Requirements

### REQ-F-01 — Parse each present property from one lookup

`batteryStateUpdateFromProperties()` shall retrieve each supported UPower
property at most once when deciding whether to populate its corresponding
`BatteryStateUpdate` member.

- Missing properties shall remain unset.
- Present but invalid variants shall retain Qt's current conversion defaults.
- Rounding, state interpretation, and charge-end-threshold sentinel behavior
  shall remain unchanged.
- No public API, D-Bus subscription, or QML behavior changes.

**Acceptance**: the existing complete, partial, invalid-variant, and charge
threshold battery parsing scenarios retain their observed outputs after the
lookup refactor.

## Constraints and Verification

- Keep the change local to `BatteryState` parsing; do not alter battery-service
  state application or UI presentation.
- Prefer `QVariantMap`'s iterator API so key presence and the stored value are
  derived from the same lookup.
- Run focused battery-state tests, `task test`, `task format-check`, and
  `git diff --check`.

## Out of Scope

- The other 47 queued Low-severity candidates after this planned tranche.
- Battery percentage clamping or validation-policy changes.
- UPower D-Bus discovery, polling, or notification behavior.
