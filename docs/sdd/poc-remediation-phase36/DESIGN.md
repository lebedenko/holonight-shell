# Phase 36 — Weather Input Resilience: Design

**Input**: `poc-remediation-phase36/SPEC.md`
**Baseline**: Phase 35 accepted in `7ecfee3`.
**Status**: Complete — automated checks and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `weather-icon/WeatherIconMapper.cpp` family switches | existing icon-mapper condition matrix and unmapped-code test |
| F-02 | `weather/WeatherProvider.cpp` reply-validation helper | deterministic provider reply-success and failure-signal tests |
| F-03 | `weather/WeatherProvider.cpp` bounded-field parsing helpers | focused current, hourly, daily, and pollution out-of-range parsing tests |

## 2. Design Decisions

### 2.1 Preserve unknown-family behavior outside exhaustive switches

Remove the `default:` labels from the two `Family` switches and leave their
empty-list returns after the switches. The concrete `Family::Unmapped` cases
continue to document the intentional unknown-condition fallback, while a new
enum value produces a compiler diagnostic until both visual mappings are made.

### 2.2 Parameterize shared reply parsing with context, not policy

Move the common finished-reply work — consume body, inspect transport and HTTP
status, parse a JSON object — into the existing namespace-local helper. Give
the helper enough optional context to retain the weather path's endpoint-aware
error text without teaching it which signal to emit. `checkComplete()` and
`onGeoReply()` remain responsible for cleanup, logging, and their distinct
signals, so no retry or service policy moves across the provider boundary.

### 2.3 Normalize only values with documented closed ranges

Use small local normalization helpers at parse time for fields whose upstream
contracts already define a closed range. Missing values retain current defaults;
present but out-of-range values clamp to the nearest valid boundary. This keeps
the Q_GADGET data structures as plain value types and prevents impossible
values at the sole external-data ingress without inventing limits for
temperatures, wind, visibility, pollutant concentrations, or timestamps.

## 3. Risks and Boundaries

- Reply bodies are consumable once; the shared helper must consume each reply
  exactly once and callers must not read it again.
- HTTP success treatment must remain exactly compatible with the current
  provider behavior (a valid explicit status is successful only when it is
  200); tests must cover this boundary.
- AQI zero represents an absent or malformed field today. Clamping must not
  turn absence into a misleading AQI of 1.
- The numerical normalization should not alter valid API payloads or cache
  serialization; its scope is malformed upstream values only.
- This phase intentionally does not parse structured HTTP authentication
  failures or change retry/backoff behavior.
