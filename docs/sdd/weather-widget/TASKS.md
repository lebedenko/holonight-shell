# SDD Tasks — weather-widget

- [x] T-001: Define weather data structures (Q_GADGET CurrentWeather, HourlyEntry, DailyEntry, WeatherCache)
  - REQs: REQ-F-007, REQ-F-008, REQ-NF-003
  - Check: WeatherData.h compiles; Q_GADGET properties are accessible from QML via qmlRegisterSingletonInstance

- [x] T-002: Implement ConfigService weather config parsing and storage
  - REQs: REQ-F-021, REQ-C-006, REQ-NF-006
  - Check: ConfigService::weather() returns a WeatherConfig with correct defaults; config reload signals fire and update WeatherService on api_key/latitude/longitude changes

- [x] T-003: Implement WeatherProvider network layer with OWM API calls
  - REQs: REQ-F-002, REQ-F-001, REQ-C-001, REQ-C-003, REQ-C-010, REQ-NF-001, REQ-NF-003
  - Check: WeatherProvider::fetchCurrentWeather and fetchForecast issue HTTP GET with correct query parameters; wind speed m/s→km/h conversion yields 3.04×3.6→11 (rounded); geolocation parses ipgeolocation.io v3 JSON (latitude/longitude as strings, location.city extracted)

- [x] T-004: Implement WeatherProvider JSON parsing for current weather and forecast
  - REQs: REQ-F-002, REQ-F-007, REQ-F-008
  - Check: currentFetched signal contains CurrentWeather with temperature, humidity, windSpeed, visibility, pressure, condition, sunrise/sunset; forecastFetched signal contains QList<HourlyEntry> with timestamp, temperature, conditionId, precipitation, iconCode

- [x] T-005: Implement daily aggregation algorithm in WeatherProvider
  - REQs: REQ-F-008
  - Check: aggregateDaily() buckets hourly entries by local calendar date, computes min/max temperature, selects condition from noon-nearest entry; output has exactly 5 entries for valid 40-entry input

- [x] T-006: Create WeatherService singleton with lifecycle, backoff state machine, config wiring
  - REQs: REQ-F-001, REQ-F-005, REQ-F-006, REQ-F-023, REQ-C-005, REQ-C-012, REQ-NF-001, REQ-NF-006
  - Check: start() guards double-start; applyConfig() re-computes configured flag; exponential backoff sequence 1s→2s→4s→8s on failure and resets to base on success; onConfigChanged() slot restarts polling without app restart

- [x] T-007: Implement WeatherService cache load/save (XDG_CACHE_HOME/$HOME/.cache/holonight/weather.json)
  - REQs: REQ-F-003, REQ-C-004, REQ-NF-002
  - Check: loadCache() reads JSON synchronously at start before any network call; saveCache() writes only on successful fetch; cache file contains current, hourly, daily, and fetchedAt keys; on first launch with no network, hasData remains false but no crash

- [x] T-008: Implement WeatherService stale tracking (1-hour threshold, stale_timer, stale property)
  - REQs: REQ-F-004, REQ-F-006
  - Check: stale is true on startup if fetched_at + 3600s < now; onStaleTimer() fires 1h after last successful fetch and sets stale=true; config reload resets stale to false; staleChanged signal fires within 100ms of fetch success/failure

- [x] T-009: Implement WeatherService location resolution with config-first→IP-geolocation fallback
  - REQs: REQ-F-001, REQ-C-002
  - Check: with lat+lon in config, no geolocation call issued; without lat/lon and valid geo_api_key, geolocation proceeds and extracts city name; location_resolved flag prevents re-geolocation after startup

- [x] T-010: Implement WeatherService.iconPath() Q_INVOKABLE with OWM id→wsymbol mapping and day/night resolution
  - REQs: REQ-F-009, REQ-C-007
  - Check: iconPath(804, true) returns qrc:/HolonightShell/weather/wsymbol_0004_black_low_cloud.svg; iconPath(804, false) returns night variant; unmapped id 999 returns wsymbol_0999_unknown.svg; day/night determined by comparing timestamp to sunrise/sunset or icon code suffix fallback

- [x] T-011: Add weather sources to CMakeLists.txt (WeatherService.cpp/h, WeatherProvider.cpp/h, WeatherData.h)
  - REQs: REQ-C-006
  - Check: task build succeeds; weather .cpp/.h files are in holonight_services target; include directory added for src/services/weather

- [x] T-012: Bundle weather SVG assets via qt6_add_resources
  - REQs: REQ-C-007
  - Check: assets/weather/*.svg files are bundled with PREFIX /HolonightShell and BASE assets; QRC paths qrc:/HolonightShell/weather/<filename> resolve in QML Image source bindings; task build succeeds with GLOB check passing

- [x] T-013: Register WeatherService as QML singleton and wire into ShellApplication
  - REQs: REQ-C-005, REQ-C-011
  - Check: ShellApplication constructs WeatherService, calls reg() to register QML singleton, calls start(); QML components can import HolonightShell and bind to WeatherService.configured, WeatherService.current.temperature, etc.

- [x] T-014: Implement WeatherWidget.qml top-bar widget with icon, temperature, condition, collapse/expand, tooltip
  - REQs: REQ-F-010, REQ-F-019, REQ-F-020, REQ-F-022, REQ-NF-004
  - Check: widget displays 24px icon + temperature (cyan HoloniightPalette.accentCyan) + condition text (HoloniightPalette.onSurfaceVariant); implicitWidth collapses to 0 when hasData is false; no hardcoded hex colors; tooltip shows condition, feels-like, stale warning; barMonitorName propagated; icon resolved via WeatherService.iconPath(conditionId, isDay)

- [x] T-015: Generalize StatusPopup.qml with Loader for popupId-keyed content (title bar conditional for weather)
  - REQs: REQ-F-011, REQ-C-008
  - Check: Loader loads WeatherPopupContent.qml when popupId="weather"; title bar hidden for weather, visible for other popups; audio/network/battery/keyboard-layout popups remain unchanged (source=""→empty Loader); existing framework signals (cross-click, outside-click, Esc) still work

- [x] T-016: Extend StatusPopupSurface.cpp sizeForPopupId() for weather (460×560 px)
  - REQs: REQ-F-011
  - Check: sizeForPopupId("weather") returns {460, 560}; popup appears beneath weather widget on correct monitor; framework anchor logic routes popup to clicked monitor via barMonitorName

- [x] T-017: Create WeatherPopupContent.qml column of section components
  - REQs: REQ-F-011
  - Check: Column contains all 6 sections (current, details, hourly, daily, temperature graph, precipitation graph) plus attribution text; content fills panel without overlap; sections are loaded only when weather popup is active

- [x] T-018: Implement WeatherCurrentSection.qml (64px icon, temperature, condition, feels-like)
  - REQs: REQ-F-012, REQ-NF-004
  - Check: displays 64px icon centered, temperature (48pt cyan), condition text (uppercase, 14pt), feels-like (12pt accentBlue); no hardcoded hex; font family uses ThemeService.fixedFont

- [x] T-019: Implement WeatherDetailsGrid.qml (2×2 grid: humidity, wind, visibility, pressure)
  - REQs: REQ-F-013, REQ-NF-004
  - Check: grid displays 4 cells in 2 columns; wind speed correctly shown in km/h (converted from m/s); all units displayed (%, km, hPa); labels and values use correct HoloniightPalette colors; no hardcoded hex

- [x] T-020: Implement WeatherHourlyStrip.qml (horizontal scrollable/fixed strip of 6+ compact cards)
  - REQs: REQ-F-014, REQ-NF-004
  - Check: displays at least 6 hourly forecast cards; each card shows time (HH:MM), 24px icon, temperature; card icons resolved via WeatherService.iconPath(); all 40 hourly entries accessible via scroll or fixed display; temperatures match forecast data

- [x] T-021: Implement WeatherDailyCards.qml (5 daily cards with weekday, icon, high/low temps)
  - REQs: REQ-F-015, REQ-NF-004
  - Check: row displays 5 daily summary cards; weekday label correct (derived from DailyEntry.date); icon 32px resolved for day/night; high/low temps from tempMax/tempMin; card layout matches design; no hardcoded hex colors

- [x] T-022: Implement TemperatureGraph.qml (Canvas cyan polyline, hourly data, 5-day span)
  - REQs: REQ-F-016, REQ-NF-004
  - Check: Canvas draws cyan (#7dcfff via HoloniightPalette.accentCyan) polyline connecting 40 hourly temperature points; Y-axis scaled to min/max of data with labels at min/max/midpoint; X-axis labeled at 24-hour intervals; manual visual test confirms line matches hourly temps

- [x] T-023: Implement PrecipitationGraph.qml (Canvas violet bars, hourly 3h precip data)
  - REQs: REQ-F-017, REQ-NF-004
  - Check: Canvas or Repeater+Rectangle draws violet (#bb9af7 via HoloniightPalette.accentViolet) bars for each hourly entry with precipitation > 0; bar height proportional to precipitation mm; Y-axis labeled in mm; visual test on rainy forecast shows visible bars; zero-precip entries show minimal/no bars

- [x] T-024: Add a dedicated WeatherSection.qml to TopBar.qml with Loader gated on WeatherService.configured
  - REQs: REQ-F-023, REQ-C-006
  - Check: WeatherSection placed in TopBar between ActiveWindowSection and StatusesSection (its own bar section, NOT inside the statuses cluster); Loader.active bound to WeatherService.configured; when api_key added at runtime, configuredChanged flips active=true and the widget appears; when api_key removed, it disappears; no weather object exists in layout when unconfigured

- [x] T-025: Register 8 QML weather files in HOLONIGHT_QML_FILES (WeatherWidget, WeatherPopupContent, CurrentSection, DetailsGrid, HourlyStrip, DailyCards, TemperatureGraph, PrecipitationGraph)
  - REQs: REQ-C-006
  - Check: task build succeeds; CMake GLOB QML check passes; each file has correct QT_RESOURCE_ALIAS; QRC paths resolve (qrc:/HolonightShell/Topbar/WeatherWidget.qml, etc.)

- [x] T-026: Add attribution text "Source: OpenWeather" to WeatherPopupContent.qml footer
  - REQs: REQ-F-018, REQ-NF-004
  - Check: attribution text visible in popup footer (8–10pt, HoloniightPalette.accentBlue); exact text "Source: OpenWeather"; no hardcoded hex color

- [x] T-027: Full integration test: task build, task qml-lint, task format-check, task tidy all pass
  - REQs: REQ-NF-005, REQ-C-011
  - Check: `task build` completes without compiler warnings; `task qml-lint` reports zero errors in weather QML files; `task format-check` passes (code formatted per .clang-format); `task tidy` passes (no clang-tidy warnings)

- [x] T-028: Manual visual verification on live Wayland session
  - REQs: REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-014, REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-019, REQ-F-020, REQ-NF-002, REQ-NF-004, REQ-C-009
  - Check: widget visible in top bar with icon, temp, condition; cached data shows on startup without flicker; clicking widget opens popup beneath the widget; popup displays all 6 sections with correct data; temperature graph shows cyan line, precipitation graph shows violet bars; tooltip shows on 450ms hover with condition/feels-like/stale warning; popup dismisses on widget-click/outside-click/cross-switch/Esc; multi-monitor test shows popup opens on correct monitor; no hardcoded hex colors visible
