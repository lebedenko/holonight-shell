# Weather artwork and troubleshooting browser

## Intent and interfaces

Replace popup layered PNG condition artwork with the approved 50-image collection.
Keep existing logical sizes and decode at display size times DPR. Solar and moon images
remain separate astronomy data, outside the browser. Measurement and compass SVGs remain.

`WeatherIconMapper` owns an ordered catalogue of provider, native integer code, canonical
description and artwork. Internal provider keys are `openweather` and `openmeteo`.
`WeatherIconBridge.iconFor` returns a single resource URL; `previewCatalogue` exposes
resolver inputs rather than URLs. The popup hero and forecasts use `WeatherIcon`. The topbar retains its original QML, SVG resources,
and legacy selection via `WeatherService.iconPath()`, including a native-WMO compatibility mapping.
Application and QML test resources use `holonight_weather_resources`.

Published code references: [OpenWeather](https://openweathermap.org/api/weather-conditions)
and [Open-Meteo](https://open-meteo.com/en/docs#weather_variable_documentation).
Include WMO 97 as heavy thunderstorm. Native codes are never converted across providers.
Popup OpenWeather descriptions stay exactly as supplied by the provider. Open-Meteo returns
numeric weather codes; its documented native descriptions are used without converting them
to another provider's terminology. WMO descriptions retain intensity,
freezing and hail qualifiers. Popup descriptions wrap rather than elide. Only the topbar
uses its existing presentation-only `shortCondition()` mapping; it never modifies shared data.

## Selection decisions

Clear and dry cloud categories use day/night variants. Overcast always uses `overcast`.
Unknown daylight gives clear conditions `unavailable` and other dry clouds `cloudy`.
Precipitation, thunderstorms and fog use neutral artwork regardless of cloud cover or daylight.
There is no celestial-visibility threshold, English hail heuristic or wind hurricane override.
Freezing drizzle, freezing rain, hail-bearing thunder, sleet and rain/snow mixes remain distinct.
Dense and shower drizzle use ordinary drizzle; shower sleet uses ordinary sleet; heavy rain and snow
showers use heavy rain and snow respectively; snow grains use snow. Heavy precipitation accompanying thunder does not imply heavy thunder;
only OpenWeather 212 and WMO 97 select severe thunder artwork. Volcanic ash is distinct.
Unknown codes or provider identity use `unavailable`. Unknown moon phase stays unknown.

Current daylight prefers valid sunrise/sunset and otherwise uses provider daylight.
A visible consumer refreshes current solar time each second. Hourly entries use their own
provider daylight. Future daily conditions use day presentation. Daily content stays unchanged.

## Startup and browser lifecycle

Application-owned `ShellRuntime` captures `HOLONIGHT_TROUBLESHOOT` once during startup,
accepting exactly `true`. The constant property is neither persisted nor re-read by popups.
A conditional Loader owns the checkable standard Test control in the Current Weather header; it starts unchecked.
Conditional Loaders own the adjacent header navigation and hero caption, destroyed on uncheck to restore live bindings.
Hero geometry and logical icon sizes stay identical with Test on or off.
Order is OpenWeather then Open-Meteo, ascending native code, day then night only when
artwork differs; neutral cases appear once. Navigation wraps and every enable resets to case 0.
Only hero artwork/caption are previews; requests, provider settings, live text and cache proceed.
Controls have accessible names and keyboard focus; caption wraps at the bottom of the icon column.
Approved PNGs have different painted footprints within their canvases; optical scaling remains unchanged.

## Cache and acceptance

Cache version 4 serializes provider identity, native condition ID, daylight value and
availability. Version 3 is rejected because conversion had already discarded distinctions.
Cached Open-Meteo descriptions are reconstructed from their native codes on load, so older
locally overridden labels cannot survive a restart. Cached OpenWeather response wording stays intact.
Fresh provider fetches follow the normal path. No recovery conversion is attempted.

Focused tests cover provider parsing, cache rejection/round trips, condition regressions,
independent browser code lists/order, unknown moon data, startup values, every PNG and
QML browser lifecycle/layout. Build, lint, qmltypes, formatting and compositor checks are
recorded in implementation verification; historical review evidence remains unchanged.
