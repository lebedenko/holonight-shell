# Phase 23 — Single-Lookup Battery Property Parsing: Design

**Input**: `poc-remediation-phase23/SPEC.md`
**Baseline**: Phase 22 accepted in `4e8fa56`.
**Status**: Complete — implementation, automated validation, and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `libs/holonight-core/src/BatteryState.cpp` | `tests/test_battery_state.cpp` |

## 2. Design Decisions

### 2.1 Use one iterator lookup per supported key

For each supported UPower field, use `QVariantMap::constFind()` and compare the
result to `constEnd()`. The iterator establishes both presence and access to
the stored `QVariant`, avoiding the current `contains()` plus `value()` pair.

### 2.2 Preserve conversion semantics exactly

Continue applying the existing Qt conversion functions to the retrieved
`QVariant`. This keeps invalid inputs, rounding, UPower state mapping, and the
zero charge-end-threshold sentinel byte-for-byte equivalent in behavior.

### 2.3 Use existing parsing tests as regression coverage

`test_battery_state.cpp` already covers full and partial property maps,
invalid variants, state mapping, time/health/cycle values, and threshold
sentinels. Run these focused tests to prove the implementation-only refactor
did not change observable results; add a test only if an unrepresented branch
is discovered during implementation.

## 3. Risks and Boundaries

- `QVariantMap` is an ordered map, so repeated lookups are not free; this phase
  improves only that local cost and makes no claim about user-visible latency.
- Iterator lifetime is limited to the immutable function parameter; do not
  mutate the map while an iterator is used.
- Replacing conversion helpers or clamping values would expand the phase beyond
  the established parsing contract.
