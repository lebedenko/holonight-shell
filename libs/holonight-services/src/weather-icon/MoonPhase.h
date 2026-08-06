#pragma once

#include <QObject>

#include <cstdint>

namespace WeatherIconNs {
Q_NAMESPACE
enum class MoonPhase : std::uint8_t {
  New,
  WaxingCrescent,
  FirstQuarter,
  WaxingGibbous,
  Full,
  WaningGibbous,
  LastQuarter,
  WaningCrescent,
};
Q_ENUM_NS(MoonPhase)
}  // namespace WeatherIconNs
