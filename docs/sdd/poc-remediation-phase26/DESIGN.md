# Phase 26 — Weather Coordinate Validation: Design

**Input**: `poc-remediation-phase26/SPEC.md`
**Baseline**: Phase 25 accepted in `3c291a1`.
**Status**: Complete — implementation, automated validation, and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `libs/holonight-config/src/ConfigParsers.cpp` | `tests/test_config_parsers.cpp` |

## 2. Design Decisions

### 2.1 Validate at the configuration boundary

`WeatherService` treats present latitude and longitude as immediately usable.
The parser is therefore the narrowest boundary that can prevent malformed
hand-edited values from becoming live API inputs.

### 2.2 Reject rather than clamp

Clamping a location silently changes the user's intended position to a pole or
date-line boundary. Invalid values instead become absent, preserving the
existing IP-geolocation fallback semantics when a geo key is configured.

### 2.3 Keep coordinate behavior symmetric

A small optional-coordinate parser applies both numeric-type and finite/range
checks, with each coordinate's real-world bounds passed explicitly. This avoids
duplicating validation branches while retaining coordinate-specific diagnostics.

## 3. Risks and Boundaries

- A single valid coordinate remains stored but does not configure weather by
  itself, matching the pre-existing all-or-nothing location rule.
- No provider-level policy changes are needed because malformed config values
  never enter `WeatherService`'s configured-coordinate path.
