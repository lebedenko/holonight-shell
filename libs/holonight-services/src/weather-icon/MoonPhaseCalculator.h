#pragma once

#include "MoonPhase.h"

#include <chrono>

// Pure synodic-month moon-phase calculation from a timestamp. No QML/Quick/GUI dependencies,
// no system clock access, no network. See REQ-F-MP-001..003, REQ-C-NONET, REQ-C-NOQML-C++.
class MoonPhaseCalculator {
 public:
  MoonPhaseCalculator() = delete;

  // Computes lunar phase from days elapsed since the reference new moon (2000-01-06 18:14 UTC),
  // modulo the synodic month (~29.530589 days), bucketed into 8 equal-width windows.
  [[nodiscard]] static WeatherIconNs::MoonPhase phaseForDate(std::chrono::system_clock::time_point timestamp);
};
