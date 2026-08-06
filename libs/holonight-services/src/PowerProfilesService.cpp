#include "PowerProfilesService.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>
#include <QMetaType>

#include <memory>

Q_LOGGING_CATEGORY(lcPowerProfiles, "holonight.powerprofiles")

namespace {
constexpr auto kFreedesktopService = "org.freedesktop.UPower.PowerProfiles";
constexpr auto kFreedesktopPath = "/org/freedesktop/UPower/PowerProfiles";
constexpr auto kHadessService = "net.hadess.PowerProfiles";
constexpr auto kHadessPath = "/net/hadess/PowerProfiles";
constexpr auto kPropertiesIface = "org.freedesktop.DBus.Properties";
constexpr auto kActiveProfileProp = "ActiveProfile";
constexpr auto kProfilesProp = "Profiles";
constexpr auto kProfileNameKey = "Profile";
}  // namespace

PowerProfilesService::PowerProfilesService(QObject* parent)
    : PowerProfilesService(std::make_unique<QtDbusPropertyClient>(), parent) {}

PowerProfilesService::PowerProfilesService(DbusPropertyClientPtr dbus, QObject* parent)
    : QObject(parent), dbus_(std::move(dbus)) {}

PowerProfilesService::PowerProfilesService(SkipInitTag /*tag*/, QObject* parent) : QObject(parent) {}

void PowerProfilesService::start() {
  if (started_) {
    return;
  }
  started_ = true;

  if (!dbus_->systemBusConnected()) {
    qCWarning(lcPowerProfiles) << "PowerProfilesService: system D-Bus not available";
    return;
  }

  setupWatcher();
  initFromService();
}

void PowerProfilesService::setupWatcher() {
  // Watch both bus names so the buttons appear/disappear if the daemon is (re)started or stopped
  // at runtime. The interface name matches the bus name for this daemon, so detection re-runs in
  // full on registration.
  watcher_ = new QDBusServiceWatcher(this);
  watcher_->setConnection(QDBusConnection::systemBus());
  watcher_->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
  watcher_->addWatchedService(QLatin1String(kFreedesktopService));
  watcher_->addWatchedService(QLatin1String(kHadessService));
  connect(watcher_, &QDBusServiceWatcher::serviceRegistered, this, [this](const QString&) { initFromService(); });
  connect(watcher_, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString& name) {
    if (name == service_) {
      qCInfo(lcPowerProfiles) << "PowerProfilesService: daemon" << name << "disappeared";
      clearState();
    }
  });
}

void PowerProfilesService::initFromService() {
  disconnectPropertiesChanged();

  // Prefer the newer freedesktop name; fall back to the legacy hadess name. The interface name
  // equals the chosen bus name for this daemon.
  if (dbus_->serviceRegistered(QLatin1String(kFreedesktopService))) {
    service_ = QLatin1String(kFreedesktopService);
    path_ = QLatin1String(kFreedesktopPath);
    iface_ = QLatin1String(kFreedesktopService);
  } else if (dbus_->serviceRegistered(QLatin1String(kHadessService))) {
    service_ = QLatin1String(kHadessService);
    path_ = QLatin1String(kHadessPath);
    iface_ = QLatin1String(kHadessService);
  } else {
    clearState();
    return;
  }

  const std::optional<QVariantMap> properties = dbus_->allProperties(service_, path_, iface_);
  if (!properties.has_value()) {
    qCWarning(lcPowerProfiles) << "PowerProfilesService: GetAll failed for" << service_;
    clearState();
    return;
  }

  if (!dbus_->connectSignal(service_, path_, QLatin1String(kPropertiesIface), QStringLiteral("PropertiesChanged"), this,
                            SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)))) {
    qCWarning(lcPowerProfiles) << "PowerProfilesService: failed to connect PropertiesChanged for" << service_;
    clearState();
    return;
  }
  properties_changed_connected_ = true;

  applyActiveProfile(properties->value(QLatin1String(kActiveProfileProp)).toString());
  applyProfiles(parseProfileNames(properties->value(QLatin1String(kProfilesProp))));

  setAvailable(true);
}

void PowerProfilesService::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                               const QStringList& /*invalidated*/) {
  if (interface != iface_) {
    return;
  }
  if (changed.contains(QLatin1String(kActiveProfileProp))) {
    applyActiveProfile(changed.value(QLatin1String(kActiveProfileProp)).toString());
  }
  if (changed.contains(QLatin1String(kProfilesProp))) {
    applyProfiles(parseProfileNames(changed.value(QLatin1String(kProfilesProp))));
  }
}

void PowerProfilesService::setProfile(const QString& profile) {
  if (!available_ || profile.isEmpty()) {
    return;
  }
  // No optimistic update: leave active_profile_ untouched and wait for the daemon's
  // PropertiesChanged echo so the UI always reflects the daemon's real state.
  if (!dbus_->setProperty(service_, path_, iface_, QLatin1String(kActiveProfileProp), profile)) {
    qCWarning(lcPowerProfiles) << "PowerProfilesService: failed to set profile" << profile;
  }
}

void PowerProfilesService::applyActiveProfile(const QString& profile) {
  if (active_profile_ == profile) {
    return;
  }
  active_profile_ = profile;
  emit activeProfileChanged();
}

void PowerProfilesService::applyProfiles(const QStringList& names) {
  const bool performance = names.contains(QStringLiteral("performance"));
  const bool balanced = names.contains(QStringLiteral("balanced"));
  const bool power_saver = names.contains(QStringLiteral("power-saver"));
  if (performance == has_performance_ && balanced == has_balanced_ && power_saver == has_power_saver_) {
    return;
  }
  has_performance_ = performance;
  has_balanced_ = balanced;
  has_power_saver_ = power_saver;
  emit profilesChanged();
}

void PowerProfilesService::setAvailable(bool value) {
  if (available_ == value) {
    return;
  }
  available_ = value;
  emit availableChanged();
}

void PowerProfilesService::disconnectPropertiesChanged() {
  if (!properties_changed_connected_) {
    return;
  }

  dbus_->disconnectSignal(service_, path_, QLatin1String(kPropertiesIface), QStringLiteral("PropertiesChanged"), this,
                          SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
  properties_changed_connected_ = false;
}

void PowerProfilesService::clearState() {
  disconnectPropertiesChanged();
  service_.clear();
  path_.clear();
  iface_.clear();
  applyActiveProfile({});
  applyProfiles({});
  setAvailable(false);
}

QStringList PowerProfilesService::parseProfileNames(const QVariant& value) {
  // Profiles is aa{sv}: an array of dicts, each with a "Profile" string entry naming the profile.
  QStringList names;
  const auto appendName = [&names](const QVariantMap& entry) {
    const QString name = entry.value(QLatin1String(kProfileNameKey)).toString();
    if (!name.isEmpty()) {
      names << name;
    }
  };

  if (value.userType() == qMetaTypeId<QDBusArgument>()) {
    auto arg = value.value<QDBusArgument>();
    QList<QVariantMap> entries;
    arg >> entries;
    for (const auto& entry : entries) {
      appendName(entry);
    }
    return names;
  }

  if (value.canConvert<QList<QVariantMap>>()) {
    const auto entries = value.value<QList<QVariantMap>>();
    for (const auto& entry : entries) {
      appendName(entry);
    }
    return names;
  }

  if (value.canConvert<QVariantList>()) {
    const QVariantList entries = value.toList();
    for (const auto& entry : entries) {
      if (entry.canConvert<QVariantMap>()) {
        appendName(entry.toMap());
      }
    }
    return names;
  }

  return names;
}
