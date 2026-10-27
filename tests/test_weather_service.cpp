#include "ConfigService.h"
#include "WeatherData.h"
#include "WeatherProvider.h"
#include "WeatherService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <QUrlQuery>

#include <cstring>
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

  void geocodeCity(const QString& city, const QString& language) override {
    last_city = city;
    last_lang = language;
    ++geocode_city_calls;
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
  QString last_city;
  int geocode_city_calls{0};
  int fetch_weather_calls{0};
  int fetch_geo_calls{0};
  int reverse_geocode_calls{0};
  int cancel_pending_calls{0};
};

}  // namespace

// --- Part 1: Icon Mapping Tests (existing) ---

TEST(WeatherService, IconPathMapsKnownDayAndNightConditions) {
  const std::vector<std::pair<int, QString>> cases{
      {200, "11"}, {299, "11"}, {300, "09"}, {399, "09"}, {500, "10"}, {504, "10"},
      {511, "13"}, {520, "09"}, {531, "09"}, {600, "13"}, {699, "13"}, {700, "50"},
      {799, "50"}, {800, "01"}, {801, "02"}, {802, "03"}, {803, "04"}, {804, "04"},
  };
  for (const auto& [condition_id, icon] : cases) {
    EXPECT_EQ(WeatherService::iconPath(condition_id, true), weatherIcon(icon + QStringLiteral("d.svg")));
    EXPECT_EQ(WeatherService::iconPath(condition_id, false), weatherIcon(icon + QStringLiteral("n.svg")));
  }
}

TEST(WeatherService, IconPathFallsBackForUnknownCondition) {
  EXPECT_EQ(WeatherService::iconPath(9999, true), weatherIcon(QStringLiteral("03d.svg")));
  EXPECT_EQ(WeatherService::iconPath(9999, false), weatherIcon(QStringLiteral("03n.svg")));
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
  const QJsonObject automatic{
      {QStringLiteral("city"), QStringLiteral("Kyiv")},
      {QStringLiteral("country_name"), QStringLiteral("Ukraine")},
      {QStringLiteral("state_prov"), QStringLiteral("Kyiv City")},
  };
  EXPECT_EQ(WeatherProvider::parseLocationJson(automatic),
            (WeatherLocation{QStringLiteral("Kyiv"), QStringLiteral("Ukraine")}));

  const QJsonObject reverse{
      {QStringLiteral("name"), QStringLiteral("Lviv")},
      {QStringLiteral("country"), QStringLiteral("UA")},
      {QStringLiteral("state"), QStringLiteral("Lviv Oblast")},
  };
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
  QJsonObject entry{{"dt", 0}, {"pop", 1.5}, {"clouds", -1}};
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
  const QJsonObject entry{{"dt", 0}, {"pop", -0.2}, {"moon_phase", 1.5}};

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

TEST(WeatherProviderParsing, PopulatePollutionDataRejectsInvalidAqi) {
  QJsonObject high_aqi{{"list", QJsonArray{QJsonObject{{"main", QJsonObject{{"aqi", 7}}}}}}};
  CurrentWeather current;

  WeatherProvider::populatePollutionData(current, high_aqi);
  EXPECT_EQ(current.aqi, 0);

  WeatherProvider::populatePollutionData(current,
                                         QJsonObject{{"list", QJsonArray{QJsonObject{{"main", QJsonObject{}}}}}});
  EXPECT_EQ(current.aqi, 0);

  for (const auto& invalid : QJsonArray{0, 6, 2.5, QJsonValue::Null, "2"}) {
    CurrentWeather unavailable;
    WeatherProvider::populatePollutionData(
        unavailable, QJsonObject{{"list", QJsonArray{QJsonObject{{"main", QJsonObject{{"aqi", invalid}}}}}}});
    EXPECT_FALSE(unavailable.available_fields.contains("aqi"));
  }

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
    const QJsonObject identity{{"provider", "openweathermap"},
                               {"source", "manual"},
                               {"city", ""},
                               {"country", ""},
                               {"lang", "en"},
                               {"latitude", 45.0},
                               {"longitude", -75.0}};
    root["version"] = 3;
    root["identity"] = QString::fromLatin1(
        QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256)
            .toHex());
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
  triggerRefreshTimer(service);
  raw_provider->triggerWeatherFetched({}, {replacement}, {});
  ASSERT_EQ(service.hourlyVariant().size(), 1);
  EXPECT_EQ(getHourlyVariantCache(service).at(0).value<HourlyEntry>().temperature, 18.0);
}

TEST_F(WeatherServiceTest, OpenMeteoNeedsNoKeyAndDisplayChangesDoNotRefetch) {
  const QByteArray base = "[weather]\nprovider='open-meteo'\nlocation_source='manual'\nlatitude=0.0\nlongitude=0.0\n";
  writeTempConfig(config_path, base);
  ConfigService config;
  auto provider = std::make_unique<FakeWeatherProvider>();
  auto* fake = provider.get();
  WeatherService service(&config, std::move(provider));
  service.start();
  ASSERT_TRUE(service.configured());
  ASSERT_EQ(fake->fetch_weather_calls, 1);
  EXPECT_EQ(fake->last_units, "metric");
  CurrentWeather weather;
  weather.temperature = 20;
  fake->triggerWeatherFetched(weather, {}, {});
  writeTempConfig(config_path,
                  base +
                      "temp_unit='fahrenheit'\nwind_unit='mph'\npressure_unit='inhg'\nshow_in_bar=false\ncompact_mode="
                      "true\nshow_location=false\nshow_feels_like=false\nrefresh_interval=60\n");
  ASSERT_TRUE(QTest::qWaitFor([&] { return service.temperatureUnit() == "°F"; }));
  EXPECT_EQ(fake->fetch_weather_calls, 1);
  EXPECT_TRUE(service.hasData());
  EXPECT_DOUBLE_EQ(service.currentVariant().value<CurrentWeather>().temperature, 20);
  EXPECT_EQ(service.formatTemperature(20), "68°F");
  EXPECT_NEAR(service.displayWind(16.09344), 10, 1e-6);
  EXPECT_NEAR(service.displayPressure(1013.25), 29.9213, 0.001);
  EXPECT_FALSE(service.showInBar());
  EXPECT_TRUE(service.compactMode());
  EXPECT_FALSE(service.showLocation());
  EXPECT_FALSE(service.showFeelsLike());
  EXPECT_EQ(getRefreshTimerInterval(service), 60000);
}

TEST_F(WeatherServiceTest, ExplicitAutomaticModeIgnoresInactiveCoordinatesAndLabels) {
  writeTempConfig(config_path,
                  "[weather]\nprovider='open-meteo'\nlocation_source='auto'\ngeo_api_key='geo'\nlatitude=0."
                  "0\nlongitude=0.0\ncity='Inactive'\ncountry='Inactive'\n");
  ConfigService config;
  auto provider = std::make_unique<FakeWeatherProvider>();
  auto* fake = provider.get();
  WeatherService service(&config, std::move(provider));
  service.start();
  EXPECT_EQ(fake->fetch_weather_calls, 0);
  EXPECT_EQ(fake->fetch_geo_calls, 1);
  fake->triggerGeoFetched(50, 30, {"Kyiv", "Ukraine"});
  EXPECT_EQ(fake->last_lat, 50);
  EXPECT_EQ(fake->last_lon, 30);
  EXPECT_EQ(service.locationLabel(), "Kyiv, Ukraine");
}

TEST_F(WeatherServiceTest, LocationChangesClearPreviousDataAndWaitForLidResume) {
  const QByteArray base = "[weather]\nprovider='open-meteo'\nlocation_source='manual'\nlongitude=0.0\n";
  writeTempConfig(config_path, base + "latitude=0.0\n");
  ConfigService config;
  auto provider = std::make_unique<FakeWeatherProvider>();
  auto* fake = provider.get();
  WeatherService service(&config, std::move(provider));
  service.start();
  fake->triggerWeatherFetched({}, {}, {});
  ASSERT_TRUE(service.hasData());
  service.pauseActivity();
  writeTempConfig(config_path, base + "latitude=10.0\n");
  ASSERT_TRUE(QTest::qWaitFor([&] { return config.weather().latitude == 10.0; }));
  EXPECT_FALSE(service.hasData());
  EXPECT_EQ(fake->fetch_weather_calls, 1);
  fake->triggerWeatherFetched({}, {}, {});
  EXPECT_FALSE(service.hasData());
  service.resumeActivity();
  EXPECT_EQ(fake->fetch_weather_calls, 2);
  EXPECT_EQ(fake->last_lat, 10);
}

TEST_F(WeatherServiceTest, CacheReusesMatchingDataAndRejectsAnotherProviderOrLegacyFormat) {
  const QByteArray base = "[weather]\nprovider='open-meteo'\nlocation_source='manual'\nlatitude=0.0\nlongitude=0.0\n";
  writeTempConfig(config_path, base);
  ConfigService config;
  {
    auto provider = std::make_unique<FakeWeatherProvider>();
    auto* fake = provider.get();
    WeatherService service(&config, std::move(provider));
    service.start();
    CurrentWeather weather;
    weather.temperature = 17;
    weather.available_fields = {"temperature"};
    HourlyEntry hourly;
    hourly.available_fields = {"temperature", "precipitation"};
    DailyEntry daily;
    daily.available_fields = {"moonPhase"};
    fake->triggerWeatherFetched(weather, {hourly}, {daily});
  }
  {
    WeatherService service(&config, std::make_unique<FakeWeatherProvider>());
    service.start();
    EXPECT_TRUE(service.hasData());
    EXPECT_DOUBLE_EQ(service.currentVariant().value<CurrentWeather>().temperature, 17);
    EXPECT_EQ(service.currentVariant().value<CurrentWeather>().available_fields, QStringList{"temperature"});
    EXPECT_TRUE(service.hourlyVariant()[0].value<HourlyEntry>().available_fields.contains("precipitation"));
    EXPECT_TRUE(service.dailyVariant()[0].value<DailyEntry>().available_fields.contains("moonPhase"));
  }
  writeTempConfig(config_path, "[weather]\nprovider='openweathermap'\napi_key='key'\nlatitude=0.0\nlongitude=0.0\n");
  ASSERT_TRUE(QMetaObject::invokeMethod(&config, "parseFile", Qt::DirectConnection));
  {
    WeatherService service(&config, std::make_unique<FakeWeatherProvider>());
    service.start();
    EXPECT_FALSE(service.hasData());
  }
  writeCache(QDateTime::currentSecsSinceEpoch(), 17);
  QFile cache(tmp_cache.path() + "/holonight/weather.json");
  ASSERT_TRUE(cache.open(QIODevice::ReadOnly));
  auto root = QJsonDocument::fromJson(cache.readAll()).object();
  cache.close();
  root["version"] = 2;
  ASSERT_TRUE(cache.open(QIODevice::WriteOnly));
  cache.write(QJsonDocument(root).toJson());
  cache.close();
  WeatherService service(&config, std::make_unique<FakeWeatherProvider>());
  service.start();
  EXPECT_FALSE(service.hasData());
}

TEST(WeatherProviderParsing, OpenMeteoUsesCanonicalUnitsAndMapsForecasts) {
  const auto doc = QJsonDocument::fromJson(R"({
    "current":{"time":3600,"temperature_2m":20,"apparent_temperature":19,"relative_humidity_2m":50,"weather_code":63,"is_day":1,"pressure_msl":1013,"wind_speed_10m":18,"wind_gusts_10m":30},
    "hourly":{"time":[0,3600,7200],"temperature_2m":[10,20,21],"weather_code":[0,63,95],"precipitation":[0,1.5,2],"precipitation_probability":[0,40,80],"visibility":[10000,9000,8000],"uv_index":[0,3,4],"is_day":[0,1,1]},
    "daily":{"time":[0],"temperature_2m_min":[10],"temperature_2m_max":[22],"weather_code":[63],"sunrise":[1000],"sunset":[9000],"uv_index_max":[5],"precipitation_probability_max":[80]}
  })");
  const auto data = WeatherProvider::parseOpenMeteo(doc.object());
  EXPECT_EQ(data.current.condition_id, 501);
  EXPECT_EQ(data.current.wind_speed, 18);
  EXPECT_DOUBLE_EQ(data.current.temperature, 20);
  EXPECT_DOUBLE_EQ(data.current.visibility, 9);
  EXPECT_EQ(data.current.aqi, 0);
  EXPECT_EQ(data.current.sunrise, 1000);
  ASSERT_EQ(data.hourly.size(), 2);
  EXPECT_DOUBLE_EQ(data.hourly[0].precipitation, 1.5);
  EXPECT_DOUBLE_EQ(data.hourly[0].pop, 0.4);
  EXPECT_EQ(data.hourly[1].condition_id, 200);
  ASSERT_EQ(data.daily.size(), 1);
  EXPECT_DOUBLE_EQ(data.daily[0].temp_min, 10);
  EXPECT_DOUBLE_EQ(data.daily[0].temp_max, 22);
  EXPECT_GE(data.daily[0].moon_phase, 0);
  EXPECT_EQ(WeatherProvider::conditionFromWmo(99), 201);
  EXPECT_EQ(WeatherProvider::conditionFromWmo(999), 0);
}

namespace {
class FixtureReply : public QNetworkReply {
 public:
  explicit FixtureReply(const QNetworkRequest& request, QObject* parent) : QNetworkReply(parent) {
    setRequest(request);
    setUrl(request.url());
    open(QIODevice::ReadOnly);
  }
  void abort() override { aborted = true; }
  void complete(const QByteArray& bytes, bool fail = false) {
    bytes_ = bytes;
    if (fail) setError(QNetworkReply::ConnectionRefusedError, "fixture failure");
    setFinished(true);
    emit readyRead();
    emit finished();
  }
  bool aborted{false};

 protected:
  qint64 readData(char* data, qint64 size) override {
    const auto count = std::min<qint64>(size, bytes_.size() - offset_);
    if (count <= 0) return -1;
    memcpy(data, bytes_.constData() + offset_, count);
    offset_ += count;
    return count;
  }

 private:
  QByteArray bytes_;
  qint64 offset_{0};
};
class FixtureNetwork : public QNetworkAccessManager {
 public:
  QList<FixtureReply*> requests;

 protected:
  QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override {
    auto* reply = new FixtureReply(request, this);
    requests.append(reply);
    return reply;
  }
};
}  // namespace

TEST(WeatherProviderNetwork, DispatchesProvidersWithCanonicalUnitsAndCancelsObsoleteReplies) {
  FixtureNetwork network;
  WeatherProvider provider(&network, nullptr);
  QSignalSpy spy(&provider, &WeatherProvider::weatherFetched);
  provider.setProvider("openweathermap");
  provider.fetchWeather(0, 0, "key", "imperial", "de");
  ASSERT_EQ(network.requests.size(), 2);
  EXPECT_EQ(network.requests[0]->url().host(), "api.openweathermap.org");
  EXPECT_EQ(QUrlQuery(network.requests[0]->url()).queryItemValue("units"), "metric");
  auto* obsolete = network.requests[0];
  provider.setProvider("open-meteo");
  provider.fetchWeather(10, 20, "", "metric", "en");
  ASSERT_EQ(network.requests.size(), 3);
  EXPECT_TRUE(obsolete->aborted);
  EXPECT_EQ(network.requests[2]->url().host(), "api.open-meteo.com");
  EXPECT_FALSE(QUrlQuery(network.requests[2]->url()).hasQueryItem("appid"));
  network.requests[2]->complete(R"({"current":{"temperature_2m":12,"time":3600},"hourly":{},"daily":{}})");
  EXPECT_EQ(spy.count(), 1);
  obsolete->complete(R"({"current":{"temp":99}})");
  EXPECT_EQ(spy.count(), 1);
  provider.fetchWeather(10, 20, "", "metric", "en");
  provider.cancelPending();
  network.requests[3]->complete("{}");
  EXPECT_EQ(spy.count(), 1);
}

TEST(WeatherConversions, ConvertsAllSupportedUnitsFromCanonicalValues) {
  WeatherService service(nullptr, std::make_unique<FakeWeatherProvider>());
  EXPECT_DOUBLE_EQ(service.displayTemperature(0, "°C"), 0);
  EXPECT_DOUBLE_EQ(service.displayTemperature(0, "°F"), 32);
  EXPECT_DOUBLE_EQ(service.displayTemperature(0, "K"), 273.15);
  EXPECT_DOUBLE_EQ(service.displayWind(36, "km/h"), 36);
  EXPECT_DOUBLE_EQ(service.displayWind(36, "m/s"), 10);
  EXPECT_NEAR(service.displayWind(1.852, "knots"), 1, 1e-9);
  EXPECT_NEAR(service.displayWind(1.609344, "mph"), 1, 1e-9);
  EXPECT_DOUBLE_EQ(service.displayPressure(1000, "hPa"), 1000);
  EXPECT_DOUBLE_EQ(service.displayPressure(1000, "bar"), 1);
  EXPECT_NEAR(service.displayPressure(1013.25, "mmHg"), 760, 0.001);
  EXPECT_NEAR(service.displayPressure(1013.25, "inHg"), 29.9213, 0.001);
}
TEST(WeatherProviderNetwork, CityResolutionAcceptsZeroAndReportsEmptyResults) {
  FixtureNetwork network;
  WeatherProvider provider(&network, nullptr);
  QSignalSpy success(&provider, &WeatherProvider::geoFetched);
  QSignalSpy failure(&provider, &WeatherProvider::geoError);
  provider.geocodeCity("Null Island", "en");
  EXPECT_EQ(network.requests[0]->url().host(), "geocoding-api.open-meteo.com");
  network.requests[0]->complete(R"({"results":[{"name":"Null Island","latitude":0,"longitude":0}]})");
  ASSERT_EQ(success.count(), 1);
  EXPECT_DOUBLE_EQ(success.at(0).at(0).toDouble(), 0);
  provider.geocodeCity("Unknown", "en");
  network.requests[1]->complete("{}");
  EXPECT_EQ(failure.count(), 1);
  EXPECT_EQ(success.count(), 1);
}

TEST_F(WeatherServiceTest, ManualCityResolvesWithoutAnIpKeyAndResumesAfterIdle) {
  const QByteArray base = "[weather]\nprovider='open-meteo'\nlocation_source='manual'\n";
  writeTempConfig(config_path, base + "city='Lviv'\ncountry='Ukraine'\n");
  ConfigService config;
  auto provider = std::make_unique<FakeWeatherProvider>();
  auto* fake = provider.get();
  WeatherService service(&config, std::move(provider));
  service.start();
  EXPECT_EQ(fake->last_city, "Lviv, Ukraine");
  EXPECT_EQ(fake->fetch_geo_calls, 0);
  EXPECT_EQ(fake->fetch_weather_calls, 0);
  service.onIdleChanged(true);
  writeTempConfig(config_path, base + "city='Kyiv'\ncountry='Ukraine'\nlang='de'\n");
  ASSERT_TRUE(QTest::qWaitFor([&] { return config.weather().city == "Kyiv"; }));
  EXPECT_EQ(fake->geocode_city_calls, 1);
  fake->triggerGeoFetched(49, 24, {"Lviv", "Ukraine"});
  EXPECT_FALSE(isLocationResolved(service));
  service.onIdleChanged(false);
  EXPECT_EQ(fake->last_city, "Kyiv, Ukraine");
  EXPECT_EQ(fake->last_lang, "de");
  fake->triggerGeoFetched(50, 30, {"Kyiv", "Ukraine"});
  EXPECT_EQ(fake->fetch_weather_calls, 1);
  EXPECT_EQ(fake->last_lat, 50);
}
TEST_F(WeatherServiceTest, ProviderCredentialsAndLanguageChangesReplaceRequests) {
  const QByteArray base =
      "[weather]\nlocation_source='manual'\nlatitude=0.0\nlongitude=0.0\ncity='Pinned'\ncountry='Pinned'\n";
  writeTempConfig(config_path, base + "provider='open-meteo'\n");
  ConfigService config;
  auto provider = std::make_unique<FakeWeatherProvider>();
  auto* fake = provider.get();
  WeatherService service(&config, std::move(provider));
  service.start();
  writeTempConfig(config_path, base + "provider='openweathermap'\napi_key='new'\nlang='de'\n");
  ASSERT_TRUE(QTest::qWaitFor([&] { return fake->fetch_weather_calls == 2; }));
  EXPECT_EQ(fake->cancel_pending_calls, 1);
  EXPECT_EQ(fake->last_api_key, "new");
  EXPECT_EQ(fake->last_lang, "de");
  writeTempConfig(config_path, base + "provider='openweathermap'\napi_key='replacement'\nlang='fr'\n");
  ASSERT_TRUE(QTest::qWaitFor([&] { return fake->fetch_weather_calls == 3; }));
  EXPECT_EQ(fake->cancel_pending_calls, 2);
  EXPECT_EQ(fake->last_api_key, "replacement");
  EXPECT_EQ(fake->last_lang, "fr");
}

TEST(WeatherProviderParsing, AvailabilityDistinguishesZeroFromMissingAndMalformed) {
  const auto current =
      WeatherProvider::parseCurrentJson(QJsonObject{{"temp", 0}, {"humidity", QJsonValue::Null}, {"wind_deg", "bad"}});
  EXPECT_TRUE(current.available_fields.contains("temperature"));
  EXPECT_FALSE(current.available_fields.contains("humidity"));
  EXPECT_FALSE(current.available_fields.contains("windDirection"));
  EXPECT_FALSE(current.available_fields.contains("timeUpdated"));
  const auto hourly = WeatherProvider::parseHourlyJson(
      QJsonArray{QJsonObject{}, QJsonObject{{"dt", 1.5}}, QJsonObject{{"dt", 0}, {"temp", 0}}});
  ASSERT_EQ(hourly.size(), 1);
  EXPECT_TRUE(hourly[0].available_fields.contains("temperature"));
  EXPECT_TRUE(hourly[0].available_fields.contains("precipitation"));
  EXPECT_FALSE(hourly[0].available_fields.contains("pop"));
  const auto daily = WeatherProvider::parseDailyJson(
      QJsonArray{QJsonObject{{"dt", QJsonValue::Null}}, QJsonObject{{"dt", 0}, {"temp", QJsonObject{{"min", 0}}}}});
  ASSERT_EQ(daily.size(), 1);
  EXPECT_TRUE(daily[0].available_fields.contains("tempMin"));
  EXPECT_FALSE(daily[0].available_fields.contains("tempMax"));
}

TEST(WeatherProviderParsing, OpenMeteoAvailabilityHandlesShortArraysAndDerivedValues) {
  const auto data = WeatherProvider::parseOpenMeteo(QJsonDocument::fromJson(R"({
    "current":{"time":3600,"temperature_2m":0},
    "hourly":{"time":[3600,7200,null],"temperature_2m":[0],"visibility":[null],"uv_index":[0]},
    "daily":{"time":[0,null],"temperature_2m_min":[0],"sunrise":[null]}
  })")
                                                        .object());
  EXPECT_TRUE(data.current.available_fields.contains("temperature"));
  EXPECT_TRUE(data.current.available_fields.contains("uvi"));
  EXPECT_FALSE(data.current.available_fields.contains("visibility"));
  EXPECT_FALSE(data.current.available_fields.contains("sunrise"));
  ASSERT_EQ(data.hourly.size(), 2);
  EXPECT_TRUE(data.hourly[0].available_fields.contains("temperature"));
  EXPECT_FALSE(data.hourly[1].available_fields.contains("temperature"));
  ASSERT_EQ(data.daily.size(), 1);
  EXPECT_TRUE(data.daily[0].available_fields.contains("moonPhase"));
}

TEST(WeatherProviderNetwork, PublishesWeatherWhenOptionalPollutionFailsAndRetriesNextRefresh) {
  FixtureNetwork network;
  WeatherProvider provider(&network, nullptr);
  QSignalSpy success(&provider, &WeatherProvider::weatherFetched);
  QSignalSpy failure(&provider, &WeatherProvider::fetchError);
  provider.fetchWeather(0, 0, "key", "metric", "en");
  network.requests[0]->complete(R"({"current":{"temp":0,"dt":3600}})");
  network.requests[1]->complete("", true);
  ASSERT_EQ(success.count(), 1);
  EXPECT_EQ(failure.count(), 0);
  const auto current = success[0][0].value<CurrentWeather>();
  EXPECT_TRUE(current.available_fields.contains("temperature"));
  EXPECT_FALSE(current.available_fields.contains("aqi"));
  provider.fetchWeather(0, 0, "key", "metric", "en");
  EXPECT_EQ(network.requests.size(), 4);
}

TEST(WeatherProviderNetwork, PublishesPartialOpenMeteoForecastWithoutCurrentTemperature) {
  FixtureNetwork network;
  WeatherProvider provider(&network, nullptr);
  provider.setProvider("open-meteo");
  QSignalSpy success(&provider, &WeatherProvider::weatherFetched);
  provider.fetchWeather(0, 0, "", "metric", "en");
  network.requests[0]->complete(
      R"({"current":{"time":3600,"temperature_2m":null},"hourly":{"time":[3600],"temperature_2m":[0]}})");
  ASSERT_EQ(success.count(), 1);
  EXPECT_FALSE(success[0][0].value<CurrentWeather>().available_fields.contains("temperature"));
  const auto hourly = success[0][1].value<QList<HourlyEntry>>();
  ASSERT_EQ(hourly.size(), 1);
  EXPECT_TRUE(hourly[0].available_fields.contains("temperature"));
}
