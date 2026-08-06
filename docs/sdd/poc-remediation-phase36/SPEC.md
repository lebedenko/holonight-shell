# Phase 36 — Weather Input Resilience

**Status**: Complete — automated checks and user verification passed.

## Objective

Remediate three related weather-service Low-severity items from the Phase 7
backlog. This tranche gives the weather data boundary one reply-validation path,
keeps provider-derived percentage and AQI values within their documented ranges,
and restores compiler exhaustiveness diagnostics for weather-icon families.

| Source | Phase 36 item | Impact |
|---|---|---|
| U-07 I-01 | Restore exhaustive weather-icon family switches | Adding an icon family cannot silently map to an empty layer list. |
| U-07 I-03 | Consolidate network-reply JSON parsing | Weather and geolocation replies apply the same transport, HTTP, and JSON validation rules. |
| U-07 I-10 | Bound weather numeric data at the provider boundary | Malformed upstream percentage and AQI values cannot reach QML as impossible readings. |

## Functional Requirements

### REQ-F-01 — Weather-icon switches remain exhaustive

`dayLayers()` and `nightLayers()` shall explicitly handle every current
`Family` value without a `default:` label.

- `Family::Unmapped` continues to produce an empty layer list.
- Every supported OpenWeather condition code retains its current day and night
  layer sequence.
- A future `Family` enumerator must receive compiler diagnostics until both
  layer mappings are deliberately defined.

**Acceptance**: the mapper builds with the normal warning policy; unmapped
codes still return an empty list and the existing supported-code matrix retains
its current results.

### REQ-F-02 — Finished replies share one validation path

`WeatherProvider` shall use one internal helper to validate a finished network
reply and parse its JSON object for both weather/pollution and geolocation
flows.

- Network errors, non-success HTTP statuses, and malformed/non-object JSON
  remain failures that emit the existing appropriate error signal.
- The weather/pollution path retains endpoint context in its reported failure;
  geolocation retains its existing error behavior.
- Successful weather, pollution, and geolocation payload handling remains
  unchanged.

**Acceptance**: the existing deterministic provider parsing coverage and
focused implementation review confirm one helper owns successful-object and
failure-class validation without live network access.

### REQ-F-03 — Provider values respect weather-data invariants

The provider shall normalize externally supplied bounded values before they are
published in `CurrentWeather`, `HourlyEntry`, or `DailyEntry`.

- Humidity and cloud cover are constrained to 0–100.
- Probability of precipitation and moon phase are constrained to 0.0–1.0.
- Precipitation is never negative.
- Air-quality index is constrained to the OpenWeather 1–5 scale when present;
  the existing zero value continues to represent absent data.
- Valid in-range values and all unrelated temperature, time, wind, condition,
  and pollutant fields retain their existing conversions.

**Acceptance**: parsing deliberately out-of-range API objects exposes only the
specified boundary values, while existing normal-payload tests remain unchanged.

## Constraints and Verification

- Keep the change within `WeatherProvider`, `WeatherIconMapper`, and focused
  C++ tests; do not change weather polling/backoff policy, QML, API endpoints,
  cache format, or public QML property names.
- Preserve the existing message/signal ownership: the provider chooses whether
  a failure is weather or geolocation, and `WeatherService` continues to own
  retry policy.
- Use the existing test seam and deterministic Qt reply facilities; no live
  OpenWeather or geolocation request is permitted in automated coverage.
- Run focused weather-provider and icon-mapper tests, `task test`,
  `task architecture-check`, `task format-check`, and `git diff --check`.

## Out of Scope

- Structured authentication-failure handling and retry policy (U-07 I-07).
- Weather `QVariantList` caching (U-07 I-08) and calendar parser dispatch
  optimization (U-07 I-09).
- Launcher cache prepared-query reuse/schema recovery and every other queued
  Phase 7 Low-severity candidate.
