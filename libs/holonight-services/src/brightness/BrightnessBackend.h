#pragma once

#include <QObject>

// Abstract backlight-control backend. SysfsBackend reads /sys/class/backlight and writes via
// logind D-Bus; NullBrightnessBackend is used when no backlight device is present.
class BrightnessBackend : public QObject {
  Q_OBJECT
 public:
  ~BrightnessBackend() override = default;

  BrightnessBackend(const BrightnessBackend&) = delete;
  BrightnessBackend& operator=(const BrightnessBackend&) = delete;
  BrightnessBackend(BrightnessBackend&&) = delete;
  BrightnessBackend& operator=(BrightnessBackend&&) = delete;

  [[nodiscard]] virtual int maxBrightness() const = 0;
  [[nodiscard]] virtual int currentBrightness() const = 0;
  virtual void setBrightness(int value) = 0;

 Q_SIGNALS:
  void brightnessChanged(int new_value);

 protected:
  explicit BrightnessBackend(QObject* parent = nullptr);
};
