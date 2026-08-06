#include "WeatherIconBridge.h"

#include "MoonPhase.h"
#include "WeatherIconMapper.h"

WeatherIconBridge::WeatherIconBridge(QObject* parent) : QObject(parent) {}

namespace {

constexpr int kHurricaneWindSpeedKmh = 119;

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

int presentationConditionCode(int condition_code, const QString& condition_description, int wind_speed_kmh) {
  if (condition_description.contains(QStringLiteral("hail"), Qt::CaseInsensitive)) {
    return WeatherIconMapper::kHailConditionCode;
  }
  if (wind_speed_kmh >= kHurricaneWindSpeedKmh) {
    return WeatherIconMapper::kHurricaneConditionCode;
  }
  return condition_code;
}

}  // namespace

QStringList WeatherIconBridge::layersFor(int condition_code, bool is_day, const QDateTime& date,
                                         const QVariant& moon_phase) {
  const WeatherIconNs::MoonPhase moon_phase_val =
      is_day ? WeatherIconNs::MoonPhase::WaningCrescent : variantToMoonPhase(moon_phase);
  return WeatherIconMapper::mapLayers(condition_code, is_day, moon_phase_val);
}

QStringList WeatherIconBridge::layersForWeather(int condition_code, bool is_day, const QDateTime& date,
                                                const QString& condition_description, int wind_speed_kmh,
                                                const QVariant& moon_phase) {
  Q_UNUSED(date)
  const WeatherIconNs::MoonPhase moon_phase_val =
      is_day ? WeatherIconNs::MoonPhase::WaningCrescent : variantToMoonPhase(moon_phase);
  const int effective_condition_code = presentationConditionCode(condition_code, condition_description, wind_speed_kmh);
  return WeatherIconMapper::mapLayers(effective_condition_code, is_day, moon_phase_val);
}

QString WeatherIconBridge::moonPhaseIcon(const QVariant& moon_phase) {
  const WeatherIconNs::MoonPhase moon_phase_val = variantToMoonPhase(moon_phase);
  return WeatherIconMapper::moonLayerName(moon_phase_val);
}

QString WeatherIconBridge::moonPhaseDescription(const QVariant& moon_phase) {
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
