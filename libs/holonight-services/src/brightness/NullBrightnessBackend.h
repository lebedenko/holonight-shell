#pragma once

#include "BrightnessBackend.h"

// Fallback backend used when /sys/class/backlight/ has no devices.
// All reads return 0; setBrightness is a no-op; brightnessChanged is never emitted.
class NullBrightnessBackend final : public BrightnessBackend {
  Q_OBJECT
 public:
  explicit NullBrightnessBackend(QObject* parent = nullptr) : BrightnessBackend(parent) {}

  [[nodiscard]] int maxBrightness() const override { return 0; }
  [[nodiscard]] int currentBrightness() const override { return 0; }
  void setBrightness(int /*value*/) override {}
};
