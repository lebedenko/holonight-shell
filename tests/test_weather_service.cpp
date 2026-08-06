#include "ConfigService.h"
#include "WeatherData.h"
#include "WeatherProvider.h"
#include "WeatherService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTimeZone>

#include <gtest/gtest.h>

namespace {

QString weatherIcon(const QString& file_name) { return QStringLiteral("qrc:/HolonightShell/weather/") + file_name; }

QString setTempXdg(const QTemporaryDir& tmp) {
  qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
  return tmp.path() + "/holonight/config.toml";
}

void writeTempConfig(const QString& path, const QByteArray& content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write(content);
}

// Subclass WeatherProvider to override virtual methods and bypass actual network activity.
class FakeWeatherProvider : public WeatherProvider {
 public:
  using WeatherProvider::WeatherProvider;

  void fetchWeather(double lat, double lon, const QString& api_key, const QString& units,
                    const QString& lang) override {
    last_lat = lat;
    last_lon = lon;
    last_api_key = api_key;
    last_units = units;
    last_lang = lang;
    fetch_weather_calls++;
  }

  void fetchGeolocation(const QString& geo_api_key) override {
    last_geo_api_key = geo_api_key;
    fetch_geo_calls++;
  }

  void reverseGeocode(double lat, double lon, const QString& api_key) override {
    last_reverse_lat = lat;
    last_reverse_lon = lon;
    last_reverse_api_key = api_key;
    reverse_geocode_calls++;
  }

  void cancelPending() override { cancel_pending_calls++; }

  void triggerWeatherFetched(const CurrentWeather& weather, const QList<HourlyEntry>& hourly,
                             const QList<DailyEntry>& daily) {
    emit weatherFetched(weather, hourly, daily);
  }

  void triggerFetchError(const QString& msg, FetchErrorKind kind = FetchErrorKind::Transient) {
    emit fetchError(kind, msg);
  }

  void triggerGeoFetched(double lat, double lon, const WeatherLocation& location) {
    emit geoFetched(lat, lon, location);
  }

  void triggerGeoError(const QString& msg) { emit geoError(msg); }
  void triggerLocationFetched(const WeatherLocation& location) { emit locationFetched(location); }
  void triggerLocationError(const QString& msg) { emit locationError(msg); }

  double last_lat{0.0};
  double last_lon{0.0};
  QString last_api_key;
  QString last_units;
  QString last_lang;
  QString last_geo_api_key;
  double last_reverse_lat{0.0};
  double last_reverse_lon{0.0};
  QString last_reverse_api_key;
  int fetch_weather_calls{0};
  int fetch_geo_calls{0};
  int reverse_geocode_calls{0};
  int cancel_pending_calls{0};
};

}  // namespace

// --- Part 1: Icon Mapping Tests (existing) ---

TEST(WeatherService, IconPathMapsKnownDayAndNightConditions) {
  EXPECT_EQ(WeatherService::iconPath(800, true), weatherIcon(QStringLiteral("wsymbol_0001_sunny.svg")));
  EXPECT_EQ(WeatherService::iconPath(800, false), weatherIcon(QStringLiteral("wsymbol_0008_clear_sky_night.svg")));

  EXPECT_EQ(WeatherService::iconPath(500, true),
            weatherIcon(QStringLiteral("wsymbol_0017_cloudy_with_light_rain.svg")));
  EXPECT_EQ(WeatherService::iconPath(500, false),
            weatherIcon(QStringLiteral("wsymbol_0033_cloudy_with_light_rain_night.svg")));
}

TEST(WeatherService, IconPathUsesSameIconWhenNoNightVariantExists) {
  EXPECT_EQ(WeatherService::iconPath(762, true), weatherIcon(QStringLiteral("wsymbol_0091_volcanic_ash.svg")));
  EXPECT_EQ(WeatherService::iconPath(762, false), weatherIcon(QStringLiteral("wsymbol_0091_volcanic_ash.svg")));
}

TEST(WeatherService, IconPathFallsBackForUnknownCondition) {
  EXPECT_EQ(WeatherService::iconPath(9999, true), weatherIcon(QStringLiteral("wsymbol_0999_unknown.svg")));
  EXPECT_EQ(WeatherService::iconPath(9999, false), weatherIcon(QStringLiteral("wsymbol_0999_unknown.svg")));
}

// --- Part 2: Deterministic Response Parsing Tests ---

TEST(WeatherProviderParsing, ParseCurrentJsonSuccess) {
  QJsonObject current_obj;
  current_obj["temp"] = 21.5;
  current_obj["feels_like"] = 20.0;
  current_obj["humidity"] = 65;
  current_obj["pressure"] = 1013;
  current_obj["uvi"] = 5.5;
  current_obj["dew_point"] = 14.2;
  current_obj["clouds"] = 40;
  current_obj["wind_speed"] = 5.0;  // 5.0 m/s * 3.6 = 18 km/h
  current_obj["wind_deg"] = 45;
  current_obj["wind_gust"] = 8.0;  // 8.0 m/s * 3.6 = 29 km/h
  current_obj["visibility"] = 10000.0;
  current_obj["sunrise"] = 1600000000.0;
  current_obj["sunset"] = 1600045000.0;
  current_obj["dt"] = 1600020000.0;

  QJsonObject weather_info;
  weather_info["id"] = 801;
  weather_info["description"] = "few clouds";
  weather_info["icon"] = "02d";
  QJsonArray weather_arr;
  weather_arr.append(weather_info);
  current_obj["weather"] = weather_arr;

  CurrentWeather parsed = WeatherProvider::parseCurrentJson(current_obj);
  EXPECT_DOUBLE_EQ(parsed.temperature, 21.5);
  EXPECT_DOUBLE_EQ(parsed.feels_like, 20.0);
  EXPECT_EQ(parsed.humidity, 65);
  EXPECT_EQ(parsed.pressure, 1013);
  EXPECT_DOUBLE_EQ(parsed.uvi, 5.5);
  EXPECT_DOUBLE_EQ(parsed.dew_point, 14.2);
  EXPECT_EQ(parsed.clouds, 40);
  EXPECT_EQ(parsed.wind_speed, 18);
  EXPECT_EQ(parsed.wind_direction, 45);
  EXPECT_EQ(parsed.wind_gust, 29);
  EXPECT_DOUBLE_EQ(parsed.visibility, 10.0);
  EXPECT_EQ(parsed.sunrise, 1600000000);
  EXPECT_EQ(parsed.sunset, 1600045000);
  EXPECT_FALSE(parsed.time_updated.isEmpty());
}

TEST(WeatherProviderParsing, ParsesLocationWithoutAdministrativeRegion) {
  const QJsonObject automatic{{QStringLiteral("city"), QStringLiteral("Kyiv")},
                              {QStringLiteral("country_name"), QStringLiteral("Ukraine")},
                              {QStringLiteral("state_prov"), QStringLiteral("Kyiv City")}};
  EXPECT_EQ(WeatherProvider::parseLocationJson(automatic),
            (WeatherLocation{QStringLiteral("Kyiv"), QStringLiteral("Ukraine")}));

  const QJsonObject reverse{{QStringLiteral("name"), QStringLiteral("Lviv")},
                            {QStringLiteral("country"), QStringLiteral("UA")},
                            {QStringLiteral("state"), QStringLiteral("Lviv Oblast")}};
  EXPECT_EQ(WeatherProvider::parseLocationJson(reverse),
            (WeatherLocation{QStringLiteral("Lviv"), QStringLiteral("Ukraine")}));
}

TEST(WeatherProviderParsing, ParseCurrentJsonMalformedOrEmpty) {
  QJsonObject current_obj;  // Empty
  CurrentWeather parsed = WeatherProvider::parseCurrentJson(current_obj);
  EXPECT_DOUBLE_EQ(parsed.temperature, 0.0);
  EXPECT_DOUBLE_EQ(parsed.feels_like, 0.0);
  EXPECT_EQ(parsed.condition_id, 0);
  EXPECT_TRUE(parsed.condition.isEmpty());
  EXPECT_EQ(parsed.wind_speed, 0);
}

TEST(WeatherProviderParsing, ParseCurrentJsonClampsPercentages) {
  const QJsonObject current_obj{{"humidity", -20}, {"clouds", 140}};

  const CurrentWeather parsed = WeatherProvider::parseCurrentJson(current_obj);

  EXPECT_EQ(parsed.humidity, 0);
  EXPECT_EQ(parsed.clouds, 100);
}

TEST(WeatherProviderParsing, ParseHourlyJsonSuccess) {
  QJsonArray list;
  QJsonObject entry;
  entry["dt"] = 1600020000.0;
  entry["temp"] = 19.5;
  entry["pop"] = 0.2;
  entry["uvi"] = 1.2;
  entry["clouds"] = 75;

  QJsonObject rain_obj;
  rain_obj["1h"] = 0.5;
  entry["rain"] = rain_obj;

  QJsonObject weather_info;
  weather_info["id"] = 500;
  weather_info["description"] = "light rain";
  weather_info["icon"] = "10d";
  QJsonArray weather_arr;
  weather_arr.append(weather_info);
  entry["weather"] = weather_arr;

  list.append(entry);

  QList<HourlyEntry> parsed = WeatherProvider::parseHourlyJson(list);
  ASSERT_EQ(parsed.size(), 1);
  EXPECT_EQ(parsed[0].timestamp, 1600020000);
  EXPECT_DOUBLE_EQ(parsed[0].temperature, 19.5);
  EXPECT_DOUBLE_EQ(parsed[0].precipitation, 0.5);
  EXPECT_DOUBLE_EQ(parsed[0].pop, 0.2);
  EXPECT_DOUBLE_EQ(parsed[0].uvi, 1.2);
  EXPECT_EQ(parsed[0].clouds, 75);
  EXPECT_EQ(parsed[0].condition_id, 500);
  EXPECT_EQ(parsed[0].condition, "light rain");
  EXPECT_EQ(parsed[0].icon_code, "10d");
}

TEST(WeatherProviderParsing, ParseHourlyJsonClampsBoundedValues) {
  QJsonObject entry{{"pop", 1.5}, {"clouds", -1}};
  entry["rain"] = QJsonObject{{"1h", -2.0}};
  entry["snow"] = QJsonObject{{"1h", -3.0}};

  const QList<HourlyEntry> parsed = WeatherProvider::parseHourlyJson(QJsonArray{entry});

  ASSERT_EQ(parsed.size(), 1);
  EXPECT_DOUBLE_EQ(parsed[0].pop, 1.0);
  EXPECT_EQ(parsed[0].clouds, 0);
  EXPECT_DOUBLE_EQ(parsed[0].precipitation, 0.0);
}

TEST(WeatherProviderParsing, ParseDailyJsonSuccess) {
  QJsonArray list;
  QJsonObject entry;
  entry["dt"] = 1600020000.0;
  entry["sunrise"] = 1600000000.0;
  entry["sunset"] = 1600045000.0;
  entry["pop"] = 0.8;
  entry["uvi"] = 6.0;
  entry["moon_phase"] = 0.25;
  entry["summary"] = "Rainy day";

  QJsonObject temp_obj;
  temp_obj["min"] = 15.0;
  temp_obj["max"] = 22.0;
  entry["temp"] = temp_obj;

  QJsonObject weather_info;
  weather_info["id"] = 501;
  weather_info["description"] = "moderate rain";
  weather_info["icon"] = "10d";
  QJsonArray weather_arr;
  weather_arr.append(weather_info);
  entry["weather"] = weather_arr;

  list.append(entry);

  QList<DailyEntry> parsed = WeatherProvider::parseDailyJson(list);
  ASSERT_EQ(parsed.size(), 1);
  EXPECT_EQ(parsed[0].date, 1600020000);
  EXPECT_EQ(parsed[0].sunrise, 1600000000);
  EXPECT_EQ(parsed[0].sunset, 1600045000);
  EXPECT_DOUBLE_EQ(parsed[0].temp_min, 15.0);
  EXPECT_DOUBLE_EQ(parsed[0].temp_max, 22.0);
  EXPECT_DOUBLE_EQ(parsed[0].pop, 0.8);
  EXPECT_DOUBLE_EQ(parsed[0].uvi, 6.0);
  EXPECT_DOUBLE_EQ(parsed[0].moon_phase, 0.25);
  EXPECT_EQ(parsed[0].summary, "Rainy day");
  EXPECT_EQ(parsed[0].condition_id, 501);
  EXPECT_EQ(parsed[0].condition, "moderate rain");
  EXPECT_EQ(parsed[0].icon_code, "10d");
}

TEST(WeatherProviderParsing, ParseDailyJsonClampsProbabilityAndMoonPhase) {
  const QJsonObject entry{{"pop", -0.2}, {"moon_phase", 1.5}};

  const QList<DailyEntry> parsed = WeatherProvider::parseDailyJson(QJsonArray{entry});

  ASSERT_EQ(parsed.size(), 1);
  EXPECT_DOUBLE_EQ(parsed[0].pop, 0.0);
  EXPECT_DOUBLE_EQ(parsed[0].moon_phase, 1.0);
}

TEST(WeatherProviderParsing, PopulatePollutionDataSuccess) {
  CurrentWeather current;
  current.aqi = 0;

  QJsonObject pollution_obj;
  QJsonArray list;
  QJsonObject entry;
  QJsonObject main;
  main["aqi"] = 3;
  entry["main"] = main;

  QJsonObject components;
  components["pm2_5"] = 12.5;
  components["pm10"] = 20.0;
  components["co"] = 250.0;
  components["no2"] = 15.2;
  components["o3"] = 45.0;
  components["so2"] = 5.5;
  entry["components"] = components;

  list.append(entry);
  pollution_obj["list"] = list;

  WeatherProvider::populatePollutionData(current, pollution_obj);

  EXPECT_EQ(current.aqi, 3);
  EXPECT_DOUBLE_EQ(current.pm2_5, 12.5);
  EXPECT_DOUBLE_EQ(current.pm10, 20.0);
  EXPECT_DOUBLE_EQ(current.co, 250.0);
  EXPECT_DOUBLE_EQ(current.no2, 15.2);
  EXPECT_DOUBLE_EQ(current.o3, 45.0);
  EXPECT_DOUBLE_EQ(current.so2, 5.5);
}

TEST(WeatherProviderParsing, PopulatePollutionDataClampsAqiButPreservesMissingValue) {
  QJsonObject high_aqi{{"list", QJsonArray{QJsonObject{{"main", QJsonObject{{"aqi", 7}}}}}}};
  CurrentWeather current;

  WeatherProvider::populatePollutionData(current, high_aqi);
  EXPECT_EQ(current.aqi, 5);

  WeatherProvider::populatePollutionData(current,
                                         QJsonObject{{"list", QJsonArray{QJsonObject{{"main", QJsonObject{}}}}}});
  EXPECT_EQ(current.aqi, 5);

  CurrentWeather missing_aqi;
  WeatherProvider::populatePollutionData(missing_aqi,
                                         QJsonObject{{"list", QJsonArray{QJsonObject{{"main", QJsonObject{}}}}}});
  EXPECT_EQ(missing_aqi.aqi, 0);
}

// --- Part 3: WeatherService Tests ---

class WeatherServiceTest : public ::testing::Test {
 protected:
  QTemporaryDir tmp_config;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  QTemporaryDir tmp_cache;   // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  QString config_path;       // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

  void SetUp() override {
    qputenv("XDG_CONFIG_HOME", tmp_config.path().toUtf8());
    qputenv("XDG_CACHE_HOME", tmp_cache.path().toUtf8());
    config_path = tmp_config.path() + "/holonight/config.toml";
  }

  void TearDown() override {
    qunsetenv("XDG_CONFIG_HOME");
    qunsetenv("XDG_CACHE_HOME");
  }

  void writeCache(qint64 fetched_at, double temp) {
    QDir().mkpath(tmp_cache.path() + "/holonight");
    QFile file(tmp_cache.path() + "/holonight/weather.json");
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));

    QJsonObject current_obj;
    current_obj["temp"] = temp;
    current_obj["feels"] = temp - 1.0;
    current_obj["cond"] = "Clear";
    current_obj["cond_id"] = 800;
    current_obj["wind_deg"] = 225;
    current_obj["updated"] = "2026-06-19T16:00:00Z";

    QJsonObject root;
    root["fetched_at"] = static_cast<double>(fetched_at);
    root["current"] = current_obj;
    root["hourly"] = QJsonArray();
    root["daily"] = QJsonArray();

    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  }

  [[nodiscard]] static bool isLocationResolved(const WeatherService& service) { return service.location_resolved_; }
  [[nodiscard]] static double getLatitude(const WeatherService& service) { return service.lat_; }
  [[nodiscard]] static double getLongitude(const WeatherService& service) { return service.lon_; }
  [[nodiscard]] static int getBackoffStep(const WeatherService& service) { return service.backoff_step_; }
  [[nodiscard]] static bool isRefreshTimerActive(const WeatherService& service) {
    return service.refresh_timer_.isActive();
  }
  [[nodiscard]] static int getRefreshTimerInterval(const WeatherService& service) {
    return service.refresh_timer_.interval();
  }
  [[nodiscard]] static const QVariantList& getHourlyVariantCache(const WeatherService& service) {
    return service.hourly_variant_;
  }
  [[nodiscard]] static QString formatLocation(const WeatherLocation& location) {
    return WeatherService::formatLocationLabel(location);
  }
  static void triggerRefreshTimer(WeatherService& service) { service.onRefreshTimer(); }
};

TEST_F(WeatherServiceTest, FormatsLocationComponentsWithoutDanglingPunctuation) {
  EXPECT_EQ(formatLocation({QStringLiteral(" Lviv "), QStringLiteral(" Ukraine ")}), QStringLiteral("Lviv, Ukraine"));
  EXPECT_EQ(formatLocation({QStringLiteral("Lviv"), {}}), QStringLiteral("Lviv"));
  EXPECT_EQ(formatLocation({{}, QStringLiteral("Ukraine")}), QStringLiteral("Ukraine"));
  EXPECT_TRUE(formatLocation({}).isEmpty());
}

TEST_F(WeatherServiceTest, PinnedLocationUsesConfiguredLabelWithoutReverseLookup) {
  writeTempConfig(config_path,
                  "[weather]\n"
                  "api_key = \"key\"\n"
                  "latitude = 49.8397\n"
                  "longitude = 24.0297\n"
                  "city = \"Lviv\"\n"
                  "country = \"Ukraine\"\n");
  ConfigService config;
  auto provider = std::make_unique<FakeWeatherProvider>();
  auto* raw_provider = provider.get();
  WeatherService service(&config, std::move(provider));
  QSignalSpy location_spy(&service, &WeatherService::locationChanged);

  service.start();

  EXPECT_EQ(service.locationLabel(), QStringLiteral("Lviv, Ukraine"));
  EXPECT_EQ(location_spy.count(), 1);
  EXPECT_EQ(raw_provider->reverse_geocode_calls, 0);
  EXPECT_EQ(raw_provider->fetch_weather_calls, 1);
}

TEST_F(WeatherServiceTest, PinnedCityOnlyResolvesCountryWithoutBlockingWeather) {
  writeTempConfig(config_path,
                  "[weather]\n"
                  "api_key = \"key\"\n"
                  "latitude = 49.8397\n"
                  "longitude = 24.0297\n"
                  "city = \"Lviv\"\n");
  ConfigService config;
  auto provider = std::make_unique<FakeWeatherProvider>();
  auto* raw_provider = provider.get();
  WeatherService service(&config, std::move(provider));

  service.start();

  EXPECT_EQ(raw_provider->reverse_geocode_calls, 1);
  EXPECT_EQ(raw_provider->fetch_weather_calls, 1);
  EXPECT_EQ(service.locationLabel(), QStringLiteral("Lviv"));

  raw_provider->triggerLocationFetched(
      {.city = QStringLiteral("Lviv municipality"), .country = QStringLiteral("Ukraine")});
  EXPECT_EQ(service.locationLabel(), QStringLiteral("Lviv, Ukraine"));
}

TEST_F(WeatherServiceTest, ConfiguredStateValidation) {
  // Scenario 1: Empty config (no api_key)
  {
    writeTempConfig(config_path, "[weather]\n");
    ConfigService config;
    WeatherService service(&config);
    service.applyConfig();
    EXPECT_FALSE(service.configured());
  }

  // Scenario 2: api_key present but no coords/geo_api_key
  {
    writeTempConfig(config_path,
                    "[weather]\n"
                    "api_key = \"dummy\"\n");
    ConfigService config;
    WeatherService service(&config);
    service.applyConfig();
    EXPECT_FALSE(service.configured());
  }

  // Scenario 3: api_key and coordinates present
  {
    writeTempConfig(config_path,
                    "[weather]\n"
                    "api_key = \"dummy\"\n"
                    "latitude = 45.0\n"
                    "longitude = -75.0\n");
    ConfigService config;
    WeatherService service(&config);
    service.applyConfig();
    EXPECT_TRUE(service.configured());
  }

  // Scenario 4: api_key and geo_api_key present
  {
    writeTempConfig(config_path,
                    "[weather]\n"
                    "api_key = \"dummy\"\n"
                    "geo_api_key = \"dummy_geo\"\n");
    ConfigService config;
    WeatherService service(&config);
    service.applyConfig();
    EXPECT_TRUE(service.configured());
  }
}

TEST_F(WeatherServiceTest, CacheLoadBehavior) {
  // Case A: Recent Cache
  {
    qint64 now = QDateTime::currentSecsSinceEpoch();
    writeCache(now - 300, 22.5);  // 5 minutes old

    writeTempConfig(config_path,
                    "[weather]\n"
                    "api_key = \"dummy\"\n"
                    "latitude = 45.0\n"
                    "longitude = -75.0\n");
    ConfigService config;
    auto provider = std::make_unique<FakeWeatherProvider>();
    WeatherService service(&config, std::move(provider));

    service.start();

    EXPECT_TRUE(service.hasData());
    EXPECT_FALSE(service.stale());
    auto cur = service.currentVariant().value<CurrentWeather>();
    EXPECT_DOUBLE_EQ(cur.temperature, 22.5);
    EXPECT_EQ(cur.wind_direction, 225);
  }

  // Case B: Stale Cache
  {
    qint64 now = QDateTime::currentSecsSinceEpoch();
    writeCache(now - 7200, 15.0);  // 2 hours old (threshold is 1 hour)

    writeTempConfig(config_path,
                    "[weather]\n"
                    "api_key = \"dummy\"\n"
                    "latitude = 45.0\n"
                    "longitude = -75.0\n");
    ConfigService config;
    auto provider = std::make_unique<FakeWeatherProvider>();
    WeatherService service(&config, std::move(provider));

    service.start();

    EXPECT_TRUE(service.hasData());
    EXPECT_TRUE(service.stale());
  }
}

TEST_F(WeatherServiceTest, FetchAndGeoResolutionProcess) {
  writeTempConfig(config_path,
                  "[weather]\n"
                  "api_key = \"key_weather\"\n"
                  "geo_api_key = \"key_geo\"\n"
                  "refresh_interval = 60\n");
  ConfigService config;
  auto provider_ptr = std::make_unique<FakeWeatherProvider>();
  auto* raw_provider = provider_ptr.get();
  WeatherService service(&config, std::move(provider_ptr));

  QSignalSpy spy_has_data(&service, &WeatherService::hasDataChanged);
  QSignalSpy spy_stale(&service, &WeatherService::staleChanged);

  service.start();

  // Since geo_api_key is set but no coords, it should start geolocation resolution.
  EXPECT_EQ(raw_provider->fetch_geo_calls, 1);
  EXPECT_EQ(raw_provider->last_geo_api_key, "key_geo");

  // Geolocation fails -> schedules backoff retry.
  raw_provider->triggerGeoError("geo failed");
  EXPECT_TRUE(service.stale());
  EXPECT_EQ(getBackoffStep(service), 1);
  EXPECT_TRUE(isRefreshTimerActive(service));
  EXPECT_EQ(getRefreshTimerInterval(service), 1000);  // Base backoff 1s

  // Reset/trigger again
  raw_provider->fetch_geo_calls = 0;
  triggerRefreshTimer(service);
  EXPECT_EQ(raw_provider->fetch_geo_calls, 1);

  // Geolocation succeeds -> should fetch weather.
  raw_provider->triggerGeoFetched(40.7128, -74.0060,
                                  {.city = QStringLiteral("New York"), .country = QStringLiteral("United States")});
  EXPECT_TRUE(isLocationResolved(service));
  EXPECT_DOUBLE_EQ(getLatitude(service), 40.7128);
  EXPECT_DOUBLE_EQ(getLongitude(service), -74.0060);
  EXPECT_EQ(service.locationLabel(), QStringLiteral("New York, United States"));
  EXPECT_EQ(raw_provider->fetch_weather_calls, 1);
  EXPECT_EQ(raw_provider->last_api_key, "key_weather");

  // Fetch fails -> backoff increments, refresh timer starts.
  raw_provider->triggerFetchError("fetch failed");
  EXPECT_TRUE(service.stale());
  EXPECT_EQ(getBackoffStep(service), 2);
  EXPECT_TRUE(isRefreshTimerActive(service));
  EXPECT_EQ(getRefreshTimerInterval(service), 2000);  // 2s backoff

  // Trigger retry timer.
  triggerRefreshTimer(service);
  EXPECT_EQ(raw_provider->fetch_weather_calls, 2);

  // Fetch success on retry.
  CurrentWeather mock_weather;
  mock_weather.temperature = 18.0;
  mock_weather.feels_like = 17.5;
  mock_weather.condition = "Partly Cloudy";
  mock_weather.condition_id = 801;

  raw_provider->triggerWeatherFetched(mock_weather, {}, {});

  EXPECT_TRUE(service.hasData());
  EXPECT_FALSE(service.stale());
  EXPECT_EQ(getBackoffStep(service), 0);
  EXPECT_TRUE(isRefreshTimerActive(service));
  EXPECT_EQ(getRefreshTimerInterval(service), 60000);  // refresh_interval is 60s
}

TEST_F(WeatherServiceTest, IdlePauseCancelsPendingWorkAndResumeFetchesImmediately) {
  writeTempConfig(config_path,
                  "[weather]\n"
                  "api_key = \"key_weather\"\n"
                  "latitude = 40.0\n"
                  "longitude = -70.0\n"
                  "refresh_interval = 60\n");
  ConfigService config;
  auto provider_ptr = std::make_unique<FakeWeatherProvider>();
  auto* raw_provider = provider_ptr.get();
  WeatherService service(&config, std::move(provider_ptr));

  service.start();
  ASSERT_EQ(raw_provider->fetch_weather_calls, 1);

  service.onIdleChanged(true);

  EXPECT_EQ(raw_provider->cancel_pending_calls, 1);
  EXPECT_FALSE(isRefreshTimerActive(service));

  service.onIdleChanged(false);

  EXPECT_EQ(raw_provider->fetch_weather_calls, 2);
  EXPECT_TRUE(isRefreshTimerActive(service));
  EXPECT_EQ(getRefreshTimerInterval(service), 60000);
}

TEST_F(WeatherServiceTest, AuthenticationFailureStopsAutomaticRetries) {
  writeTempConfig(config_path,
                  "[weather]\n"
                  "api_key = \"revoked\"\n"
                  "latitude = 40.0\n"
                  "longitude = -70.0\n");
  ConfigService config;
  auto provider = std::make_unique<FakeWeatherProvider>();
  auto* raw_provider = provider.get();
  WeatherService service(&config, std::move(provider));
  QSignalSpy authentication_spy(&service, &WeatherService::authenticationFailed);

  service.start();
  raw_provider->triggerFetchError(QStringLiteral("HTTP 401 on /data/3.0/onecall"),
                                  WeatherProvider::FetchErrorKind::Authentication);

  EXPECT_EQ(authentication_spy.count(), 1);
  EXPECT_FALSE(isRefreshTimerActive(service));
  EXPECT_EQ(getBackoffStep(service), 0);
  EXPECT_TRUE(service.stale());
}

TEST_F(WeatherServiceTest, ForecastVariantsAreRebuiltWhenForecastChanges) {
  writeTempConfig(config_path,
                  "[weather]\n"
                  "api_key = \"key\"\n"
                  "latitude = 40.0\n"
                  "longitude = -70.0\n");
  ConfigService config;
  auto provider = std::make_unique<FakeWeatherProvider>();
  auto* raw_provider = provider.get();
  WeatherService service(&config, std::move(provider));
  service.start();

  HourlyEntry first;
  first.temperature = 12.0;
  raw_provider->triggerWeatherFetched({}, {first}, {});
  ASSERT_EQ(service.hourlyVariant().size(), 1);
  EXPECT_EQ(getHourlyVariantCache(service).at(0).value<HourlyEntry>().temperature, 12.0);

  HourlyEntry replacement;
  replacement.temperature = 18.0;
  raw_provider->triggerWeatherFetched({}, {replacement}, {});
  ASSERT_EQ(service.hourlyVariant().size(), 1);
  EXPECT_EQ(getHourlyVariantCache(service).at(0).value<HourlyEntry>().temperature, 18.0);
}
