#include "WeatherService.h"

#include "ConfigService.h"
#include "WeatherProvider.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QSaveFile>

#include <algorithm>
#include <utility>

namespace {

// ---- Cache (de)serialization -------------------------------------------------------------

QJsonObject currentToJson(const CurrentWeather& cur) {
  QJsonObject obj;
  obj.insert(QStringLiteral("temp"), cur.temperature);
  obj.insert(QStringLiteral("feels"), cur.feels_like);
  obj.insert(QStringLiteral("cond"), cur.condition);
  obj.insert(QStringLiteral("cond_id"), cur.condition_id);
  obj.insert(QStringLiteral("humidity"), cur.humidity);
  obj.insert(QStringLiteral("wind"), cur.wind_speed);
  obj.insert(QStringLiteral("wind_deg"), cur.wind_direction);
  obj.insert(QStringLiteral("vis"), cur.visibility);
  obj.insert(QStringLiteral("pressure"), cur.pressure);
  obj.insert(QStringLiteral("icon"), cur.icon_code);
  obj.insert(QStringLiteral("updated"), cur.time_updated);
  obj.insert(QStringLiteral("sunrise"), static_cast<double>(cur.sunrise));
  obj.insert(QStringLiteral("sunset"), static_cast<double>(cur.sunset));
  obj.insert(QStringLiteral("uvi"), cur.uvi);
  obj.insert(QStringLiteral("dew_point"), cur.dew_point);
  obj.insert(QStringLiteral("clouds"), cur.clouds);
  obj.insert(QStringLiteral("wind_gust"), cur.wind_gust);
  obj.insert(QStringLiteral("aqi"), cur.aqi);
  obj.insert(QStringLiteral("pm2_5"), cur.pm2_5);
  obj.insert(QStringLiteral("pm10"), cur.pm10);
  obj.insert(QStringLiteral("co"), cur.co);
  obj.insert(QStringLiteral("no2"), cur.no2);
  obj.insert(QStringLiteral("o3"), cur.o3);
  obj.insert(QStringLiteral("so2"), cur.so2);
  return obj;
}

CurrentWeather currentFromJson(const QJsonObject& obj) {
  CurrentWeather cur;
  cur.temperature = obj.value(QStringLiteral("temp")).toDouble(0.0);
  cur.feels_like = obj.value(QStringLiteral("feels")).toDouble(0.0);
  cur.condition = obj.value(QStringLiteral("cond")).toString();
  cur.condition_id = obj.value(QStringLiteral("cond_id")).toInt(0);
  cur.humidity = obj.value(QStringLiteral("humidity")).toInt(0);
  cur.wind_speed = obj.value(QStringLiteral("wind")).toInt(0);
  cur.wind_direction = obj.value(QStringLiteral("wind_deg")).toInt(0);
  cur.visibility = obj.value(QStringLiteral("vis")).toDouble(0.0);
  cur.pressure = obj.value(QStringLiteral("pressure")).toInt(0);
  cur.icon_code = obj.value(QStringLiteral("icon")).toString();
  cur.time_updated = obj.value(QStringLiteral("updated")).toString();
  cur.sunrise = static_cast<qint64>(obj.value(QStringLiteral("sunrise")).toDouble(0.0));
  cur.sunset = static_cast<qint64>(obj.value(QStringLiteral("sunset")).toDouble(0.0));
  cur.uvi = obj.value(QStringLiteral("uvi")).toDouble(0.0);
  cur.dew_point = obj.value(QStringLiteral("dew_point")).toDouble(0.0);
  cur.clouds = obj.value(QStringLiteral("clouds")).toInt(0);
  cur.wind_gust = obj.value(QStringLiteral("wind_gust")).toInt(0);
  cur.aqi = obj.value(QStringLiteral("aqi")).toInt(0);
  cur.pm2_5 = obj.value(QStringLiteral("pm2_5")).toDouble(0.0);
  cur.pm10 = obj.value(QStringLiteral("pm10")).toDouble(0.0);
  cur.co = obj.value(QStringLiteral("co")).toDouble(0.0);
  cur.no2 = obj.value(QStringLiteral("no2")).toDouble(0.0);
  cur.o3 = obj.value(QStringLiteral("o3")).toDouble(0.0);
  cur.so2 = obj.value(QStringLiteral("so2")).toDouble(0.0);
  return cur;
}

QJsonArray hourlyToJson(const QList<HourlyEntry>& hourly) {
  QJsonArray arr;
  for (const HourlyEntry& entry : hourly) {
    QJsonObject obj;
    obj.insert(QStringLiteral("dt"), static_cast<double>(entry.timestamp));
    obj.insert(QStringLiteral("temp"), entry.temperature);
    obj.insert(QStringLiteral("cond_id"), entry.condition_id);
    obj.insert(QStringLiteral("precip"), entry.precipitation);
    obj.insert(QStringLiteral("icon"), entry.icon_code);
    obj.insert(QStringLiteral("cond"), entry.condition);
    obj.insert(QStringLiteral("pop"), entry.pop);
    obj.insert(QStringLiteral("uvi"), entry.uvi);
    obj.insert(QStringLiteral("clouds"), entry.clouds);
    arr.append(obj);
  }
  return arr;
}

QList<HourlyEntry> hourlyFromJson(const QJsonArray& arr) {
  QList<HourlyEntry> out;
  out.reserve(arr.size());
  for (const auto& value : arr) {
    const QJsonObject obj = value.toObject();
    HourlyEntry entry;
    entry.timestamp = static_cast<qint64>(obj.value(QStringLiteral("dt")).toDouble(0.0));
    entry.temperature = obj.value(QStringLiteral("temp")).toDouble(0.0);
    entry.condition_id = obj.value(QStringLiteral("cond_id")).toInt(0);
    entry.precipitation = obj.value(QStringLiteral("precip")).toDouble(0.0);
    entry.icon_code = obj.value(QStringLiteral("icon")).toString();
    entry.condition = obj.value(QStringLiteral("cond")).toString();
    entry.pop = obj.value(QStringLiteral("pop")).toDouble(0.0);
    entry.uvi = obj.value(QStringLiteral("uvi")).toDouble(0.0);
    entry.clouds = obj.value(QStringLiteral("clouds")).toInt(0);
    out.append(entry);
  }
  return out;
}

QJsonArray dailyToJson(const QList<DailyEntry>& daily) {
  QJsonArray arr;
  for (const DailyEntry& entry : daily) {
    QJsonObject obj;
    obj.insert(QStringLiteral("date"), static_cast<double>(entry.date));
    obj.insert(QStringLiteral("t_min"), entry.temp_min);
    obj.insert(QStringLiteral("t_max"), entry.temp_max);
    obj.insert(QStringLiteral("cond_id"), entry.condition_id);
    obj.insert(QStringLiteral("cond"), entry.condition);
    obj.insert(QStringLiteral("icon"), entry.icon_code);
    obj.insert(QStringLiteral("pop"), entry.pop);
    obj.insert(QStringLiteral("uvi"), entry.uvi);
    obj.insert(QStringLiteral("moon_phase"), entry.moon_phase);
    obj.insert(QStringLiteral("summary"), entry.summary);
    obj.insert(QStringLiteral("sunrise"), static_cast<double>(entry.sunrise));
    obj.insert(QStringLiteral("sunset"), static_cast<double>(entry.sunset));
    arr.append(obj);
  }
  return arr;
}

QList<DailyEntry> dailyFromJson(const QJsonArray& arr) {
  QList<DailyEntry> out;
  out.reserve(arr.size());
  for (const auto& value : arr) {
    const QJsonObject obj = value.toObject();
    DailyEntry entry;
    entry.date = static_cast<qint64>(obj.value(QStringLiteral("date")).toDouble(0.0));
    entry.temp_min = obj.value(QStringLiteral("t_min")).toDouble(0.0);
    entry.temp_max = obj.value(QStringLiteral("t_max")).toDouble(0.0);
    entry.condition_id = obj.value(QStringLiteral("cond_id")).toInt(0);
    entry.condition = obj.value(QStringLiteral("cond")).toString();
    entry.icon_code = obj.value(QStringLiteral("icon")).toString();
    entry.pop = obj.value(QStringLiteral("pop")).toDouble(0.0);
    entry.uvi = obj.value(QStringLiteral("uvi")).toDouble(0.0);
    entry.moon_phase = obj.value(QStringLiteral("moon_phase")).toDouble(0.0);
    entry.summary = obj.value(QStringLiteral("summary")).toString();
    entry.sunrise = static_cast<qint64>(obj.value(QStringLiteral("sunrise")).toDouble(0.0));
    entry.sunset = static_cast<qint64>(obj.value(QStringLiteral("sunset")).toDouble(0.0));
    out.append(entry);
  }
  return out;
}

// ---- Icon mapping ------------------------------------------------------------------------

using IconPair = QPair<QString, QString>;  // {day filename, night filename}

const QHash<int, IconPair>& iconTable() {
  static const QHash<int, IconPair> kTable = [] {
    QHash<int, IconPair> map;
    const auto add = [&map](std::initializer_list<int> ids, const char* day, const char* night) {
      for (int code : ids) {
        map.insert(code, {QString::fromLatin1(day), QString::fromLatin1(night)});
      }
    };
    // Thunderstorm (2xx)
    add({200, 201, 202, 230, 231, 232}, "wsymbol_0016_thundery_showers.svg", "wsymbol_0032_thundery_showers_night.svg");
    add({210, 211, 212, 221}, "wsymbol_0024_thunderstorms.svg", "wsymbol_0040_thunderstorms_night.svg");
    // Drizzle (3xx)
    add({300, 301, 302, 310, 311}, "wsymbol_0048_drizzle.svg", "wsymbol_0066_drizzle_night.svg");
    add({312}, "wsymbol_0081_heavy_drizzle.svg", "wsymbol_0082_heavy_drizzle_night.svg");
    add({313, 314, 321}, "wsymbol_0009_light_rain_showers.svg", "wsymbol_0025_light_rain_showers_night.svg");
    // Rain (5xx)
    add({500, 501}, "wsymbol_0017_cloudy_with_light_rain.svg", "wsymbol_0033_cloudy_with_light_rain_night.svg");
    add({502, 503}, "wsymbol_0018_cloudy_with_heavy_rain.svg", "wsymbol_0034_cloudy_with_heavy_rain_night.svg");
    add({504}, "wsymbol_0051_extreme_rain.svg", "wsymbol_0069_extreme_rain_night.svg");
    add({511}, "wsymbol_0050_freezing_rain.svg", "wsymbol_0068_freezing_rain_night.svg");
    add({520, 521}, "wsymbol_0009_light_rain_showers.svg", "wsymbol_0025_light_rain_showers_night.svg");
    add({522}, "wsymbol_0010_heavy_rain_showers.svg", "wsymbol_0026_heavy_rain_showers_night.svg");
    add({531}, "wsymbol_0085_extreme_rain_showers.svg", "wsymbol_0086_extreme_rain_showers_night.svg");
    // Snow (6xx)
    add({600, 601}, "wsymbol_0019_cloudy_with_light_snow.svg", "wsymbol_0035_cloudy_with_light_snow_night.svg");
    add({602}, "wsymbol_0020_cloudy_with_heavy_snow.svg", "wsymbol_0036_cloudy_with_heavy_snow_night.svg");
    add({611, 612, 615, 616}, "wsymbol_0021_cloudy_with_sleet.svg", "wsymbol_0037_cloudy_with_sleet_night.svg");
    add({613}, "wsymbol_0087_heavy_sleet_showers.svg", "wsymbol_0088_heavy_sleet_showers_night.svg");
    add({620}, "wsymbol_0011_light_snow_showers.svg", "wsymbol_0027_light_snow_showers_night.svg");
    add({621}, "wsymbol_0012_heavy_snow_showers.svg", "wsymbol_0028_heavy_snow_showers_night.svg");
    add({622}, "wsymbol_0052_extreme_snow.svg", "wsymbol_0070_extreme_snow_night.svg");
    // Atmosphere (7xx)
    add({701}, "wsymbol_0006_mist.svg", "wsymbol_0063_mist_night.svg");
    add({711}, "wsymbol_0055_smoke.svg", "wsymbol_0073_smoke_night.svg");
    add({721}, "wsymbol_0005_hazy_sun.svg", "wsymbol_0063_mist_night.svg");
    add({731, 751, 761}, "wsymbol_0056_dust_sand.svg", "wsymbol_0074_dust_sand_night.svg");
    add({741}, "wsymbol_0007_fog.svg", "wsymbol_0064_fog_night.svg");
    add({762}, "wsymbol_0091_volcanic_ash.svg", "wsymbol_0091_volcanic_ash.svg");
    add({771}, "wsymbol_0060_windy.svg", "wsymbol_0078_windy_night.svg");
    add({781}, "wsymbol_0079_tornado.svg", "wsymbol_0079_tornado.svg");
    // Clear / clouds (8xx)
    add({800}, "wsymbol_0001_sunny.svg", "wsymbol_0008_clear_sky_night.svg");
    add({801}, "wsymbol_0002_sunny_intervals.svg", "wsymbol_0041_partly_cloudy_night.svg");
    add({802}, "wsymbol_0003_white_cloud.svg", "wsymbol_0042_cloudy_night.svg");
    add({803}, "wsymbol_0043_mostly_cloudy.svg", "wsymbol_0044_mostly_cloudy_night.svg");
    add({804}, "wsymbol_0004_black_low_cloud.svg", "wsymbol_0004_black_low_cloud.svg");
    return map;
  }();
  return kTable;
}

}  // namespace

WeatherService::WeatherService(ConfigService* config, QObject* parent)
    : WeatherService(config, std::make_unique<WeatherProvider>(), parent) {}

WeatherService::WeatherService(ConfigService* config, std::unique_ptr<WeatherProvider> provider, QObject* parent)
    : QObject(parent), config_(config), provider_(std::move(provider)) {
  refresh_timer_.setSingleShot(true);
  stale_timer_.setSingleShot(true);
  connect(&refresh_timer_, &QTimer::timeout, this, &WeatherService::onRefreshTimer);
  connect(&stale_timer_, &QTimer::timeout, this, &WeatherService::onStaleTimer);
  connectProvider();
  if (config_ != nullptr) {
    connect(config_, &ConfigService::weatherChanged, this, &WeatherService::onConfigChanged);
  }
}

WeatherService::~WeatherService() = default;

void WeatherService::connectProvider() {
  connect(provider_.get(), &WeatherProvider::weatherFetched, this, &WeatherService::onWeatherFetched);
  connect(provider_.get(), &WeatherProvider::geoFetched, this, &WeatherService::onGeoFetched);
  connect(provider_.get(), &WeatherProvider::locationFetched, this, &WeatherService::onLocationFetched);
  connect(provider_.get(), &WeatherProvider::fetchError, this, &WeatherService::onFetchError);
  connect(provider_.get(), &WeatherProvider::geoError, this, &WeatherService::onGeoError);
  connect(provider_.get(), &WeatherProvider::locationError, this, &WeatherService::onLocationError);
}

void WeatherService::start() {
  if (started_) {
    return;
  }
  started_ = true;
  applyConfig();
  loadCache();
  evaluateStaleOnLoad();
  if (!configured_) {
    qCInfo(lcWeather) << "weather not configured (missing api_key or location source); widget hidden";
    return;
  }
  resolveLocation();
}

void WeatherService::applyConfig() {
  if (config_ == nullptr) {
    setConfigured(false);
    return;
  }
  const HoloNight::ShellConfig::WeatherConfig& cfg = config_->weather();
  api_key_ = cfg.api_key;
  geo_api_key_ = cfg.geo_api_key;
  configured_city_ = cfg.city;
  configured_country_ = cfg.country;
  units_ = cfg.units;
  lang_ = cfg.lang;
  refresh_interval_ = cfg.refresh_interval;
  setConfigured(computeConfigured());
}

bool WeatherService::computeConfigured() const {
  if (config_ == nullptr || api_key_.isEmpty()) {
    return false;
  }
  const HoloNight::ShellConfig::WeatherConfig& cfg = config_->weather();
  const bool has_coords = cfg.latitude.has_value() && cfg.longitude.has_value();
  return has_coords || !geo_api_key_.isEmpty();
}

void WeatherService::onConfigChanged() {
  applyConfig();
  if (!configured_) {
    refresh_timer_.stop();
    stale_timer_.stop();
    return;
  }
  resetBackoff();
  setStale(false);
  location_resolved_ = false;  // coordinates may have changed; re-resolve
  resolveLocation();
}

void WeatherService::resolveLocation() {
  if (config_ == nullptr) {
    return;
  }
  const HoloNight::ShellConfig::WeatherConfig& cfg = config_->weather();
  if (cfg.latitude.has_value() && cfg.longitude.has_value()) {
    lat_ = *cfg.latitude;
    lon_ = *cfg.longitude;
    location_resolved_ = true;
    setResolvedLocation(mergeConfiguredLocation(location_));
    if (configured_city_.trimmed().isEmpty() || configured_country_.trimmed().isEmpty()) {
      provider_->reverseGeocode(lat_, lon_, api_key_);
    }
    fetch();
    return;
  }
  if (!geo_api_key_.isEmpty()) {
    geo_pending_ = true;
    provider_->fetchGeolocation(geo_api_key_);
  }
}

void WeatherService::onGeoFetched(double lat, double lon, const WeatherLocation& location) {
  geo_pending_ = false;
  lat_ = lat;
  lon_ = lon;
  location_resolved_ = true;
  setResolvedLocation(mergeConfiguredLocation(location));
  qCInfo(lcWeather) << "location resolved via geolocation:" << location_label_ << lat << lon;
  fetch();
}

void WeatherService::onGeoError(const QString& message) {
  geo_pending_ = false;
  qCWarning(lcWeather) << "geolocation failed:" << message;
  setStale(true);
  stepBackoff();
  refresh_timer_.start(nextBackoffMs());
}

void WeatherService::onLocationFetched(const WeatherLocation& location) {
  setResolvedLocation(mergeConfiguredLocation(location));
  if (has_data_) {
    saveCache();
  }
}

void WeatherService::onLocationError(const QString& message) {
  qCWarning(lcWeather) << "location name unavailable; weather fetch continues:" << message;
}

void WeatherService::onRefreshTimer() {
  if (!configured_) {
    return;
  }
  if (!location_resolved_) {
    resolveLocation();
    return;
  }
  fetch();
}

void WeatherService::fetch() {
  if (!configured_ || !location_resolved_) {
    return;
  }
  fetch_failed_ = false;
  fetch_pending_ = true;
  provider_->fetchWeather(lat_, lon_, api_key_, units_, lang_);
}

void WeatherService::onWeatherFetched(const CurrentWeather& weather, const QList<HourlyEntry>& hourly,
                                      const QList<DailyEntry>& daily) {
  current_ = weather;
  hourly_ = hourly;
  daily_ = daily;
  rebuildForecastVariants();
  fetch_pending_ = false;
  emit currentChanged();
  emit forecastChanged();
  maybeComplete();
}

void WeatherService::onFetchError(WeatherProvider::FetchErrorKind kind, const QString& message) {
  if (fetch_failed_) {
    return;  // the other half of this cycle already reported failure
  }
  fetch_failed_ = true;
  qCWarning(lcWeather) << "fetch failed:" << message;
  setStale(true);
  if (kind == WeatherProvider::FetchErrorKind::Authentication) {
    refresh_timer_.stop();
    emit authenticationFailed(message);
    return;
  }
  stepBackoff();
  refresh_timer_.start(nextBackoffMs());
}

void WeatherService::maybeComplete() {
  if (fetch_failed_ || fetch_pending_) {
    return;
  }
  fetched_at_ = QDateTime::currentSecsSinceEpoch();
  setHasData(true);
  setStale(false);
  saveCache();
  startStaleTimer();
  resetBackoff();
  refresh_timer_.start(refresh_interval_ * 1000);
  qCInfo(lcWeather) << "weather updated;" << hourly_.size() << "hourly," << daily_.size() << "daily";
}

void WeatherService::resetBackoff() { backoff_step_ = 0; }

void WeatherService::stepBackoff() { ++backoff_step_; }

int WeatherService::nextBackoffMs() const {
  if (backoff_step_ <= 0) {
    return refresh_interval_ * 1000;
  }
  const int shift = backoff_step_ - 1;
  qint64 secs = refresh_interval_;
  if (shift < 30) {
    secs = std::min<qint64>(static_cast<qint64>(kBackoffBaseSecs) << shift, refresh_interval_);
  }
  return static_cast<int>(secs * 1000);
}

void WeatherService::evaluateStaleOnLoad() {
  const bool fresh = fetched_at_ > 0 && (QDateTime::currentSecsSinceEpoch() - fetched_at_) <= kStaleThresholdSecs;
  setStale(!fresh);
}

void WeatherService::startStaleTimer() { stale_timer_.start(kStaleThresholdSecs * 1000); }

void WeatherService::onStaleTimer() { setStale(true); }

void WeatherService::onIdleChanged(bool idle) {
  idle_paused_ = idle;
  if (idle) {
    refresh_timer_.stop();
    provider_->cancelPending();
    geo_pending_ = false;
    fetch_pending_ = false;
  } else if (!lid_paused_) {
    fetch();
    refresh_timer_.start(refresh_interval_ * 1000);
  }
}

void WeatherService::pauseActivity() {
  if (lid_paused_) {
    return;
  }
  lid_paused_ = true;
  refresh_timer_.stop();
  provider_->cancelPending();
  geo_pending_ = false;
  fetch_pending_ = false;
}

void WeatherService::resumeActivity() {
  if (!lid_paused_) {
    return;
  }
  lid_paused_ = false;
  if (!idle_paused_) {
    fetch();
    refresh_timer_.start(refresh_interval_ * 1000);
  }
}

void WeatherService::setConfigured(bool value) {
  if (configured_ == value) {
    return;
  }
  configured_ = value;
  emit configuredChanged();
}

void WeatherService::setHasData(bool value) {
  if (has_data_ == value) {
    return;
  }
  has_data_ = value;
  emit hasDataChanged();
}

void WeatherService::setStale(bool value) {
  if (stale_ == value) {
    return;
  }
  stale_ = value;
  emit staleChanged();
}

QString WeatherService::formatLocationLabel(const WeatherLocation& location) {
  QString city = location.city.trimmed();
  QString country = location.country.trimmed();
  if (city.isEmpty()) {
    return country;
  }
  if (country.isEmpty()) {
    return city;
  }
  return city + QStringLiteral(", ") + country;
}

WeatherLocation WeatherService::mergeConfiguredLocation(const WeatherLocation& location) const {
  return {.city = configured_city_.trimmed().isEmpty() ? location.city : configured_city_,
          .country = configured_country_.trimmed().isEmpty() ? location.country : configured_country_};
}

void WeatherService::setResolvedLocation(const WeatherLocation& location) {
  const WeatherLocation normalized{.city = location.city.trimmed(), .country = location.country.trimmed()};
  const QString label = formatLocationLabel(normalized);
  if (location_ == normalized && location_label_ == label) {
    return;
  }
  location_ = normalized;
  location_label_ = label;
  emit locationChanged();
}

QVariant WeatherService::currentVariant() const { return QVariant::fromValue(current_); }

QVariantList WeatherService::hourlyVariant() const { return hourly_variant_; }

QVariantList WeatherService::dailyVariant() const { return daily_variant_; }

void WeatherService::rebuildForecastVariants() {
  hourly_variant_.clear();
  hourly_variant_.reserve(hourly_.size());
  for (const HourlyEntry& entry : hourly_) {
    hourly_variant_.append(QVariant::fromValue(entry));
  }

  daily_variant_.clear();
  daily_variant_.reserve(daily_.size());
  for (const DailyEntry& entry : daily_) {
    daily_variant_.append(QVariant::fromValue(entry));
  }
}

QString WeatherService::iconPath(int condition_id, bool is_day) {
  const QString prefix = QStringLiteral("qrc:/HolonightShell/weather/");
  const auto& table = iconTable();
  const auto found = table.constFind(condition_id);
  if (found == table.constEnd()) {
    return prefix + QStringLiteral("wsymbol_0999_unknown.svg");
  }
  return prefix + (is_day ? found->first : found->second);
}

QString WeatherService::cachePath() {
  QString cache = qEnvironmentVariable("XDG_CACHE_HOME");
  if (cache.isEmpty()) {
    cache = QDir::homePath() + QLatin1String("/.cache");
  }
  return cache + QLatin1String("/holonight/weather.json");
}

void WeatherService::loadCache() {
  QFile file(cachePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return;  // no cache yet — not an error
  }
  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
    qCWarning(lcWeather) << "ignoring malformed weather cache:" << parse_error.errorString();
    return;
  }
  const QJsonObject root = doc.object();
  current_ = currentFromJson(root.value(QStringLiteral("current")).toObject());
  hourly_ = hourlyFromJson(root.value(QStringLiteral("hourly")).toArray());
  daily_ = dailyFromJson(root.value(QStringLiteral("daily")).toArray());
  const QJsonObject location = root.value(QStringLiteral("location")).toObject();
  setResolvedLocation({.city = location.value(QStringLiteral("city")).toString(),
                       .country = location.value(QStringLiteral("country")).toString()});
  rebuildForecastVariants();
  fetched_at_ = static_cast<qint64>(root.value(QStringLiteral("fetched_at")).toDouble(0.0));
  setHasData(true);
  emit currentChanged();
  emit forecastChanged();
}

void WeatherService::saveCache() const {
  const QString path = cachePath();
  const QDir dir = QFileInfo(path).dir();
  if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
    qCWarning(lcWeather) << "failed to create cache directory:" << dir.absolutePath();
    return;
  }
  QJsonObject root;
  root.insert(QStringLiteral("fetched_at"), static_cast<double>(fetched_at_));
  root.insert(QStringLiteral("current"), currentToJson(current_));
  root.insert(QStringLiteral("hourly"), hourlyToJson(hourly_));
  root.insert(QStringLiteral("daily"), dailyToJson(daily_));
  QJsonObject location;
  location.insert(QStringLiteral("city"), location_.city);
  location.insert(QStringLiteral("country"), location_.country);
  root.insert(QStringLiteral("location"), location);

  QSaveFile out(path);
  if (!out.open(QIODevice::WriteOnly)) {
    qCWarning(lcWeather) << "failed to write weather cache:" << path;
    return;
  }
  out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  if (!out.commit()) {
    qCWarning(lcWeather) << "failed to commit weather cache:" << path;
  }
}
