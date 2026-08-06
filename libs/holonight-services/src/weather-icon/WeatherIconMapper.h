#pragma once

#include "MoonPhase.h"

#include <QList>
#include <QString>

// Pure condition-code + day/night + moon-phase -> ordered layer-basename mapping.
// No QML/Quick/GUI dependencies (links Qt6::Core only). See REQ-F-LM-001..003, REQ-C-NOQML-C++.
class WeatherIconMapper {
 public:
  static constexpr int kHailConditionCode = -10001;
  static constexpr int kHurricaneConditionCode = -10002;

  WeatherIconMapper() = delete;

  // Returns layer basenames (no extension, no path) bottom-to-top for the given OWM condition
  // code. moon_phase is consulted only when is_day is false. Unmapped condition codes (511, 2xx,
  // 6xx, 7xx) yield an empty list (REQ-F-LM-002).
  [[nodiscard]] static QList<QString> mapLayers(int condition_code, bool is_day, WeatherIconNs::MoonPhase moon_phase);

  [[nodiscard]] static QString moonLayerName(WeatherIconNs::MoonPhase moon_phase);
};
