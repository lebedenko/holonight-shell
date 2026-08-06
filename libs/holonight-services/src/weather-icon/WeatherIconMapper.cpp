#include "WeatherIconMapper.h"

#include "WeatherIconLayer.h"

#include <algorithm>
#include <array>

namespace {

enum class Family : std::uint8_t {
  Clear,
  FewClouds,
  ScatteredClouds,
  BrokenClouds,
  Mist,
  Smoke,
  DustStorm,
  Wind,
  Tornado,
  LightDrizzle,
  HeavyDrizzle,
  Rain,
  HeavyRain,
  FreezingRain,
  Hail,
  Hurricane,
  Showers,
  Snow,
  HeavySnow,
  Sleet,
  RainSnowMix,
  SnowShowers,
  Thunderstorm,
  HeavyThunderstorm,
  Unmapped,
};

constexpr std::array kLightDrizzleCodes{300, 301, 310, 313, 315};
constexpr std::array kHeavyDrizzleCodes{302, 311, 312, 314};
constexpr std::array kRainCodes{500, 501};
constexpr std::array kHeavyRainCodes{502, 503, 504};
constexpr std::array kFreezingRainCodes{511};
constexpr std::array kShowersCodes{321, 520, 521, 522, 531};
constexpr std::array kSnowCodes{600, 601, 620};
constexpr std::array kHeavySnowCodes{602, 622};
constexpr std::array kSleetCodes{611, 612};
constexpr std::array kRainSnowMixCodes{615, 616};
constexpr std::array kSnowShowersCodes{613, 621};
constexpr std::array kThunderstormCodes{200, 201, 202, 230, 231, 232};
constexpr std::array kHeavyThunderstormCodes{210, 211, 212, 221};
constexpr std::array kMistCodes{701, 721, 741, 762};
constexpr std::array kDustStormCodes{731, 751, 761};

[[nodiscard]] Family conditionFamily(int condition_code) {
  if (condition_code == 800) {
    return Family::Clear;
  }
  if (condition_code == 801) {
    return Family::FewClouds;
  }
  if (condition_code == 802) {
    return Family::ScatteredClouds;
  }
  if (condition_code == 803 || condition_code == 804) {
    return Family::BrokenClouds;
  }
  if (std::ranges::any_of(kMistCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::Mist;
  }
  if (condition_code == 711) {
    return Family::Smoke;
  }
  if (std::ranges::any_of(kDustStormCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::DustStorm;
  }
  if (condition_code == 771) {
    return Family::Wind;
  }
  if (condition_code == 781) {
    return Family::Tornado;
  }
  if (condition_code == WeatherIconMapper::kHailConditionCode) {
    return Family::Hail;
  }
  if (condition_code == WeatherIconMapper::kHurricaneConditionCode) {
    return Family::Hurricane;
  }
  if (std::ranges::any_of(kLightDrizzleCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::LightDrizzle;
  }
  if (std::ranges::any_of(kHeavyDrizzleCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::HeavyDrizzle;
  }
  if (std::ranges::any_of(kRainCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::Rain;
  }
  if (std::ranges::any_of(kHeavyRainCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::HeavyRain;
  }
  if (std::ranges::any_of(kFreezingRainCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::FreezingRain;
  }
  if (std::ranges::any_of(kShowersCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::Showers;
  }
  if (std::ranges::any_of(kSnowCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::Snow;
  }
  if (std::ranges::any_of(kHeavySnowCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::HeavySnow;
  }
  if (std::ranges::any_of(kSleetCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::Sleet;
  }
  if (std::ranges::any_of(kRainSnowMixCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::RainSnowMix;
  }
  if (std::ranges::any_of(kSnowShowersCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::SnowShowers;
  }
  if (std::ranges::any_of(kThunderstormCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::Thunderstorm;
  }
  if (std::ranges::any_of(kHeavyThunderstormCodes, [condition_code](int code) { return code == condition_code; })) {
    return Family::HeavyThunderstorm;
  }
  return Family::Unmapped;
}

[[nodiscard]] QList<QString> dayLayers(Family family) {
  switch (family) {
    case Family::Clear:
      return {WeatherIconLayer::kSun.toString()};
    case Family::FewClouds:
      return {WeatherIconLayer::kSun.toString(), WeatherIconLayer::kFewCloudsOverlayDay.toString()};
    case Family::ScatteredClouds:
      return {WeatherIconLayer::kSun.toString(), WeatherIconLayer::kScatteredCloudsDay.toString()};
    case Family::BrokenClouds:
      return {WeatherIconLayer::kSun.toString(), WeatherIconLayer::kBrokenCloudsDay.toString()};
    case Family::Mist:
      return {WeatherIconLayer::kMistDay.toString()};
    case Family::Smoke:
      return {WeatherIconLayer::kSmokeDay.toString()};
    case Family::DustStorm:
      return {WeatherIconLayer::kDustStormDay.toString()};
    case Family::Wind:
      return {WeatherIconLayer::kWindDay.toString()};
    case Family::Tornado:
      return {WeatherIconLayer::kTornadoDay.toString()};
    case Family::LightDrizzle:
      return {WeatherIconLayer::kLightDrizzleDay.toString()};
    case Family::HeavyDrizzle:
      return {WeatherIconLayer::kHeavyDrizzleDay.toString()};
    case Family::Rain:
      return {WeatherIconLayer::kRainDay.toString()};
    case Family::HeavyRain:
      return {WeatherIconLayer::kHeavyRainDay.toString()};
    case Family::FreezingRain:
      return {WeatherIconLayer::kFreezingRainDay.toString()};
    case Family::Hail:
      return {WeatherIconLayer::kHailDay.toString()};
    case Family::Hurricane:
      return {WeatherIconLayer::kHurricaneDay.toString()};
    case Family::Showers:
      return {WeatherIconLayer::kShowersDay.toString()};
    case Family::Snow:
      return {WeatherIconLayer::kSnowDay.toString()};
    case Family::HeavySnow:
      return {WeatherIconLayer::kHeavySnowDay.toString()};
    case Family::Sleet:
      return {WeatherIconLayer::kSleetDay.toString()};
    case Family::RainSnowMix:
      return {WeatherIconLayer::kRainSnowMixDay.toString()};
    case Family::SnowShowers:
      return {WeatherIconLayer::kSnowShowersDay.toString()};
    case Family::Thunderstorm:
      return {WeatherIconLayer::kThunderstormDay.toString()};
    case Family::HeavyThunderstorm:
      return {WeatherIconLayer::kHeavyThunderstormDay.toString()};
    case Family::Unmapped:
      return {};
  }
  return {};
}

[[nodiscard]] QList<QString> nightLayers(Family family, const QString& moon_layer) {
  switch (family) {
    case Family::Clear:
      return {WeatherIconLayer::kStarField.toString(), moon_layer};
    case Family::FewClouds:
      return {WeatherIconLayer::kStarField.toString(), moon_layer, WeatherIconLayer::kFewCloudsOverlayNight.toString()};
    case Family::ScatteredClouds:
      return {WeatherIconLayer::kStarField.toString(), moon_layer, WeatherIconLayer::kScatteredCloudsNight.toString()};
    case Family::BrokenClouds:
      return {WeatherIconLayer::kStarField.toString(), moon_layer, WeatherIconLayer::kBrokenCloudsNight.toString()};
    case Family::Mist:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kMistNight.toString()};
    case Family::Smoke:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kSmokeNight.toString()};
    case Family::DustStorm:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kDustStormNight.toString()};
    case Family::Wind:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kWindNight.toString()};
    case Family::Tornado:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kTornadoNight.toString()};
    case Family::LightDrizzle:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kLightDrizzleNight.toString()};
    case Family::HeavyDrizzle:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kHeavyDrizzleNight.toString()};
    case Family::Rain:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kRainNight.toString()};
    case Family::HeavyRain:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kHeavyRainNight.toString()};
    case Family::FreezingRain:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kFreezingRainNight.toString()};
    case Family::Hail:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kHailNight.toString()};
    case Family::Hurricane:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kHurricaneNight.toString()};
    case Family::Showers:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kShowersNight.toString()};
    case Family::Snow:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kSnowNight.toString()};
    case Family::HeavySnow:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kHeavySnowNight.toString()};
    case Family::Sleet:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kSleetNight.toString()};
    case Family::RainSnowMix:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kRainSnowMixNight.toString()};
    case Family::SnowShowers:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kSnowShowersNight.toString()};
    case Family::Thunderstorm:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kThunderstormNight.toString()};
    case Family::HeavyThunderstorm:
      return {WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kHeavyThunderstormNight.toString()};
    case Family::Unmapped:
      return {};
  }
  return {};
}

}  // namespace

QString WeatherIconMapper::moonLayerName(WeatherIconNs::MoonPhase moon_phase) {
  using WeatherIconNs::MoonPhase;
  switch (moon_phase) {
    case MoonPhase::New:
      return WeatherIconLayer::kMoonNew.toString();
    case MoonPhase::WaxingCrescent:
      return WeatherIconLayer::kMoonWaxingCrescent.toString();
    case MoonPhase::FirstQuarter:
      return WeatherIconLayer::kMoonFirstQuarter.toString();
    case MoonPhase::WaxingGibbous:
      return WeatherIconLayer::kMoonWaxingGibbous.toString();
    case MoonPhase::Full:
      return WeatherIconLayer::kMoonFull.toString();
    case MoonPhase::WaningGibbous:
      return WeatherIconLayer::kMoonWaningGibbous.toString();
    case MoonPhase::LastQuarter:
      return WeatherIconLayer::kMoonLastQuarter.toString();
    case MoonPhase::WaningCrescent:
      return WeatherIconLayer::kMoonWaningCrescent.toString();
  }
  return WeatherIconLayer::kMoonNew.toString();
}

QList<QString> WeatherIconMapper::mapLayers(int condition_code, bool is_day, WeatherIconNs::MoonPhase moon_phase) {
  const Family family = conditionFamily(condition_code);
  if (is_day) {
    return dayLayers(family);
  }
  return nightLayers(family, moonLayerName(moon_phase));
}
