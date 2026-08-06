#pragma once

#include "WeatherData.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;

Q_DECLARE_LOGGING_CATEGORY(lcWeather)

// Pure network + JSON adapter. Builds OpenWeatherMap and ipgeolocation.io requests, issues them
// asynchronously via QNetworkAccessManager, parses the responses into the WeatherData value
// types, and emits the results. Holds no policy (no timer, no cache, no backoff) — that lives in
// WeatherService. Kept as a separate, injectable class so WeatherService can be unit-tested with
// a mock provider.
class WeatherProvider : public QObject {
  Q_OBJECT

 public:
  enum class FetchErrorKind : quint8 { Transient, Authentication };
  Q_ENUM(FetchErrorKind)

  explicit WeatherProvider(QObject* parent = nullptr);
  ~WeatherProvider() override = default;

  WeatherProvider(const WeatherProvider&) = delete;
  WeatherProvider& operator=(const WeatherProvider&) = delete;
  WeatherProvider(WeatherProvider&&) = delete;
  WeatherProvider& operator=(WeatherProvider&&) = delete;

  virtual void fetchWeather(double lat, double lon, const QString& api_key, const QString& units, const QString& lang);
  virtual void fetchGeolocation(const QString& geo_api_key);
  virtual void reverseGeocode(double lat, double lon, const QString& api_key);
  virtual void cancelPending();

  [[nodiscard]] static WeatherLocation parseLocationJson(const QJsonObject& root);
  [[nodiscard]] static CurrentWeather parseCurrentJson(const QJsonObject& root);
  [[nodiscard]] static QList<HourlyEntry> parseHourlyJson(const QJsonArray& arr);
  [[nodiscard]] static QList<DailyEntry> parseDailyJson(const QJsonArray& arr);
  static void populatePollutionData(CurrentWeather& current, const QJsonObject& pollution_obj);

 Q_SIGNALS:
  void weatherFetched(const CurrentWeather& weather, const QList<HourlyEntry>& hourly, const QList<DailyEntry>& daily);
  void geoFetched(double lat, double lon, const WeatherLocation& location);
  void locationFetched(const WeatherLocation& location);
  void locationError(const QString& message);
  void fetchError(FetchErrorKind kind, const QString& message);
  void geoError(const QString& message);

 private:
  void checkComplete();
  void cleanupWeatherReplies();
  void cleanupReplies();
  void onGeoReply(QNetworkReply* reply);
  void onLocationReply(QNetworkReply* reply);

  QNetworkAccessManager nam_;
  QNetworkReply* geo_reply_{nullptr};
  QNetworkReply* location_reply_{nullptr};
  QNetworkReply* weather_reply_{nullptr};
  QNetworkReply* pollution_reply_{nullptr};
};
