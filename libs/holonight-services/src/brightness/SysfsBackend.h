#pragma once

#include "BrightnessBackend.h"

#include <QString>

#include <memory>

class QSocketNotifier;

// Production backend. Selects the /sys/class/backlight/ device with the highest max_brightness,
// reads brightness via sysfs, writes via logind D-Bus SetBrightness, and tracks external changes
// (Fn keys) via inotify IN_MODIFY on the brightness sysfs file.
class SysfsBackend final : public BrightnessBackend {
  Q_OBJECT
 public:
  explicit SysfsBackend(QObject* parent = nullptr);
  ~SysfsBackend() override;

  SysfsBackend(const SysfsBackend&) = delete;
  SysfsBackend& operator=(const SysfsBackend&) = delete;
  SysfsBackend(SysfsBackend&&) = delete;
  SysfsBackend& operator=(SysfsBackend&&) = delete;

  [[nodiscard]] int maxBrightness() const override { return max_brightness_; }
  [[nodiscard]] int currentBrightness() const override;
  void setBrightness(int value) override;

 private Q_SLOTS:
  void onInotifyEvent();

 private:
  void selectDevice();
  void resolveSessionPath();
  void setupInotify();
  [[nodiscard]] int readBrightness() const;

  QString device_name_;
  QString brightness_path_;
  int max_brightness_{0};
  QString session_path_;
  int ifd_{-1};
  int iwd_{-1};
  // notifier_ must be declared after ifd_/iwd_ so its destructor fires first,
  // disabling the notifier before close(ifd_) is called.
  std::unique_ptr<QSocketNotifier> notifier_;
};
