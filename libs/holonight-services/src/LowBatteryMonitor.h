#pragma once

#include <QObject>

class BatteryService;

// Fires one-shot desktop notifications when battery percent crosses warning/critical thresholds
// while discharging. Resets per-cycle dedup state when the battery starts charging or is
// inserted/removed. Not a QML singleton — pure C++ wired in ShellApplication.
class LowBatteryMonitor : public QObject {
  Q_OBJECT

 public:
  explicit LowBatteryMonitor(BatteryService* battery, QObject* parent = nullptr);
  ~LowBatteryMonitor() override = default;

  LowBatteryMonitor(const LowBatteryMonitor&) = delete;
  LowBatteryMonitor& operator=(const LowBatteryMonitor&) = delete;
  LowBatteryMonitor(LowBatteryMonitor&&) = delete;
  LowBatteryMonitor& operator=(LowBatteryMonitor&&) = delete;

  // Test seam: inject thresholds directly, bypassing QSettings.
  void setThresholds(int warning_pct, int critical_pct);

  [[nodiscard]] int warningThreshold() const { return warning_threshold_; }
  [[nodiscard]] int criticalThreshold() const { return critical_threshold_; }

 private Q_SLOTS:
  void onPercentChanged();
  void onChargingChanged();
  void onDischargingChanged();
  void onFullyChargedChanged();
  void onPresentChanged();

 protected:
  virtual void sendNotification(const QString& summary, const QString& body, int urgency, int expire_timeout_ms);

 private:
  void resetState();
  void checkThresholds();

  BatteryService* battery_;
  int warning_threshold_{20};
  int critical_threshold_{10};
  bool warning_sent_{false};
  bool critical_sent_{false};
};
