#include "LowBatteryMonitor.h"

#include "BatteryService.h"

#include <QDBusInterface>
#include <QLoggingCategory>
#include <QSettings>
#include <QVariantMap>

Q_LOGGING_CATEGORY(lcLowBattery, "holonight.lowbattery")

static constexpr int kDefaultWarningThreshold{20};
static constexpr int kDefaultCriticalThreshold{10};
static constexpr int kWarningTimeoutMs{10000};
static constexpr int kCriticalTimeoutMs{0};

LowBatteryMonitor::LowBatteryMonitor(BatteryService* battery, QObject* parent) : QObject(parent), battery_(battery) {
  QSettings settings;
  warning_threshold_ =
      settings.value(QStringLiteral("holonight/power/warningThreshold"), kDefaultWarningThreshold).toInt();
  critical_threshold_ =
      settings.value(QStringLiteral("holonight/power/criticalThreshold"), kDefaultCriticalThreshold).toInt();

  if (critical_threshold_ >= warning_threshold_) {
    qCWarning(lcLowBattery) << "criticalThreshold" << critical_threshold_ << ">= warningThreshold" << warning_threshold_
                            << "— clamping critical to" << (warning_threshold_ - 1);
    critical_threshold_ = warning_threshold_ - 1;
  }

  connect(battery_, &BatteryService::percentChanged, this, &LowBatteryMonitor::onPercentChanged);
  connect(battery_, &BatteryService::chargingChanged, this, &LowBatteryMonitor::onChargingChanged);
  connect(battery_, &BatteryService::dischargingChanged, this, &LowBatteryMonitor::onDischargingChanged);
  connect(battery_, &BatteryService::fullyChargedChanged, this, &LowBatteryMonitor::onFullyChargedChanged);
  connect(battery_, &BatteryService::presentChanged, this, &LowBatteryMonitor::onPresentChanged);
}

void LowBatteryMonitor::setThresholds(int warning_pct, int critical_pct) {
  warning_threshold_ = warning_pct;
  critical_threshold_ = critical_pct;
}

void LowBatteryMonitor::onPercentChanged() { checkThresholds(); }

void LowBatteryMonitor::onChargingChanged() {
  if (battery_->charging()) {
    resetState();
  }
}

void LowBatteryMonitor::onDischargingChanged() { checkThresholds(); }

void LowBatteryMonitor::onFullyChargedChanged() {
  if (battery_->fullyCharged()) {
    resetState();
  }
}

void LowBatteryMonitor::onPresentChanged() {
  if (!battery_->present()) {
    resetState();
  } else {
    // Re-check after battery re-insertion so a new discharge cycle can notify.
    checkThresholds();
  }
}

void LowBatteryMonitor::resetState() {
  warning_sent_ = false;
  critical_sent_ = false;
}

void LowBatteryMonitor::checkThresholds() {
  if (!battery_->discharging()) {
    return;
  }

  const int pct = battery_->percent();

  if (pct <= critical_threshold_ && !critical_sent_) {
    critical_sent_ = true;
    warning_sent_ = true;
    sendNotification(QStringLiteral("Battery critical"), QStringLiteral("%1% remaining — plug in now").arg(pct), 2,
                     kCriticalTimeoutMs);
    return;
  }

  if (pct <= warning_threshold_ && !warning_sent_) {
    warning_sent_ = true;
    sendNotification(QStringLiteral("Battery low"), QStringLiteral("%1% remaining").arg(pct), 1, kWarningTimeoutMs);
  }
}

void LowBatteryMonitor::sendNotification(const QString& summary, const QString& body, int urgency,
                                         int expire_timeout_ms) {  // NOLINT(readability-make-member-function-const)
  QDBusInterface notif(QStringLiteral("org.freedesktop.Notifications"),
                       QStringLiteral("/org/freedesktop/Notifications"),
                       QStringLiteral("org.freedesktop.Notifications"));
  if (!notif.isValid()) {
    qCWarning(lcLowBattery) << "org.freedesktop.Notifications not available";
    return;
  }

  const QString icon = (urgency >= 2) ? QStringLiteral("battery-low") : QStringLiteral("battery-caution");
  const QVariantMap hints{{QStringLiteral("urgency"), static_cast<quint8>(urgency)}};

  notif.call(QStringLiteral("Notify"), QStringLiteral("HoloNight Shell"), static_cast<uint>(0), icon, summary, body,
             QStringList{}, hints, expire_timeout_ms);
}
