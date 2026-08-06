# Weather Popup Location — Design

**Date:** 2026-07-25
**Status:** Implemented and verified
**Spec:** `docs/sdd/weather-popup-location/SPEC.md`

## 1. Current State

The project already has most of the location plumbing, but it stops short of a UI contract:

- `WeatherConfig` parses and round-trips an optional `city`, but `WeatherService` never reads it.
- `WeatherProvider::fetchGeolocation()` parses `location.city`, and its signal carries the city, but
  `WeatherService::onGeoFetched()` only logs it.
- Pinned coordinates bypass geolocation entirely, so they have no runtime place name unless config supplies one.
- `WeatherCache` stores weather data and its fetch timestamp, but no location identity.
- `WeatherPopupContent.qml` owns the `CURRENT WEATHER` heading and is the smallest component boundary for the subtitle.

The design completes those existing seams without moving popup ownership or broadening the weather data structs.

## 2. Data Flow

```text
                         [weather] config
                    lat/lon, city?, country?
                               |
                               v
                      WeatherService precedence
                      /                     \
        complete configured name       incomplete name
                 |                           |
                 |                    WeatherProvider
                 |                 reverseGeocode(lat, lon)
                 |                           |
                 +------------+--------------+
                              |
                       resolved city/country
                              |
                 locationLabel + weather cache
                              |
                              v
                 WeatherPopupContent subtitle

Automatic path:

ipgeolocation.io response -> lat/lon + city/country -> WeatherService
                                               \----> normal weather fetch
```

Location resolution is a startup/config-change concern. It is not repeated on each periodic weather refresh.

## 3. Configuration Contract

Add `QString country` beside the existing `WeatherConfig::city`.

```cpp
struct WeatherConfig {
  // existing fields...
  QString city;
  QString country;
  // existing fields...
};
```

`parseWeather()` reads `weather.country` exactly as it reads `weather.city`. `ConfigWriter` writes the key only when
non-empty. No `MissingDefaults` flag is added, preserving the current treatment of optional display metadata.

Precedence is:

1. Complete configured `city` + `country`.
2. Provider-resolved missing values for pinned coordinates.
3. City + country from automatic IP geolocation.
4. Last known cache value while new resolution is pending or fails.
5. Empty label if no human-readable value has ever been resolved.

Configured non-empty components win individually. For example, configured `city = "Lviv"` plus a reverse-geocoded
country `Ukraine` produces `Lviv, Ukraine`; the provider cannot overwrite the configured city.

## 4. Provider Interfaces

Introduce a small plain value type in `WeatherData.h`:

```cpp
struct WeatherLocation {
  QString city;
  QString country;

  bool operator==(const WeatherLocation&) const = default;
};
```

It is an internal C++ transfer type rather than a `Q_GADGET`: QML consumes only the final string. This avoids growing
the already large `CurrentWeather` gadget with non-meteorological state.

Change the automatic-location signal to carry the value type:

```cpp
void geoFetched(double lat, double lon, const WeatherLocation& location);
```

Add a distinct pinned-coordinate lookup:

```cpp
virtual void reverseGeocode(double lat, double lon, const QString& api_key);

void locationFetched(const WeatherLocation& location);
void locationError(const QString& message);
```

`reverseGeocode()` calls OpenWeather's reverse geocoding endpoint with `limit=1`. The parser reads only `name` and
`country`; it deliberately ignores `state` and `local_names`. OpenWeather returns a two-letter country code, which is
converted to a display country name with Qt's locale facilities. If conversion is unavailable, the code is retained as
a safe country fallback rather than inserting an administrative region.

The reverse-geocoding reply has its own pointer and cleanup path. It must not participate in `checkComplete()`, which
coordinates the weather and pollution replies, and its failure must not emit `fetchError`.

## 5. Service State and Resolution

Add to `WeatherService`:

```cpp
Q_PROPERTY(QString locationLabel READ locationLabel NOTIFY locationChanged)

[[nodiscard]] QString locationLabel() const;

Q_SIGNALS:
  void locationChanged();

private:
  void setResolvedLocation(const WeatherLocation& location);
  [[nodiscard]] WeatherLocation mergeConfiguredLocation(const WeatherLocation& provider_location) const;
  [[nodiscard]] static QString formatLocationLabel(const WeatherLocation& location);

  WeatherLocation location_;
  QString location_label_;
  bool location_name_pending_{false};
```

`formatLocationLabel()` trims both fields, joins two non-empty fields with `", "`, and returns the one available
component unchanged. It is pure and directly unit-testable.

For pinned coordinates:

- coordinates become usable immediately;
- a complete configured name is applied synchronously and weather fetching begins;
- an incomplete name starts reverse geocoding and weather fetching independently;
- the reverse-geocoding result later updates only location state.

This parallel behavior is intentional: place-name lookup must never delay meteorological data. A failure logs a warning
and leaves the previous label intact.

For automatic coordinates, the existing geolocation response already contains both naming and coordinate data, so
`onGeoFetched()` applies the location and starts the weather fetch in one path.

`setResolvedLocation()` compares the formatted label before emitting. This gives QML the smallest useful contract and
prevents redundant bindings from reevaluating.

## 6. Cache Migration

Extend `WeatherCache` with `WeatherLocation location`. Serialize it as:

```json
{
  "location": {
    "city": "Lviv",
    "country": "Ukraine"
  }
}
```

The object is optional on read. Existing cache documents therefore deserialize to an empty `WeatherLocation` without a
schema bump or destructive migration.

The cache remains weather-snapshot-owned:

- loading applies cached location before network resolution;
- successful name resolution updates in-memory location;
- the next normal successful weather save persists the location;
- no API keys or newly persisted coordinates are added.

If name resolution succeeds after the current weather cycle has already saved, `WeatherService` performs a cache save
when weather data exists so the resolved label is not lost on an immediate shutdown.

## 7. QML Layout

Keep `SectionLabel` and the current conditions within the existing first `Column`. Add one `Text` between the heading
and `RowLayout`:

```qml
Text {
    width: parent.width
    visible: text.length > 0
    text: WeatherService.locationLabel
    color: HoloniightPalette.textSecondary
    font.family: AppearanceService.uiFont
    font.pixelSize: AppearanceService.bodyFontSize
    elide: Text.ElideRight
}
```

The implementing task must confirm the actual available appearance size token; it must reuse an existing token rather
than introduce a hardcoded typography scale. The containing column already owns vertical flow, so the subtitle requires
no anchors and no new wrapper.

The existing current-weather row keeps its fixed height. The first column grows only by the subtitle's implicit height
and existing spacing. The popup's `Flickable` already derives `contentHeight` from `stack.height`, so smaller displays
remain scrollable without a geometry change.

All existing literal weather headings are outside this pipeline's scope. The new subtitle is provider/config data and is
not translated with `qsTr()`.

## 8. Test Strategy

### Configuration tests

- parse and preserve `weather.country`;
- keep absent country empty across writes;
- preserve existing city-only configurations.

### Provider tests

- parse automatic geolocation city and country;
- parse reverse-geocoding city and ISO country;
- ignore `state`;
- reject empty/malformed reverse-geocoding results;
- keep location errors separate from weather fetch errors.

### Service tests

- formatting for both, single, empty, and whitespace-padded components;
- configured-name precedence;
- reverse lookup only for incomplete pinned names;
- automatic-location propagation;
- no redundant `locationChanged`;
- weather fetch proceeds when reverse lookup fails;
- old-cache compatibility and new-cache round trip.

### QML tests

Extend the fake weather singleton with `locationLabel` and a setter. Verify that:

- `Lviv, Ukraine` appears below the current heading;
- an empty label does not reserve subtitle height;
- a long label elides within the content width.

### Validation

- focused weather/config GTests;
- focused QML smoke test;
- `task qml-lint`;
- `task qmltypes-check`;
- `task test`;
- live Hyprland visual check.

## 9. Rejected Alternatives

### Put the location beside the heading

This saves vertical space but competes with long localized place names and makes alignment dependent on popup width. A
subtitle is more robust and matches the source brief's preferred hierarchy.

### Put the location beneath the weather condition

This couples place identity to the large hero component and makes it less discoverable when scanning section headings.

### Display raw coordinates as fallback

Coordinates are useful diagnostics but poor identity text. The subtitle is omitted when a human-readable label is
unavailable; coordinates remain deferred to the proposed diagnostic tooltip.

### Store location in `CurrentWeather`

Location describes the request context, not an observation returned by the One Call current-weather object. Keeping it
in service/cache state prevents unrelated current-weather parsing and QML contracts from growing.

### Add the interactive picker now

Search, focus handling, saved locations, and persistence are a substantially larger product and state-management
surface. They should build on the stable display contract from this pipeline.
