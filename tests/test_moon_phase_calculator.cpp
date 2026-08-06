#include "MoonPhase.h"
#include "MoonPhaseCalculator.h"

#include <chrono>
#include <gtest/gtest.h>

using WeatherIconNs::MoonPhase;

namespace {

constexpr std::chrono::system_clock::time_point kReferenceNewMoon =
    std::chrono::sys_days{std::chrono::year{2000} / std::chrono::January / 6} + std::chrono::hours{18} +
    std::chrono::minutes{14};

std::chrono::system_clock::time_point daysAfterReference(double days) {
  const auto offset =
      std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::duration<double>{days * 86400.0});
  return kReferenceNewMoon + offset;
}

}  // namespace

TEST(MoonPhaseCalculator, ReferenceEpochYieldsNew) {
  EXPECT_EQ(MoonPhaseCalculator::phaseForDate(kReferenceNewMoon), MoonPhase::New);
}

TEST(MoonPhaseCalculator, WaxingCrescentMidWindow) {
  EXPECT_EQ(MoonPhaseCalculator::phaseForDate(daysAfterReference(5.54)), MoonPhase::WaxingCrescent);
}

TEST(MoonPhaseCalculator, FirstQuarterMidWindow) {
  EXPECT_EQ(MoonPhaseCalculator::phaseForDate(daysAfterReference(9.23)), MoonPhase::FirstQuarter);
}

TEST(MoonPhaseCalculator, WaxingGibbousMidWindow) {
  EXPECT_EQ(MoonPhaseCalculator::phaseForDate(daysAfterReference(12.92)), MoonPhase::WaxingGibbous);
}

TEST(MoonPhaseCalculator, FullMidWindow) {
  // Window 4 of 8: [4 * 29.530589/8, 5 * 29.530589/8) ~= [14.765, 18.457). Midpoint ~16.61.
  EXPECT_EQ(MoonPhaseCalculator::phaseForDate(daysAfterReference(16.6)), MoonPhase::Full);
}

TEST(MoonPhaseCalculator, WaningGibbousMidWindow) {
  // Window 5: [18.457, 22.148). Midpoint ~20.3.
  EXPECT_EQ(MoonPhaseCalculator::phaseForDate(daysAfterReference(20.3)), MoonPhase::WaningGibbous);
}

TEST(MoonPhaseCalculator, LastQuarterMidWindow) {
  // Window 6: [22.148, 25.839). Midpoint ~24.0.
  EXPECT_EQ(MoonPhaseCalculator::phaseForDate(daysAfterReference(24.0)), MoonPhase::LastQuarter);
}

TEST(MoonPhaseCalculator, WaningCrescentMidWindow) {
  // Window 7: [25.839, 29.530589). Midpoint ~27.7.
  EXPECT_EQ(MoonPhaseCalculator::phaseForDate(daysAfterReference(27.7)), MoonPhase::WaningCrescent);
}

TEST(MoonPhaseCalculator, WrapsAtSynodicMonthBoundary) {
  EXPECT_EQ(MoonPhaseCalculator::phaseForDate(daysAfterReference(29.530589)), MoonPhase::New);
}

TEST(MoonPhaseCalculator, WrapsAtMultipleSynodicMonths) {
  EXPECT_EQ(MoonPhaseCalculator::phaseForDate(daysAfterReference(29.530589 * 3)), MoonPhase::New);
}

TEST(MoonPhaseCalculator, DeterministicForRepeatedCalls) {
  const auto timestamp = daysAfterReference(16.6);
  EXPECT_EQ(MoonPhaseCalculator::phaseForDate(timestamp), MoonPhaseCalculator::phaseForDate(timestamp));
}

TEST(MoonPhaseCalculator, HandlesTimestampsBeforeEpoch) {
  // 5.54 days before reference wraps to (29.530589 - 5.54) = 23.99 days into the previous cycle,
  // landing in window 6 (LastQuarter, [22.1479, 25.8392)) — comfortably clear of either boundary.
  const MoonPhase phase = MoonPhaseCalculator::phaseForDate(daysAfterReference(-5.54));
  EXPECT_EQ(phase, MoonPhase::LastQuarter);
}

TEST(MoonPhaseCalculator, HandlesTimestampsManyCyclesBeforeEpoch) {
  const MoonPhase phase = MoonPhaseCalculator::phaseForDate(daysAfterReference(-29.530589 * 5));
  EXPECT_EQ(phase, MoonPhase::New);
}
