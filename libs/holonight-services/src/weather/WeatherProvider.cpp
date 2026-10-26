#include "WeatherProvider.h"

#include "MoonPhaseCalculator.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QStringList>
#include <QTimeZone>
#include <QUrl>
#include <QUrlQuery>

#include <cmath>
#include <optional>

Q_LOGGING_CATEGORY(lcWeather, "holonight.weather")

namespace {

constexpr const char* kOwmBase = "https://api.openweathermap.org/data/3.0/";
constexpr const char* kGeoUrl = "https://api.ipgeolocation.io/v3/ipgeo";
constexpr const char* kReverseGeoUrl = "https://api.openweathermap.org/geo/1.0/reverse";
constexpr double kMsToKmh = 3.6;

QUrl owmUrl(const QString& endpoint, double lat, double lon, const QString& api_key, const QString& units,
            const QString& lang) {
  QUrl url(QString::fromLatin1(kOwmBase) + endpoint);
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("lat"), QString::number(lat, 'f', 6));
  query.addQueryItem(QStringLiteral("lon"), QString::number(lon, 'f', 6));
  query.addQueryItem(QStringLiteral("appid"), api_key);
  query.addQueryItem(QStringLiteral("units"), units);
  query.addQueryItem(QStringLiteral("lang"), lang);
  url.setQuery(query);
  return url;
}

// Validates a finished reply and returns its JSON object body. On failure, fills error_out with a
// human-readable message and returns std::nullopt. Does not emit signals — the caller picks the
// appropriate error signal (weather vs geolocation).
std::optional<QJsonObject> parseReplyBody(QNetworkReply* reply, QString& error_out,
                                          WeatherProvider::FetchErrorKind* kind_out = nullptr,
                                          const QString& endpoint = {}) {
  const QByteArray body = reply->readAll();
  const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
  if (kind_out != nullptr) {
    const int status_code = status.toInt();
    *kind_out = status_code == 401 || status_code == 403 ? WeatherProvider::FetchErrorKind::Authentication
                                                         : WeatherProvider::FetchErrorKind::Transient;
  }

  if (reply->error() != QNetworkReply::NoError) {
    error_out =
        endpoint.isEmpty()
            ? QStringLiteral("network error: %1 (HTTP %2)")
                  .arg(reply->errorString(), status.isValid() ? status.toString() : QStringLiteral("?"))
            : QStringLiteral("network error on %1: %2 (HTTP %3)")
                  .arg(endpoint, reply->errorString(), status.isValid() ? status.toString() : QStringLiteral("?"));
    return std::nullopt;
  }
  if (status.isValid() && status.toInt() != 200) {
    error_out = endpoint.isEmpty() ? QStringLiteral("HTTP %1").arg(status.toInt())
                                   : QStringLiteral("HTTP %1 on %2").arg(status.toInt()).arg(endpoint);
    return std::nullopt;
  }

  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(body, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
    error_out = endpoint.isEmpty()
                    ? QStringLiteral("malformed JSON: %1").arg(parse_error.errorString())
                    : QStringLiteral("malformed JSON from %1: %2").arg(endpoint, parse_error.errorString());
    return std::nullopt;
  }
  return doc.object();
}

int clampPercentage(int value) { return std::clamp(value, 0, 100); }

double clampUnitInterval(double value) { return std::clamp(value, 0.0, 1.0); }

double precipFromEntry(const QJsonObject& entry) {
  double precip = 0.0;
  if (entry.value(QStringLiteral("rain")).isObject()) {
    const QJsonObject rain = entry.value(QStringLiteral("rain")).toObject();
    precip += rain.value(QStringLiteral("1h")).toDouble(rain.value(QStringLiteral("3h")).toDouble(0.0));
  }
  if (entry.value(QStringLiteral("snow")).isObject()) {
    const QJsonObject snow = entry.value(QStringLiteral("snow")).toObject();
    precip += snow.value(QStringLiteral("1h")).toDouble(snow.value(QStringLiteral("3h")).toDouble(0.0));
  }
  return std::max(precip, 0.0);
}

QUrl airPollutionUrl(double lat, double lon, const QString& api_key) {
  QUrl url(QStringLiteral("https://api.openweathermap.org/data/2.5/air_pollution"));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("lat"), QString::number(lat, 'f', 6));
  query.addQueryItem(QStringLiteral("lon"), QString::number(lon, 'f', 6));
  query.addQueryItem(QStringLiteral("appid"), api_key);
  url.setQuery(query);
  return url;
}
QString conditionDescription(int condition_id) {
  if (condition_id == 800) {
    return QStringLiteral("Clear sky");
  }
  if (condition_id == 801) {
    return QStringLiteral("Few clouds");
  }
  if (condition_id == 802) {
    return QStringLiteral("Scattered clouds");
  }
  if (condition_id == 804) {
    return QStringLiteral("Overcast clouds");
  }
  if (condition_id == 741) {
    return QStringLiteral("Fog");
  }
  if (condition_id == 511) {
    return QStringLiteral("Freezing rain");
  }
  if (condition_id >= 600 && condition_id < 700) {
    return QStringLiteral("Snow");
  }
  if (condition_id >= 500 && condition_id < 600) {
    return QStringLiteral("Rain");
  }
  if (condition_id >= 300 && condition_id < 400) {
    return QStringLiteral("Drizzle");
  }
  if (condition_id >= 200 && condition_id < 300) {
    return QStringLiteral("Thunderstorm");
  }
  return QStringLiteral("Unknown");
}

}  // namespace

void WeatherProvider::populatePollutionData(CurrentWeather& current, const QJsonObject& pollution_obj) {
  const QJsonArray list = pollution_obj.value(QStringLiteral("list")).toArray();
  if (list.isEmpty()) {
    return;
  }
  const QJsonObject entry = list.first().toObject();
  const QJsonObject main = entry.value(QStringLiteral("main")).toObject();
  const QJsonValue aqi = main.value(QStringLiteral("aqi"));
  if (aqi.isDouble()) {
    current.aqi = std::clamp(aqi.toInt(), 1, 5);
  }

  const QJsonObject components = entry.value(QStringLiteral("components")).toObject();
  current.pm2_5 = components.value(QStringLiteral("pm2_5")).toDouble(0.0);
  current.pm10 = components.value(QStringLiteral("pm10")).toDouble(0.0);
  current.co = components.value(QStringLiteral("co")).toDouble(0.0);
  current.no2 = components.value(QStringLiteral("no2")).toDouble(0.0);
  current.o3 = components.value(QStringLiteral("o3")).toDouble(0.0);
  current.so2 = components.value(QStringLiteral("so2")).toDouble(0.0);
}

WeatherProvider::WeatherProvider(QObject* parent) : WeatherProvider(nullptr, parent) {}
WeatherProvider::WeatherProvider(QNetworkAccessManager* network, QObject* parent)
    : QObject(parent), nam_(this), network_((network != nullptr) ? network : &nam_) {
  network_->setTransferTimeout(20000);
}

void WeatherProvider::fetchWeather(double lat, double lon, const QString& api_key, const QString& units,
                                   const QString& lang) {
  cleanupWeatherReplies();

  if (provider_name_ == QLatin1String("open-meteo")) {
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery query;
    query.addQueryItem("latitude", QString::number(lat, 'f', 6));
    query.addQueryItem("longitude", QString::number(lon, 'f', 6));
    query.addQueryItem("current",
                       "temperature_2m,apparent_temperature,relative_humidity_2m,is_day,weather_code,cloud_cover,"
                       "pressure_msl,wind_speed_10m,wind_direction_10m,wind_gusts_10m");
    query.addQueryItem("hourly",
                       "temperature_2m,weather_code,precipitation,precipitation_probability,uv_index,cloud_cover,"
                       "visibility,dew_point_2m,is_day");
    query.addQueryItem(
        "daily",
        "temperature_2m_min,temperature_2m_max,weather_code,precipitation_probability_max,uv_index_max,sunrise,sunset");
    query.addQueryItem("temperature_unit", "celsius");
    query.addQueryItem("wind_speed_unit", "kmh");
    query.addQueryItem("precipitation_unit", "mm");
    query.addQueryItem("timeformat", "unixtime");
    query.addQueryItem("timezone", "auto");
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setTransferTimeout(20000);
    weather_reply_ = network_->get(request);
    connect(weather_reply_, &QNetworkReply::finished, this, [this] {
      QString error;
      const auto root = parseReplyBody(weather_reply_, error);
      if (!root || !root->value("current").toObject().value("temperature_2m").isDouble() ||
          !root->value("current").toObject().value("time").isDouble() || !root->value("hourly").isObject() ||
          !root->value("daily").isObject()) {
        cleanupWeatherReplies();
        emit fetchError(FetchErrorKind::Transient,
                        error.isEmpty() ? QStringLiteral("Incomplete forecast response") : error);
        return;
      }
      const auto data = parseOpenMeteo(*root);
      cleanupWeatherReplies();
      emit weatherFetched(data.current, data.hourly, data.daily);
    });
    return;
  }

  const QUrl weather_url = owmUrl(QStringLiteral("onecall"), lat, lon, api_key, QStringLiteral("metric"), lang);
  Q_UNUSED(units)
  weather_reply_ = network_->get(QNetworkRequest(weather_url));

  const QUrl pollution_url = airPollutionUrl(lat, lon, api_key);
  pollution_reply_ = network_->get(QNetworkRequest(pollution_url));

  connect(weather_reply_, &QNetworkReply::finished, this, &WeatherProvider::checkComplete);
  connect(pollution_reply_, &QNetworkReply::finished, this, &WeatherProvider::checkComplete);
}

void WeatherProvider::fetchGeolocation(const QString& geo_api_key) {
  if (geo_reply_ != nullptr) {
    disconnect(geo_reply_, nullptr, this, nullptr);
    geo_reply_->abort();
    geo_reply_->deleteLater();
    geo_reply_ = nullptr;
  }
  QUrl url(QString::fromLatin1(kGeoUrl));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("apiKey"), geo_api_key);
  url.setQuery(query);
  geo_reply_ = network_->get(QNetworkRequest(url));
  connect(geo_reply_, &QNetworkReply::finished, this, [this] { onGeoReply(geo_reply_); });
}

void WeatherProvider::reverseGeocode(double lat, double lon, const QString& api_key) {
  if (location_reply_ != nullptr) {
    disconnect(location_reply_, nullptr, this, nullptr);
    location_reply_->abort();
    location_reply_->deleteLater();
  }
  QUrl url(QString::fromLatin1(kReverseGeoUrl));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("lat"), QString::number(lat, 'f', 6));
  query.addQueryItem(QStringLiteral("lon"), QString::number(lon, 'f', 6));
  query.addQueryItem(QStringLiteral("limit"), QStringLiteral("1"));
  query.addQueryItem(QStringLiteral("appid"), api_key);
  url.setQuery(query);
  location_reply_ = network_->get(QNetworkRequest(url));
  connect(location_reply_, &QNetworkReply::finished, this, [this] { onLocationReply(location_reply_); });
}

void WeatherProvider::cancelPending() { cleanupReplies(); }

void WeatherProvider::cleanupWeatherReplies() {
  if (weather_reply_ != nullptr) {
    disconnect(weather_reply_, nullptr, this, nullptr);
    weather_reply_->abort();
    weather_reply_->deleteLater();
    weather_reply_ = nullptr;
  }
  if (pollution_reply_ != nullptr) {
    disconnect(pollution_reply_, nullptr, this, nullptr);
    pollution_reply_->abort();
    pollution_reply_->deleteLater();
    pollution_reply_ = nullptr;
  }
}

void WeatherProvider::cleanupReplies() {
  cleanupWeatherReplies();
  if (geo_reply_ != nullptr) {
    disconnect(geo_reply_, nullptr, this, nullptr);
    geo_reply_->abort();
    geo_reply_->deleteLater();
    geo_reply_ = nullptr;
  }
  if (location_reply_ != nullptr) {
    disconnect(location_reply_, nullptr, this, nullptr);
    location_reply_->abort();
    location_reply_->deleteLater();
    location_reply_ = nullptr;
  }
}

void WeatherProvider::checkComplete() {
  if (weather_reply_ == nullptr || pollution_reply_ == nullptr) {
    return;
  }

  if (!weather_reply_->isFinished() || !pollution_reply_->isFinished()) {
    return;
  }

  QString error_msg;
  QJsonObject weather_obj;
  QJsonObject pollution_obj;

  FetchErrorKind error_kind = FetchErrorKind::Transient;
  const auto check_reply = [&error_msg, &error_kind](QNetworkReply* reply, QJsonObject& obj_out) -> bool {
    const auto parsed = parseReplyBody(reply, error_msg, &error_kind, reply->url().path());
    if (!parsed) {
      return false;
    }
    obj_out = *parsed;
    return true;
  };

  if (!check_reply(weather_reply_, weather_obj) || !check_reply(pollution_reply_, pollution_obj)) {
    cleanupWeatherReplies();
    qCWarning(lcWeather) << "fetch failed:" << error_msg;
    emit fetchError(error_kind, error_msg);
    return;
  }

  CurrentWeather current = parseCurrentJson(weather_obj.value(QStringLiteral("current")).toObject());
  populatePollutionData(current, pollution_obj);

  const QList<HourlyEntry> hourly = parseHourlyJson(weather_obj.value(QStringLiteral("hourly")).toArray());
  const QList<DailyEntry> daily = parseDailyJson(weather_obj.value(QStringLiteral("daily")).toArray());

  cleanupWeatherReplies();
  emit weatherFetched(current, hourly, daily);
}

void WeatherProvider::onGeoReply(QNetworkReply* reply) {
  if (reply == nullptr) {
    return;
  }
  if (reply != geo_reply_) {
    reply->deleteLater();
    return;
  }
  geo_reply_ = nullptr;
  reply->deleteLater();
  QString error;
  const auto obj = parseReplyBody(reply, error);
  if (!obj) {
    qCWarning(lcWeather) << "geolocation failed:" << error;
    emit geoError(error);
    return;
  }
  const QJsonObject location = obj->value(QStringLiteral("location")).toObject();
  // ipgeolocation.io v3 returns latitude/longitude as strings.
  bool lat_ok = false;
  bool lon_ok = false;
  const double lat = location.value(QStringLiteral("latitude")).toString().toDouble(&lat_ok);
  const double lon = location.value(QStringLiteral("longitude")).toString().toDouble(&lon_ok);
  if (!lat_ok || !lon_ok || !std::isfinite(lat) || !std::isfinite(lon) || std::abs(lat) > 90 || std::abs(lon) > 180) {
    emit geoError(QStringLiteral("geolocation response missing latitude/longitude"));
    return;
  }
  emit geoFetched(lat, lon, parseLocationJson(location));
}

void WeatherProvider::onLocationReply(QNetworkReply* reply) {
  if (reply == nullptr) {
    return;
  }
  if (reply != location_reply_) {
    reply->deleteLater();
    return;
  }
  location_reply_ = nullptr;
  const QByteArray body = reply->readAll();
  const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
  const auto fail = [this, reply](const QString& message) {
    reply->deleteLater();
    qCWarning(lcWeather) << "reverse geocoding failed:" << message;
    emit locationError(message);
  };
  if (reply->error() != QNetworkReply::NoError || (status.isValid() && status.toInt() != 200)) {
    fail(QStringLiteral("%1 (HTTP %2)")
             .arg(reply->errorString(), status.isValid() ? status.toString() : QStringLiteral("?")));
    return;
  }
  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(body, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !doc.isArray() || doc.array().isEmpty()) {
    fail(QStringLiteral("malformed or empty response: %1").arg(parse_error.errorString()));
    return;
  }
  reply->deleteLater();
  const WeatherLocation location = parseLocationJson(doc.array().first().toObject());
  if (location.city.trimmed().isEmpty() && location.country.trimmed().isEmpty()) {
    emit locationError(QStringLiteral("response missing city and country"));
    return;
  }
  emit locationFetched(location);
}

WeatherLocation WeatherProvider::parseLocationJson(const QJsonObject& root) {
  WeatherLocation location;
  location.city = root.value(QStringLiteral("city")).toString(root.value(QStringLiteral("name")).toString()).trimmed();
  location.country = root.value(QStringLiteral("country_name")).toString().trimmed();
  if (location.country.isEmpty()) {
    const QString country_code = root.value(QStringLiteral("country")).toString().trimmed().toUpper();
    const QLocale::Territory territory = QLocale::codeToTerritory(country_code);
    location.country = territory == QLocale::AnyTerritory ? country_code : QLocale::territoryToString(territory);
  }
  return location;
}

CurrentWeather WeatherProvider::parseCurrentJson(const QJsonObject& root) {
  CurrentWeather out;
  const QJsonObject& obj = root;

  out.temperature = obj.value(QStringLiteral("temp")).toDouble(0.0);
  out.feels_like = obj.value(QStringLiteral("feels_like")).toDouble(0.0);
  out.humidity = clampPercentage(obj.value(QStringLiteral("humidity")).toInt(0));
  out.pressure = obj.value(QStringLiteral("pressure")).toInt(0);
  out.uvi = obj.value(QStringLiteral("uvi")).toDouble(0.0);
  out.dew_point = obj.value(QStringLiteral("dew_point")).toDouble(0.0);
  out.clouds = clampPercentage(obj.value(QStringLiteral("clouds")).toInt(0));

  const QJsonArray weather = obj.value(QStringLiteral("weather")).toArray();
  if (!weather.isEmpty()) {
    const QJsonObject first = weather.first().toObject();
    out.condition_id = first.value(QStringLiteral("id")).toInt(0);
    out.condition = first.value(QStringLiteral("description")).toString();
    out.icon_code = first.value(QStringLiteral("icon")).toString();
  }

  // OWM "metric" returns wind in m/s; the widget displays km/h.
  const double wind_ms = obj.value(QStringLiteral("wind_speed")).toDouble(0.0);
  out.wind_speed = static_cast<int>(std::lround(wind_ms * kMsToKmh));
  out.wind_direction = obj.value(QStringLiteral("wind_deg")).toInt(0);

  const double wind_gust_ms = obj.value(QStringLiteral("wind_gust")).toDouble(0.0);
  out.wind_gust = static_cast<int>(std::lround(wind_gust_ms * kMsToKmh));

  out.visibility = obj.value(QStringLiteral("visibility")).toDouble(0.0) / 1000.0;

  out.sunrise = static_cast<qint64>(obj.value(QStringLiteral("sunrise")).toDouble(0.0));
  out.sunset = static_cast<qint64>(obj.value(QStringLiteral("sunset")).toDouble(0.0));

  const auto dt_secs = static_cast<qint64>(obj.value(QStringLiteral("dt")).toDouble(0.0));
  out.time_updated = QDateTime::fromSecsSinceEpoch(dt_secs, QTimeZone::UTC).toString(Qt::ISODate);
  return out;
}

QList<HourlyEntry> WeatherProvider::parseHourlyJson(const QJsonArray& arr) {
  QList<HourlyEntry> out;
  out.reserve(arr.size());
  for (const auto& value : arr) {
    const QJsonObject entry = value.toObject();
    HourlyEntry hourly;
    hourly.timestamp = static_cast<qint64>(entry.value(QStringLiteral("dt")).toDouble(0.0));
    hourly.temperature = entry.value(QStringLiteral("temp")).toDouble(0.0);
    hourly.precipitation = precipFromEntry(entry);
    hourly.pop = clampUnitInterval(entry.value(QStringLiteral("pop")).toDouble(0.0));
    hourly.uvi = entry.value(QStringLiteral("uvi")).toDouble(0.0);
    hourly.clouds = clampPercentage(entry.value(QStringLiteral("clouds")).toInt(0));

    const QJsonArray weather = entry.value(QStringLiteral("weather")).toArray();
    if (!weather.isEmpty()) {
      const QJsonObject first = weather.first().toObject();
      hourly.condition_id = first.value(QStringLiteral("id")).toInt(0);
      hourly.condition = first.value(QStringLiteral("description")).toString();
      hourly.icon_code = first.value(QStringLiteral("icon")).toString();
    }
    out.append(hourly);
  }
  return out;
}

QList<DailyEntry> WeatherProvider::parseDailyJson(const QJsonArray& arr) {
  QList<DailyEntry> out;
  out.reserve(arr.size());
  for (const auto& value : arr) {
    const QJsonObject entry = value.toObject();
    DailyEntry daily;
    daily.date = static_cast<qint64>(entry.value(QStringLiteral("dt")).toDouble(0.0));
    daily.sunrise = static_cast<qint64>(entry.value(QStringLiteral("sunrise")).toDouble(0.0));
    daily.sunset = static_cast<qint64>(entry.value(QStringLiteral("sunset")).toDouble(0.0));

    const QJsonObject temp = entry.value(QStringLiteral("temp")).toObject();
    daily.temp_min = temp.value(QStringLiteral("min")).toDouble(0.0);
    daily.temp_max = temp.value(QStringLiteral("max")).toDouble(0.0);

    daily.pop = clampUnitInterval(entry.value(QStringLiteral("pop")).toDouble(0.0));
    daily.uvi = entry.value(QStringLiteral("uvi")).toDouble(0.0);
    daily.moon_phase = clampUnitInterval(entry.value(QStringLiteral("moon_phase")).toDouble(0.0));
    daily.summary = entry.value(QStringLiteral("summary")).toString();

    const QJsonArray weather = entry.value(QStringLiteral("weather")).toArray();
    if (!weather.isEmpty()) {
      const QJsonObject first = weather.first().toObject();
      daily.condition_id = first.value(QStringLiteral("id")).toInt(0);
      daily.condition = first.value(QStringLiteral("description")).toString();
      daily.icon_code = first.value(QStringLiteral("icon")).toString();
    }
    out.append(daily);
  }
  return out;
}

void WeatherProvider::geocodeCity(const QString& city, const QString& language) {
  if (location_reply_ != nullptr) {
    disconnect(location_reply_, nullptr, this, nullptr);
    location_reply_->abort();
    location_reply_->deleteLater();
  }
  QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
  QUrlQuery query;
  query.addQueryItem("name", city);
  query.addQueryItem("language", language);
  query.addQueryItem("count", "1");
  url.setQuery(query);
  QNetworkRequest request(url);
  request.setTransferTimeout(15000);
  location_reply_ = network_->get(request);
  connect(location_reply_, &QNetworkReply::finished, this, [this] {
    auto* reply = location_reply_;
    location_reply_ = nullptr;
    QString error;
    const auto root = parseReplyBody(reply, error);
    reply->deleteLater();
    if (!root) {
      emit geoError(error);
      return;
    }
    const auto results = root->value("results").toArray();
    if (results.isEmpty()) {
      emit geoError(QStringLiteral("No matching location"));
      return;
    }
    const auto item = results.first().toObject();
    const auto latitude = item.value("latitude");
    const auto longitude = item.value("longitude");
    if (!latitude.isDouble() || !longitude.isDouble() || !std::isfinite(latitude.toDouble()) ||
        !std::isfinite(longitude.toDouble()) || std::abs(latitude.toDouble()) > 90 ||
        std::abs(longitude.toDouble()) > 180) {
      emit geoError(QStringLiteral("Invalid location coordinates"));
      return;
    }
    emit geoFetched(latitude.toDouble(), longitude.toDouble(), parseLocationJson(item));
  });
}

int WeatherProvider::conditionFromWmo(int code) {
  switch (code) {
    case 0:
      return 800;
    case 1:
      return 801;
    case 2:
      return 802;
    case 3:
      return 804;
    case 45:
    case 48:
      return 741;
    case 51:
      return 300;
    case 53:
      return 301;
    case 55:
      return 302;
    case 56:
    case 57:
      return 511;
    case 61:
      return 500;
    case 63:
      return 501;
    case 65:
      return 502;
    case 66:
    case 67:
      return 511;
    case 71:
    case 77:
      return 600;
    case 73:
      return 601;
    case 75:
      return 602;
    case 80:
      return 520;
    case 81:
      return 521;
    case 82:
      return 522;
    case 85:
      return 620;
    case 86:
      return 622;
    case 95:
      return 200;
    case 96:
    case 99:
      return 201;
    default:
      return 0;
  }
}

WeatherCache WeatherProvider::parseOpenMeteo(const QJsonObject& root) {
  WeatherCache data;
  const auto cur = root.value("current").toObject();
  auto& current = data.current;
  current.temperature = cur.value("temperature_2m").toDouble();
  current.feels_like = cur.value("apparent_temperature").toDouble();
  current.humidity = cur.value("relative_humidity_2m").toInt();
  current.pressure = qRound(cur.value("pressure_msl").toDouble());
  current.wind_speed = qRound(cur.value("wind_speed_10m").toDouble());
  current.wind_gust = qRound(cur.value("wind_gusts_10m").toDouble());
  current.wind_direction = cur.value("wind_direction_10m").toInt();
  current.clouds = cur.value("cloud_cover").toInt();
  current.condition_id = conditionFromWmo(cur.value("weather_code").toInt(-1));

  current.condition = conditionDescription(current.condition_id);
  current.icon_code = (cur.value("is_day").toInt() != 0) ? "01d" : "01n";
  const auto timestamp = static_cast<qint64>(cur.value("time").toDouble());
  current.time_updated = QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::UTC).toString(Qt::ISODate);
  const auto hourly = root.value("hourly").toObject();
  const auto times = hourly.value("time").toArray();
  const auto forecastValue = [](const QJsonObject& object, const char* key, qsizetype index) {
    const auto values = object.value(QLatin1String(key)).toArray();
    return index < values.size() ? values[index].toDouble() : 0.0;
  };
  for (qsizetype i = 0; i < times.size(); ++i) {
    const auto time = static_cast<qint64>(times[i].toDouble());
    if (time <= timestamp && timestamp < time + 3600) {
      current.uvi = forecastValue(hourly, "uv_index", i);
      current.dew_point = forecastValue(hourly, "dew_point_2m", i);
      current.visibility = forecastValue(hourly, "visibility", i) / 1000.0;
    }
    if (time < timestamp || data.hourly.size() >= 48) {
      continue;
    }
    HourlyEntry entry;
    entry.timestamp = time;
    entry.temperature = forecastValue(hourly, "temperature_2m", i);
    entry.condition_id = conditionFromWmo(static_cast<int>(forecastValue(hourly, "weather_code", i)));
    entry.condition = conditionDescription(entry.condition_id);
    entry.icon_code = forecastValue(hourly, "is_day", i) > 0 ? "01d" : "01n";
    entry.precipitation = forecastValue(hourly, "precipitation", i);
    entry.pop = forecastValue(hourly, "precipitation_probability", i) / 100.0;
    entry.uvi = forecastValue(hourly, "uv_index", i);
    entry.clouds = static_cast<int>(forecastValue(hourly, "cloud_cover", i));
    data.hourly.append(entry);
  }
  const auto daily = root.value("daily").toObject();
  const auto dates = daily.value("time").toArray();
  for (qsizetype i = 0; i < dates.size(); ++i) {
    DailyEntry entry;
    entry.date = static_cast<qint64>(dates[i].toDouble());
    entry.temp_min = forecastValue(daily, "temperature_2m_min", i);
    entry.temp_max = forecastValue(daily, "temperature_2m_max", i);
    entry.condition_id = conditionFromWmo(static_cast<int>(forecastValue(daily, "weather_code", i)));
    entry.condition = conditionDescription(entry.condition_id);
    entry.icon_code = "01d";
    entry.pop = forecastValue(daily, "precipitation_probability_max", i) / 100.0;
    entry.uvi = forecastValue(daily, "uv_index_max", i);
    entry.sunrise = static_cast<qint64>(forecastValue(daily, "sunrise", i));
    entry.sunset = static_cast<qint64>(forecastValue(daily, "sunset", i));
    entry.moon_phase = static_cast<int>(MoonPhaseCalculator::phaseForDate(
                           std::chrono::system_clock::time_point{std::chrono::seconds{entry.date}})) /
                       8.0;
    data.daily.append(entry);
  }
  if (!data.daily.isEmpty()) {
    current.sunrise = data.daily.first().sunrise;
    current.sunset = data.daily.first().sunset;
  }
  return data;
}
