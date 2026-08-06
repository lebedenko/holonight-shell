# SDD Tasks — weather-popup-location

**Spec:** `docs/sdd/weather-popup-location/SPEC.md`
**Design:** `docs/sdd/weather-popup-location/DESIGN.md`
**Status:** Complete

- [x] T-001: Extend weather configuration with optional country metadata
  - REQs: REQ-F-002, REQ-F-006, REQ-NF-002
  - Change: Add `WeatherConfig::country`; parse and conditionally write `[weather].country`; update config
    documentation.
  - Check: Parser and writer tests cover present, absent, and city-only configurations.

- [x] T-002: Add the internal weather-location value type and format contract
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003
  - Change: Add `WeatherLocation`; add the pure trim/join formatter and guarded `locationLabel` setter in
    `WeatherService`.
  - Check: Unit tests cover two components, either component alone, empty values, whitespace, commas, and no redundant
    NOTIFY emission.

- [x] T-003: Preserve city and country from automatic geolocation
  - REQs: REQ-F-004, REQ-F-007, REQ-NF-003
  - Change: Parse the provider response's city and country and carry both through the geolocation signal.
  - Check: Provider and service tests prove the label is applied and weather fetching still starts with the resolved
    coordinates.

- [x] T-004: Resolve names for pinned coordinates
  - REQs: REQ-F-003, REQ-F-005, REQ-F-006, REQ-F-007, REQ-NF-003
  - Change: Add an asynchronous OpenWeather reverse-geocoding request with independent reply/error handling; merge its
    result with configured metadata.
  - Check: Tests prove complete configured names skip lookup, incomplete names trigger one lookup, `state` is ignored,
    and lookup failure neither delays nor retries weather data.

- [x] T-005: Persist resolved location in the weather cache
  - REQs: REQ-F-007, REQ-F-008, REQ-NF-002
  - Change: Add optional city/country cache serialization and restore it before network resolution.
  - Check: Tests load a legacy cache, round-trip a new cache, retain a cached label on resolution failure, and replace it
    after successful new resolution.

- [x] T-006: Expose the subtitle through QML test services
  - REQs: REQ-F-001, REQ-NF-004
  - Change: Add `locationLabel`, NOTIFY behavior, and a deterministic setter to `FakeWeatherService`.
  - Check: Existing QML harness registration and weather tests continue to initialize without warnings.

- [x] T-007: Add the weather popup location subtitle
  - REQs: REQ-F-009, REQ-F-010, REQ-NF-001
  - Change: Add a secondary, elided location `Text` immediately below `CURRENT WEATHER` in
    `WeatherPopupContent.qml`, without an icon or wrapper.
  - Check: QML tests verify ordering, visible/non-visible states, width containment, and no empty-label height gap.

- [x] T-008: Run focused automated verification
  - REQs: REQ-NF-004
  - Check: Covered by `task test` on 2026-07-25: all 985 registered tests passed; one unrelated
    compositor-dependent test was skipped by its fixture.

- [x] T-009: Run project-level QML and regression verification
  - REQs: REQ-F-010, REQ-NF-001, REQ-NF-002
  - Check: `task qml-lint`, `task qmltypes-check`, `task format-check`, and `task test` passed on 2026-07-25.

- [ ] T-010: Complete live compositor verification
  - REQs: REQ-F-003, REQ-F-007, REQ-F-009, REQ-F-010
  - Check: In a live Hyprland session, verify pinned and automatic locations, cached startup, a long location, dark and
    light themes, missing-label behavior, scrolling, and all existing popup dismissal paths. Capture a screenshot for
    review.
  - Finalization note: Deferred on 2026-07-25. The user requested pipeline finalization after automated verification;
    no live visual result is claimed.

- [x] T-011: User approval — close the SDD pipeline
  - REQs: SDD process
  - Check: The user explicitly requested finalization and commit on 2026-07-25.
