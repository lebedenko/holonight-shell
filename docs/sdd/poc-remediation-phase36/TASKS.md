# Phase 36 — Weather Input Resilience: Tasks

**Status**: Complete — automated checks and user verification passed.

## Pre-flight

- [x] T-243: Revalidate U-07 I-01, I-03, and I-10 against current HEAD.
  - Check: confirm both `Family` switches retain `default:`, weather and
    geolocation still parse finished replies through separate implementations,
    and provider parsing still publishes unbounded humidity/clouds/pop/moon
    phase/precipitation/AQI values; re-grep all existing weather test seams.
  - Result: both mapper switches still had `default:` labels, `checkComplete()`
    duplicated `parseReplyBody()`'s reply validation, and all scoped fields
    passed through without range normalization. Existing focused seams were in
    `test_weather_service.cpp` and `test_weather_icon_mapper.cpp`.

## Implementation and Tests

- [x] T-244: Restore exhaustive icon-family switch checking.
  - REQs: REQ-F-01
  - Files: `libs/holonight-services/src/weather-icon/WeatherIconMapper.cpp`.
  - Check: remove only the two `default:` labels, retain `Family::Unmapped`
    handling and post-switch empty fallbacks, and preserve all layer ordering.
  - Result: `dayLayers()` and `nightLayers()` now explicitly handle
    `Family::Unmapped` and return an empty list after their exhaustive switches.

- [x] T-245: Consolidate finished-reply JSON validation.
  - REQs: REQ-F-02
  - Files: `libs/holonight-services/src/weather/WeatherProvider.cpp`.
  - Check: both completed weather/pollution and geolocation replies route
    through one parsing helper; error context and existing cleanup/signal
    behavior remain appropriate to each caller.
  - Result: `checkComplete()` and `onGeoReply()` both use `parseReplyBody()`;
    the weather path supplies its endpoint for its existing contextual errors,
    while geolocation retains its prior generic messages and signal handling.

- [x] T-246: Normalize bounded weather values at parse time.
  - REQs: REQ-F-03
  - Files: `libs/holonight-services/src/weather/WeatherProvider.cpp`.
  - Check: normalize only humidity, cloud cover, POP, moon phase,
    precipitation, and AQI; preserve absent-data defaults and every valid
    in-range field conversion.
  - Result: provider-local helpers clamp humidity/clouds, POP, moon phase, and
    precipitation; numeric AQI values clamp to 1–5 while missing AQI remains
    the existing zero value.

- [x] T-247: Add deterministic weather regression coverage.
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03
  - Files: `tests/test_weather_service.cpp`, `tests/test_weather_icon_mapper.cpp`.
  - Check: assert the existing mapper matrix/unmapped fallback and
    out-of-range current, hourly, daily, and pollution payloads yield bounded
    observable data; review that the shared reply helper retains each caller's
    success/failure ownership without adding a live network dependency.
  - Result: added deterministic parsing tests for percentage, POP, moon-phase,
    precipitation, and AQI boundaries. Existing mapper tests retain the full
    supported-condition matrix and unknown-code fallback coverage.

## Validation and Handoff

- [x] T-248: Run focused and project validation.
  - Check: focused weather-provider and icon-mapper tests, `task test`,
    `task architecture-check`, `task format-check`, and `git diff --check`;
    record unrelated existing failures separately.
  - Result: focused `WeatherProviderParsing.*` and `WeatherIconMapper.*`
    coverage passed 37/37; `task test` and `task architecture-check` passed.
    Direct changed-file `clang-format --dry-run --Werror` and `git diff --check`
    passed. `task format-check` reports only the four pre-existing formatting
    violations in `libs/holonight-core/src/HyprlandWorkspaceService.cpp` at
    lines 56, 232, 257, and 295.

- [x] T-249: Record user acceptance and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the queued Low backlog
    from 32 to 29 only after acceptance evidence is recorded.
  - Result: user verified weather data remains usable while malformed bounded
    values are normalized. `2e4e5b6` (`fix: harden weather provider inputs`)
    implements accepted U-07 I-01, I-03, and I-10; the other 29 Low-severity
    candidates remain queued.
