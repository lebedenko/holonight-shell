# Phase 26 — Weather Coordinate Validation

**Status**: Complete — implementation, automated validation, and user verification passed.

## Objective

Remediate U-01 I-10: reject malformed configured weather coordinates before
they can reach the OpenWeatherMap request path.

| Source | Phase 26 item | Impact |
|---|---|---|
| U-01 I-10 | Weather coordinate range validation | Hand-edited invalid latitude and longitude values no longer configure API requests with impossible coordinates. |

## Functional Requirements

### REQ-F-01 — Validate optional weather coordinates

The configuration parser shall accept configured latitude only within
`[-90, 90]` and longitude only within `[-180, 180]`.

- Each coordinate remains optional; an absent coordinate continues to enable
  IP-geolocation fallback.
- Values outside their respective ranges, non-finite values, and values of an
  incompatible TOML type shall be ignored and logged through the existing
  configuration logging category.
- Valid boundary values, including `0.0`, shall be preserved.

**Acceptance**: valid boundary coordinates remain available to the weather
service; out-of-range coordinates are absent from the parsed configuration and
cannot be used to form a weather API request.

## Constraints and Verification

- Keep validation at the TOML configuration boundary.
- Reuse existing parser logging and optional-coordinate semantics.
- Run focused config-parser tests, `task test`, `task format-check`, and
  `git diff --check`.

## Out of Scope

- Changing weather-provider or geolocation response validation.
- Clamping malformed configured coordinates instead of rejecting them.
- The remaining 45 queued Low-severity candidates.
