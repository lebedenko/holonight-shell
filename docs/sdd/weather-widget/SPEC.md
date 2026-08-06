# SPEC — weather-widget

## Overview

The `weather-widget` feature displays current weather conditions and a detailed forecast in the holonight-shell top bar, sourced from the OpenWeatherMap (OWM) 2.5 API. The top bar widget shows a weather icon, temperature, and condition text; clicking opens a rich popup with current conditions, a details grid, hourly and daily forecasts, and temperature/precipitation graphs. All data is cached locally for instant startup display and persists across restarts. The implementation follows established C++/QML patterns with async network calls, configuration via `config.toml`, and palette compliance via HoloniightPalette.

### Scope and Non-Goals

**v1 Scope:**
- OpenWeatherMap 2.5 Current Weather and Forecast 5 Day / 3 Hour APIs
- Location resolution: config-first (latitude + longitude) or IP-geolocation fallback
- Current conditions widget + detailed forecast popup
- Hourly forecast strip (3-hourly intervals, next 5 days)
- Daily forecast cards (aggregated min/max/condition from 3-hourly data)
- Temperature line graph and precipitation bar graph
- Cache persistence and stale-data handling with exponential backoff on failures
- Configuration via `[weather]` table in config.toml

**Explicit v1 Non-Goals (out of scope):**
- UV Index display
- Moon phase data
- Air quality index
- Dynamic accent tinting based on weather conditions
- Animated hero particles or visual effects
- Scroll-to-cycle forecast sections
- Multi-source weather providers or fallback provider chains

---

## Requirements

### Functional

#### REQ-F-001: WeatherService shall resolve location from config or IP geolocation
**Requirement:** The `WeatherService` C++ singleton shall determine the weather location using a two-tier strategy: (1) if both `latitude` and `longitude` are set in the `[weather]` config table, use them directly; (2) otherwise, perform a single IP-geolocation lookup at startup via `https://api.ipgeolocation.io/v3/ipgeo?apiKey=<geo_api_key>` and extract `location.latitude`, `location.longitude`, and `location.city` from the JSON response.

**Acceptance Criterion:**
- When config specifies `latitude = 51.5074` and `longitude = -0.1278`, the service uses these coordinates and does not perform IP geolocation
- When config omits latitude/longitude, the service queries ipgeolocation.io and extracts the correct coordinates and city name
- If IP geolocation fails (no network, HTTP error, malformed JSON), the service logs the error and does not crash; it retries on next startup
- The geolocation lookup blocks only at startup (not on any subsequent refresh)

---

#### REQ-F-002: WeatherService shall fetch current weather and forecast via OWM API
**Requirement:** The `WeatherService` shall issue two asynchronous HTTP GET requests at each refresh interval:
1. Current Weather: `https://api.openweathermap.org/data/2.5/weather?lat=<lat>&lon=<lon>&appid=<api_key>&units=<units>&lang=<lang>`
2. Forecast: `https://api.openweathermap.org/data/2.5/forecast?lat=<lat>&lon=<lon>&appid=<api_key>&units=<units>&lang=<lang>`

Both requests shall include query parameters: `lat`, `lon`, `appid` (from config), `units` (metric/imperial/standard from config, default "metric"), and `lang` (from config, default "en").

**Acceptance Criterion:**
- Both API calls are issued asynchronously (non-blocking)
- HTTP 200 responses with valid JSON are parsed correctly
- The service correctly constructs query strings with all required parameters
- Requests respect the configured `api_key` and geolocation data

---

#### REQ-F-003: WeatherService shall cache responses to disk
**Requirement:** After successful API responses, the `WeatherService` shall persist the combined current + forecast JSON to `$XDG_CACHE_HOME/holonight/weather.json` (fallback to `~/.cache/holonight/weather.json` if `XDG_CACHE_HOME` is unset). At startup, the service shall synchronously load this file (if it exists) and make the data immediately available to QML before any network request.

**Acceptance Criterion:**
- On first launch with no network, a blank weather widget is shown (section implicitWidth: 0)
- On second launch after a successful first fetch, the cached data appears immediately in the widget
- The cache file contains valid JSON combining current and forecast keys
- The cache persists across restarts and is updated only on successful API responses

---

#### REQ-F-004: WeatherService shall expose stale state to QML
**Requirement:** The `WeatherService` shall track whether the current cached data is stale (last successful fetch was > 1 hour ago, or no successful fetch has occurred since startup). A boolean `stale` property shall be exposed to QML with a NOTIFY signal; QML components may apply subtle visual treatment (e.g., reduced opacity, tooltip note) when `stale` is true.

**Acceptance Criterion:**
- `stale` is false immediately after a successful fetch
- `stale` becomes true if no successful refresh occurs for 60 minutes
- `stale` is true if the application starts with only cached data from a previous session
- QML bindings to `stale` update within 100 ms of a fetch success/failure

---

#### REQ-F-005: WeatherService shall implement polling on a configurable interval
**Requirement:** The `WeatherService` shall use a `QTimer` to periodically issue both current + forecast API calls at an interval specified by the `refresh_interval` config key (seconds, default 600, must be > 0). The timer shall start immediately after startup cache load.

**Acceptance Criterion:**
- The refresh interval reads from config and defaults to 600 seconds
- If `refresh_interval <= 0` in config, the service logs a warning and uses 600 as fallback
- The QTimer fires at the configured interval
- Two consecutive API calls (current + forecast) are issued within 100 ms of each other
- After a successful response, the next timer tick is 600 (or configured) seconds away

---

#### REQ-F-006: WeatherService shall handle API failures with exponential backoff
**Requirement:** On API failure (HTTP non-200, malformed JSON, network unreachable, or 401 Unauthorized), the service shall (1) retain the last successful cached data, (2) mark the data as stale, (3) log the error with category/code, and (4) implement exponential backoff for retries: next retry at 1s, then 2s, 4s, 8s, capped at the configured `refresh_interval`. A successful fetch resets the backoff to the base interval.

**Acceptance Criterion:**
- After a 401 (bad API key), the first retry occurs at 1s, second at 2s, third at 4s, etc.
- After a network timeout, the same backoff sequence applies
- The stale flag is set immediately on failure
- A successful fetch after 3 failures resets backoff; the next interval is 600s
- Cached data is shown in the widget even during failed fetches (not blank)

---

#### REQ-F-007: WeatherService shall expose current weather to QML as a property struct
**Requirement:** The `WeatherService` shall expose a QML-accessible property containing current weather data: `temperature`, `feelsLike`, `condition` (display string), `conditionId` (OWM numeric id), `humidity` (%), `windSpeed` (km/h, converted from OWM m/s), `visibility` (km), `pressure` (hPa), `iconCode` (e.g., "04n"), and `timeUpdated` (ISO 8601 string). These shall update via NOTIFY signal on fetch success.

**Acceptance Criterion:**
- QML can bind to `WeatherService.current.temperature` and receive updates
- `windSpeed` is correctly converted: OWM m/s × 3.6 = km/h (e.g., 3.04 m/s → 10.94 km/h, rounded to 11 km/h for display)
- `temperature` and `feelsLike` are in the units specified by OWM (°C for metric, °F for imperial)
- The property updates within 200 ms of a successful fetch

---

#### REQ-F-008: WeatherService shall expose forecast data to QML
**Requirement:** The `WeatherService` shall expose a QML-accessible list of forecast entries (hourly 3-hour intervals from the OWM 5-day forecast) and a derived list of daily summaries. Each hourly entry contains `timestamp`, `temperature`, `conditionId`, and `precipitation` (mm); each daily entry contains `date`, `tempMin`, `tempMax`, `conditionId`, and `condition`. Daily entries are computed by aggregating hourly entries: per calendar day, select the minimum temperature, maximum temperature, and the condition of the entry closest to noon UTC.

**Acceptance Criterion:**
- Hourly forecast list contains entries for the next 40 time steps (5 days × 8 per day)
- Each hourly entry has valid `timestamp` and `temperature` from OWM
- Daily forecast contains exactly 5 entries (one per calendar day)
- For a day with OWM entries at 00:00, 03:00, 06:00 … 21:00, the daily `tempMin` is the minimum of all entries and `tempMax` is the maximum
- The daily `condition` is the icon/id from the entry closest to 12:00 UTC
- The lists update within 200 ms of a successful fetch

---

#### REQ-F-009: WeatherService shall map OWM condition id and icon code to SVG asset paths
**Requirement:** The `WeatherService` shall maintain a deterministic lookup table mapping (OWM condition id, day/night) → SVG filename (e.g., "wsymbol_0001_sunny.svg"). The mapping shall support all OWM condition ids (200–900); unmapped ids shall default to "wsymbol_0999_unknown.svg". Day/night is determined by comparing the forecast/current `timestamp` to local sunrise/sunset times (from OWM `sys` object in current weather); if unavailable, use the `icon` code's d/n suffix. SVG assets are bundled via `qt6_add_resources` under `qrc:/HolonightShell/weather/` with absolute paths `/HolonightShell/weather/wsymbol_*.svg`.

**Acceptance Criterion:**
- OWM id 804 (overcast) on day → resolves to a specific wsymbol file (e.g., wsymbol_0004_black_low_cloud.svg)
- OWM id 804 on night → resolves to a night variant (e.g., wsymbol_0004_black_low_cloud_night.svg)
- OWM id 999 (unmapped) → resolves to wsymbol_0999_unknown.svg
- The QRC path for any resolved SVG is `qrc:/HolonightShell/weather/<filename>`
- A visual test shows the correct icon displayed for various weather conditions

---

#### REQ-F-010: WeatherWidget shall display icon, temperature, and condition text in the top bar
**Requirement:** A `WeatherWidget.qml` component shall render the current weather in the top bar as:
- A 24×24 px SVG image sourced from the mapped icon path
- The current `temperature` in large text (color: cyan #7dcfff from HoloniightPalette, no hardcoded hex)
- The `condition` text label (e.g., "Light Rain") in smaller text (color: #c0caf5 from HoloniightPalette)
- All elements are horizontally stacked with consistent spacing
- If `WeatherService` is **not configured** (see REQ-F-023), the widget is **not rendered at all** — not instantiated in the top bar
- If `WeatherService` is configured but has no data yet, the section collapses to `implicitWidth: 0` (the "absent" pattern used by BatterySection) until data arrives

**Acceptance Criterion:**
- The widget displays icon (24px), temperature (14pt+), and condition (10pt) in a readable layout
- Font family is JetBrains Mono or system monospace
- Colors match HoloniightPalette values (no hardcoded hex values in the QML file)
- When weather is not configured, no weather item exists in the top bar layout (verified via QML object tree, not merely width 0)
- On startup with cached data (and configured), the widget is immediately visible (no blank flash)
- On startup configured but with no data, the widget is absent (width: 0, height: 0) until the first fetch succeeds
- Clicking the widget opens the forecast popup (see REQ-F-011)

---

#### REQ-F-011: WeatherWidget shall integrate with StatusPopupSurface framework
**Requirement:** Clicking the `WeatherWidget` shall toggle a new popup with id "weather" via the existing `StatusPopupSurface` layer-shell framework. The popup shall be approximately 460 px wide (fixed size per design), tall enough for all content sections (500+ px), and display beneath the widget on the clicked monitor. The popup uses the established framework's visual treatment (notch, glow, cyan border, four dismissal paths: widget-click toggle, cross-widget switch, outside-click overlay, Esc key).

**Acceptance Criterion:**
- Clicking the weather widget opens the popup with 450 ms animation (framework default)
- The popup appears centered horizontally under the clicked monitor's weather widget
- The popup is dismissed by: (1) clicking the widget again, (2) clicking another service widget, (3) clicking outside, or (4) pressing Esc
- The popup has no title bar (unlike audio/network); content fills from the top
- Visual inspection on a multi-monitor setup shows the popup opens on the correct monitor

---

#### REQ-F-012: Forecast popup shall display current conditions section
**Requirement:** The forecast popup shall include a "Current Conditions" section at the top containing:
- A large weather icon (64×64 px)
- The `temperature` in largest font (36 pt+, cyan from HoloniightPalette)
- The `condition` text (e.g., "Light Rain", 14 pt, #c0caf5)
- "Feels like X°C" text (12 pt, #7aa2f7 from HoloniightPalette)

**Acceptance Criterion:**
- All elements are vertically stacked and centered
- Font sizes and colors match the design mockup (no hardcoded hex)
- The section is immediately visible when the popup opens
- Visual test: temperature and "feels like" are readable and properly formatted

---

#### REQ-F-013: Forecast popup shall display details grid
**Requirement:** Below the current conditions, a 2×2 grid displays:
- Humidity: `humidity` value + "%" (e.g., "65%")
- Wind: `windSpeed` in km/h (converted from OWM m/s) + " km/h" (e.g., "11 km/h")
- Visibility: `visibility` in km + " km" (e.g., "10 km")
- Pressure: `pressure` value + " hPa" (e.g., "1013 hPa")

Each cell has a label (gray, smaller font) and value (white, larger font). No hardcoded colors.

**Acceptance Criterion:**
- All four metrics are visible in a 2×2 layout
- Wind speed is correctly converted (e.g., 3.04 m/s → 11 km/h), not displayed in m/s
- All values are formatted with units (%, km, hPa)
- Colors use HoloniightPalette tokens (#c0caf5 for values, #7aa2f7 for labels)
- Manual test: hover over each cell and confirm the metric and value are correct

---

#### REQ-F-014: Forecast popup shall display hourly forecast strip
**Requirement:** A horizontal scrollable (or fixed-width) "Hourly Forecast" section displays the next several 3-hourly forecast entries as a strip of compact cards. Each card shows:
- Time (HH:MM format, derived from entry `timestamp`)
- Weather icon (24×24 px)
- Temperature (e.g., "12°")

Cards are arranged left-to-right with fixed width (e.g., 80 px each). The strip may be scrollable if it exceeds the popup width, or fixed-height with truncation to show ~6 cards at a time.

**Acceptance Criterion:**
- At least 6 hourly forecast cards are visible or scrollable
- Each card shows the correct time, icon, and temperature
- Temperature values match the hourly forecast data
- Icon paths are resolved correctly for each 3-hour interval
- Scrolling (if present) is smooth and does not stall the UI

---

#### REQ-F-015: Forecast popup shall display daily forecast cards
**Requirement:** A "Next 5 Days" section displays 5 daily summary cards in a row or grid:
- Weekday label (e.g., "Monday", from the date)
- Weather icon (32×32 px, representing the daily condition)
- High temp (e.g., "18°C", from daily `tempMax`)
- Low temp (e.g., "14°C", from daily `tempMin`)

Cards are stacked vertically or arranged in a compact grid layout. The daily condition icon is derived from the aggregated `conditionId` (closest to noon UTC).

**Acceptance Criterion:**
- All 5 days are visible or scrollable
- Weekday labels are correct and readable (no hardcoded day names; use locale/calendar)
- High and low temperatures are correctly aggregated from the hourly data
- Icons are resolved correctly for each day
- Card layout is consistent with the design mockup

---

#### REQ-F-016: Forecast popup shall display temperature line graph
**Requirement:** A "Temperature Trend" section displays a line graph with:
- X-axis: time (hourly entries from the forecast, labeled at 24-hour intervals or similar)
- Y-axis: temperature (°C or °F per config units)
- A cyan line (#7dcfff from HoloniightPalette) tracing the hourly temperatures across the 5-day forecast
- Grid lines (subtle, #1f1f2e or similar from palette)
- Y-axis labels at min/max/midpoint

The graph shall be interactive: hover to show a tooltip with the temperature and time for a specific point (optional, but recommended for usability).

**Acceptance Criterion:**
- The line graph displays 40 hourly data points (5 days)
- The line color is cyan (#7dcfff) from HoloniightPalette
- The graph scales correctly to the popup width/height
- Y-axis labels are readable and cover the temperature range in the data
- A manual visual test confirms the line matches the hourly forecast temps

---

#### REQ-F-017: Forecast popup shall display precipitation bar graph
**Requirement:** A "Precipitation" section displays a bar graph with:
- X-axis: time (hourly entries, labeled at 24-hour intervals or similar)
- Y-axis: precipitation (mm)
- Bars colored in violet (#bb9af7 from HoloniightPalette) for each 3-hourly interval
- Bar height proportional to `precipitation` (mm)
- Bars with zero precipitation are either omitted or shown as minimal height

The graph shall fit within the popup width and be readable at small scales.

**Acceptance Criterion:**
- Bars are displayed for each hourly entry with precipitation > 0
- Bar height is proportional to the precipitation amount (e.g., 5 mm → taller than 1 mm)
- Bar color is violet (#bb9af7) from HoloniightPalette
- Y-axis labels show precipitation units (mm)
- A manual visual test on a rainy forecast day shows visible bars; a clear day shows minimal/no bars

---

#### REQ-F-018: Forecast popup shall include attribution text
**Requirement:** At the bottom of the popup, display the text "Source: OpenWeather" in small gray text (8-10 pt, #7aa2f7 from HoloniightPalette) to comply with OWM API attribution requirements.

**Acceptance Criterion:**
- The attribution text is visible and readable in the popup footer
- The text is "Source: OpenWeather" (exact wording per OWM terms)
- No hardcoded hex color; uses HoloniightPalette

---

#### REQ-F-019: WeatherWidget hover tooltip shall display condition and action prompt
**Requirement:** A hover tooltip (using existing `BarTooltipArea` pattern) shall display:
- The current `condition` text (e.g., "Light Rain")
- "Feels like X°" (X from `feelsLike` property)
- "Click for forecast" action prompt
- A note "Stale (last update > 1h ago)" if `stale` is true

The tooltip shall appear on 450 ms hover delay (matching other bar tooltips).

**Acceptance Criterion:**
- Hovering over the weather widget for 450 ms shows the tooltip
- Tooltip displays the condition, feels-like, and action prompt
- If data is stale, the tooltip includes a stale warning
- Tooltip is dismissed on mouse exit
- Multi-monitor test: tooltip appears on the correct monitor

---

#### REQ-F-020: WeatherWidget shall propagate barMonitorName to child components
**Requirement:** Following the established architecture (per CLAUDE.md and mem:ActiveWindowService), the `WeatherWidget` shall receive `required property string barMonitorName` from `TopBar.qml` at construction and propagate it to all child components that may create popups or tooltips (e.g., `BarTooltipArea`). This ensures popups open on the correct monitor in a multi-monitor setup.

**Acceptance Criterion:**
- `TopBar.qml` sets `barMonitorName` via `setInitialProperties` before QML construction
- `WeatherWidget` receives the property and uses it to route popup display
- A multi-monitor test confirms the popup opens under the clicked monitor's widget, not a default/primary monitor

---

#### REQ-F-021: Config schema [weather] shall have documented defaults
**Requirement:** The config schema for `[weather]` table (validated and applied by `ConfigService`) shall include:
- `api_key` (string): default = `""` (empty — **no key is baked into the binary**; weather is disabled until the user supplies one)
- `geo_api_key` (string): default = `""` (empty — required only when latitude/longitude are unset, to enable IP-geolocation fallback)
- `latitude` (double, optional): default = unset (triggers geolocation fallback)
- `longitude` (double, optional): default = unset (triggers geolocation fallback)
- `city` (string, optional): default = unset (populated by geolocation response if available)
- `units` (string: "metric"|"imperial"|"standard"): default = "metric"
- `lang` (string): default = "en"
- `refresh_interval` (int, seconds): default = 600, must be > 0 (validate; warn and use 600 if invalid)

Config is loaded by `ConfigService` (same pattern as [appearance], [background]) and exposed to `WeatherService` via a NOTIFY signal on changes.

**Acceptance Criterion:**
- A default config.toml (or sample) documents the [weather] section with all keys and defaults
- `ConfigService` parses the [weather] table without crashing if it's missing (uses all defaults)
- If `refresh_interval <= 0`, the service logs a warning and uses 600
- If `latitude` or `longitude` is missing, geolocation is triggered (not an error)
- Config changes (e.g., user updates api_key) trigger a WeatherService reload

---

#### REQ-F-022: WeatherService shall handle missing or cached data gracefully on startup
**Requirement:** If the application starts with no network and no cached data exists:
- The `WeatherService.current` property shall be unset (empty/null QML value)
- The `WeatherWidget` shall render with `implicitWidth: 0` (invisible, no layout gaps)
- The geolocation and first API fetch shall proceed in the background
- Once the first fetch succeeds, the widget shall appear in the top bar with a smooth visual transition

**Acceptance Criterion:**
- A test on a first-ever launch with no network shows the weather widget absent (not blank/broken)
- After the first successful fetch (simulated or real), the widget appears
- The top bar layout does not shift or flicker when the widget appears
- The application does not crash or show errors if weather is unavailable at startup

---

#### REQ-F-023: WeatherService shall expose a `configured` state and gate the widget on it
**Requirement:** The `WeatherService` shall expose a read-only `configured` boolean property (with NOTIFY). Weather is considered **configured** when `api_key` is non-empty AND a usable location source exists — i.e. either both `latitude` and `longitude` are set in config, OR `geo_api_key` is non-empty (enabling IP-geolocation fallback). When not configured, the service shall NOT resolve location, NOT issue any network request, NOT start the refresh timer, and shall expose `configured = false` (and `hasData = false`). The `WeatherWidget` shall not be instantiated in the top bar while `configured` is false. The `configured` property re-evaluates on every config reload (REQ-NF-006), so adding an `api_key` at runtime makes the widget appear without an application restart.

**Acceptance Criterion:**
- With an empty `api_key`, `configured` is false, no HTTP request is ever issued (verified by network trace), and the top bar contains no weather item
- With `api_key` set but no `latitude`/`longitude` and empty `geo_api_key`, `configured` is false (no location source)
- With `api_key` set and `geo_api_key` set (no lat/lon), `configured` is true and geolocation proceeds
- With `api_key` set and `latitude`+`longitude` set, `configured` is true and geolocation is skipped
- Editing config.toml to add a valid `api_key` at runtime flips `configured` to true and the widget appears within one config-reload debounce cycle, with no app restart
- Removing the `api_key` at runtime flips `configured` to false and the widget disappears

---

### Non-Functional

#### REQ-NF-001: All network calls shall be asynchronous
**Requirement:** The `WeatherService` shall never block the Qt event loop for network requests. All API calls (geolocation, current weather, forecast) shall use `QNetworkAccessManager` with asynchronous signals/slots, and JSON parsing shall be non-blocking.

**Acceptance Criterion:**
- The application remains responsive (animations smooth, UI responsive to input) during network requests
- Network timeouts do not freeze the top bar
- A time profile (manual or automated) shows the main thread idle > 90% of the time during normal operation

---

#### REQ-NF-002: No UI flash on cache load
**Requirement:** The `WeatherWidget` shall display cached data immediately at startup without a visual flash, flicker, or blank state. Cached data shall be loaded synchronously in `WeatherService::init()` before any async network call.

**Acceptance Criterion:**
- A test with a populated cache file shows the weather widget visible within 50 ms of main window show
- No blank/missing widget state appears before cached data renders
- Switching between cached and fresh data (e.g., during a refresh) does not flicker

---

#### REQ-NF-003: Wind speed conversion shall be accurate
**Requirement:** The `windSpeed` property exposed to QML shall convert OWM m/s to km/h via multiplication by 3.6, rounded to the nearest integer for display. This conversion is a critical verification point given the difference in units.

**Acceptance Criterion:**
- OWM response `wind.speed = 3.04` (m/s) yields `windSpeed = 11` (km/h, rounded from 10.944)
- OWM response `wind.speed = 0.5` (m/s) yields `windSpeed = 2` (km/h, rounded from 1.8)
- OWM response `wind.speed = 10.0` (m/s) yields `windSpeed = 36` (km/h, rounded from 36.0)
- All conversions are tested in a unit test or integration test before release

---

#### REQ-NF-004: All colors shall be sourced from HoloniightPalette
**Requirement:** The `WeatherWidget` and weather popup QML components shall not contain hardcoded hex color values (e.g., `"#7dcfff"`, `"#bb9af7"`). All colors shall be imported via `import Holonight` and accessed via `HoloniightPalette.<token>`.

**Acceptance Criterion:**
- A code review of all weather QML files shows zero `#[0-9a-fA-F]{6}` hex values in color bindings
- Colors import via `import Holonight; HoloniightPalette.<token>` pattern
- The widget respects system theme changes (manual test: change HoloNight palette and confirm widget colors update)

---

#### REQ-NF-005: Code shall pass all linting and formatting checks
**Requirement:** All C++ and QML code for the weather feature shall pass `task format-check`, `task qml-lint`, and `task tidy` without errors or warnings.

**Acceptance Criterion:**
- `task build` completes without compiler warnings
- `task qml-lint` reports zero warnings for all weather QML files
- `task format-check` reports the code is correctly formatted per .clang-format
- `task tidy` reports zero warnings per .clang-tidy rules

---

#### REQ-NF-006: Config reload shall not restart the application
**Requirement:** If the user updates the `[weather]` config table (e.g., changes `api_key` or `latitude`), the `WeatherService` shall receive a NOTIFY signal from `ConfigService`, reload the config, and restart polling without requiring an application restart.

**Acceptance Criterion:**
- Editing config.toml to change `latitude` triggers a geolocation skip on the next refresh (uses new latitude)
- Changing `api_key` is reflected on the next API call (old key is discarded)
- No crash or UI stall occurs during config reload
- The stale flag is reset after config reload

---

### Constraints

#### REQ-C-001: Weather data source shall be OpenWeatherMap 2.5 API only
**Requirement:** Weather data shall be sourced exclusively from the OpenWeatherMap 2.5 Current Weather and Forecast 5 Day endpoints. No other weather APIs (e.g., Weather.gov, DarkSky, national weather services) shall be supported in v1.

**Acceptance Criterion:**
- The service makes HTTP requests only to `api.openweathermap.org`
- The code does not import or link against any alternative weather library or SDK
- The API payload is parsed from OWM's documented JSON schema

---

#### REQ-C-002: Location resolution shall support config-first strategy
**Requirement:** Location resolution shall prioritize the `[weather]` config table: if both `latitude` AND `longitude` are present, use them and skip geolocation entirely. Only if either is absent shall IP geolocation be performed.

**Acceptance Criterion:**
- A config with `latitude = 51.5` and `longitude = -0.1` skips the geolocation API call
- A config with only `latitude = 51.5` (no longitude) triggers geolocation
- A config with neither key triggers geolocation
- The decision is logged for diagnostics

---

#### REQ-C-003: Geolocation shall use ipgeolocation.io
**Requirement:** IP-based geolocation (fallback when config lacks latitude/longitude) shall use only `https://api.ipgeolocation.io/v3/ipgeo?apiKey=<key>`, extracting `location.latitude`, `location.longitude`, and `location.city` from the response JSON.

**Acceptance Criterion:**
- The geolocation query uses the documented ipgeolocation.io endpoint
- Latitude and longitude are correctly parsed from the `location` object
- City name is extracted and available to config (for display, optional use)

---

#### REQ-C-004: Cache location shall be $XDG_CACHE_HOME/holonight/weather.json
**Requirement:** The cache file path shall be determined by: (1) `$XDG_CACHE_HOME/holonight/weather.json` if `XDG_CACHE_HOME` is set, or (2) `~/.cache/holonight/weather.json` if not.

**Acceptance Criterion:**
- On a system with `XDG_CACHE_HOME=/custom`, the cache path is `/custom/holonight/weather.json`
- On a system without `XDG_CACHE_HOME`, the cache path is `~/.cache/holonight/weather.json`
- The service creates the `holonight/` directory if it does not exist (using `QDir::mkpath()` or similar)
- The cache file is world-readable; no sensitive API keys are stored

---

#### REQ-C-005: WeatherService shall register as a QML singleton
**Requirement:** The `WeatherService` shall be exposed to QML via `qmlRegisterSingletonInstance<WeatherService>()` in the C++ main function, following the pattern of `BatteryService`, `NetworkService`, etc. Only one instance shall exist throughout the application lifetime.

**Acceptance Criterion:**
- QML components can access the service via `import HolonightShell` and bind to `WeatherService.<property>`
- The singleton is initialized at application startup, before any QML component accesses it
- No crashes due to multiple instances or uninitialized service

---

#### REQ-C-006: Weather QML files shall follow src/qml/Topbar/ layout
**Requirement:** Weather-related QML files (`WeatherWidget.qml`, `WeatherSection.qml`, weather popup content components) shall be located in `src/qml/Topbar/` (or a subdirectory like `src/qml/Topbar/Weather/`) following the established per-directory layout. All files must be registered in `CMakeLists.txt` with correct `QT_RESOURCE_ALIAS` to strip the `src/qml/` prefix.

**Acceptance Criterion:**
- Files exist at `src/qml/Topbar/WeatherWidget.qml` and `WeatherSection.qml` (or similar)
- Each file is listed in `HOLONIGHT_QML_FILES` in CMakeLists.txt
- `task build` succeeds without missing-file errors
- QRC paths resolve correctly (e.g., `qrc:/HolonightShell/Topbar/WeatherWidget.qml`)

---

#### REQ-C-007: SVG assets shall be bundled under qrc:/HolonightShell/weather/
**Requirement:** Weather icon SVGs (wsymbol_*.svg files, currently staged in `assets/weather/`) shall be bundled via `qt6_add_resources` in CMakeLists.txt with `PREFIX "/HolonightShell"` and `BASE "assets"`. This results in QRC paths like `qrc:/HolonightShell/weather/wsymbol_0001_sunny.svg`.

**Acceptance Criterion:**
- SVG assets in `assets/weather/` are registered in CMakeLists.txt
- The QML Image component loads SVG assets using `qrc:/HolonightShell/weather/<filename>` paths
- A visual test displays icons correctly in the widget and popup
- `task build` succeeds without missing-resource warnings

---

#### REQ-C-008: StatusPopupSurface framework shall be extended without modification
**Requirement:** The weather popup shall integrate with the existing `StatusPopupSurface` layer-shell framework by extending its popupId and contentArea logic (via a Loader or similar QML pattern) without modifying the framework's core classes. The framework's C++ `sizeForPopupId()` shall include an entry for "weather" (460 px wide, 500+ px tall).

**Acceptance Criterion:**
- `StatusPopupSurface` C++ code is not modified; only extended (e.g., new case in `sizeForPopupId()`)
- QML content is loaded conditionally via a Loader keyed by `popupId`
- Existing popups (audio, network, battery, keyboard) remain functional and unchanged
- The weather popup uses the framework's notch, border, glow, and dismissal logic

---

#### REQ-C-009: Popup integration shall support multi-monitor routing
**Requirement:** The weather popup shall open on the correct monitor in multi-monitor setups, using the same `barMonitorName` routing pattern established for other bar popups (AudioPopup, NetworkPopup, etc.). The popup framework handles this via `anchor` positioning; the weather service/widget must propagate monitor information correctly.

**Acceptance Criterion:**
- A multi-monitor test with the popup open on HDMI-1 shows the popup opens under the weather widget on HDMI-1
- Clicking the widget on a second monitor (HDMI-2) switches the popup to HDMI-2 without jumping
- The framework's existing multi-monitor routing is reused; no new C++ code is needed for routing

---

#### REQ-C-010: No new external dependencies
**Requirement:** The weather feature shall use only Qt6 built-in modules (`QtNetwork`, `QtCore`, `QtQml`) and internal project libraries. No new third-party dependencies (curl, libcurl, boost, libuv, external JSON library) shall be added.

**Acceptance Criterion:**
- `QNetworkAccessManager` is used for HTTP requests (no curl)
- `QJsonDocument` and `QJsonObject` are used for JSON parsing (no external library)
- CMakeLists.txt does not add new `find_package()` or external dependency links
- `task build` succeeds with no new vcpkg or system library requirements

---

#### REQ-C-011: Implementation shall follow established C++/QML architecture patterns
**Requirement:** The `WeatherService` (C++) shall follow the pattern of `BatteryService`, `NetworkService`, `ActiveWindowService`, etc.: QObject singleton, Q_PROPERTY with NOTIFY signals, async network calls via QNetworkAccessManager, and QML integration via qmlRegisterSingletonInstance. The QML widgets shall follow `BarSection` composition and use `HoloniightPalette` for colors.

**Acceptance Criterion:**
- `WeatherService` class structure mirrors existing services (QObject, Q_OBJECT, Q_PROPERTY)
- Async network calls use QNetworkAccessManager signals (finished, error) not blocking calls
- QML components use the same BarSection + padding pattern as BatterySection
- Code review confirms architectural consistency with existing services

---

#### REQ-C-012: Logging shall use qCInfo/qCWarning for diagnostic output
**Requirement:** All diagnostic logging in `WeatherService` shall use `qCInfo()` and `qCWarning()` (with a named logging category) for output that is visible without debug environment variables. Debug-level output shall use `qCDebug()` and require `QT_LOGGING_RULES` to enable.

**Acceptance Criterion:**
- API fetch success/failure is logged with `qCInfo()` or `qCWarning()` (visible by default)
- Config reload events are logged (visible by default)
- Detailed JSON parsing traces use `qCDebug()` (hidden by default)
- No `std::cerr` or `std::cout` usage in service code (no mixed logging)

---

## Acceptance Criteria Summary

### Visual & Functional Acceptance (manual testing on a live Wayland session)

- [ ] Weather widget appears in top bar with icon, temperature, and condition text
- [ ] Widget displays cached data immediately on startup (no blank flash)
- [ ] Widget is absent (implicitWidth: 0) when no data is available
- [ ] Clicking the widget opens a popup beneath the widget
- [ ] Popup displays current conditions, details grid, hourly strip, daily cards, and graphs
- [ ] Temperature graph shows cyan line tracing hourly temperatures
- [ ] Precipitation graph shows violet bars for each 3-hourly interval
- [ ] Hovering widget shows tooltip with condition, feels-like, and "Click for forecast"
- [ ] Popup dismisses on: widget click, outside click, cross-widget switch, Esc key
- [ ] Multi-monitor test: popup opens under the correct monitor's widget

### API & Data Acceptance

- [ ] OpenWeatherMap 2.5 Current Weather API is called with correct lat/lon/appid/units/lang
- [ ] Forecast 5 Day API is called with same parameters
- [ ] JSON responses are parsed correctly; no crashes on malformed data
- [ ] Wind speed is converted: 3.04 m/s → 11 km/h (×3.6, rounded)
- [ ] Daily forecast aggregates min/max/condition from hourly data
- [ ] OWM condition ids map to SVG icons; unknown ids → wsymbol_0999_unknown.svg
- [ ] Day/night icon variants are selected correctly

### Configuration & Caching Acceptance

- [ ] Config `[weather]` table is parsed; all keys have documented defaults
- [ ] Config `latitude` + `longitude` present → no geolocation call
- [ ] Config `latitude` or `longitude` absent → IP geolocation is performed
- [ ] Cache file is written to $XDG_CACHE_HOME/holonight/weather.json or ~/.cache/holonight/weather.json
- [ ] Cache persists across restarts and is loaded synchronously at startup
- [ ] Config reload via ConfigService updates weather data without app restart
- [ ] `refresh_interval` <= 0 is rejected with warning; fallback to 600s

### Failure & Resilience Acceptance

- [ ] API failure (HTTP 401, timeout, network error) marks data as stale; last-good data is shown
- [ ] Exponential backoff: 1s, 2s, 4s, 8s on repeated failures; successful fetch resets to base interval
- [ ] App does not crash on network failure, malformed JSON, or missing cache
- [ ] Stale flag triggers tooltip warning "Stale (last update > 1h ago)"
- [ ] Offline startup (no network, no cache) shows absent widget; appears after first successful fetch

### Code Quality Acceptance

- [ ] `task build` completes without warnings
- [ ] `task qml-lint` reports zero errors in weather QML files
- [ ] `task format-check` passes; code is formatted per .clang-format
- [ ] `task tidy` passes; C++ adheres to .clang-tidy rules
- [ ] No hardcoded hex colors in QML; all colors from HoloniightPalette
- [ ] Logging uses qCInfo/qCWarning (visible by default), qCDebug (opt-in via QT_LOGGING_RULES)

### Integration Acceptance

- [ ] WeatherService is registered as QML singleton; `import HolonightShell` works
- [ ] WeatherWidget is included in TopBar.qml at appropriate position
- [ ] Popup uses StatusPopupSurface framework; no new framework code added
- [ ] barMonitorName is propagated for multi-monitor popup routing
- [ ] Weather does not block UI during network calls; animations/interactions remain smooth
- [ ] No new external dependencies; only Qt6 + internal libraries
