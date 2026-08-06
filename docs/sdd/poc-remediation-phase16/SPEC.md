# Phase 16 — Power Service Bounds and Cleanup

**Status**: Complete — implementation and live acceptance passed.

## Objective

Remediate three Small, Low-severity U-03 findings with a shared power-service
scope. This tranche bounds externally supplied brightness values, removes a
provably unreachable Power Profiles parsing branch, and gives the configurable
idle threshold's default a stable name.

| Source | Phase 16 item | Impact |
|---|---|---|
| U-03 I-04 | Remove dead `parseProfileNames()` branch | Profile parsing keeps its supported representations without redundant D-Bus argument handling. |
| U-03 I-06 | Clamp derived brightness percent | A raw backlight value outside its advertised range cannot expose a percentage below 0 or above 100. |
| U-03 I-09 | Name the default idle threshold | The QML-configurable five-minute default is discoverable and has one source of truth. |

## Functional Requirements

### REQ-F-01 — Brightness percent is a UI percentage

`BrightnessService` shall return and emit an integer brightness percentage in
the inclusive range `[0, 100]` whenever a backlight is available.

- Preserve zero-backlight behavior: the result remains `0` and no division is
  attempted.
- Preserve current rounding before clamping.
- Apply the same bound to the initial value and values received from the
  backend's external-change signal.

**Acceptance**: deterministic service tests cover initial and externally
reported raw values below zero and above `maxBrightness()`.

### REQ-F-02 — Profile parsing retains only reachable representations

`PowerProfilesService::parseProfileNames()` shall retain support for its
existing `QDBusArgument`, `QList<QVariantMap>`, and `QVariantList` inputs,
while removing the trailing branch that is unreachable after the explicit
`QDBusArgument` type check.

**Acceptance**: existing and/or focused tests show all supported profile-list
representations produce the same capability state; unsupported values yield no
profile names.

### REQ-F-03 — Idle default is named without behavior change

`IdleService` shall define its default idle threshold as a named constant and
use that constant for `idle_threshold_ms_` initialization.

- The default stays `300'000` milliseconds.
- The public property, setter, and persistence behavior do not change.

**Acceptance**: the existing default-threshold test remains explicit and
passes unchanged in behavior.

## Constraints and Verification

- Keep this to U-03 I-04/I-06/I-09; do not pull in U-03 I-03's timeout
  consolidation, I-07's D-Bus reconnect lifecycle, or I-10's sysfs parsing
  policy.
- Do not change QML names, user settings keys, or power-profile D-Bus
  representations.
- Add or refine deterministic C++ tests before project validation.
- Run focused tests, `task test`, `task architecture-check`,
  `task format-check`, and `git diff --check`.

## Out of Scope

- The other 55 queued Low-severity candidates after this planned tranche.
- UPower percentage validation in `BatteryService`/`LowBatteryMonitor`.
- Sysfs device discovery and malformed sysfs-file handling.
- Any live compositor UI redesign.
