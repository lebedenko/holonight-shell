#include "MoonPhaseCalculator.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kSynodicMonthDays{29.530589};

// 2000-01-06 18:14:00 UTC — a known new moon, used as the calculation reference epoch.
const std::chrono::system_clock::time_point kReferenceNewMoon =
    std::chrono::sys_days{std::chrono::year{2000} / std::chrono::January / 6} + std::chrono::hours{18} +
    std::chrono::minutes{14};

[[nodiscard]] double daysElapsedSinceReference(std::chrono::system_clock::time_point timestamp) {
  const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(timestamp - kReferenceNewMoon);
  return elapsed.count() / 86400.0;
}

[[nodiscard]] int normalizedPhaseIndex(double days_elapsed) {
  double cycle_position = std::fmod(days_elapsed, kSynodicMonthDays);
  if (cycle_position < 0.0) {
    cycle_position += kSynodicMonthDays;
  }
  const int index = static_cast<int>(std::floor(cycle_position / (kSynodicMonthDays / 8.0)));
  return std::min(index, 7);
}

}  // namespace

WeatherIconNs::MoonPhase MoonPhaseCalculator::phaseForDate(std::chrono::system_clock::time_point timestamp) {
  const double days_elapsed = daysElapsedSinceReference(timestamp);
  const int index = normalizedPhaseIndex(days_elapsed);
  return static_cast<WeatherIconNs::MoonPhase>(index);
}
