#include "WeatherIconBridge.h"

#include "MoonPhase.h"
#include "WeatherIconMapper.h"

#include <cmath>

WeatherIconBridge::WeatherIconBridge(QObject* parent) : QObject(parent) {}

namespace {

// variantToMoonPhase() classifies an external 0..1 phase fraction (weather-API convention: 0/1 =
// new moon, 0.5 = full). These are hand-tuned, NOT a uniform eighths split like
// MoonPhaseCalculator's date-based bucketing (different input domain — see class comment) — the
// four named quarter/full/new phases get a narrow ~2%-wide window so near-exact values snap to
// the crisp icon; the four in-between phases get the remaining ~23%-wide bands.
constexpr double kMoonNewLowerBound = 0.01;  // New: v < 0.01 || v > 0.99
constexpr double kMoonNewUpperBound = 0.99;
constexpr double kMoonFirstQuarterLow = 0.24;
constexpr double kMoonFirstQuarterHigh = 0.26;
constexpr double kMoonFullLow = 0.49;
constexpr double kMoonFullHigh = 0.51;
constexpr double kMoonLastQuarterLow = 0.74;
constexpr double kMoonLastQuarterHigh = 0.76;

WeatherIconNs::MoonPhase variantToMoonPhase(const QVariant& moon_phase) {
  using WeatherIconNs::MoonPhase;

  MoonPhase moon_phase_val{MoonPhase::WaningCrescent};  // default fallback

  if (moon_phase.isValid() && !moon_phase.isNull()) {
    bool parse_ok = false;
    double val = moon_phase.toDouble(&parse_ok);
    if (parse_ok) {
      if (val < kMoonNewLowerBound || val > kMoonNewUpperBound) {
        moon_phase_val = MoonPhase::New;
      } else if (val >= kMoonNewLowerBound && val < kMoonFirstQuarterLow) {
        moon_phase_val = MoonPhase::WaxingCrescent;
      } else if (val >= kMoonFirstQuarterLow && val <= kMoonFirstQuarterHigh) {
        moon_phase_val = MoonPhase::FirstQuarter;
      } else if (val > kMoonFirstQuarterHigh && val < kMoonFullLow) {
        moon_phase_val = MoonPhase::WaxingGibbous;
      } else if (val >= kMoonFullLow && val <= kMoonFullHigh) {
        moon_phase_val = MoonPhase::Full;
      } else if (val > kMoonFullHigh && val < kMoonLastQuarterLow) {
        moon_phase_val = MoonPhase::WaningGibbous;
      } else if (val >= kMoonLastQuarterLow && val <= kMoonLastQuarterHigh) {
        moon_phase_val = MoonPhase::LastQuarter;
      } else {
        moon_phase_val = MoonPhase::WaningCrescent;
      }
    }
  }
  return moon_phase_val;
}

}  // namespace

QString WeatherIconBridge::moonPhaseIcon(const QVariant& moon_phase) {
  bool valid = false;
  const double value = moon_phase.toDouble(&valid);
  if (!moon_phase.isValid() || moon_phase.isNull() || !valid || !std::isfinite(value) || value < 0 || value > 1)
    return QStringLiteral("unavailable");
  const WeatherIconNs::MoonPhase moon_phase_val = variantToMoonPhase(moon_phase);
  static const QStringList names{"moon-new",  "moon-waxing-crescent", "moon-first-quarter", "moon-waxing-gibbous",
                                 "moon-full", "moon-waning-gibbous",  "moon-last-quarter",  "moon-waning-crescent"};
  return names.at(static_cast<int>(moon_phase_val));
}

QString WeatherIconBridge::moonPhaseDescription(const QVariant& moon_phase) {
  bool valid = false;
  const double value = moon_phase.toDouble(&valid);
  if (!moon_phase.isValid() || moon_phase.isNull() || !valid || !std::isfinite(value) || value < 0 || value > 1)
    return QStringLiteral("Unknown moon phase");
  using WeatherIconNs::MoonPhase;
  const WeatherIconNs::MoonPhase moon_phase_val = variantToMoonPhase(moon_phase);
  switch (moon_phase_val) {
    case MoonPhase::New:
      return QStringLiteral("New Moon");
    case MoonPhase::WaxingCrescent:
      return QStringLiteral("Waxing Crescent");
    case MoonPhase::FirstQuarter:
      return QStringLiteral("First Quarter");
    case MoonPhase::WaxingGibbous:
      return QStringLiteral("Waxing Gibbous");
    case MoonPhase::Full:
      return QStringLiteral("Full Moon");
    case MoonPhase::WaningGibbous:
      return QStringLiteral("Waning Gibbous");
    case MoonPhase::LastQuarter:
      return QStringLiteral("Last Quarter");
    case MoonPhase::WaningCrescent:
      return QStringLiteral("Waning Crescent");
  }
  return QStringLiteral("New Moon");
}

QString WeatherIconBridge::iconFor(const QString& provider, int code, bool is_day, bool daylight_available) {
  return "qrc:/HolonightShell/weather/" + WeatherIconMapper::icon(provider, code, is_day, daylight_available) + ".png";
}
QVariantList WeatherIconBridge::previewCatalogue() const {
  QVariantList result;
  for (const auto& entry : WeatherIconMapper::catalogue()) {
    const bool variants = WeatherIconMapper::icon(entry.provider, entry.code, true) !=
                          WeatherIconMapper::icon(entry.provider, entry.code, false);
    for (int index = 0; index < (variants ? 2 : 1); ++index) {
      result.append(QVariantMap{{"provider", entry.provider},
                                {"providerName", entry.provider == "openweather" ? "OpenWeather" : "Open-Meteo"},
                                {"code", entry.code},
                                {"description", entry.description},
                                {"isDay", index == 0},
                                {"variant", variants ? (index == 0 ? "day" : "night") : "neutral"}});
    }
  }
  return result;
}
