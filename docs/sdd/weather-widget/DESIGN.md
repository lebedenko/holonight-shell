# DESIGN — weather-widget

## Table of Contents

1. [Component Inventory](#1-component-inventory)
2. [Data Structures](#2-data-structures)
3. [WeatherService Class Design](#3-weatherservice-class-design)
4. [WeatherProvider Class Design](#4-weatherprovider-class-design)
5. [OWM Icon Mapping Table](#5-owm-icon-mapping-table)
6. [Daily Aggregation Algorithm](#6-daily-aggregation-algorithm)
7. [ConfigService Additions](#7-configservice-additions)
8. [StatusPopup.qml Generalization](#8-statuspopupqml-generalization)
9. [StatusPopupSurface C++ Extension](#9-statuspopupsurface-c-extension)
10. [QML Component Designs](#10-qml-component-designs)
11. [CMake Changes](#11-cmake-changes)
12. [ShellApplication Wiring](#12-shellapplication-wiring)
13. [Data Flow](#13-data-flow)
14. [Key Decisions, Alternatives, and Risks](#14-key-decisions-alternatives-and-risks)

---

## 1. Component Inventory

### 1.1 New Files

#### C++ — service layer (`src/services/weather/`)

| File | Purpose |
|------|---------|
| `src/services/weather/WeatherService.h` | QObject singleton exposed to QML; owns QTimer, backoff state, cache I/O, config wiring |
| `src/services/weather/WeatherService.cpp` | Implementation of the above |
| `src/services/weather/WeatherProvider.h` | Pure network adapter — builds OWM and ipgeolocation URLs, fires QNetworkAccessManager requests, emits parsed-data signals |
| `src/services/weather/WeatherProvider.cpp` | Implementation of the above |
| `src/services/weather/WeatherData.h` | Plain data structs: `CurrentWeather`, `HourlyEntry`, `DailyEntry`, `WeatherCache`; no QObject |

**Rationale for separating WeatherProvider from WeatherService:** WeatherService handles lifecycle (timer, backoff, cache, QML property updates). WeatherProvider handles network I/O and JSON parsing. This mirrors the `AudioService` + `PulseAudioBackend` split and keeps WeatherService unit-testable by injecting a mock provider.

**No separate WeatherModel class.** The forecast lists (`QList<HourlyEntry>` and `QList<DailyEntry>`) are exposed directly from WeatherService as `Q_PROPERTY` of type `QVariantList`. Each struct is a `Q_GADGET` (see §2), so QML can read fields by name without a `QAbstractListModel`. This avoids model lifecycle complexity for a fixed-size list (max 40 hourly entries, max 5 daily entries) that is always replaced wholesale on every fetch. A `QAbstractListModel` would add 300 lines of boilerplate for no incremental-update benefit.

#### QML — `src/qml/Topbar/`

| File | Purpose |
|------|---------|
| `src/qml/Topbar/WeatherSection.qml` | `Loader` that gates instantiation on `WeatherService.configured` (REQ-F-023); placed as a standalone sibling section in `TopBar.qml` |
| `src/qml/Topbar/WeatherWidget.qml` | Standalone framed top-bar section: own slanted `Canvas` frame (matching `StatusesSection`) holding 24px icon + temperature + condition text; collapse, hover, popup trigger |
| `src/qml/Topbar/WeatherPopupContent.qml` | Root of the weather popup content; loaded by the generalized `StatusPopup.qml` Loader |
| `src/qml/Topbar/WeatherCurrentSection.qml` | Current conditions: 64px icon, temp, condition, feels-like (REQ-F-012) |
| `src/qml/Topbar/WeatherDetailsGrid.qml` | 2×2 details grid: humidity, wind, visibility, pressure (REQ-F-013) |
| `src/qml/Topbar/WeatherHourlyStrip.qml` | Horizontal hourly forecast strip with 6 visible cards (REQ-F-014) |
| `src/qml/Topbar/WeatherDailyCards.qml` | Row of 5 daily summary cards (REQ-F-015) |
| `src/qml/Topbar/TemperatureGraph.qml` | Canvas-based cyan polyline temperature graph (REQ-F-016) |
| `src/qml/Topbar/PrecipitationGraph.qml` | Canvas/Repeater-based violet precipitation bar graph (REQ-F-017) |

### 1.2 Modified Files

| File | Change |
|------|--------|
| `src/core/ConfigService.h` | Add `WeatherConfig` struct; add `weather_` member, `weather()` accessor, `weatherChanged()` signal |
| `src/core/ConfigService.cpp` | Add `parseWeather()` free function; add weather `missing` flags to `MissingDefaults`; extend `parseFile()`, `defaultLinesForSection()`, `insertMissingSectionDefaults()`, `writeConfig()` |
| `src/surfaces/StatusPopupSurface.cpp` | Add `"weather"` case to `sizeForPopupId()` |
| `src/qml/Topbar/StatusPopup.qml` | Generalize: replace hardcoded `titleText` + empty `contentArea` with a `Loader` keyed by `popupId` |
| `src/qml/Topbar/TopBar.qml` | Add `WeatherSection` as a standalone sibling section between `ActiveWindowSection` and `StatusesSection` |
| `src/app/ShellApplication.h` | Add `WeatherService* weather_ = nullptr;` member and forward declaration |
| `src/app/ShellApplication.cpp` | Construct `WeatherService`, call `reg()`, call `weather_->start()` |
| `CMakeLists.txt` | Add weather .cpp/.h to `holonight_services`; add 9 new QML files to `HOLONIGHT_QML_FILES`; add `qt6_add_resources` block for `assets/weather/*.svg`; add `src/services/weather` to include path |

---

## 2. Data Structures

All defined in `src/services/weather/WeatherData.h`.

```cpp
// A single 3-hourly forecast entry from OWM /forecast list[].
struct HourlyEntry {
    Q_GADGET
    Q_PROPERTY(qint64  timestamp    MEMBER timestamp)
    Q_PROPERTY(double  temperature  MEMBER temperature)
    Q_PROPERTY(int     conditionId  MEMBER conditionId)
    Q_PROPERTY(double  precipitation MEMBER precipitation)
    Q_PROPERTY(QString iconCode     MEMBER iconCode)
public:
    qint64  timestamp{0};      // Unix seconds (UTC), from OWM list[i].dt
    double  temperature{0.0};  // °C/°F per units config, from list[i].main.temp
    int     conditionId{0};    // OWM condition id, from list[i].weather[0].id
    double  precipitation{0.0};// mm/3h, from list[i].rain["3h"] or snow["3h"]; 0 if absent
    QString iconCode;          // e.g. "04n", from list[i].weather[0].icon
};

// Aggregated daily summary (one per calendar day in local time).
struct DailyEntry {
    Q_GADGET
    Q_PROPERTY(qint64  date        MEMBER date)
    Q_PROPERTY(double  tempMin     MEMBER tempMin)
    Q_PROPERTY(double  tempMax     MEMBER tempMax)
    Q_PROPERTY(int     conditionId MEMBER conditionId)
    Q_PROPERTY(QString condition   MEMBER condition)
    Q_PROPERTY(QString iconCode    MEMBER iconCode)
public:
    qint64  date{0};           // Unix seconds of midnight (local) for that day
    double  tempMin{0.0};
    double  tempMax{0.0};
    int     conditionId{0};    // from the entry nearest noon UTC that day
    QString condition;         // display string, from list[i].weather[0].description
    QString iconCode;          // e.g. "02d", from list[i].weather[0].icon
};

// Current weather (from OWM /weather endpoint).
struct CurrentWeather {
    Q_GADGET
    Q_PROPERTY(double  temperature  MEMBER temperature)
    Q_PROPERTY(double  feelsLike    MEMBER feelsLike)
    Q_PROPERTY(QString condition    MEMBER condition)
    Q_PROPERTY(int     conditionId  MEMBER conditionId)
    Q_PROPERTY(int     humidity     MEMBER humidity)
    Q_PROPERTY(int     windSpeed    MEMBER windSpeed)
    Q_PROPERTY(double  visibility   MEMBER visibility)
    Q_PROPERTY(int     pressure     MEMBER pressure)
    Q_PROPERTY(QString iconCode     MEMBER iconCode)
    Q_PROPERTY(QString timeUpdated  MEMBER timeUpdated)
    Q_PROPERTY(qint64  sunrise      MEMBER sunrise)
    Q_PROPERTY(qint64  sunset       MEMBER sunset)
public:
    double  temperature{0.0};  // main.temp
    double  feelsLike{0.0};    // main.feels_like
    QString condition;         // weather[0].description
    int     conditionId{0};    // weather[0].id
    int     humidity{0};       // main.humidity (%)
    int     windSpeed{0};      // wind.speed * 3.6, rounded to int (km/h)
    double  visibility{0.0};   // visibility / 1000.0 (km)
    int     pressure{0};       // main.pressure (hPa)
    QString iconCode;          // weather[0].icon
    QString timeUpdated;       // ISO 8601 UTC string of dt
    qint64  sunrise{0};        // sys.sunrise (Unix UTC); used for day/night resolution
    qint64  sunset{0};         // sys.sunset  (Unix UTC)
};

// Combined cache structure — serialized to/from weather.json.
struct WeatherCache {
    CurrentWeather   current;
    QList<HourlyEntry> hourly;   // raw 3-hourly entries from OWM; up to 40
    QList<DailyEntry>  daily;    // aggregated; up to 6 (5 displayed)
    qint64 fetchedAt{0};         // Unix seconds; used for stale detection
};
```

**Why Q_GADGET, not QAbstractListModel?**
`Q_GADGET` value types are read-only from QML via property accessors and work seamlessly as elements of `QVariantList`. The forecast data is always replaced wholesale (never incrementally updated), so the incremental-change machinery of `QAbstractListModel` (beginInsertRows, dataChanged, etc.) provides no benefit. `QVariantList` + `Q_GADGET` gives clean `model[i].temperature` access in QML Repeaters and ListViews. The 40-entry ceiling means performance is not a concern.

**windSpeed computation:** `qRound(wind_speed_ms * 3.6)` — performed during JSON parsing in WeatherProvider, stored as `int` (km/h). This satisfies REQ-NF-003.

---

## 3. WeatherService Class Design

### 3.1 Header (`src/services/weather/WeatherService.h`)

```cpp
#pragma once

#include "WeatherData.h"

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariant>
#include <QString>

#include <memory>

class WeatherProvider;
class ConfigService;

class WeatherService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool configured READ configured NOTIFY configuredChanged)
    Q_PROPERTY(bool hasData    READ hasData    NOTIFY hasDataChanged)
    Q_PROPERTY(bool stale      READ stale      NOTIFY staleChanged)
    Q_PROPERTY(QVariant current  READ currentVariant  NOTIFY currentChanged)
    Q_PROPERTY(QVariantList hourly READ hourlyVariant NOTIFY forecastChanged)
    Q_PROPERTY(QVariantList daily  READ dailyVariant  NOTIFY forecastChanged)

public:
    struct SkipInitTag {};
    static constexpr SkipInitTag SkipInit{};

    explicit WeatherService(ConfigService* config, QObject* parent = nullptr);
    explicit WeatherService(ConfigService* config, std::unique_ptr<WeatherProvider> provider,
                            QObject* parent = nullptr);
    explicit WeatherService(SkipInitTag, QObject* parent = nullptr);
    ~WeatherService() override = default;

    WeatherService(const WeatherService&) = delete;
    WeatherService& operator=(const WeatherService&) = delete;
    WeatherService(WeatherService&&) = delete;
    WeatherService& operator=(WeatherService&&) = delete;

    [[nodiscard]] bool    configured()     const { return configured_; }
    [[nodiscard]] bool    hasData()        const { return has_data_; }
    [[nodiscard]] bool    stale()          const { return stale_; }
    [[nodiscard]] QVariant   currentVariant()  const;
    [[nodiscard]] QVariantList hourlyVariant() const;
    [[nodiscard]] QVariantList dailyVariant()  const;

    // Called by ShellApplication::startServices().
    void start();

    // Exposed for testing; called internally by onConfigChanged / start().
    void applyConfig();

    Q_INVOKABLE [[nodiscard]] QString iconPath(int condition_id, bool is_day) const;

Q_SIGNALS:
    void configuredChanged();
    void hasDataChanged();
    void staleChanged();
    void currentChanged();
    void forecastChanged();

private Q_SLOTS:
    void onConfigChanged();
    void onRefreshTimer();
    void onCurrentFetched(const CurrentWeather& weather);
    void onForecastFetched(const QList<HourlyEntry>& hourly, const QList<DailyEntry>& daily);
    void onFetchError(const QString& message);
    void onGeoFetched(double lat, double lon, const QString& city);
    void onGeoError(const QString& message);
    void onStaleTimer();

private:
    void loadCache();
    void saveCache() const;
    void resolveLocation();
    void scheduleFetch();
    void fetch();
    void setConfigured(bool value);
    void setHasData(bool value);
    void setStale(bool value);
    void resetBackoff();
    void stepBackoff();

    // configured = api_key non-empty AND (lat+lon set OR geo_api_key non-empty).
    // Re-evaluated in applyConfig(); gates all network activity and widget rendering.
    [[nodiscard]] bool computeConfigured() const;

    ConfigService*                  config_{nullptr};
    std::unique_ptr<WeatherProvider> provider_;

    QTimer refresh_timer_;
    QTimer stale_timer_;   // fires 1h after last good fetch; sets stale=true

    // Backoff state
    int backoff_step_{0};          // 0 = no backoff
    static constexpr int kBackoffBase{1};    // seconds; doubles each step
    static constexpr int kBackoffCap{600};   // max backoff = base interval

    // Data state
    CurrentWeather      current_;
    QList<HourlyEntry>  hourly_;
    QList<DailyEntry>   daily_;
    qint64              fetched_at_{0};
    bool                configured_{false};
    bool                has_data_{false};
    bool                stale_{true};

    // Location state
    double  lat_{0.0};
    double  lon_{0.0};
    bool    location_resolved_{false};

    // Config snapshot (re-read in applyConfig() when config signals fire)
    QString api_key_;
    QString geo_api_key_;
    QString units_{"metric"};
    QString lang_{"en"};
    int     refresh_interval_{600};

    bool started_{false};
    bool geo_pending_{false};   // true while IP geolocation in-flight
    bool fetch_pending_{false}; // true while any OWM request in-flight

    static constexpr int kStaleThresholdSecs{3600};
    static QString cachePath();
};
```

### 3.2 Lifecycle

**Construction** (in `ShellApplication` constructor):
```cpp
weather_ = new WeatherService(config_service_, this);
```
`WeatherProvider` is constructed internally using the default constructor unless injected (test seam).

**`start()` sequence:**
1. Guard double-start with `started_` flag (same as `BatteryService::start()`).
2. Call `applyConfig()` — reads `WeatherConfig` from `ConfigService::weather()`, snapshots fields, and calls `setConfigured(computeConfigured())`.
3. Call `loadCache()` — synchronously reads `cachePath()`, parses JSON into `current_`/`hourly_`/`daily_`/`fetched_at_`. If valid, calls `setHasData(true)`. (Cache is loaded regardless of `configured`, but is only *displayed* when the widget is rendered, which requires `configured`.)
4. Compute stale from `fetched_at_`: if `QDateTime::currentSecsSinceEpoch() - fetched_at_ > kStaleThresholdSecs`, call `setStale(true)`.
5. **If `!configured_`: stop here** — do not resolve location, do not start timers, do not fetch. Log `qCInfo(lcWeather) << "weather not configured (missing api_key or location source); widget hidden"`.
6. Call `resolveLocation()`.
7. Start `stale_timer_` with remaining time before staleness (or immediately if already stale).

**`computeConfigured()`:**
```cpp
bool WeatherService::computeConfigured() const {
    if (api_key_.isEmpty()) return false;
    const bool has_coords = config_->weather().latitude.has_value()
                         && config_->weather().longitude.has_value();
    return has_coords || !geo_api_key_.isEmpty();
}
```

**Config reload** (`onConfigChanged()` slot, connected to `ConfigService::weatherChanged()`):
1. Call `applyConfig()` — re-snapshots fields and calls `setConfigured(computeConfigured())`.
2. **If newly `!configured_`** (was configured, now not): stop `refresh_timer_` and `stale_timer_`, leave cached data in place but the widget will unmount (REQ-F-023). Return.
3. **If newly `configured_`** (was not, now is): proceed to step 4 as a fresh start (the widget mounts).
4. Reset backoff (`backoff_step_ = 0`).
5. If `location_resolved_`, call `scheduleFetch()` immediately; else call `resolveLocation()` again (in case `api_key` or `geo_api_key` changed).
6. Call `setStale(false)` — a config change is treated as a fresh start for stale tracking.

**`resolveLocation()` logic:**
```
if (config has valid lat AND lon):
    lat_ = config_lat; lon_ = config_lon; location_resolved_ = true
    scheduleFetch()
else:
    geo_pending_ = true
    provider_->fetchGeolocation(geo_api_key_)
```

**`scheduleFetch()` / backoff:**
- On success (`onCurrentFetched` + `onForecastFetched` both arrive): `resetBackoff()` → `refresh_timer_.start(refresh_interval_ * 1000)`.
- On error (`onFetchError`): `stepBackoff()` → restart timer at `backoff_secs * 1000`. Then `setStale(true)`.
- `stepBackoff()`: `backoff_step_++`; next interval = `min(kBackoffBase << (backoff_step_ - 1), min(kBackoffCap, refresh_interval_))`.
- `resetBackoff()`: `backoff_step_ = 0`.

**Handling partial fetch (current vs forecast):** Both are issued in `fetch()`. Two in-flight flags `current_pending_` and `forecast_pending_` (booleans) are set true. `onCurrentFetched` and `onForecastFetched` each clear their flag. Only when both are false does the service call `saveCache()`, emit signals, and call `resetBackoff()`. If either emits `onFetchError`, the remaining request is not cancelled (OWM allows independent use), but the combined result is treated as failure and backoff applies. Cache is only saved if both succeed.

**Stale timer:** A single-shot `QTimer` fires `kStaleThresholdSecs` after the last successful fetch. `onStaleTimer()` calls `setStale(true)`. It is restarted on every successful complete fetch. If `fetched_at_` is already old on startup, the timer fires immediately (start with `interval = 0`).

### 3.3 Cache

**Path:** `WeatherService::cachePath()`:
```cpp
QString WeatherService::cachePath() {
    QString xdg = qEnvironmentVariable("XDG_CACHE_HOME");
    if (xdg.isEmpty()) {
        xdg = QDir::homePath() + QLatin1String("/.cache");
    }
    return xdg + QLatin1String("/holonight/weather.json");
}
```
Directory is created via `QDir::mkpath()` before writing.

**Cache JSON schema:**
```json
{
  "fetched_at": 1748600000,
  "current": { "temperature": 18.2, "feels_like": 16.0, ... },
  "hourly": [ { "dt": 1748600000, "temp": 18.2, "cond_id": 800, "precip": 0.0, "icon": "01d" }, ... ],
  "daily":  [ { "date": 1748563200, "t_min": 14.0, "t_max": 21.3, "cond_id": 800, "cond": "clear sky", "icon": "01d" }, ... ]
}
```
No API keys are stored (REQ-C-004).

**Load:** `QJsonDocument::fromJson(file.readAll())`. Missing fields default to zero-values. On parse failure, logs `qCWarning` and leaves data unchanged.

**Save:** Uses `QSaveFile` (atomic write, same as `ConfigService::writeConfig()`).

### 3.4 QML-Facing API (complete contract)

| Kind | Name | Type | Notes |
|------|------|------|-------|
| Q_PROPERTY | `configured` | `bool` | api_key non-empty AND (lat+lon set OR geo_api_key non-empty); gates widget rendering & all network activity (REQ-F-023) |
| Q_PROPERTY | `hasData` | `bool` | false until first valid data (cache or network) |
| Q_PROPERTY | `stale` | `bool` | true on startup with cache-only, true after 1h without refresh |
| Q_PROPERTY | `current` | `QVariant` (wrapping `CurrentWeather` Q_GADGET) | all subfields accessible as `WeatherService.current.temperature` |
| Q_PROPERTY | `hourly` | `QVariantList` of `HourlyEntry` Q_GADGETs | up to 40 entries |
| Q_PROPERTY | `daily` | `QVariantList` of `DailyEntry` Q_GADGETs | up to 6 entries (5 shown) |
| Q_INVOKABLE | `iconPath(conditionId, isDay)` | `QString` | returns `"qrc:/HolonightShell/weather/<filename>"` or unknown fallback |
| NOTIFY | `configuredChanged` | — | fires when config reload changes the configured state |
| NOTIFY | `hasDataChanged` | — | |
| NOTIFY | `staleChanged` | — | |
| NOTIFY | `currentChanged` | — | fires when `current_` is updated |
| NOTIFY | `forecastChanged` | — | fires when both `hourly_` and `daily_` are updated |

**`currentVariant()`** wraps `current_` in `QVariant::fromValue(current_)`. Because `CurrentWeather` has `Q_GADGET`, QML property access works: `WeatherService.current.temperature`.

**`iconPath(int condition_id, bool is_day)`:** Calls the static mapping table (§5) and prepends `"qrc:/HolonightShell/weather/"`. If the id maps to nothing, returns the unknown fallback. This is callable from both QML Image `source:` bindings and C++ (WeatherProvider does not call it — the mapping is QML-side only, to keep C++ free of QRC path strings).

---

## 4. WeatherProvider Class Design

### 4.1 Header (`src/services/weather/WeatherProvider.h`)

```cpp
#pragma once

#include "WeatherData.h"

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>

class WeatherProvider : public QObject {
    Q_OBJECT

public:
    explicit WeatherProvider(QObject* parent = nullptr);
    ~WeatherProvider() override = default;

    WeatherProvider(const WeatherProvider&) = delete;
    WeatherProvider& operator=(const WeatherProvider&) = delete;
    WeatherProvider(WeatherProvider&&) = delete;
    WeatherProvider& operator=(WeatherProvider&&) = delete;

    void fetchCurrentWeather(double lat, double lon,
                             const QString& api_key,
                             const QString& units,
                             const QString& lang);

    void fetchForecast(double lat, double lon,
                       const QString& api_key,
                       const QString& units,
                       const QString& lang);

    void fetchGeolocation(const QString& geo_api_key);

Q_SIGNALS:
    void currentFetched(const CurrentWeather& weather);
    void forecastFetched(const QList<HourlyEntry>& hourly, const QList<DailyEntry>& daily);
    void geoFetched(double lat, double lon, const QString& city);

    // Emitted for any error (network, HTTP non-200, JSON parse).
    void fetchError(const QString& message);
    void geoError(const QString& message);

private Q_SLOTS:
    void onCurrentReply(QNetworkReply* reply);
    void onForecastReply(QNetworkReply* reply);
    void onGeoReply(QNetworkReply* reply);

private:
    [[nodiscard]] static CurrentWeather   parseCurrentJson(const QJsonObject& root);
    [[nodiscard]] static QList<HourlyEntry> parseForecastHourly(const QJsonArray& list);
    [[nodiscard]] static QList<DailyEntry>  aggregateDaily(const QList<HourlyEntry>& hourly);

    QNetworkAccessManager nam_;  // owned, parented to this WeatherProvider
};
```

**`QNetworkAccessManager` lifetime:** `nam_` is a member (not a pointer), constructed in `WeatherProvider`'s constructor with `this` as implicit parent context. This avoids the Qt warning about QNAM created without a parent in a non-GUI thread. Since `WeatherProvider` lives on the main thread (same as all services), this is straightforward.

**OWM URL construction:**
```
https://api.openweathermap.org/data/2.5/weather?lat=<lat>&lon=<lon>&appid=<key>&units=<units>&lang=<lang>
https://api.openweathermap.org/data/2.5/forecast?lat=<lat>&lon=<lon>&appid=<key>&units=<units>&lang=<lang>
https://api.ipgeolocation.io/v3/ipgeo?apiKey=<geo_key>
```
Built via `QUrlQuery`. Requests are issued with `nam_.get(QNetworkRequest(url))`. The `finished` signal of each `QNetworkReply` is connected to the appropriate `on*Reply` slot. HTTP 401 and other non-200 codes are treated as `fetchError`.

**JSON parsing — field access safety:** All `QJsonObject::value()` calls check `.isUndefined()` / `.isNull()` before use. Missing optional fields (e.g., `rain`, `snow` in hourly) default to 0. A try-catch-style approach is not used (QJsonDocument does not throw); instead, each access falls back to a default via `.toDouble(0.0)` etc.

**Geolocation parsing from ipgeolocation.io v3:**
```json
{ "location": { "latitude": "51.5074", "longitude": "-0.1278", "city": "London" } }
```
Latitude and longitude are returned as **strings** in the v3 API (verified against the v3 schema). Parse via `obj["location"]["latitude"].toString().toDouble()`.

**Daily aggregation** is performed inside `aggregateDaily()` called from `onForecastReply`. See §6 for the algorithm.

---

## 5. OWM Icon Mapping Table

The mapping is implemented as a `static const QHash<int, std::pair<QString, QString>>` inside `WeatherService::iconPath()`, where the pair is `{day_filename, night_filename}`. For groups, a range check covers all ids in the group. The lookup order is: exact id match first, then group range, then fallback.

OWM condition id groups and their mappings to actual `assets/weather/` filenames:

### Group 2xx — Thunderstorm

| OWM id | Description | Day SVG | Night SVG |
|--------|-------------|---------|-----------|
| 200–202 | Thunderstorm with rain/drizzle | `wsymbol_0016_thundery_showers.svg` | `wsymbol_0032_thundery_showers_night.svg` |
| 210–212 | Light/heavy thunderstorm | `wsymbol_0024_thunderstorms.svg` | `wsymbol_0040_thunderstorms_night.svg` |
| 221 | Ragged thunderstorm | `wsymbol_0024_thunderstorms.svg` | `wsymbol_0040_thunderstorms_night.svg` |
| 230–232 | Thunderstorm with drizzle | `wsymbol_0016_thundery_showers.svg` | `wsymbol_0032_thundery_showers_night.svg` |

### Group 3xx — Drizzle

| OWM id | Description | Day SVG | Night SVG |
|--------|-------------|---------|-----------|
| 300–302 | Light/moderate/heavy drizzle | `wsymbol_0048_drizzle.svg` | `wsymbol_0066_drizzle_night.svg` |
| 310–311 | Light/moderate drizzle rain | `wsymbol_0048_drizzle.svg` | `wsymbol_0066_drizzle_night.svg` |
| 312 | Heavy drizzle rain | `wsymbol_0081_heavy_drizzle.svg` | `wsymbol_0082_heavy_drizzle_night.svg` |
| 313–314 | Shower/heavy shower drizzle | `wsymbol_0009_light_rain_showers.svg` | `wsymbol_0025_light_rain_showers_night.svg` |
| 321 | Shower drizzle | `wsymbol_0009_light_rain_showers.svg` | `wsymbol_0025_light_rain_showers_night.svg` |

### Group 5xx — Rain

| OWM id | Description | Day SVG | Night SVG |
|--------|-------------|---------|-----------|
| 500 | Light rain | `wsymbol_0017_cloudy_with_light_rain.svg` | `wsymbol_0033_cloudy_with_light_rain_night.svg` |
| 501 | Moderate rain | `wsymbol_0017_cloudy_with_light_rain.svg` | `wsymbol_0033_cloudy_with_light_rain_night.svg` |
| 502–503 | Heavy/very heavy rain | `wsymbol_0018_cloudy_with_heavy_rain.svg` | `wsymbol_0034_cloudy_with_heavy_rain_night.svg` |
| 504 | Extreme rain | `wsymbol_0051_extreme_rain.svg` | `wsymbol_0069_extreme_rain_night.svg` |
| 511 | Freezing rain | `wsymbol_0050_freezing_rain.svg` | `wsymbol_0068_freezing_rain_night.svg` |
| 520–521 | Light/moderate shower rain | `wsymbol_0009_light_rain_showers.svg` | `wsymbol_0025_light_rain_showers_night.svg` |
| 522 | Heavy shower rain | `wsymbol_0010_heavy_rain_showers.svg` | `wsymbol_0026_heavy_rain_showers_night.svg` |
| 531 | Ragged shower rain | `wsymbol_0085_extreme_rain_showers.svg` | `wsymbol_0086_extreme_rain_showers_night.svg` |

### Group 6xx — Snow

| OWM id | Description | Day SVG | Night SVG |
|--------|-------------|---------|-----------|
| 600–601 | Light/moderate snow | `wsymbol_0019_cloudy_with_light_snow.svg` | `wsymbol_0035_cloudy_with_light_snow_night.svg` |
| 602 | Heavy snow | `wsymbol_0020_cloudy_with_heavy_snow.svg` | `wsymbol_0036_cloudy_with_heavy_snow_night.svg` |
| 611–612 | Sleet/light shower sleet | `wsymbol_0021_cloudy_with_sleet.svg` | `wsymbol_0037_cloudy_with_sleet_night.svg` |
| 613 | Shower sleet | `wsymbol_0087_heavy_sleet_showers.svg` | `wsymbol_0088_heavy_sleet_showers_night.svg` |
| 615–616 | Light/moderate rain and snow | `wsymbol_0021_cloudy_with_sleet.svg` | `wsymbol_0037_cloudy_with_sleet_night.svg` |
| 620 | Light shower snow | `wsymbol_0011_light_snow_showers.svg` | `wsymbol_0027_light_snow_showers_night.svg` |
| 621 | Shower snow | `wsymbol_0012_heavy_snow_showers.svg` | `wsymbol_0028_heavy_snow_showers_night.svg` |
| 622 | Heavy shower snow | `wsymbol_0052_extreme_snow.svg` | `wsymbol_0070_extreme_snow_night.svg` |

### Group 7xx — Atmosphere

| OWM id | Description | Day SVG | Night SVG |
|--------|-------------|---------|-----------|
| 701 | Mist | `wsymbol_0006_mist.svg` | `wsymbol_0063_mist_night.svg` |
| 711 | Smoke | `wsymbol_0055_smoke.svg` | `wsymbol_0073_smoke_night.svg` |
| 721 | Haze | `wsymbol_0005_hazy_sun.svg` | `wsymbol_0063_mist_night.svg` |
| 731 | Dust/sand whirls | `wsymbol_0056_dust_sand.svg` | `wsymbol_0074_dust_sand_night.svg` |
| 741 | Fog | `wsymbol_0007_fog.svg` | `wsymbol_0064_fog_night.svg` |
| 751 | Sand | `wsymbol_0056_dust_sand.svg` | `wsymbol_0074_dust_sand_night.svg` |
| 761 | Dust | `wsymbol_0056_dust_sand.svg` | `wsymbol_0074_dust_sand_night.svg` |
| 762 | Volcanic ash | `wsymbol_0091_volcanic_ash.svg` | `wsymbol_0091_volcanic_ash.svg` |
| 771 | Squalls | `wsymbol_0060_windy.svg` | `wsymbol_0078_windy_night.svg` |
| 781 | Tornado | `wsymbol_0079_tornado.svg` | `wsymbol_0079_tornado.svg` |

### Group 800 — Clear

| OWM id | Description | Day SVG | Night SVG |
|--------|-------------|---------|-----------|
| 800 | Clear sky | `wsymbol_0001_sunny.svg` | `wsymbol_0008_clear_sky_night.svg` |

### Group 80x — Clouds

| OWM id | Description | Day SVG | Night SVG |
|--------|-------------|---------|-----------|
| 801 | Few clouds (11–25%) | `wsymbol_0002_sunny_intervals.svg` | `wsymbol_0041_partly_cloudy_night.svg` |
| 802 | Scattered clouds (25–50%) | `wsymbol_0003_white_cloud.svg` | `wsymbol_0042_cloudy_night.svg` |
| 803 | Broken clouds (51–84%) | `wsymbol_0043_mostly_cloudy.svg` | `wsymbol_0044_mostly_cloudy_night.svg` |
| 804 | Overcast clouds (85–100%) | `wsymbol_0004_black_low_cloud.svg` | `wsymbol_0004_black_low_cloud.svg` |

### Fallback

Any unmapped id → `wsymbol_0999_unknown.svg` (day and night identical).

### Day/Night Resolution Algorithm

Priority order:
1. Compare `timestamp` (Unix UTC) against `current_.sunrise` and `current_.sunset` from the OWM `/weather` `sys` object. If `sunrise <= timestamp < sunset` → day; otherwise → night. This is the primary method for all data points as long as current weather has been fetched at least once.
2. If `current_.sunrise == 0` (no current weather yet — startup with forecast-only cache not expected, but defensive): parse the OWM icon code suffix. Icon codes end in `d` (day) or `n` (night). Use that suffix.
3. Absolute fallback: assume day.

The `iconPath(int condition_id, bool is_day)` Q_INVOKABLE receives `is_day` pre-computed by the QML caller (WeatherWidget, hourly card, daily card) using `WeatherService.current.sunrise` and `WeatherService.current.sunset` compared against the entry's timestamp or `Date.now() / 1000` for the current conditions widget.

---

## 6. Daily Aggregation Algorithm

Implemented in `WeatherProvider::aggregateDaily(const QList<HourlyEntry>& hourly)`.

**Input:** The raw 3-hourly list from OWM `/forecast`, up to 40 entries (5 days × 8 slots). OWM returns entries starting from the next available 3h slot, so on a mid-day call there may be 39 or fewer entries, and the first and last days may be partial.

**Algorithm:**

```
buckets = {}  // key: local calendar date string "YYYY-MM-DD", value: list of entries for that day

for each entry in hourly:
    local_date = QDateTime::fromSecsSinceEpoch(entry.timestamp, Qt::LocalTime).date().toString("yyyy-MM-dd")
    buckets[local_date].append(entry)

daily_list = []
for each date_key in buckets (sorted ascending):
    entries = buckets[date_key]
    t_min = min(e.temperature for e in entries)
    t_max = max(e.temperature for e in entries)

    // Entry nearest to noon local time
    noon_secs = QDateTime(QDate::fromString(date_key, "yyyy-MM-dd"),
                          QTime(12, 0), Qt::LocalTime).toSecsSinceEpoch()
    best = entry with min |entry.timestamp - noon_secs|
    cond_id = best.conditionId
    cond_str = best.condition   // stored in HourlyEntry (see note below)
    icon_code = best.iconCode

    daily_list.append(DailyEntry{ date=midnight_secs, t_min, t_max, cond_id, cond_str, icon_code })

return daily_list[0..4]  // at most 5 entries shown; up to 6 may exist (partial first + last days)
```

**Noon-nearest rule:** The OWM description states "noon UTC" in the spec (REQ-F-008). This design uses **noon local time** instead, because the user views weather in their local timezone. The difference is at most a few hours and only affects the condition icon selection, not min/max temps. Local time is always available via `Qt::LocalTime`.

**HourlyEntry condition string:** `HourlyEntry` stores a `condition` string (`weather[0].description`) in addition to the fields shown in §2. The struct definition in the header adds: `QString condition;`. This field is needed only for daily aggregation (to populate `DailyEntry.condition`) and is not exposed as a Q_PROPERTY on `HourlyEntry` to keep the QML surface small.

**Output count:** Yields 5–6 day buckets depending on launch time. A launch at 00:01 local time produces 5–6 full-day buckets for 40 entries. A launch at 23:59 produces a 1-entry partial "today" bucket plus 5 more days = 6 buckets. The UI displays exactly 5 (index 0..4 of the sorted list). The 6th bucket (if present) is silently dropped at render time by the `WeatherDailyCards.qml` Repeater model.

**Precipitation per 3h:** OWM `list[i].rain` is an object with key `"3h"` (or absent for non-rainy periods). Similarly `list[i].snow["3h"]`. Parse defensively:
```cpp
double precip = 0.0;
if (obj.contains("rain") && obj["rain"].isObject()) {
    precip += obj["rain"].toObject().value("3h").toDouble(0.0);
}
if (obj.contains("snow") && obj["snow"].isObject()) {
    precip += obj["snow"].toObject().value("3h").toDouble(0.0);
}
entry.precipitation = precip;
```

---

## 7. ConfigService Additions

### 7.1 `WeatherConfig` struct (add to `ConfigService.h`)

```cpp
struct WeatherConfig {
    QString api_key;
    QString geo_api_key;
    // std::optional<double> is used for lat/lon because toml++ readStr/readInt use a
    // bool& missing out-param that cannot differentiate "key present but 0.0" from "absent".
    // For coordinates, 0.0 is a valid value (null island), so a missing sentinel is required.
    std::optional<double> latitude;
    std::optional<double> longitude;
    QString city;
    QString units{"metric"};
    QString lang{"en"};
    int refresh_interval{600};

    bool operator==(const WeatherConfig&) const = default;
};
```

**Why `std::optional<double>` for lat/lon:** The existing `readInt`/`readStr` helpers use a `bool& missing` out-parameter. They cannot be used for doubles (no `readDouble` helper exists), and they cannot distinguish "key absent" from "key present with value 0.0". Since latitude 0.0 is valid (Gulf of Guinea), a `std::optional<double>` is the correct representation. The TOML++ node_view API is used directly for these two fields without going through the existing helpers.

**No `has_coords` bool:** `std::optional` is idiomatic C++23 and avoids a separate sentinel variable. `WeatherConfig::operator==` compares optionals correctly by default comparison.

### 7.2 `parseWeather()` function (add to anonymous namespace in `ConfigService.cpp`)

```cpp
// New missing-defaults flags — added to MissingDefaults struct:
// bool weather_api_key{false};
// bool weather_geo_api_key{false};
// bool weather_units{false};
// bool weather_lang{false};
// bool weather_refresh_interval{false};
// (latitude, longitude, city are intentionally optional — no missing flag)

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
WeatherConfig parseWeather(const toml::table& tbl, MissingDefaults& missing) {
    WeatherConfig cfg;
    const auto sec = tbl["weather"];

    cfg.api_key = readStr(sec["api_key"], cfg.api_key, "weather.api_key", missing.weather_api_key);
    cfg.geo_api_key = readStr(sec["geo_api_key"], cfg.geo_api_key, "weather.geo_api_key", missing.weather_geo_api_key);
    cfg.units = readStr(sec["units"], cfg.units, "weather.units", missing.weather_units);
    cfg.lang  = readStr(sec["lang"],  cfg.lang,  "weather.lang",  missing.weather_lang);
    cfg.refresh_interval = readPositiveInt(sec["refresh_interval"], cfg.refresh_interval,
                                           "weather.refresh_interval", missing.weather_refresh_interval);
    // refresh_interval <= 0 handled by readPositiveInt (logs warning, returns default)

    // lat/lon: direct node_view access, no readDouble helper needed
    if (const auto lat_node = sec["latitude"]; lat_node) {
        if (const auto val = lat_node.value<double>()) {
            cfg.latitude = *val;
        } else {
            qCWarning(lcConfig) << "Config: weather.latitude must be a floating-point number; ignoring";
        }
    }
    if (const auto lon_node = sec["longitude"]; lon_node) {
        if (const auto val = lon_node.value<double>()) {
            cfg.longitude = *val;
        } else {
            qCWarning(lcConfig) << "Config: weather.longitude must be a floating-point number; ignoring";
        }
    }

    // city is informational only; populated by geolocation, stored back by WeatherService
    bool city_missing_ignored = false;
    cfg.city = readStr(sec["city"], cfg.city, "weather.city", city_missing_ignored);

    return cfg;
}
// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
```

### 7.3 `ConfigService` member additions

In `ConfigService.h`, add:
```cpp
// After BackgroundConfig background_;
WeatherConfig weather_;
```
```cpp
// Accessor:
[[nodiscard]] const WeatherConfig& weather() const { return weather_; }
```
```cpp
// Signal:
void weatherChanged();
```

### 7.4 `parseFile()` extension (in `ConfigService.cpp`)

Inside `ConfigService::parseFile()`, after `const auto local_background = parseBackground(table, missing);`:
```cpp
const auto local_weather = parseWeather(table, missing);
// ...
if (local_weather != weather_) {
    weather_ = local_weather;
    emit weatherChanged();
}
```

### 7.5 `defaultLinesForSection()` extension

Add a new `else if` branch at the end of the function (after the `"background"` branch):
```cpp
} else if (section == QLatin1String("weather")) {
    const WeatherConfig weather;  // defaults
    if (missing.weather_api_key) {
        lines << QStringLiteral("api_key = \"\" # obtain from openweathermap.org");
    }
    if (missing.weather_geo_api_key) {
        lines << QStringLiteral("geo_api_key = \"\" # obtain from ipgeolocation.io (used if lat/lon absent)");
    }
    if (missing.weather_units) {
        lines << QStringLiteral("units = %1 # metric | imperial | standard").arg(tomlQuote(weather.units));
    }
    if (missing.weather_lang) {
        lines << QStringLiteral("lang = %1").arg(tomlQuote(weather.lang));
    }
    if (missing.weather_refresh_interval) {
        lines << QStringLiteral("refresh_interval = %1 # seconds; must be > 0").arg(weather.refresh_interval);
    }
    // lat/lon intentionally not written — absence triggers geolocation
}
```

### 7.6 `insertMissingSectionDefaults()` call extension (in `writeMissingDefaults()`)

Add after the `"background"` call:
```cpp
insertMissingSectionDefaults(lines, QStringLiteral("weather"),
                             defaultLinesForSection(QStringLiteral("weather"), missing));
```

### 7.7 `writeConfig()` extension

At the end of `writeConfig()`, after the `[background]` block:
```cpp
out << "\n";
out << "[weather]\n";
out << "api_key = \"\" # obtain from openweathermap.org\n";
out << "geo_api_key = \"\" # obtain from ipgeolocation.io; used when lat/lon are absent\n";
out << "# latitude = 51.5074  # uncomment to skip IP geolocation\n";
out << "# longitude = -0.1278\n";
out << "units = " << tomlQuote(weather_.units) << " # metric | imperial | standard\n";
out << "lang = " << tomlQuote(weather_.lang) << "\n";
out << "refresh_interval = " << weather_.refresh_interval << " # seconds\n";
```

### 7.8 `MissingDefaults` struct extension

Add to the `MissingDefaults` struct in the anonymous namespace:
```cpp
bool weather_api_key{false};
bool weather_geo_api_key{false};
bool weather_units{false};
bool weather_lang{false};
bool weather_refresh_interval{false};
```
Update `MissingDefaults::any()` to OR in the new fields.

---

## 8. StatusPopup.qml Generalization

The current `StatusPopup.qml` has a hardcoded title bar + empty content area. The weather popup has no title bar per REQ-F-011. The generalization replaces both with a `Loader` that selects the per-popup content component.

### 8.1 Design

**Rule:** Weather popup has no title bar. All other popups (audio, network, battery, keyboard-layout) keep a title bar. This is handled by the content components themselves: the existing popups will each receive their own `*PopupContent.qml` component with a built-in title row. For the current iteration where those popups still have empty `contentArea`, the title bar stays in `StatusPopup.qml` but is made conditional.

**Generalization approach — Loader keyed by popupId:**

```qml
// Replace the titleText + contentArea block with:

// Title bar: shown for all popups except "weather"
Text {
    id: titleText
    visible: root.popupId !== "weather"
    x: root.panelLeft + 16
    y: root.panelTop + 14
    width: root.panelRight - root.panelLeft - 32
    text: root.displayTitle
    color: HoloniightPalette.onSurface
    font.pixelSize: 14
    font.weight: Font.Medium
    elide: Text.ElideRight
}

Loader {
    id: contentLoader
    x: root.panelLeft
    y: root.popupId === "weather" ? root.panelTop : (titleText.y + titleText.height + 8)
    width: root.panelRight - root.panelLeft
    height: root.panelBottom - y - (root.popupId === "weather" ? 0 : 16)

    source: root.contentSource
}

readonly property string contentSource: {
    switch (root.popupId) {
        case "weather": return "qrc:/HolonightShell/Topbar/WeatherPopupContent.qml"
        default:        return ""   // other popups: empty Loader until future iterations add content
    }
}
```

**Why a Loader instead of a StackLayout or inline conditionals:** A `Loader` is the established Qt Quick pattern for on-demand component loading. It avoids constructing all popup content objects in memory when only one popup is open at a time. `source: ""` silently does nothing (no item created), which preserves existing behavior for audio/network/battery/keyboard-layout popups that still use the empty content area.

**`popupTitles` map:** Kept as-is for the `displayTitle` computation that `titleText` uses. Weather does not appear in the map (the title is hidden anyway).

### 8.2 Complete modified `StatusPopup.qml` structure

The outer shape, glow, `panelShape` Canvas, entry animation, and Esc handler remain **entirely unchanged**. The only modification is the block after `panelShape`:

```
BEFORE:
  Text { id: titleText … }
  Item { id: contentArea … }

AFTER:
  Text { id: titleText; visible: root.popupId !== "weather" … }
  Loader { id: contentLoader; source: root.contentSource … }
  readonly property string contentSource: { switch … }
```

This change is minimal and backward-compatible: all existing popups that have `popupId` in `{"audio","network","battery","keyboard-layout"}` get `source: ""` → empty Loader → no content rendered (same as today's empty `contentArea`).

---

## 9. StatusPopupSurface C++ Extension

In `StatusPopupSurface.cpp`, `sizeForPopupId()` (line 74):

```cpp
QSize StatusPopupSurface::sizeForPopupId(const QString& popup_id) {
    if (popup_id == QLatin1String("audio")) {
        return {kAudioWidth, kAudioHeight};
    }
    if (popup_id == QLatin1String("weather")) {
        return {kWeatherWidth, kWeatherHeight};
    }
    return {kDefaultWidth, kDefaultHeight};
}
```

Add constants at the top of the anonymous namespace:
```cpp
constexpr int kWeatherWidth  = 460;
constexpr int kWeatherHeight = 560;
```

**Height computation:** The popup content stack:
- `WeatherCurrentSection`: 64px icon + 36pt temp (~48px) + 14pt condition (~20px) + 12pt feels-like (~18px) + padding = ~180px
- `WeatherDetailsGrid`: 2 rows × ~40px + padding = ~100px
- `WeatherHourlyStrip`: ~90px (single row of cards + label)
- `WeatherDailyCards`: 5 cards × ~56px horizontal = ~80px (horizontal row)
- `TemperatureGraph`: ~80px
- `PrecipitationGraph`: ~60px
- Attribution text: ~24px
- Internal padding between sections: ~12px × 6 = ~72px
- Total content: ~686px

The surface height is `content_height + kTopPadding + kGlowPadding = 560 + 6 + 24 = 590`. The panel height is `kWeatherHeight - kTopPadding - kGlowPadding = 560 - 6 - 24 = 530`. This fits all content with moderate padding if sections are compact. If actual content renders taller, the implementer adjusts `kWeatherHeight` during integration testing. Using 560 leaves some scroll room and matches the "500+ px" spec requirement.

**Note on REQ-C-008:** The C++ framework class `StatusPopupSurface` is not modified in its core logic — only `sizeForPopupId()` gains a new `if` branch, which is an extension, not a modification of existing behavior.

---

## 10. QML Component Designs

All components use `import Holonight` and `HoloniightPalette.<token>` exclusively — no hardcoded hex values.

### 10.1 `WeatherWidget.qml`

**Standalone framed section, not a cluster widget.** Unlike `BatteryWidget.qml` (which lives *inside* `StatusesSection`'s shared frame and so renders only a rounded per-widget hover pill), the weather is its own top-bar section and must follow the same visual rules as the sibling framed sections (`StatusesSection`/`TraySection`/`ClockSection`): it draws its **own** slanted `Canvas` frame. The frame mirrors `StatusesSection` exactly (its immediate right neighbour) — same `frameInset`/`slantCut`, fill, inner-shadow and stroke layering — so the right side of the bar reads as a row of consistent angled panels. There is no `MultiEffect` glow, no `scale`-on-hover, and no rounded `hoverFrame`; like the other framed sections the frame is static and the popup is the interaction feedback.

It remains a `BarSection` root with `required property string barMonitorName`, `implicitWidth` collapsing to 0 when no data, `HoverHandler` (subtle icon-opacity only), `BarTooltipArea`, and `StatusPopupTriggerArea` (`popupId: "weather"`).

```qml
import QtQuick
import HolonightShell
import Holonight

BarSection {
    id: root

    required property string barMonitorName
    readonly property bool ready: WeatherService.hasData

    // Frame geometry — identical tokens to StatusesSection's slant frame
    readonly property int frameInset: 1
    readonly property int slantCut: 12
    readonly property int contentLeftMargin: 20 + root.slantCut
    readonly property int contentRightMargin: 20
    readonly property int inheritedSectionPadding: 8
    readonly property color frameFill: HoloniightPalette.surface
    readonly property color frameStroke: HoloniightPalette.borderPassive

    // Collapse to 0 when no data (REQ-F-010 / REQ-C-008 "absent" pattern)
    implicitWidth: root.ready
        ? (root.contentLeftMargin + contentRow.implicitWidth + root.contentRightMargin)
        : 0
    Behavior on implicitWidth { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    // Own slanted frame (visible: root.ready), drawn exactly like StatusesSection's Canvas:
    //   moveTo(inset, top) → lineTo(width-inset, top) → lineTo(width-inset, bottom)
    //                      → lineTo(inset+slantCut, bottom) → close
    // fill + inner-shadow stroke + onSurface hairline + frameStroke @ 0.4 alpha.
    Canvas { id: frameCanvas; visible: root.ready /* ...slant path + layered strokes... */ }

    // Content: 24px icon + temp (cyan, fixedFont, Medium) + condition (onSurfaceVariant, uppercase),
    // is_day computed from sunrise/sunset vs current time, source via WeatherService.iconPath().
    Row { id: contentRow /* icon + tempText + condText, anchored left with contentLeftMargin */ }

    HoverHandler { id: hoverHandler }          // subtle icon opacity 0.92 → 1.0 only

    BarTooltipArea {
        barMonitorName: root.barMonitorName
        title: root.ready ? root.capitalize(WeatherService.current.condition) : ""
        description: root.ready
            ? "Feels like " + Math.round(WeatherService.current.feelsLike) + "°\nClick for forecast"
              + (WeatherService.stale ? "\nStale — last update over 1h ago" : "")
            : ""
    }

    StatusPopupTriggerArea { id: popupTrigger; popupId: "weather"; barMonitorName: root.barMonitorName }

    onFrameFillChanged: frameCanvas.requestPaint()
    onFrameStrokeChanged: frameCanvas.requestPaint()
}
```

**Placement — standalone section gated by `WeatherSection.qml`:** the widget is *not* added to `StatusesSection`'s `statusRow`. Instead a dedicated `WeatherSection.qml` `Loader` wraps it, gating instantiation on `WeatherService.configured`, and is placed in `TopBar.qml` as a sibling section between `ActiveWindowSection` and `StatusesSection`. This satisfies REQ-F-023 "not rendered at all" — when unconfigured the `Loader` holds no item, so no weather object exists in the layout (not merely a zero-width one). When the user adds an `api_key` at runtime, `configuredChanged` flips `active` true and the section mounts:
```qml
// WeatherSection.qml
Loader {
    id: root
    required property string barMonitorName
    active: WeatherService.configured
    visible: active
    sourceComponent: WeatherWidget { barMonitorName: root.barMonitorName }
}
```
```qml
// TopBar.qml — sibling between ActiveWindowSection and StatusesSection
WeatherSection {
    barMonitorName: root.barMonitorName
    Layout.alignment: Qt.AlignVCenter
    Layout.leftMargin: root.primarySectionMargin
}
```
The `WeatherWidget`'s own `implicitWidth: 0` collapse (and `visible: root.ready` frame) still applies *within* the loaded section for the configured-but-no-data-yet case (REQ-F-010).

### 10.2 `WeatherPopupContent.qml`

Root `Item` sized to `parent.width × parent.height` (the Loader fills the panel area). Contains a `Column` of all sections plus a bottom attribution `Text`. All sections are children; no external popups or surfaces are created here.

```qml
import QtQuick
import QtQuick.Layouts
import HolonightShell
import Holonight

Item {
    id: root

    anchors.fill: parent

    // Outer padding inside the panel
    readonly property int sidePad: 16
    readonly property int topPad:  16

    Column {
        x: root.sidePad
        y: root.topPad
        width: root.width - root.sidePad * 2
        spacing: 12

        WeatherCurrentSection  { width: parent.width }
        WeatherDetailsGrid     { width: parent.width }
        WeatherHourlyStrip     { width: parent.width }
        WeatherDailyCards      { width: parent.width }
        TemperatureGraph       { width: parent.width; height: 80 }
        PrecipitationGraph     { width: parent.width; height: 60 }

        Text {
            id: attribution
            width: parent.width
            text: "Source: OpenWeather"
            color: HoloniightPalette.accentBlue   // #7aa2f7 token
            font.pixelSize: 9
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
```

### 10.3 `WeatherCurrentSection.qml`

```qml
Column {
    id: root
    spacing: 4
    horizontalItemAlignment: Qt.AlignHCenter

    // 64×64 weather icon
    Image {
        id: heroIcon
        anchors.horizontalCenter: parent.horizontalCenter
        width: 64; height: 64
        source: WeatherService.hasData
            ? WeatherService.iconPath(WeatherService.current.conditionId, isDay)
            : ""
        fillMode: Image.PreserveAspectFit

        readonly property bool isDay:
            (Date.now() / 1000) >= WeatherService.current.sunrise &&
            (Date.now() / 1000) <  WeatherService.current.sunset
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: WeatherService.hasData
            ? Math.round(WeatherService.current.temperature) + "°"
            : ""
        color: HoloniightPalette.accentCyan
        font.pixelSize: 48
        font.family: ThemeService.fixedFont
        font.weight: Font.Bold
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        // QML Text has no textTransform; uppercase in JS to match the mockup ("LIGHT RAIN").
        text: WeatherService.hasData ? WeatherService.current.condition.toUpperCase() : ""
        color: HoloniightPalette.onSurfaceVariant
        font.pixelSize: 14
        font.family: ThemeService.uiFont
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: WeatherService.hasData
            ? "Feels like " + Math.round(WeatherService.current.feelsLike) + "°"
            : ""
        color: HoloniightPalette.accentBlue
        font.pixelSize: 12
        font.family: ThemeService.fixedFont
    }
}
```

### 10.4 `WeatherDetailsGrid.qml`

A `GridLayout` with `columns: 2`, each cell a small `Column` with a label (`HoloniightPalette.accentBlue`, 10px) and a value (`HoloniightPalette.onSurface`, 13px).

```qml
GridLayout {
    id: root
    columns: 2
    columnSpacing: 12
    rowSpacing: 8

    // Reusable inline component for each cell
    component DetailCell: Column {
        property string label: ""
        property string value: ""
        spacing: 2
        Text { text: parent.label; color: HoloniightPalette.accentBlue; font.pixelSize: 10; font.family: ThemeService.fixedFont }
        Text { text: parent.value; color: HoloniightPalette.onSurface;   font.pixelSize: 13; font.family: ThemeService.fixedFont }
    }

    DetailCell {
        label: "Humidity"
        value: WeatherService.hasData ? WeatherService.current.humidity + "%" : "—"
        Layout.fillWidth: true
    }
    DetailCell {
        label: "Wind"
        value: WeatherService.hasData ? WeatherService.current.windSpeed + " km/h" : "—"
        Layout.fillWidth: true
    }
    DetailCell {
        label: "Visibility"
        value: WeatherService.hasData ? WeatherService.current.visibility.toFixed(0) + " km" : "—"
        Layout.fillWidth: true
    }
    DetailCell {
        label: "Pressure"
        value: WeatherService.hasData ? WeatherService.current.pressure + " hPa" : "—"
        Layout.fillWidth: true
    }
}
```

### 10.5 `WeatherHourlyStrip.qml`

A `Column` with a section label and a `ListView` (horizontal, `orientation: ListView.Horizontal`, `clip: true`, `flickableDirection: Flickable.HorizontalFlick`). Each delegate is a fixed-80px-wide `Column`: time label + 24px icon + temp.

```qml
Column {
    id: root
    spacing: 6
    width: parent.width

    Text { text: "Hourly"; color: HoloniightPalette.onSurfaceVariant; font.pixelSize: 10; font.family: ThemeService.fixedFont }

    ListView {
        id: strip
        width: parent.width
        height: 76
        orientation: ListView.Horizontal
        clip: true
        flickableDirection: Flickable.HorizontalFlick
        model: WeatherService.hourly
        spacing: 4

        delegate: Column {
            required property var modelData  // HourlyEntry Q_GADGET
            width: 76
            height: strip.height
            spacing: 3
            horizontalItemAlignment: Qt.AlignHCenter

            Text {
                text: Qt.formatTime(new Date(modelData.timestamp * 1000), "HH:mm")
                color: HoloniightPalette.onSurfaceVariant
                font.pixelSize: 10
                font.family: ThemeService.fixedFont
            }

            Image {
                width: 28; height: 28
                source: WeatherService.iconPath(modelData.conditionId, isDay)
                fillMode: Image.PreserveAspectFit

                readonly property bool isDay:
                    modelData.timestamp >= WeatherService.current.sunrise &&
                    modelData.timestamp <  WeatherService.current.sunset
            }

            Text {
                text: Math.round(modelData.temperature) + "°"
                color: HoloniightPalette.onSurface
                font.pixelSize: 12
                font.family: ThemeService.fixedFont
            }
        }
    }
}
```

**Note (CLAUDE.md Canvas gotcha):** No Canvas in this component — all access is via delegate `required property var modelData`. No unqualified access issues.

### 10.6 `WeatherDailyCards.qml`

A `Column` with a section label and a `Row` of 5 card `Column`s. Width is distributed equally: each card = `(parentWidth - 4*spacing) / 5`. Cards show: weekday abbreviated, 32px icon, high/low temps.

```qml
Column {
    id: root
    spacing: 6
    width: parent.width

    Text { text: "Next 5 Days"; color: HoloniightPalette.onSurfaceVariant; font.pixelSize: 10 }

    Row {
        width: parent.width
        spacing: 4

        Repeater {
            model: Math.min(WeatherService.daily.length, 5)
            delegate: Column {
                required property int index
                width: (root.width - 16) / 5
                spacing: 3
                horizontalItemAlignment: Qt.AlignHCenter

                property var entry: WeatherService.daily[index]

                Text {
                    // Locale weekday name from entry.date (Unix timestamp)
                    text: Qt.formatDate(new Date(entry.date * 1000), "ddd")
                    color: HoloniightPalette.onSurfaceVariant
                    font.pixelSize: 10
                    font.family: ThemeService.fixedFont
                }

                Image {
                    width: 32; height: 32
                    source: WeatherService.iconPath(entry.conditionId, true)  // daily: always show day variant
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    text: Math.round(entry.tempMax) + "°"
                    color: HoloniightPalette.accentCyan
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    font.family: ThemeService.fixedFont
                }

                Text {
                    text: Math.round(entry.tempMin) + "°"
                    color: HoloniightPalette.onSurfaceVariant
                    font.pixelSize: 11
                    font.family: ThemeService.fixedFont
                }
            }
        }
    }
}
```

**Day/night for daily cards:** Daily cards use the day variant unconditionally (`isDay: true`). The daily condition represents the whole day and the day icon is visually clearer. This is a deliberate UX choice, not an omission.

### 10.7 `TemperatureGraph.qml`

A `Canvas` drawing a cyan polyline across the hourly temperature data.

```qml
import QtQuick
import HolonightShell
import Holonight

Canvas {
    id: root
    antialiasing: true

    readonly property var temps: {
        const list = WeatherService.hourly
        const result = []
        for (let i = 0; i < list.length; i++) {
            result.push(list[i].temperature)
        }
        return result
    }

    // Qualify ALL access with root id inside onPaint (CLAUDE.md Canvas gotcha)
    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()

        const pts = root.temps
        if (pts.length < 2) return

        const w = root.width
        const h = root.height
        const padV = 12
        const padH = 8

        const minT = Math.min.apply(null, pts)
        const maxT = Math.max.apply(null, pts)
        const range = (maxT - minT) || 1

        function xPos(i) { return padH + (i / (pts.length - 1)) * (w - 2 * padH) }
        function yPos(t) { return h - padV - ((t - minT) / range) * (h - 2 * padV) }

        // Subtle grid lines
        ctx.strokeStyle = Qt.rgba(HoloniightPalette.surface.r, HoloniightPalette.surface.g,
                                   HoloniightPalette.surface.b, 0.6)
        ctx.lineWidth = 1
        const midT = (minT + maxT) / 2
        for (const t of [minT, midT, maxT]) {
            const y = yPos(t)
            ctx.beginPath()
            ctx.moveTo(padH, y)
            ctx.lineTo(w - padH, y)
            ctx.stroke()
        }

        // Temperature polyline
        ctx.strokeStyle = HoloniightPalette.accentCyan
        ctx.lineWidth = 2
        ctx.lineJoin = "round"
        ctx.lineCap = "round"
        ctx.beginPath()
        ctx.moveTo(xPos(0), yPos(pts[0]))
        for (let i = 1; i < pts.length; i++) {
            ctx.lineTo(xPos(i), yPos(pts[i]))
        }
        ctx.stroke()

        // Y-axis labels at min/mid/max
        ctx.fillStyle = HoloniightPalette.onSurfaceVariant
        ctx.font = "9px " + ThemeService.fixedFont
        ctx.fillText(Math.round(maxT) + "°", 0, yPos(maxT) + 4)
        ctx.fillText(Math.round(midT) + "°", 0, yPos(midT) + 4)
        ctx.fillText(Math.round(minT) + "°", 0, yPos(minT) + 4)
    }

    Connections {
        target: WeatherService
        function onForecastChanged() { root.requestPaint() }
    }

    Component.onCompleted: root.requestPaint()
}
```

**CLAUDE.md Canvas gotchas applied:**
- All property access inside `onPaint` is qualified with `root.`.
- `Connections.target` is outside `onPaint`, so no qualification needed there.
- `MultiEffect` is not used in this component.

### 10.8 `PrecipitationGraph.qml`

A `Canvas` drawing violet bars. Bars with `precipitation == 0` are rendered at height 2px (minimal presence) so the x-axis is visible. If all entries are 0, the graph is a flat baseline.

```qml
Canvas {
    id: root
    antialiasing: true

    readonly property var precipValues: {
        const list = WeatherService.hourly
        const result = []
        for (let i = 0; i < list.length; i++) {
            result.push(list[i].precipitation)
        }
        return result
    }

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()

        const vals = root.precipValues
        if (vals.length === 0) return

        const w = root.width
        const h = root.height
        const padV = 8
        const padH = 8
        const barW = Math.max(2, (w - 2 * padH) / vals.length - 2)
        const maxP = Math.max.apply(null, vals)
        const scale = maxP > 0 ? (h - padV - 4) / maxP : 1

        ctx.fillStyle = HoloniightPalette.accentViolet   // #bb9af7 token

        for (let i = 0; i < vals.length; i++) {
            const barH = Math.max(2, vals[i] * scale)
            const x = padH + i * ((w - 2 * padH) / vals.length)
            const y = h - padV - barH
            ctx.fillRect(x, y, barW, barH)
        }

        // Y-axis label
        if (maxP > 0) {
            ctx.fillStyle = HoloniightPalette.onSurfaceVariant
            ctx.font = "9px " + ThemeService.fixedFont
            ctx.fillText(maxP.toFixed(1) + " mm", 0, padV + 8)
        }
    }

    Connections {
        target: WeatherService
        function onForecastChanged() { root.requestPaint() }
    }

    Component.onCompleted: root.requestPaint()
}
```

---

## 11. CMake Changes

### 11.1 `holonight_services` library — add weather source files

In the `add_library(holonight_services STATIC ...)` block, add after `src/services/ThemeService.cpp`:
```cmake
    src/services/weather/WeatherData.h
    src/services/weather/WeatherProvider.h
    src/services/weather/WeatherProvider.cpp
    src/services/weather/WeatherService.h
    src/services/weather/WeatherService.cpp
```

In the `target_include_directories(holonight_services PUBLIC ...)` block, add:
```cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/weather
```

No new `find_package` or `target_link_libraries` entries are needed: `Qt6::Network` is already linked to `holonight_services` (line 138 of current CMakeLists.txt) and `QNetworkAccessManager` is part of `Qt6::Network` (REQ-C-010 satisfied).

### 11.2 `HOLONIGHT_QML_FILES` — add 9 new QML files (alphabetical order maintained)

The list is sorted. Insert the new entries in alphabetical order within the existing `src/qml/Topbar/` block:

```cmake
    src/qml/Topbar/PrecipitationGraph.qml          # after BarTooltipArea, BarBackground, etc.
    src/qml/Topbar/TemperatureGraph.qml
    src/qml/Topbar/WeatherCurrentSection.qml
    src/qml/Topbar/WeatherDailyCards.qml
    src/qml/Topbar/WeatherDetailsGrid.qml
    src/qml/Topbar/WeatherHourlyStrip.qml
    src/qml/Topbar/WeatherPopupContent.qml
    src/qml/Topbar/WeatherSection.qml
    src/qml/Topbar/WeatherWidget.qml
```

Exact insertion positions (alphabetical within the full sorted list):

```
...
src/qml/Topbar/NetworkWidget.qml         (existing)
src/qml/Topbar/PrecipitationGraph.qml    (NEW — after N, before S)
src/qml/Controls/SessionIcon.qml         (existing)
src/qml/Topbar/SessionPopup.qml          (existing)
src/qml/Topbar/SessionSection.qml        (existing)
src/qml/Topbar/StatusPopup.qml           (existing)
src/qml/Topbar/StatusPopupDismissOverlay.qml (existing)
src/qml/Topbar/StatusPopupTriggerArea.qml    (existing)
src/qml/Topbar/StatusesSection.qml       (existing)
src/qml/Topbar/TemperatureGraph.qml      (NEW — after S, before T)
src/qml/Topbar/TooltipPopup.qml          (existing)
src/qml/Controls/UtilityIcon.qml         (existing)
src/qml/Topbar/WeatherCurrentSection.qml (NEW)
src/qml/Topbar/WeatherDailyCards.qml     (NEW)
src/qml/Topbar/WeatherDetailsGrid.qml    (NEW)
src/qml/Topbar/WeatherHourlyStrip.qml    (NEW)
src/qml/Topbar/WeatherPopupContent.qml   (NEW)
src/qml/Topbar/WeatherSection.qml        (NEW)
src/qml/Topbar/WeatherWidget.qml         (NEW)
src/qml/Topbar/WorkspacePill.qml         (existing)
src/qml/Topbar/WorkspaceSection.qml      (existing)
```

The `GLOB_RECURSE` vs explicit-list check at CMakeLists.txt lines 265–275 will catch any mismatch at configure time — this is the existing safety net.

### 11.3 `qt6_add_resources` — bundle weather SVG assets

Add a new `qt6_add_resources` call **after** the `qt6_add_qml_module` block and before the `target_link_libraries(holonight-shell PRIVATE holonight_app)` line:

```cmake
file(GLOB WEATHER_SVG_FILES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/weather/*.svg"
)

qt6_add_resources(holonight-shell "weather_icons"
    PREFIX "/HolonightShell"
    BASE   "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    FILES  ${WEATHER_SVG_FILES}
)
```

**Why GLOB here:** The existing `qt6_add_resources` pattern for non-QML assets uses `PREFIX "/HolonightShell"` and `BASE "assets"` (per CLAUDE.md). The 91 SVG files in `assets/weather/` would require an impractical explicit list. Using `GLOB` with `CONFIGURE_DEPENDS` means CMake re-runs when new SVG files are added. This matches the spirit of the `GLOB_RECURSE` already used for `HOLONIGHT_DISCOVERED_QML_FILES`. The result: `assets/weather/wsymbol_0001_sunny.svg` → alias `weather/wsymbol_0001_sunny.svg` → QRC path `qrc:/HolonightShell/weather/wsymbol_0001_sunny.svg`.

---

## 12. ShellApplication Wiring

### 12.1 `ShellApplication.h` additions

```cpp
// Forward declaration (add with other forward decls):
class WeatherService;

// Member (add after ThemeService* theme_ = nullptr;):
WeatherService* weather_ = nullptr;
```

### 12.2 `ShellApplication.cpp` additions

**Include:**
```cpp
#include "WeatherService.h"
```

**Constructor member-initializer list** (add after `theme_` line):
```cpp
weather_(new WeatherService(config_service_, this)),
```

**`registerQmlTypes()`** (add after `reg(theme_, "ThemeService");`):
```cpp
reg(weather_, "WeatherService");
```

**`startServices()`** (add after `network_->start();`):
```cpp
weather_->start();
```

No changes to `startShell()`.

**`WeatherService` construction order:** `WeatherService` requires `ConfigService*` (passed in constructor). `config_service_` is constructed first in the initializer list (line 28 of current `ShellApplication.cpp`), so `weather_` can safely be constructed after it.

---

## 13. Data Flow

### 13.1 Startup Sequence

```
ShellApplication::startServices()
  └── weather_->start()
        ├── applyConfig()          — snapshot WeatherConfig from ConfigService
        ├── loadCache()            — sync file read → parse JSON → populate current_/hourly_/daily_
        │     ├── [cache exists]  → setHasData(true); evaluate stale from fetched_at_
        │     └── [no cache]      → has_data_ = false; stale_ = true
        ├── start stale_timer_     — fires kStaleThresholdSecs after fetched_at_ (or 0 if already stale)
        └── resolveLocation()
              ├── [lat+lon in config] → location_resolved_ = true → scheduleFetch()
              └── [missing lat/lon]   → provider_->fetchGeolocation(geo_api_key_)
                    └── [async]
                          ├── onGeoFetched(lat, lon, city)
                          │     └── location_resolved_ = true → scheduleFetch()
                          └── onGeoError(msg)
                                └── qCWarning; geo_pending_ = false
                                    (no retry at startup; next tick via refresh_timer_)
```

### 13.2 Fetch Cycle

```
scheduleFetch()
  └── refresh_timer_.start(interval_ms)
        └── [timer fires] → onRefreshTimer()
              └── fetch()
                    ├── provider_->fetchCurrentWeather(lat_, lon_, api_key_, units_, lang_)
                    └── provider_->fetchForecast(lat_, lon_, api_key_, units_, lang_)

[Two async paths run in parallel:]

Path A: onCurrentFetched(weather)
  └── current_ = weather
      current_pending_ = false
      if !forecast_pending_: → onBothComplete()

Path B: onForecastFetched(hourly, daily)
  └── hourly_ = hourly; daily_ = daily
      forecast_pending_ = false
      if !current_pending_: → onBothComplete()

onBothComplete()
  ├── fetched_at_ = QDateTime::currentSecsSinceEpoch()
  ├── setHasData(true)
  ├── setStale(false)
  ├── emit currentChanged()
  ├── emit forecastChanged()
  ├── saveCache()
  ├── stale_timer_.start(kStaleThresholdSecs * 1000)
  └── resetBackoff() → scheduleFetch() with base interval

Path C: onFetchError(msg)
  ├── qCWarning(lcWeather) << msg
  ├── setStale(true)
  └── stepBackoff() → scheduleFetch() with backoff interval
```

### 13.3 Config Reload

```
ConfigService emits weatherChanged()
  └── WeatherService::onConfigChanged()
        ├── applyConfig()      — update api_key_, units_, etc.
        ├── resetBackoff()
        ├── setStale(false)
        └── [lat/lon changed or missing]
              ├── resolveLocation()   — re-run geolocation if needed
              └── [already resolved] → scheduleFetch() immediately
```

---

## 14. Key Decisions, Alternatives, and Risks

### 14.1 OWM 2.5 API Deprecation

**Status:** OWM 2.5 is in a "deprecated but still operational" state as of 2024–2025. OWM officially announced migration to their 3.0 API (One Call), which requires a paid subscription (free tier discontinued for new accounts). For existing free-tier accounts, 2.5 remains accessible.

**Decision:** Use OWM 2.5 as specified by REQ-C-001. The `WeatherProvider` URL constants are isolated in `WeatherProvider.cpp`; migrating to 3.0 later requires only changing URL strings and the JSON path for the `one_call` response structure. No architectural changes needed.

**Risk:** If OWM 2.5 is fully sunset during development or use, the widget will fail. The `onFetchError` path + stale display + exponential backoff handle this gracefully (widget shows stale data, not blank). User must update `api_key` and potentially the URL when migrating.

### 14.2 QNetworkAccessManager Lifetime

**Decision:** `QNetworkAccessManager nam_` is a direct member of `WeatherProvider` (not a pointer, not a shared instance). `WeatherProvider` is owned by `WeatherService` via `std::unique_ptr<WeatherProvider>`. `WeatherService` is owned by `ShellApplication` as a raw pointer parented to `ShellApplication` (following every other service). This lifetime chain is safe: QNAM is destroyed with `WeatherProvider`, which is destroyed with `WeatherService`, which is destroyed last via Qt parent ownership.

**Alternative considered:** Sharing the QNAM with `NetworkService` or using `QNetworkAccessManager::globalInstance()`. Rejected: sharing QNAM across unrelated services creates coupling and per-request header/proxy configuration conflicts. Qt's global instance has uncertain lifetime in plugin contexts.

### 14.3 JSON Schema Fragility

**Risk:** OWM JSON fields (`rain`, `snow`, `sys.sunrise`) may be absent in some conditions (clear day has no `rain` key; marine/sea locations may lack `sys`). 

**Mitigation:** All field accesses use `.value("key").toDouble(0.0)` / `.toDouble(0)` / `.toString("")` with explicit default. `rain` and `snow` objects are checked for existence before field access (see §6 precipitation parsing). `sys.sunrise`/`sys.sunset` default to 0, which causes the icon-code `d/n` suffix fallback path (§5 day/night resolution).

### 14.4 Transparent API Keys in Config

**Reality:** `api_key` and `geo_api_key` are stored in plaintext in `$XDG_CONFIG_HOME/holonight/config.toml`. This is the same model used by every other shell that integrates with OWM (Waybar, Polybar, i3status-rust all store API keys in config files). The file is user-readable only by default (mode 600 via `QSaveFile`).

**Design choice:** Do not store keys in the cache file (REQ-C-004). Log a `qCWarning` if `api_key` is empty rather than silently failing with a 401.

### 14.5 Canvas Repaint on Data Change

**Risk:** `TemperatureGraph` and `PrecipitationGraph` use `Canvas.requestPaint()` triggered by `WeatherService.forecastChanged`. If this signal fires during an active paint (theoretically possible in rapid config reload), double-paint may occur.

**Mitigation:** `requestPaint()` is idempotent in Qt Quick — multiple calls within a single frame are coalesced to one actual paint. This is safe.

**CLAUDE.md Canvas gotcha applied:** All property accesses inside `onPaint` handlers are qualified with `root.` (e.g., `root.temps`, `root.precipValues`, `root.width`). `ThemeService.fixedFont` is accessed directly as a singleton, which qmllint allows; the `--unqualified disable` flag in the CMake `qml-lint` target suppresses any residual warnings for singleton access.

### 14.6 Popup Height Overflow

**Risk:** If fewer than 5 hourly entries or fewer than 5 daily entries are available (first-launch, data partially loaded), sections may be shorter than expected, leaving whitespace at the bottom of the popup.

**Mitigation:** All sections guard with `WeatherService.hasData` and `WeatherService.hourly.length > 0`. Sections with empty data render at minimal height. The popup panel has a fixed height (not dynamically sized); empty space at the bottom is acceptable — the content Column will not overflow.

**Not a crash risk:** `QVariantList` access with `model: WeatherService.hourly` in ListView safely renders 0 delegates when empty.

### 14.7 Multi-Monitor Popup Routing

**Reuse, not new code.** The `StatusPopupTriggerArea` in `WeatherWidget.qml` calls `StatusPopupSurface.toggle(popupId, barMonitorName, globalPos.x, width)` — identical to all other widgets. The `barMonitorName` flows from `TopBar.qml` → `WeatherSection.qml` → `WeatherWidget.qml` as a `required property string barMonitorName`, following the chain documented in CLAUDE.md and the memory files. No new C++ routing code is needed (REQ-C-009 satisfied).

### 14.8 Q_GADGET vs QAbstractListModel Decision Detail

**Chosen:** `Q_GADGET` value types in `QVariantList`.

**`Q_GADGET` pros:** No model boilerplate; works directly with `Repeater { model: WeatherService.hourly }` + `required property var modelData`; read-only from QML (sufficient — forecast data is never modified from QML); list replacement is atomic (one `forecastChanged` signal, no begin/endResetModel).

**`Q_GADGET` cons:** The entire `QVariantList` is replaced on each fetch (Qt copies the list). For 40 entries of `HourlyEntry` (5 doubles + 1 int + 1 QString = ~50 bytes each), this is ~2 KB per replacement — negligible.

**`QAbstractListModel` would be appropriate if:** entries were updated individually (e.g., real-time temperature stream). That is not the case here.

### 14.9 WeatherProvider as a Separate Class (Test Seam)

**Rationale:** By injecting `WeatherProvider` via `std::unique_ptr<WeatherProvider>` in the test constructor of `WeatherService`, unit tests can provide a mock `WeatherProvider` subclass that emits signals without making real network calls. This pattern is identical to `BatteryService(DbusPropertyClientPtr dbus, ...)` where `DbusPropertyClientPtr` is injected for testing. The `SkipInitTag` constructor is also provided for tests that need a service with no initialization at all.
