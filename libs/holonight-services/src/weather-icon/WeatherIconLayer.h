#pragma once

#include <QLatin1StringView>

// Layer basenames (no extension, no path) returned by WeatherIconMapper::mapLayers.
// Shared between the mapper implementation and its unit tests to avoid duplicated literals.
namespace WeatherIconLayer {
inline constexpr QLatin1StringView kSun{"sun"};
inline constexpr QLatin1StringView kStarField{"star-field"};
inline constexpr QLatin1StringView kBrokenCloudsDay{"broken-clouds-day"};
inline constexpr QLatin1StringView kBrokenCloudsNight{"broken-clouds-night"};
inline constexpr QLatin1StringView kFewCloudsOverlayDay{"few-clouds-overlay-day"};
inline constexpr QLatin1StringView kFewCloudsOverlayNight{"few-clouds-overlay-night"};
inline constexpr QLatin1StringView kScatteredCloudsDay{"scattered-clouds-day"};
inline constexpr QLatin1StringView kScatteredCloudsNight{"scattered-clouds-night"};
inline constexpr QLatin1StringView kDustStormDay{"dust-storm-day"};
inline constexpr QLatin1StringView kDustStormNight{"dust-storm-night"};
inline constexpr QLatin1StringView kHeavyDrizzleDay{"heavy-drizzle-day"};
inline constexpr QLatin1StringView kHeavyDrizzleNight{"heavy-drizzle-night"};
inline constexpr QLatin1StringView kLightDrizzleDay{"light-drizzle-day"};
inline constexpr QLatin1StringView kLightDrizzleNight{"light-drizzle-night"};
inline constexpr QLatin1StringView kMistDay{"mist-day"};
inline constexpr QLatin1StringView kMistNight{"mist-night"};
inline constexpr QLatin1StringView kRainDay{"rain-day"};
inline constexpr QLatin1StringView kRainNight{"rain-night"};
inline constexpr QLatin1StringView kFreezingRainDay{"freezing-rain-day"};
inline constexpr QLatin1StringView kFreezingRainNight{"freezing-rain-night"};
inline constexpr QLatin1StringView kHailDay{"hail-day"};
inline constexpr QLatin1StringView kHailNight{"hail-night"};
inline constexpr QLatin1StringView kHeavyRainDay{"heavy-rain-day"};
inline constexpr QLatin1StringView kHeavyRainNight{"heavy-rain-night"};
inline constexpr QLatin1StringView kHeavySnowDay{"heavy-snow-day"};
inline constexpr QLatin1StringView kHeavySnowNight{"heavy-snow-night"};
inline constexpr QLatin1StringView kHeavyThunderstormDay{"heavy-thunderstorm-day"};
inline constexpr QLatin1StringView kHeavyThunderstormNight{"heavy-thunderstorm-night"};
inline constexpr QLatin1StringView kHurricaneDay{"hurricane-day"};
inline constexpr QLatin1StringView kHurricaneNight{"hurricane-night"};
inline constexpr QLatin1StringView kRainSnowMixDay{"rain-snow-mix-day"};
inline constexpr QLatin1StringView kRainSnowMixNight{"rain-snow-mix-night"};
inline constexpr QLatin1StringView kShowersDay{"showers-day"};
inline constexpr QLatin1StringView kShowersNight{"showers-night"};
inline constexpr QLatin1StringView kSleetDay{"sleet-day"};
inline constexpr QLatin1StringView kSleetNight{"sleet-night"};
inline constexpr QLatin1StringView kSmokeDay{"smoke-day"};
inline constexpr QLatin1StringView kSmokeNight{"smoke-night"};
inline constexpr QLatin1StringView kSnowDay{"snow-day"};
inline constexpr QLatin1StringView kSnowNight{"snow-night"};
inline constexpr QLatin1StringView kSnowShowersDay{"snow-showers-day"};
inline constexpr QLatin1StringView kSnowShowersNight{"snow-showers-night"};
inline constexpr QLatin1StringView kThunderstormDay{"thunderstorm-day"};
inline constexpr QLatin1StringView kThunderstormNight{"thunderstorm-night"};
inline constexpr QLatin1StringView kTornadoDay{"tornado-day"};
inline constexpr QLatin1StringView kTornadoNight{"tornado-night"};
inline constexpr QLatin1StringView kWindDay{"wind-day"};
inline constexpr QLatin1StringView kWindNight{"wind-night"};
// Moon phase basenames keyed by MoonPhase enum value — see WeatherIconMapper.cpp for the lookup.
inline constexpr QLatin1StringView kMoonNew{"moon-new"};
inline constexpr QLatin1StringView kMoonWaxingCrescent{"moon-waxing-crescent"};
inline constexpr QLatin1StringView kMoonFirstQuarter{"moon-first-quarter"};
inline constexpr QLatin1StringView kMoonWaxingGibbous{"moon-waxing-gibbous"};
inline constexpr QLatin1StringView kMoonFull{"moon-full"};
inline constexpr QLatin1StringView kMoonWaningGibbous{"moon-waning-gibbous"};
inline constexpr QLatin1StringView kMoonLastQuarter{"moon-last-quarter"};
inline constexpr QLatin1StringView kMoonWaningCrescent{"moon-waning-crescent"};
}  // namespace WeatherIconLayer
