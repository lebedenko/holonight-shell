#pragma once

#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>

struct WeatherLocation {
  QString city;
  QString country;

  bool operator==(const WeatherLocation&) const = default;
};

// Plain value types describing parsed weather data. Each forecast-bearing struct is a
// Q_GADGET so it can be wrapped in a QVariant / QVariantList and read field-by-field from
// QML (e.g. `WeatherService.current.temperature`, `model.modelData.conditionId`) without a
// QAbstractListModel. Lists are always replaced wholesale on each fetch, so the value-type
// approach carries no incremental-update cost.
//
// Q_PROPERTY names stay camelCase (the QML-facing contract); the backing members are
// lower_case to satisfy readability-identifier-naming (MemberCase: lower_case).

// A single hourly forecast entry from OWM /onecall hourly list.
struct HourlyEntry {
  Q_GADGET
  Q_PROPERTY(qint64 timestamp MEMBER timestamp)
  Q_PROPERTY(double temperature MEMBER temperature)
  Q_PROPERTY(int conditionId MEMBER condition_id)
  Q_PROPERTY(double precipitation MEMBER precipitation)
  Q_PROPERTY(QString iconCode MEMBER icon_code)
  Q_PROPERTY(QString condition MEMBER condition)
  Q_PROPERTY(double pop MEMBER pop)
  Q_PROPERTY(double uvi MEMBER uvi)
  Q_PROPERTY(int clouds MEMBER clouds)

 public:
  qint64 timestamp{0};        // Unix seconds (UTC), from hourly[i].dt
  double temperature{0.0};    // degrees in the configured unit, from hourly[i].temp
  int condition_id{0};        // OWM condition id, from hourly[i].weather[0].id
  double precipitation{0.0};  // mm/1h, from hourly[i].rain["1h"] + hourly[i].snow["1h"]; 0 if absent
  QString icon_code;          // e.g. "04n", from hourly[i].weather[0].icon
  QString condition;          // weather[0].description; used for parsing
  double pop{0.0};            // probability of precipitation (0.0 to 1.0)
  double uvi{0.0};            // UV Index
  int clouds{0};              // cloud cover %

  bool operator==(const HourlyEntry&) const = default;
};

// Daily summary direct from OWM /onecall daily list.
struct DailyEntry {
  Q_GADGET
  Q_PROPERTY(qint64 date MEMBER date)
  Q_PROPERTY(double tempMin MEMBER temp_min)
  Q_PROPERTY(double tempMax MEMBER temp_max)
  Q_PROPERTY(int conditionId MEMBER condition_id)
  Q_PROPERTY(QString condition MEMBER condition)
  Q_PROPERTY(QString iconCode MEMBER icon_code)
  Q_PROPERTY(double pop MEMBER pop)
  Q_PROPERTY(double uvi MEMBER uvi)
  Q_PROPERTY(double moonPhase MEMBER moon_phase)
  Q_PROPERTY(QString summary MEMBER summary)
  Q_PROPERTY(qint64 sunrise MEMBER sunrise)
  Q_PROPERTY(qint64 sunset MEMBER sunset)

 public:
  qint64 date{0};  // Unix seconds of local midnight for that day
  double temp_min{0.0};
  double temp_max{0.0};
  int condition_id{0};     // from the entry's weather[0].id
  QString condition;       // display string, from weather[0].description
  QString icon_code;       // e.g. "02d", from weather[0].icon
  double pop{0.0};         // probability of precipitation (0.0 to 1.0)
  double uvi{0.0};         // UV Index
  double moon_phase{0.0};  // moon phase (0.0 to 1.0)
  QString summary;         // daily AI weather summary description
  qint64 sunrise{0};       // Unix seconds sunrise time
  qint64 sunset{0};        // Unix seconds sunset time

  bool operator==(const DailyEntry&) const = default;
};

// Current weather (from OWM /onecall endpoint).
struct CurrentWeather {
  Q_GADGET
  Q_PROPERTY(double temperature MEMBER temperature)
  Q_PROPERTY(double feelsLike MEMBER feels_like)
  Q_PROPERTY(QString condition MEMBER condition)
  Q_PROPERTY(int conditionId MEMBER condition_id)
  Q_PROPERTY(int humidity MEMBER humidity)
  Q_PROPERTY(int windSpeed MEMBER wind_speed)
  Q_PROPERTY(int windDirection MEMBER wind_direction)
  Q_PROPERTY(double visibility MEMBER visibility)
  Q_PROPERTY(int pressure MEMBER pressure)
  Q_PROPERTY(QString iconCode MEMBER icon_code)
  Q_PROPERTY(QString timeUpdated MEMBER time_updated)
  Q_PROPERTY(qint64 sunrise MEMBER sunrise)
  Q_PROPERTY(qint64 sunset MEMBER sunset)
  Q_PROPERTY(double uvi MEMBER uvi)
  Q_PROPERTY(double dewPoint MEMBER dew_point)
  Q_PROPERTY(int clouds MEMBER clouds)
  Q_PROPERTY(int windGust MEMBER wind_gust)
  Q_PROPERTY(int aqi MEMBER aqi)
  Q_PROPERTY(double pm2_5 MEMBER pm2_5)
  Q_PROPERTY(double pm10 MEMBER pm10)
  Q_PROPERTY(double co MEMBER co)
  Q_PROPERTY(double no2 MEMBER no2)
  Q_PROPERTY(double o3 MEMBER o3)
  Q_PROPERTY(double so2 MEMBER so2)

 public:
  double temperature{0.0};  // temp
  double feels_like{0.0};   // feels_like
  QString condition;        // weather[0].description
  int condition_id{0};      // weather[0].id
  int humidity{0};          // humidity (%)
  int wind_speed{0};        // wind_speed * 3.6, rounded (km/h)
  int wind_direction{0};    // wind_deg, meteorological direction where wind comes from
  double visibility{0.0};   // visibility / 1000.0 (km)
  int pressure{0};          // pressure (hPa)
  QString icon_code;        // weather[0].icon
  QString time_updated;     // ISO 8601 UTC string of dt
  qint64 sunrise{0};        // sunrise (Unix UTC); drives day/night resolution
  qint64 sunset{0};         // sunset  (Unix UTC)
  double uvi{0.0};          // UV Index
  double dew_point{0.0};    // dew point temperature
  int clouds{0};            // cloud cover %
  int wind_gust{0};         // wind_gust * 3.6, rounded (km/h)
  int aqi{0};               // Air Quality Index (1-5)
  double pm2_5{0.0};        // PM2.5 (μg/m³)
  double pm10{0.0};         // PM10 (μg/m³)
  double co{0.0};           // CO (μg/m³)
  double no2{0.0};          // NO2 (μg/m³)
  double o3{0.0};           // O3 (μg/m³)
  double so2{0.0};          // SO2 (μg/m³)

  bool operator==(const CurrentWeather&) const = default;
};

// Combined snapshot serialized to/from weather.json. Holds no API keys.
struct WeatherCache {
  CurrentWeather current;
  WeatherLocation location;
  QList<HourlyEntry> hourly;  // raw 3-hourly entries; up to 40
  QList<DailyEntry> daily;    // aggregated; up to 6 (5 displayed)
  qint64 fetched_at{0};       // Unix seconds; drives stale detection

  bool operator==(const WeatherCache&) const = default;
};

Q_DECLARE_METATYPE(HourlyEntry)
Q_DECLARE_METATYPE(DailyEntry)
Q_DECLARE_METATYPE(CurrentWeather)
Q_DECLARE_METATYPE(WeatherLocation)
