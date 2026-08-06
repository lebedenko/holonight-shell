# Phase 16 — Power Service Bounds and Cleanup: Design

**Input**: `poc-remediation-phase16/SPEC.md`
**Baseline**: Phase 15 accepted in `e7c81cd`.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `brightness/BrightnessService.cpp` | `test_brightness_service.cpp` |
| F-02 | `PowerProfilesService.cpp` | `test_power_profiles_service.cpp` |
| F-03 | `idle/IdleService.h` | `test_idle_service.cpp` |

## 2. Design Decisions

### 2.1 Clamp at the percentage boundary

`computePercent()` is the single conversion boundary between raw sysfs values
and the QML-facing percentage. Calculate with the existing rounding rule, then
clamp to `[0, 100]`. Both construction and the external-change slot already
flow through this function, so no duplicate clamping is needed elsewhere.

### 2.2 Delete, rather than refactor, unreachable parsing code

The explicit `QDBusArgument` branch is evaluated before the list
representations. Once it has not matched, the trailing conditional
`canConvert<QDBusArgument>()` path cannot add a supported representation.
Remove that tail and return the accumulated empty list after the supported
forms. Keep the parsing order and data conversion behavior intact.

### 2.3 Keep the idle default private and local

Introduce a local named `constexpr` in `IdleService.h`, alongside the member
that consumes it. This is configuration documentation, not a new public API;
the current literal remains in the test assertion so the externally observable
five-minute contract is independently checked.

## 3. Risks and Boundaries

- Drivers can briefly report an out-of-range raw value. Clamping only the
  derived percentage avoids altering hardware writes or backend state.
- `QVariant` profile representations must remain accepted in their current
  order; this phase removes only code proven unreachable by that order.
- Moving the idle literal must not change the setting's default or notification
  behavior.
