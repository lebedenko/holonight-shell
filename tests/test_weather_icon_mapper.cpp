#include "WeatherIconBridge.h"
#include "WeatherIconMapper.h"

#include <QSet>

#include <gtest/gtest.h>

TEST(WeatherIconMapper, PreservesProviderSpecificPhenomena) {
  const QList<QPair<int, QString>> cases{{3, "overcast"},           {51, "drizzle"},
                                         {56, "freezing-drizzle"},  {66, "freezing-rain"},
                                         {96, "thunderstorm-hail"}, {97, "severe-thunderstorm"},
                                         {99, "thunderstorm-hail"}};
  for (const auto& [code, expected] : cases) {
    EXPECT_EQ(WeatherIconMapper::icon("openmeteo", code, true), expected);
    EXPECT_EQ(WeatherIconMapper::icon("openmeteo", code, false), expected);
  }
  EXPECT_EQ(WeatherIconMapper::icon("openweather", 613, false), "sleet");
  for (int code : {202, 221, 232}) EXPECT_EQ(WeatherIconMapper::icon("openweather", code, true), "thunderstorm");
  for (int code : {313, 314, 321}) EXPECT_EQ(WeatherIconMapper::icon("openweather", code, true), "drizzle");
  EXPECT_EQ(WeatherIconMapper::icon("openweather", 212, true), "severe-thunderstorm");
  EXPECT_EQ(WeatherIconMapper::icon("openweather", 762, true), "volcanic-ash");
  EXPECT_EQ(WeatherIconMapper::icon("openweather", 781, true), "tornado");
  EXPECT_EQ(WeatherIconMapper::icon("openweather", 804, true), "overcast");
  EXPECT_EQ(WeatherIconMapper::icon("", 0, true), "unavailable");
  EXPECT_EQ(WeatherIconMapper::icon("openmeteo", -1, true), "unavailable");
  EXPECT_EQ(WeatherIconMapper::icon("openweather", 999, true), "unavailable");
}
TEST(WeatherIconMapper, UnknownDaylightUsesNeutralDryClouds) {
  EXPECT_EQ(WeatherIconMapper::icon("openweather", 800, true, false), "unavailable");
  EXPECT_EQ(WeatherIconMapper::icon("openweather", 801, true, false), "cloudy");
  EXPECT_EQ(WeatherIconMapper::icon("openweather", 802, false, false), "cloudy");
  EXPECT_EQ(WeatherIconMapper::icon("openweather", 803, false), "mostly-cloudy-night");
}
TEST(WeatherIconBridge, CatalogueCoversIndependentCodeListsInOrder) {
  const QList<int> openweather{200, 201, 202, 210, 211, 212, 221, 230, 231, 232, 300, 301, 302, 310,
                               311, 312, 313, 314, 321, 500, 501, 502, 503, 504, 511, 520, 521, 522,
                               531, 600, 601, 602, 611, 612, 613, 615, 616, 620, 621, 622, 701, 711,
                               721, 731, 741, 751, 761, 762, 771, 781, 800, 801, 802, 803, 804};
  const QList<int> openmeteo{0,  1,  2,  3,  45, 48, 51, 53, 55, 56, 57, 61, 63, 65, 66,
                             67, 71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 97, 99};
  WeatherIconBridge bridge;
  const auto cases = bridge.previewCatalogue();
  int index = 0;
  for (const auto& provider : {QString("openweather"), QString("openmeteo")}) {
    for (int code : provider == "openweather" ? openweather : openmeteo) {
      ASSERT_LT(index, cases.size());
      auto row = cases[index++].toMap();
      EXPECT_EQ(row["provider"].toString(), provider);
      EXPECT_EQ(row["code"].toInt(), code);
      EXPECT_TRUE(row["isDay"].toBool());
      const auto day = WeatherIconMapper::icon(provider, code, true);
      const auto night = WeatherIconMapper::icon(provider, code, false);
      const bool variants = provider == "openweather" ? (code >= 800 && code <= 803) : (code >= 0 && code <= 2);
      EXPECT_EQ(day != night, variants);
      if (variants) {
        ASSERT_LT(index, cases.size());
        row = cases[index++].toMap();
        EXPECT_EQ(row["provider"].toString(), provider);
        EXPECT_EQ(row["code"].toInt(), code);
        EXPECT_FALSE(row["isDay"].toBool());
      }
    }
  }
  EXPECT_EQ(index, cases.size());
}
TEST(WeatherIconBridge, UnknownMoonPhaseStaysUnknown) {
  EXPECT_EQ(WeatherIconBridge::moonPhaseIcon({}), "unavailable");
  EXPECT_EQ(WeatherIconBridge::moonPhaseIcon(-1), "unavailable");
  EXPECT_EQ(WeatherIconBridge::moonPhaseDescription({}), "Unknown moon phase");
  EXPECT_EQ(WeatherIconBridge::moonPhaseIcon(0), "moon-new");
}

class MoonPhaseBoundaryTest : public ::testing::TestWithParam<std::pair<double, QLatin1StringView>> {};

TEST_P(MoonPhaseBoundaryTest, MatchesPreRefactorBoundaryBehavior) {
  const auto [input, expected_description] = GetParam();
  EXPECT_EQ(WeatherIconBridge::moonPhaseDescription(QVariant(input)), expected_description.toString())
      << "input=" << input;
}

INSTANTIATE_TEST_SUITE_P(
    BoundaryMatrix, MoonPhaseBoundaryTest,
    ::testing::Values(
        std::pair{-0.01, QLatin1StringView("Unknown moon phase")}, std::pair{0.0, QLatin1StringView("New Moon")},
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
