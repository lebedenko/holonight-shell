# Weather Popup Location — EARS Specification

**Project:** holonight-shell (Qt6/QML Wayland shell)
**Version:** 1.0
**Date:** 2026-07-25
**Status:** Complete
**Source brief:** `/tmp/weather-popup.md`

## Overview

The weather popup currently presents measurements without identifying the place they describe. This is easy to miss at
home and ambiguous while travelling. This pipeline adds an always-visible, human-readable location subtitle directly
below the `CURRENT WEATHER` section heading.

The preferred presentation is:

```text
CURRENT WEATHER
Lviv, Ukraine
```

The location is supporting context, not another weather measurement. It therefore uses secondary text styling, consumes
no additional horizontal column, and does not change the current-condition content hierarchy.

## Scope

This pipeline includes:

- a stable weather-location value exposed by `WeatherService`;
- city and country resolution for pinned coordinates and IP-geolocated coordinates;
- `City, Country` display formatting without administrative regions;
- cache persistence so the location is available with cached weather at startup;
- a subtitle below the weather popup's `CURRENT WEATHER` heading;
- deterministic C++ and QML regression coverage.

This pipeline does not include:

- a clickable location picker;
- saved, recent, or multiple locations;
- switching between home and current location;
- a coordinate/debug tooltip;
- location display in the top-bar weather widget;
- a broader weather-popup visual redesign.

## Ubiquitous Language

- **Pinned location:** Coordinates supplied by `[weather].latitude` and `[weather].longitude`.
- **Automatic location:** Coordinates supplied by the configured IP-geolocation provider.
- **Resolved location:** A city and country associated with the coordinates currently used for weather requests.
- **Location label:** The user-visible `City, Country` string derived from a resolved location.
- **Administrative region:** A state, oblast, province, district, county, or equivalent subdivision. It is not part of
  the location label.

## Functional Requirements

### REQ-F-001 — Expose resolved location

**Template:** Ubiquitous

`WeatherService` SHALL expose the location label used by the active weather forecast as a read-only `QString`
`locationLabel` QML property with a NOTIFY signal.

**Acceptance criteria:**

- QML can bind to `WeatherService.locationLabel`.
- A change from `Lviv, Ukraine` to `Warsaw, Poland` emits exactly one location notification.
- Re-applying an equivalent resolved label emits no redundant notification.

### REQ-F-002 — Format city and country

**Template:** Ubiquitous

Where both a non-empty city and country are available, the system SHALL format the location label as
`<city>, <country>`.

**Acceptance criteria:**

- `Lviv` and `Ukraine` produce `Lviv, Ukraine`.
- `Munich` and `Germany` produce `Munich, Germany`.
- Leading and trailing whitespace is removed from both values.
- Empty components do not produce a dangling comma.

### REQ-F-003 — Exclude administrative regions

**Template:** Negative constraint

The system SHALL NOT include state, oblast, province, district, county, or other administrative-region fields in the
location label.

**Acceptance criteria:**

- A provider result containing city `Kyiv`, region `Kyiv City`, and country `Ukraine` displays `Kyiv, Ukraine`.
- A provider result containing city `Lviv`, region `Lviv Oblast`, and country `Ukraine` displays `Lviv, Ukraine`.
- No provider region field is stored in the weather cache or exposed to QML by this pipeline.

### REQ-F-004 — Resolve automatic-location names

**Template:** Event-driven

When IP geolocation resolves the weather coordinates, the provider SHALL also return the city and country name from that
same response, and `WeatherService` SHALL use them as the active resolved location.

**Acceptance criteria:**

- A successful automatic-location response supplies latitude, longitude, city, and country to `WeatherService`.
- Weather fetching continues with the returned coordinates.
- The location label is updated before or in the same event-loop turn as the associated weather fetch begins.

### REQ-F-005 — Resolve pinned-coordinate names

**Template:** Conditional

Where pinned coordinates do not have a complete configured city and country, the provider SHALL reverse-geocode those
coordinates through OpenWeather's geocoding API using the already-configured weather API key.

**Acceptance criteria:**

- Pinned coordinates with no configured name trigger one asynchronous reverse-geocoding request during location
  resolution, not on every weather refresh.
- A successful reverse-geocoding result supplies city and country but ignores its administrative-region field.
- Weather network requests remain asynchronous and do not block the QML thread.
- No additional credential or dependency is introduced.

### REQ-F-006 — Honor configured names

**Template:** Conditional

Where `[weather].city` and `[weather].country` are both non-empty, the system SHALL use those values for the resolved
location and SHALL NOT issue a reverse-geocoding request.

**Acceptance criteria:**

- Pinned coordinates with `city = "Lviv"` and `country = "Ukraine"` display `Lviv, Ukraine`.
- The configured values take precedence over provider spelling or localization.
- Existing configurations with only `city` remain valid; the missing country is resolved asynchronously.

### REQ-F-007 — Handle location-resolution failure

**Template:** Unwanted behavior

If name resolution fails while valid weather coordinates are available, the system SHALL continue fetching and
displaying weather data and SHALL retain the last known non-empty location label.

**Acceptance criteria:**

- A reverse-geocoding HTTP, network, or parse failure does not suppress weather fetching.
- If no prior label exists, the subtitle is omitted rather than showing an empty line, malformed punctuation, raw
  coordinates, or an error message.
- Location-resolution failure does not enter or advance the weather-data retry backoff.

### REQ-F-008 — Persist the location with weather cache

**Template:** Ubiquitous

The weather cache SHALL persist the resolved city and country alongside the weather snapshot, without persisting
coordinates or credentials newly as part of this pipeline.

**Acceptance criteria:**

- A cache written after successful location resolution restores the same location label on restart.
- Older cache files without location fields continue to load successfully.
- Cached location is available to QML before the first refresh completes.
- A new resolved location replaces stale cached location data.

### REQ-F-009 — Present location beneath the section heading

**Template:** Ubiquitous

`WeatherPopupContent.qml` SHALL display the non-empty location label immediately below `CURRENT WEATHER` and above the
existing current-weather content row.

**Acceptance criteria:**

- The order is section heading, location subtitle, current-weather content.
- The subtitle uses `HoloniightPalette.textSecondary` and the established UI font.
- The subtitle is elided if it cannot fit the available width.
- No pin icon, chip, frame, or other decorative container is added.

### REQ-F-010 — Preserve popup behavior and geometry

**Template:** Constraint

The implementation SHALL preserve the existing weather popup's width, height, scrolling behavior, current-condition
layout, dismissal behavior, and section ordering.

**Acceptance criteria:**

- Existing popup geometry constants are unchanged.
- The subtitle fits within the current top padding and content flow without clipping the current-weather row.
- Hourly, summary, and details sections remain in their current order and remain reachable.

## Non-Functional Requirements

### REQ-NF-001 — Theme and localization

All new QML strings SHALL use `qsTr()` where literal user-visible text is introduced, and all colors and fonts SHALL
come from existing palette and appearance services. Provider-returned proper names SHALL not be wrapped in `qsTr()`.

### REQ-NF-002 — Compatibility

Existing weather configurations and cache files SHALL remain valid. The optional `[weather].country` key defaults to an
empty string and has no missing-default write-back.

### REQ-NF-003 — Network isolation

Location-name lookup SHALL remain in `WeatherProvider`; retry, cache, precedence, and QML-facing state SHALL remain in
`WeatherService`. A name-resolution failure SHALL be independent of the current weather and air-pollution fetch cycle.

### REQ-NF-004 — Testability

Formatting, source precedence, provider parsing, cache migration, signal behavior, failure fallback, and QML subtitle
visibility SHALL be covered by deterministic tests without live network access or a Wayland compositor.

## Deferred Follow-up

A separate SDD may make the subtitle focusable/clickable and add `Search city…`, current/home locations, recent or saved
locations, and a diagnostic tooltip containing coordinates and detection source. Those interactions require their own
focus, persistence, privacy, and service API decisions and are intentionally not implied by this pipeline.

## Completion Criteria

- REQ-F-001 through REQ-F-010 and REQ-NF-001 through REQ-NF-004 are implemented.
- Focused configuration, provider, service, cache, and QML tests pass.
- `task qml-lint`, `task qmltypes-check`, and `task test` pass.
- The popup is visually checked in a live Hyprland session with pinned, automatic, cached, long-name, and unavailable
  location-label states.
- The user reviews the result and explicitly approves closing the pipeline.
