# Live weather settings

The shared `[weather]` contract is documented in the [umbrella integration note](../../../docs/initiatives/weather-settings-sync.md).

Shell watches the product document through ConfigService. WeatherService snapshots provider,
credentials, location source and language to distinguish request changes from presentation changes.
Display units are independent of provider requests: all provider values and cached values use
Celsius, km/h, hPa, kilometres and millimetres. QML passes reactive unit labels to shared conversion
helpers, keeping icon thresholds and colour decisions on canonical values.

The injectable WeatherProvider dispatches OpenWeatherMap One Call plus pollution requests or an
Open-Meteo forecast request. Manual city-only documents are resolved with Open-Meteo geocoding;
explicit automatic mode uses only the IP key. Cancelling requests disconnects reply callbacks before
aborting them. Idle and lid gates also cover location resolution and configuration changes.

Cache version 2 hashes provider, location configuration and language, and stores resolved coordinates.
Credentials never enter the cache. Unversioned caches are discarded. Location changes clear displayed
data before resolution; matching cached data can remain available offline. A changed automatic
location clears data when the resolved coordinates differ. Open-Meteo has no AQI integration;
zero AQI renders unavailable. Moon phases use the existing synodic calculator.

Acceptance: fixture network replies, external temporary document edits, conversions, provider
switching, obsolete requests, cache compatibility, pause/resume, and QML preference/layout tests.
