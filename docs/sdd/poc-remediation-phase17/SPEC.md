# Phase 17 — Power Process and Sysfs Input Hardening

**Status**: Complete — implementation and live acceptance passed.

## Objective

Remediate the two remaining Small, Low-severity U-03 findings that affect
process-timeout clarity and malformed sysfs input handling. Phase 8 already
completed U-03 I-01's UPower percentage clamp, so it is explicitly excluded
from this tranche.

| Source | Phase 17 item | Impact |
|---|---|---|
| U-03 I-03 | Name subprocess timeout policies | The shared two-second limit is discoverable at each owning process boundary. |
| U-03 I-10 | Check sysfs numeric extraction failures | Malformed backlight files cannot be treated as successful numeric reads. |

## Functional Requirements

### REQ-F-01 — Process timeouts have named policy

`SuspendInhibitorService` and `LogindSessionResolver` shall replace their bare
`2000`-millisecond subprocess wait/kill limits with named constants.

- The synchronous `busctl` wait and asynchronous `busctl` guard keep the same
  two-second duration.
- The `loginctl` fallback keeps the same two-second duration.
- This phase does not alter process arguments, failure handling, polling, or
  the existing asynchronous lifecycle.

**Acceptance**: existing focused inhibitor and logind resolver tests retain
their behavior; no bare `2000` subprocess timeout remains at the three cited
sites.

### REQ-F-02 — Sysfs numeric reads fail explicitly

`SysfsBackend` shall check the `QTextStream` result when reading
`max_brightness` and `brightness`.

- A malformed `max_brightness` value is ignored as an unusable device value.
- A malformed current-brightness value logs a warning and returns the existing
  safe fallback of `0`.
- Existing unreadable-file behavior, device choice, inotify wiring, and logind
  brightness writes remain unchanged.

**Acceptance**: focused deterministic coverage or a narrowly testable parsing
seam demonstrates valid values are retained and malformed values use the safe
paths; existing brightness-service tests continue to pass.

## Constraints and Verification

- Keep this to U-03 I-03/I-10; do not redesign `GuardedProcessRunner`, add
  process retries, or change timeout values.
- Do not rework U-03 I-01, I-04, I-06, or I-09: they were accepted in earlier
  phases.
- Do not add new QML APIs or alter backlight hardware-write behavior.
- Run focused tests, `task test`, `task architecture-check`,
  `task format-check`, and `git diff --check`.

## Out of Scope

- U-03 I-07's Power Profiles D-Bus reconnect lifecycle.
- The remaining 53 queued Low-severity candidates after this planned tranche.
- Backlight device-removal detection, retry policy, and UI changes.
