#include "WeatherIconMapper.h"
const QList<WeatherCondition>& WeatherIconMapper::catalogue() {
  static const QList<WeatherCondition> entries{
      {QStringLiteral("openweather"), 200, QStringLiteral("thunderstorm with light rain"),
       QStringLiteral("thunderstorm")},
      {QStringLiteral("openweather"), 201, QStringLiteral("thunderstorm with rain"), QStringLiteral("thunderstorm")},
      {QStringLiteral("openweather"), 202, QStringLiteral("thunderstorm with heavy rain"),
       QStringLiteral("thunderstorm")},
      {QStringLiteral("openweather"), 210, QStringLiteral("light thunderstorm"), QStringLiteral("thunderstorm")},
      {QStringLiteral("openweather"), 211, QStringLiteral("thunderstorm"), QStringLiteral("thunderstorm")},
      {QStringLiteral("openweather"), 212, QStringLiteral("heavy thunderstorm"), QStringLiteral("severe-thunderstorm")},
      {QStringLiteral("openweather"), 221, QStringLiteral("ragged thunderstorm"), QStringLiteral("thunderstorm")},
      {QStringLiteral("openweather"), 230, QStringLiteral("thunderstorm with light drizzle"),
       QStringLiteral("thunderstorm")},
      {QStringLiteral("openweather"), 231, QStringLiteral("thunderstorm with drizzle"), QStringLiteral("thunderstorm")},
      {QStringLiteral("openweather"), 232, QStringLiteral("thunderstorm with heavy drizzle"),
       QStringLiteral("thunderstorm")},
      {QStringLiteral("openweather"), 300, QStringLiteral("light intensity drizzle"), QStringLiteral("drizzle")},
      {QStringLiteral("openweather"), 301, QStringLiteral("drizzle"), QStringLiteral("drizzle")},
      {QStringLiteral("openweather"), 302, QStringLiteral("heavy intensity drizzle"), QStringLiteral("drizzle")},
      {QStringLiteral("openweather"), 310, QStringLiteral("light intensity drizzle rain"), QStringLiteral("drizzle")},
      {QStringLiteral("openweather"), 311, QStringLiteral("drizzle rain"), QStringLiteral("drizzle")},
      {QStringLiteral("openweather"), 312, QStringLiteral("heavy intensity drizzle rain"), QStringLiteral("drizzle")},
      {QStringLiteral("openweather"), 313, QStringLiteral("shower rain and drizzle"), QStringLiteral("drizzle")},
      {QStringLiteral("openweather"), 314, QStringLiteral("heavy shower rain and drizzle"), QStringLiteral("drizzle")},
      {QStringLiteral("openweather"), 321, QStringLiteral("shower drizzle"), QStringLiteral("drizzle")},
      {QStringLiteral("openweather"), 500, QStringLiteral("light rain"), QStringLiteral("rainy")},
      {QStringLiteral("openweather"), 501, QStringLiteral("moderate rain"), QStringLiteral("rainy")},
      {QStringLiteral("openweather"), 502, QStringLiteral("heavy intensity rain"), QStringLiteral("heavy-rain")},
      {QStringLiteral("openweather"), 503, QStringLiteral("very heavy rain"), QStringLiteral("heavy-rain")},
      {QStringLiteral("openweather"), 504, QStringLiteral("extreme rain"), QStringLiteral("heavy-rain")},
      {QStringLiteral("openweather"), 511, QStringLiteral("freezing rain"), QStringLiteral("freezing-rain")},
      {QStringLiteral("openweather"), 520, QStringLiteral("light intensity shower rain"),
       QStringLiteral("rain-showers")},
      {QStringLiteral("openweather"), 521, QStringLiteral("shower rain"), QStringLiteral("rain-showers")},
      {QStringLiteral("openweather"), 522, QStringLiteral("heavy intensity shower rain"), QStringLiteral("heavy-rain")},
      {QStringLiteral("openweather"), 531, QStringLiteral("ragged shower rain"), QStringLiteral("rain-showers")},
      {QStringLiteral("openweather"), 600, QStringLiteral("light snow"), QStringLiteral("snow")},
      {QStringLiteral("openweather"), 601, QStringLiteral("snow"), QStringLiteral("snow")},
      {QStringLiteral("openweather"), 602, QStringLiteral("heavy snow"), QStringLiteral("heavy-snow")},
      {QStringLiteral("openweather"), 611, QStringLiteral("sleet"), QStringLiteral("sleet")},
      {QStringLiteral("openweather"), 612, QStringLiteral("light shower sleet"), QStringLiteral("sleet")},
      {QStringLiteral("openweather"), 613, QStringLiteral("shower sleet"), QStringLiteral("sleet")},
      {QStringLiteral("openweather"), 615, QStringLiteral("light rain and snow"), QStringLiteral("rain-snow-mix")},
      {QStringLiteral("openweather"), 616, QStringLiteral("rain and snow"), QStringLiteral("rain-snow-mix")},
      {QStringLiteral("openweather"), 620, QStringLiteral("light shower snow"), QStringLiteral("snow-showers")},
      {QStringLiteral("openweather"), 621, QStringLiteral("shower snow"), QStringLiteral("snow-showers")},
      {QStringLiteral("openweather"), 622, QStringLiteral("heavy shower snow"), QStringLiteral("heavy-snow")},
      {QStringLiteral("openweather"), 701, QStringLiteral("mist"), QStringLiteral("fog")},
      {QStringLiteral("openweather"), 711, QStringLiteral("smoke"), QStringLiteral("smoke")},
      {QStringLiteral("openweather"), 721, QStringLiteral("haze"), QStringLiteral("fog")},
      {QStringLiteral("openweather"), 731, QStringLiteral("sand/dust whirls"), QStringLiteral("dust-sand")},
      {QStringLiteral("openweather"), 741, QStringLiteral("fog"), QStringLiteral("fog")},
      {QStringLiteral("openweather"), 751, QStringLiteral("sand"), QStringLiteral("dust-sand")},
      {QStringLiteral("openweather"), 761, QStringLiteral("dust"), QStringLiteral("dust-sand")},
      {QStringLiteral("openweather"), 762, QStringLiteral("volcanic ash"), QStringLiteral("volcanic-ash")},
      {QStringLiteral("openweather"), 771, QStringLiteral("squalls"), QStringLiteral("windy")},
      {QStringLiteral("openweather"), 781, QStringLiteral("tornado"), QStringLiteral("tornado")},
      {QStringLiteral("openweather"), 800, QStringLiteral("clear sky"), QStringLiteral("sunny")},
      {QStringLiteral("openweather"), 801, QStringLiteral("few clouds"), QStringLiteral("mostly-sunny")},
      {QStringLiteral("openweather"), 802, QStringLiteral("scattered clouds"), QStringLiteral("partly-cloudy")},
      {QStringLiteral("openweather"), 803, QStringLiteral("broken clouds"), QStringLiteral("mostly-cloudy")},
      {QStringLiteral("openweather"), 804, QStringLiteral("overcast clouds"), QStringLiteral("overcast")},
      {QStringLiteral("openmeteo"), 0, QStringLiteral("Clear sky"), QStringLiteral("sunny")},
      {QStringLiteral("openmeteo"), 1, QStringLiteral("Mainly clear"), QStringLiteral("mostly-sunny")},
      {QStringLiteral("openmeteo"), 2, QStringLiteral("Partly cloudy"), QStringLiteral("partly-cloudy")},
      {QStringLiteral("openmeteo"), 3, QStringLiteral("Overcast"), QStringLiteral("overcast")},
      {QStringLiteral("openmeteo"), 45, QStringLiteral("Fog"), QStringLiteral("fog")},
      {QStringLiteral("openmeteo"), 48, QStringLiteral("Depositing rime fog"), QStringLiteral("fog")},
      {QStringLiteral("openmeteo"), 51, QStringLiteral("Light drizzle"), QStringLiteral("drizzle")},
      {QStringLiteral("openmeteo"), 53, QStringLiteral("Moderate drizzle"), QStringLiteral("drizzle")},
      {QStringLiteral("openmeteo"), 55, QStringLiteral("Dense drizzle"), QStringLiteral("drizzle")},
      {QStringLiteral("openmeteo"), 56, QStringLiteral("Light freezing drizzle"), QStringLiteral("freezing-drizzle")},
      {QStringLiteral("openmeteo"), 57, QStringLiteral("Dense freezing drizzle"), QStringLiteral("freezing-drizzle")},
      {QStringLiteral("openmeteo"), 61, QStringLiteral("Slight rain"), QStringLiteral("rainy")},
      {QStringLiteral("openmeteo"), 63, QStringLiteral("Moderate rain"), QStringLiteral("rainy")},
      {QStringLiteral("openmeteo"), 65, QStringLiteral("Heavy rain"), QStringLiteral("heavy-rain")},
      {QStringLiteral("openmeteo"), 66, QStringLiteral("Light freezing rain"), QStringLiteral("freezing-rain")},
      {QStringLiteral("openmeteo"), 67, QStringLiteral("Heavy freezing rain"), QStringLiteral("freezing-rain")},
      {QStringLiteral("openmeteo"), 71, QStringLiteral("Slight snow fall"), QStringLiteral("snow")},
      {QStringLiteral("openmeteo"), 73, QStringLiteral("Moderate snow fall"), QStringLiteral("snow")},
      {QStringLiteral("openmeteo"), 75, QStringLiteral("Heavy snow fall"), QStringLiteral("heavy-snow")},
      {QStringLiteral("openmeteo"), 77, QStringLiteral("Snow grains"), QStringLiteral("snow")},
      {QStringLiteral("openmeteo"), 80, QStringLiteral("Slight rain showers"), QStringLiteral("rain-showers")},
      {QStringLiteral("openmeteo"), 81, QStringLiteral("Moderate rain showers"), QStringLiteral("rain-showers")},
      {QStringLiteral("openmeteo"), 82, QStringLiteral("Violent rain showers"), QStringLiteral("heavy-rain")},
      {QStringLiteral("openmeteo"), 85, QStringLiteral("Slight snow showers"), QStringLiteral("snow-showers")},
      {QStringLiteral("openmeteo"), 86, QStringLiteral("Heavy snow showers"), QStringLiteral("heavy-snow")},
      {QStringLiteral("openmeteo"), 95, QStringLiteral("Slight or moderate thunderstorm"),
       QStringLiteral("thunderstorm")},
      {QStringLiteral("openmeteo"), 96, QStringLiteral("Thunderstorm with slight hail"),
       QStringLiteral("thunderstorm-hail")},
      {QStringLiteral("openmeteo"), 97, QStringLiteral("Heavy thunderstorm"), QStringLiteral("severe-thunderstorm")},
      {QStringLiteral("openmeteo"), 99, QStringLiteral("Thunderstorm with heavy hail"),
       QStringLiteral("thunderstorm-hail")},
  };
  return entries;
}
QString WeatherIconMapper::description(const QString& provider, int code) {
  for (const auto& entry : catalogue()) {
    if (entry.provider == provider && entry.code == code) return entry.description;
  }
  return QStringLiteral("Unknown condition");
}
QString WeatherIconMapper::icon(const QString& provider, int code, bool is_day, bool daylight_available) {
  QString artwork = QStringLiteral("unavailable");
  for (const auto& entry : catalogue()) {
    if (entry.provider == provider && entry.code == code) {
      artwork = entry.artwork;
      break;
    }
  }
  if (artwork == "sunny")
    return daylight_available ? (is_day ? artwork : QStringLiteral("clear-night")) : QStringLiteral("unavailable");
  if (artwork == "mostly-sunny" || artwork == "partly-cloudy" || artwork == "mostly-cloudy") {
    if (!daylight_available) return QStringLiteral("cloudy");
    if (!is_day) return artwork == "mostly-sunny" ? QStringLiteral("mostly-clear-night") : artwork + "-night";
  }
  return artwork;
}
