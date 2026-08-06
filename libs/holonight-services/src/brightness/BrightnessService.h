#pragma once

#include <QObject>
#include <QQmlEngine>

#include <memory>

class BrightnessBackend;

// QML singleton. Owns the brightness backend, exposes current brightness as a 0-100 percentage,
// and relays external changes (Fn keys via inotify) back to the slider.
class BrightnessService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(int maxBrightness READ maxBrightness CONSTANT FINAL)
  Q_PROPERTY(int brightnessPercent READ brightnessPercent NOTIFY brightnessPercentChanged FINAL)
  Q_PROPERTY(bool hasBacklight READ hasBacklight CONSTANT FINAL)

 public:
  // Production constructor: auto-selects sysfs backlight device; falls back to NullBrightnessBackend.
  explicit BrightnessService(QObject* parent = nullptr);
  // Test seam: inject a pre-built backend directly.
  explicit BrightnessService(std::unique_ptr<BrightnessBackend> backend, QObject* parent = nullptr);
  ~BrightnessService() override;

  BrightnessService(const BrightnessService&) = delete;
  BrightnessService& operator=(const BrightnessService&) = delete;
  BrightnessService(BrightnessService&&) = delete;
  BrightnessService& operator=(BrightnessService&&) = delete;

  [[nodiscard]] int maxBrightness() const { return max_brightness_; }
  [[nodiscard]] int brightnessPercent() const { return brightness_percent_; }
  [[nodiscard]] bool hasBacklight() const { return max_brightness_ > 0; }

  Q_INVOKABLE void setBrightnessPercent(int percent);

 Q_SIGNALS:
  void brightnessPercentChanged(int percent);

 private Q_SLOTS:
  void onBrightnessChanged(int new_raw);

 private:
  void init();
  [[nodiscard]] int computePercent(int raw) const;

  std::unique_ptr<BrightnessBackend> backend_;
  int max_brightness_{0};
  int brightness_percent_{0};
};
