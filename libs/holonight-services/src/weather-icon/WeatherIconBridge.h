#pragma once

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>

// Stateless QML singleton adapter over WeatherIconMapper + MoonPhaseCalculator. Holds no
// instance state; exists solely so QML can call a single ergonomic entry point
// (WeatherIconBridge.layersFor(...)) without the pure C++ classes needing any QML dependency.
// See REQ-NF-ARCH-002 (stable public API), REQ-C-NOQML-C++ (this file is the sanctioned
// exception — see docs/sdd/weather-icon-compositor/DESIGN.md §2.1.1).
class WeatherIconBridge : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  explicit WeatherIconBridge(QObject* parent = nullptr);

  // Returns layer basenames (no extension/path) for the given condition + day/night + date.
  // date is consulted for moon-phase calculation only when is_day is false.
  Q_INVOKABLE [[nodiscard]] static QStringList layersFor(int condition_code, bool is_day, const QDateTime& date,
                                                         const QVariant& moon_phase = {});

  // Applies presentation-only overrides for conditions OpenWeather does not expose as stable
  // condition IDs, then returns layer basenames like layersFor().
  Q_INVOKABLE [[nodiscard]] static QStringList layersForWeather(int condition_code, bool is_day, const QDateTime& date,
                                                                const QString& condition_description,
                                                                int wind_speed_kmh, const QVariant& moon_phase = {});

  // Returns the icon basename (e.g. "moon-waxing-crescent") for a moon phase value.
  Q_INVOKABLE [[nodiscard]] static QString moonPhaseIcon(const QVariant& moon_phase);

  // Returns a user-friendly description (e.g. "Waxing Crescent") for a moon phase value.
  Q_INVOKABLE [[nodiscard]] static QString moonPhaseDescription(const QVariant& moon_phase);
};
