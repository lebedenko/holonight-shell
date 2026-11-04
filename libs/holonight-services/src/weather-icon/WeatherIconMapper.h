#pragma once
#include <QList>
#include <QString>
struct WeatherCondition {
  QString provider;
  int code;
  QString description;
  QString artwork;
};
class WeatherIconMapper {
 public:
  static const QList<WeatherCondition>& catalogue();
  static QString icon(const QString& provider, int code, bool is_day, bool daylight_available = true);
  static QString description(const QString& provider, int code);
};
