#include "MoonPhase.h"
#include "WeatherIconBridge.h"
#include "WeatherIconLayer.h"
#include "WeatherIconMapper.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <gtest/gtest.h>

using WeatherIconNs::MoonPhase;

namespace {

QList<QString> layer(QLatin1StringView name) { return {name.toString()}; }

}  // namespace

// --- condition 800 (clear): all 16 day/night x phase combinations ---

TEST(WeatherIconMapper, ClearDayReturnsSunOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(800, true, MoonPhase::New), layer(WeatherIconLayer::kSun));
}

class ClearNightPhaseTest : public ::testing::TestWithParam<std::pair<MoonPhase, QLatin1StringView>> {};

TEST_P(ClearNightPhaseTest, ReturnsStarFieldAndMoon) {
  const auto [phase, moon_layer] = GetParam();
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(), moon_layer.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(800, false, phase), expected);
}

INSTANTIATE_TEST_SUITE_P(AllPhases, ClearNightPhaseTest,
                         ::testing::Values(std::pair{MoonPhase::New, WeatherIconLayer::kMoonNew},
                                           std::pair{MoonPhase::WaxingCrescent, WeatherIconLayer::kMoonWaxingCrescent},
                                           std::pair{MoonPhase::FirstQuarter, WeatherIconLayer::kMoonFirstQuarter},
                                           std::pair{MoonPhase::WaxingGibbous, WeatherIconLayer::kMoonWaxingGibbous},
                                           std::pair{MoonPhase::Full, WeatherIconLayer::kMoonFull},
                                           std::pair{MoonPhase::WaningGibbous, WeatherIconLayer::kMoonWaningGibbous},
                                           std::pair{MoonPhase::LastQuarter, WeatherIconLayer::kMoonLastQuarter},
                                           std::pair{MoonPhase::WaningCrescent,
                                                     WeatherIconLayer::kMoonWaningCrescent}));

TEST(WeatherIconMapper, AllEightNightPhasesAreDistinctAndDayIsSeparate) {
  // 8 night results (one per phase) must all differ from each other and from the day result.
  const QList<QString> day_result = WeatherIconMapper::mapLayers(800, true, MoonPhase::New);

  QList<QList<QString>> night_results;
  for (int phase_index = 0; phase_index < 8; ++phase_index) {
    night_results.append(WeatherIconMapper::mapLayers(800, false, static_cast<MoonPhase>(phase_index)));
  }

  for (qsizetype i = 0; i < night_results.size(); ++i) {
    EXPECT_NE(night_results.at(i), day_result) << "night phase " << i << " collides with day result";
    for (qsizetype j = i + 1; j < night_results.size(); ++j) {
      EXPECT_NE(night_results.at(i), night_results.at(j)) << "night phases " << i << " and " << j << " collide";
    }
  }
}

// --- few clouds (801) ---

TEST(WeatherIconMapper, FewCloudsDay) {
  const QList<QString> expected{WeatherIconLayer::kSun.toString(), WeatherIconLayer::kFewCloudsOverlayDay.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(801, true, MoonPhase::New), expected);
}

TEST(WeatherIconMapper, FewCloudsNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kMoonNew.toString(),
                                WeatherIconLayer::kFewCloudsOverlayNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(801, false, MoonPhase::New), expected);
}

// --- scattered clouds (802) ---

TEST(WeatherIconMapper, ScatteredCloudsDay) {
  const QList<QString> expected{WeatherIconLayer::kSun.toString(), WeatherIconLayer::kScatteredCloudsDay.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(802, true, MoonPhase::New), expected);
}

TEST(WeatherIconMapper, ScatteredCloudsNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(),
                                WeatherIconLayer::kMoonWaxingCrescent.toString(),
                                WeatherIconLayer::kScatteredCloudsNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(802, false, MoonPhase::WaxingCrescent), expected);
}

// --- broken/overcast clouds (803, 804) share one overlay ---

TEST(WeatherIconMapper, BrokenAndOvercastCloudsProduceIdenticalSequences) {
  const QList<QString> day_803 = WeatherIconMapper::mapLayers(803, true, MoonPhase::New);
  const QList<QString> day_804 = WeatherIconMapper::mapLayers(804, true, MoonPhase::New);
  EXPECT_EQ(day_803, day_804);

  const QList<QString> night_803 = WeatherIconMapper::mapLayers(803, false, MoonPhase::Full);
  const QList<QString> night_804 = WeatherIconMapper::mapLayers(804, false, MoonPhase::Full);
  EXPECT_EQ(night_803, night_804);
}

TEST(WeatherIconMapper, BrokenCloudsDayLayers) {
  const QList<QString> expected{WeatherIconLayer::kSun.toString(), WeatherIconLayer::kBrokenCloudsDay.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(803, true, MoonPhase::New), expected);
}

// --- light drizzle family: 300, 301, 310, 313, 315 ---

class LightDrizzleCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(LightDrizzleCodeTest, DayReturnsLightDrizzleOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kLightDrizzleDay));
}

INSTANTIATE_TEST_SUITE_P(BoundaryAndAllCodes, LightDrizzleCodeTest, ::testing::Values(300, 301, 310, 313, 315));

TEST(WeatherIconMapper, LightDrizzleNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(),
                                WeatherIconLayer::kLightDrizzleNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(300, false, MoonPhase::New), expected);
}

// --- heavy drizzle family: 302, 311, 312, 314 ---

class HeavyDrizzleCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(HeavyDrizzleCodeTest, DayReturnsHeavyDrizzleOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kHeavyDrizzleDay));
}

INSTANTIATE_TEST_SUITE_P(AllCodes, HeavyDrizzleCodeTest, ::testing::Values(302, 311, 312, 314));

TEST(WeatherIconMapper, HeavyDrizzleNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(),
                                WeatherIconLayer::kHeavyDrizzleNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(302, false, MoonPhase::Full), expected);
}

// --- rain family: 500, 501 ---

class RainCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(RainCodeTest, DayReturnsRainOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kRainDay));
}

INSTANTIATE_TEST_SUITE_P(BoundaryAndAllCodes, RainCodeTest, ::testing::Values(500, 501));

TEST(WeatherIconMapper, RainNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kRainNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(500, false, MoonPhase::WaxingCrescent), expected);
}

// --- heavy rain family: 502, 503, 504 ---

class HeavyRainCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(HeavyRainCodeTest, DayReturnsHeavyRainOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kHeavyRainDay));
}

INSTANTIATE_TEST_SUITE_P(AllCodes, HeavyRainCodeTest, ::testing::Values(502, 503, 504));

TEST(WeatherIconMapper, HeavyRainNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kHeavyRainNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(502, false, MoonPhase::Full), expected);
}

// --- freezing rain family: 511 ---

class FreezingRainCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(FreezingRainCodeTest, DayReturnsFreezingRainOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kFreezingRainDay));
}

INSTANTIATE_TEST_SUITE_P(AllCodes, FreezingRainCodeTest, ::testing::Values(511));

TEST(WeatherIconMapper, FreezingRainNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(),
                                WeatherIconLayer::kFreezingRainNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(511, false, MoonPhase::New), expected);
}

// --- presentation-only severe weather overrides ---

TEST(WeatherIconMapper, HailLayers) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(WeatherIconMapper::kHailConditionCode, true, MoonPhase::New),
            layer(WeatherIconLayer::kHailDay));

  const QList<QString> expected_night{WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kHailNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(WeatherIconMapper::kHailConditionCode, false, MoonPhase::Full),
            expected_night);
}

TEST(WeatherIconMapper, HurricaneLayers) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(WeatherIconMapper::kHurricaneConditionCode, true, MoonPhase::New),
            layer(WeatherIconLayer::kHurricaneDay));

  const QList<QString> expected_night{WeatherIconLayer::kStarField.toString(),
                                      WeatherIconLayer::kHurricaneNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(WeatherIconMapper::kHurricaneConditionCode, false, MoonPhase::Full),
            expected_night);
}

TEST(WeatherIconBridge, DescriptionContainingHailOverridesConditionCode) {
  EXPECT_EQ(WeatherIconBridge::layersForWeather(800, true, QDateTime{}, QStringLiteral("thunderstorm with hail"), 0),
            layer(WeatherIconLayer::kHailDay));

  const QList<QString> expected_night{WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kHailNight.toString()};
  EXPECT_EQ(WeatherIconBridge::layersForWeather(800, false, QDateTime{}, QStringLiteral("HAIL showers"), 0),
            expected_night);
}

TEST(WeatherIconBridge, HurricaneForceWindOverridesConditionCode) {
  EXPECT_EQ(WeatherIconBridge::layersForWeather(800, true, QDateTime{}, QStringLiteral("clear sky"), 119),
            layer(WeatherIconLayer::kHurricaneDay));

  const QList<QString> expected_night{WeatherIconLayer::kStarField.toString(),
                                      WeatherIconLayer::kHurricaneNight.toString()};
  EXPECT_EQ(WeatherIconBridge::layersForWeather(800, false, QDateTime{}, QStringLiteral("clear sky"), 120),
            expected_night);
}

TEST(WeatherIconBridge, HailDescriptionTakesPriorityOverWindHeuristic) {
  EXPECT_EQ(WeatherIconBridge::layersForWeather(800, true, QDateTime{}, QStringLiteral("hail"), 119),
            layer(WeatherIconLayer::kHailDay));
}

// --- showers family: 321, 520, 521, 522, 531 ---

class ShowersCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(ShowersCodeTest, DayReturnsShowersOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kShowersDay));
}

INSTANTIATE_TEST_SUITE_P(AllCodes, ShowersCodeTest, ::testing::Values(321, 520, 521, 522, 531));

TEST(WeatherIconMapper, ShowersNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kShowersNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(520, false, MoonPhase::FirstQuarter), expected);
}

// --- snow family: 600, 601, 620 ---

class SnowCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(SnowCodeTest, DayReturnsSnowOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kSnowDay));
}

INSTANTIATE_TEST_SUITE_P(AllCodes, SnowCodeTest, ::testing::Values(600, 601, 620));

TEST(WeatherIconMapper, SnowNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kSnowNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(600, false, MoonPhase::WaxingGibbous), expected);
}

// --- heavy snow family: 602, 622 ---

class HeavySnowCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(HeavySnowCodeTest, DayReturnsHeavySnowOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kHeavySnowDay));
}

INSTANTIATE_TEST_SUITE_P(AllCodes, HeavySnowCodeTest, ::testing::Values(602, 622));

TEST(WeatherIconMapper, HeavySnowNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kHeavySnowNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(602, false, MoonPhase::WaningGibbous), expected);
}

// --- sleet family: 611, 612 ---

class SleetCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(SleetCodeTest, DayReturnsSleetOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kSleetDay));
}

INSTANTIATE_TEST_SUITE_P(AllCodes, SleetCodeTest, ::testing::Values(611, 612));

TEST(WeatherIconMapper, SleetNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kSleetNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(611, false, MoonPhase::LastQuarter), expected);
}

// --- rain & snow mix family: 615, 616 ---

class RainSnowMixCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(RainSnowMixCodeTest, DayReturnsRainSnowMixOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kRainSnowMixDay));
}

INSTANTIATE_TEST_SUITE_P(AllCodes, RainSnowMixCodeTest, ::testing::Values(615, 616));

TEST(WeatherIconMapper, RainSnowMixNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(),
                                WeatherIconLayer::kRainSnowMixNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(615, false, MoonPhase::WaningCrescent), expected);
}

// --- snow showers family: 613, 621 ---

class SnowShowersCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(SnowShowersCodeTest, DayReturnsSnowShowersOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kSnowShowersDay));
}

INSTANTIATE_TEST_SUITE_P(AllCodes, SnowShowersCodeTest, ::testing::Values(613, 621));

TEST(WeatherIconMapper, SnowShowersNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(),
                                WeatherIconLayer::kSnowShowersNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(613, false, MoonPhase::New), expected);
}

// --- thunderstorm family: 200, 201, 202, 230, 231, 232 ---

class ThunderstormCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(ThunderstormCodeTest, DayReturnsThunderstormOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kThunderstormDay));
}

INSTANTIATE_TEST_SUITE_P(AllCodes, ThunderstormCodeTest, ::testing::Values(200, 201, 202, 230, 231, 232));

TEST(WeatherIconMapper, ThunderstormNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(),
                                WeatherIconLayer::kThunderstormNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(200, false, MoonPhase::WaxingCrescent), expected);
}

// --- heavy thunderstorm family: 210, 211, 212, 221 ---

class HeavyThunderstormCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(HeavyThunderstormCodeTest, DayReturnsHeavyThunderstormOnly) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New),
            layer(WeatherIconLayer::kHeavyThunderstormDay));
}

INSTANTIATE_TEST_SUITE_P(AllCodes, HeavyThunderstormCodeTest, ::testing::Values(210, 211, 212, 221));

TEST(WeatherIconMapper, HeavyThunderstormNight) {
  const QList<QString> expected{WeatherIconLayer::kStarField.toString(),
                                WeatherIconLayer::kHeavyThunderstormNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(210, false, MoonPhase::Full), expected);
}

// --- atmosphere family: 7xx ---

class MistCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(MistCodeTest, MapsToMistLayers) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kMistDay));

  const QList<QString> expected_night{WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kMistNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), false, MoonPhase::Full), expected_night);
}

INSTANTIATE_TEST_SUITE_P(AllCodes, MistCodeTest, ::testing::Values(701, 721, 741, 762));

TEST(WeatherIconMapper, SmokeLayers) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(711, true, MoonPhase::New), layer(WeatherIconLayer::kSmokeDay));

  const QList<QString> expected_night{WeatherIconLayer::kStarField.toString(),
                                      WeatherIconLayer::kSmokeNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(711, false, MoonPhase::Full), expected_night);
}

class DustStormCodeTest : public ::testing::TestWithParam<int> {};

TEST_P(DustStormCodeTest, MapsToDustStormLayers) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), true, MoonPhase::New), layer(WeatherIconLayer::kDustStormDay));

  const QList<QString> expected_night{WeatherIconLayer::kStarField.toString(),
                                      WeatherIconLayer::kDustStormNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(GetParam(), false, MoonPhase::Full), expected_night);
}

INSTANTIATE_TEST_SUITE_P(AllCodes, DustStormCodeTest, ::testing::Values(731, 751, 761));

TEST(WeatherIconMapper, WindLayers) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(771, true, MoonPhase::New), layer(WeatherIconLayer::kWindDay));

  const QList<QString> expected_night{WeatherIconLayer::kStarField.toString(), WeatherIconLayer::kWindNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(771, false, MoonPhase::Full), expected_night);
}

TEST(WeatherIconMapper, TornadoLayers) {
  EXPECT_EQ(WeatherIconMapper::mapLayers(781, true, MoonPhase::New), layer(WeatherIconLayer::kTornadoDay));

  const QList<QString> expected_night{WeatherIconLayer::kStarField.toString(),
                                      WeatherIconLayer::kTornadoNight.toString()};
  EXPECT_EQ(WeatherIconMapper::mapLayers(781, false, MoonPhase::Full), expected_night);
}

TEST(WeatherIconMapper, AllOpenWeatherConditionCodesReturnLayers) {
  const QList<int> official_codes{200, 201, 202, 210, 211, 212, 221, 230, 231, 232, 300, 301, 302, 310,
                                  311, 312, 313, 314, 321, 500, 501, 502, 503, 504, 511, 520, 521, 522,
                                  531, 600, 601, 602, 611, 612, 613, 615, 616, 620, 621, 622, 701, 711,
                                  721, 731, 741, 751, 761, 762, 771, 781, 800, 801, 802, 803, 804};

  for (int code : official_codes) {
    EXPECT_FALSE(WeatherIconMapper::mapLayers(code, true, MoonPhase::New).isEmpty()) << "day code " << code;
    EXPECT_FALSE(WeatherIconMapper::mapLayers(code, false, MoonPhase::Full).isEmpty()) << "night code " << code;
  }
}

TEST(WeatherIconMapper, UnknownConditionReturnsEmptyList) {
  EXPECT_TRUE(WeatherIconMapper::mapLayers(9999, true, MoonPhase::New).isEmpty());
  EXPECT_TRUE(WeatherIconMapper::mapLayers(9999, false, MoonPhase::Full).isEmpty());
}

// T-023: WeatherIconBridge::variantToMoonPhase() boundary matrix (via the moonPhaseDescription()
// public surface, which maps 1:1 to MoonPhase). Expected values captured from the pre-refactor
// literal-boundary implementation before F-2.17's constant extraction, per DESIGN.md §4.4's
// "STRICT no-output-change" mandate — every branch and every named boundary is covered so a
// transcription error (e.g. `<=` swapped for `<`) fails this test.
class MoonPhaseBoundaryTest : public ::testing::TestWithParam<std::pair<double, QLatin1StringView>> {};

TEST_P(MoonPhaseBoundaryTest, MatchesPreRefactorBoundaryBehavior) {
  const auto [input, expected_description] = GetParam();
  EXPECT_EQ(WeatherIconBridge::moonPhaseDescription(QVariant(input)), expected_description.toString())
      << "input=" << input;
}

INSTANTIATE_TEST_SUITE_P(
    BoundaryMatrix, MoonPhaseBoundaryTest,
    ::testing::Values(
        std::pair{-0.01, QLatin1StringView("New Moon")}, std::pair{0.0, QLatin1StringView("New Moon")},
        std::pair{0.009, QLatin1StringView("New Moon")}, std::pair{0.01, QLatin1StringView("Waxing Crescent")},
        std::pair{0.239, QLatin1StringView("Waxing Crescent")}, std::pair{0.24, QLatin1StringView("First Quarter")},
        std::pair{0.25, QLatin1StringView("First Quarter")}, std::pair{0.26, QLatin1StringView("First Quarter")},
        std::pair{0.261, QLatin1StringView("Waxing Gibbous")}, std::pair{0.489, QLatin1StringView("Waxing Gibbous")},
        std::pair{0.49, QLatin1StringView("Full Moon")}, std::pair{0.5, QLatin1StringView("Full Moon")},
        std::pair{0.51, QLatin1StringView("Full Moon")}, std::pair{0.511, QLatin1StringView("Waning Gibbous")},
        std::pair{0.739, QLatin1StringView("Waning Gibbous")}, std::pair{0.74, QLatin1StringView("Last Quarter")},
        std::pair{0.75, QLatin1StringView("Last Quarter")}, std::pair{0.76, QLatin1StringView("Last Quarter")},
        std::pair{0.761, QLatin1StringView("Waning Crescent")}, std::pair{0.99, QLatin1StringView("Waning Crescent")},
        std::pair{0.991, QLatin1StringView("New Moon")}, std::pair{1.0, QLatin1StringView("New Moon")}));
