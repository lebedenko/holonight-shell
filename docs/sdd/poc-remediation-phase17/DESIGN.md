# Phase 17 — Power Process and Sysfs Input Hardening: Design

**Input**: `poc-remediation-phase17/SPEC.md`
**Baseline**: Phase 16 accepted in `a93e26e`.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `SuspendInhibitorService.cpp`, `LogindSessionResolver.cpp` | existing inhibitor and resolver tests; source-level timeout-site check |
| F-02 | `brightness/SysfsBackend.cpp` | focused brightness/sysfs parsing coverage and existing brightness-service tests |

## 2. Design Decisions

### 2.1 Keep timeout ownership local

The same duration serves different commands and execution models: `busctl` is
owned by `SuspendInhibitorService`, while `loginctl` is owned by the logind
session resolver. Give each owner an aptly named local `constexpr`, reused by
both synchronous and asynchronous `busctl` paths. This documents policy
without manufacturing a cross-service timeout abstraction or changing the
established two-second limit.

### 2.2 Treat stream failure as invalid external input

After each integer extraction, inspect `QTextStream::status()`. Only a
successfully parsed value participates in selecting a backlight device or is
returned as current brightness. A failed brightness read retains the existing
safe `0` result but makes the reason observable through the brightness logging
category.

### 2.3 Preserve the production backend boundary

The backend continues to own `/sys/class/backlight` discovery and D-Bus
writes. If deterministic malformed-input coverage needs a small parsing seam,
keep it private to the brightness implementation and avoid adding a QML-facing
or general-purpose public API.

## 3. Risks and Boundaries

- Naming constants is behavior-preserving; accidental changes to units or
  values would affect process responsiveness, so tests and review confirm the
  retained 2,000 ms policy.
- Sysfs may be temporarily unreadable or malformed during hardware transitions.
  Rejecting stream failures avoids treating a stale/default integer as valid.
- This phase does not use the existing guarded-process utility because the
  finding is the unnamed policy, not a lifecycle defect; adopting it would
  broaden behavior and ownership beyond the remediation need.
