#include "LidStateMonitor.h"

#include <QLoggingCategory>

#include <memory>

Q_LOGGING_CATEGORY(lcLid, "holonight.lid")

static constexpr auto kUPowerService = "org.freedesktop.UPower";
static constexpr auto kUPowerPath = "/org/freedesktop/UPower";
static constexpr auto kUPowerIface = "org.freedesktop.UPower";
static constexpr auto kPropertiesIface = "org.freedesktop.DBus.Properties";

LidStateMonitor::LidStateMonitor(QObject* parent) : LidStateMonitor(std::make_unique<QtDbusPropertyClient>(), parent) {}

LidStateMonitor::LidStateMonitor(DbusPropertyClientPtr dbus, QObject* parent)
    : QObject(parent), dbus_(std::move(dbus)) {}

LidStateMonitor::LidStateMonitor(SkipInitTag /*tag*/, QObject* parent) : QObject(parent) {}

void LidStateMonitor::start() {
  if (started_) {
    return;
  }
  started_ = true;

  if (!dbus_->systemBusConnected()) {
    qCWarning(lcLid) << "LidStateMonitor: system D-Bus not available";
    return;
  }

  const std::optional<QVariantMap> props =
      dbus_->allProperties(QLatin1String(kUPowerService), QLatin1String(kUPowerPath), QLatin1String(kUPowerIface));
  if (!props.has_value()) {
    qCInfo(lcLid) << "LidStateMonitor: UPower manager properties unavailable";
    return;
  }

  const QVariant presentVar = props->value(QStringLiteral("LidIsPresent"));
  lid_present_ = presentVar.isValid() && presentVar.toBool();

  if (!lid_present_) {
    qCInfo(lcLid) << "LidStateMonitor: lid device not present (desktop?)";
    return;
  }

  lid_closed_ = props->value(QStringLiteral("LidIsClosed")).toBool();

  dbus_->connectSignal(QLatin1String(kUPowerService), QLatin1String(kUPowerPath), QLatin1String(kPropertiesIface),
                       QStringLiteral("PropertiesChanged"), this,
                       SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
}

void LidStateMonitor::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                          const QStringList& /*invalidated*/) {
  if (interface != QLatin1String(kUPowerIface)) {
    return;
  }

  const auto iter = changed.find(QStringLiteral("LidIsClosed"));
  if (iter == changed.end()) {
    return;
  }

  const bool closed = iter->toBool();
  if (closed == lid_closed_) {
    return;
  }
  lid_closed_ = closed;
  emit lidStateChanged(closed);
}
