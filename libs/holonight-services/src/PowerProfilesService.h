#pragma once

#include "DbusPropertyClient.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QDBusServiceWatcher;

// Exposes power-profiles-daemon state and lets the user switch the active profile. The daemon
// publishes the same interface under two bus names across versions: the newer
// org.freedesktop.UPower.PowerProfiles and the older net.hadess.PowerProfiles (each with its
// own object path and — unlike most aliases — its own interface name). We prefer the newer one
// and fall back to the older. The active profile is changed by writing the daemon's writable
// "ActiveProfile" property (there is no SetActiveProfile method); we never update optimistically
// and instead wait for the daemon's PropertiesChanged echo.
class PowerProfilesService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool available READ available NOTIFY availableChanged)
  Q_PROPERTY(QString activeProfile READ activeProfile NOTIFY activeProfileChanged)
  Q_PROPERTY(bool hasPerformance READ hasPerformance NOTIFY profilesChanged)
  Q_PROPERTY(bool hasBalanced READ hasBalanced NOTIFY profilesChanged)
  Q_PROPERTY(bool hasPowerSaver READ hasPowerSaver NOTIFY profilesChanged)

 public:
  struct SkipInitTag {};
  static constexpr SkipInitTag SkipInit{};

  explicit PowerProfilesService(QObject* parent = nullptr);
  explicit PowerProfilesService(DbusPropertyClientPtr dbus, QObject* parent = nullptr);
  explicit PowerProfilesService(SkipInitTag /*tag*/, QObject* parent = nullptr);
  ~PowerProfilesService() override = default;

  PowerProfilesService(const PowerProfilesService&) = delete;
  PowerProfilesService& operator=(const PowerProfilesService&) = delete;
  PowerProfilesService(PowerProfilesService&&) = delete;
  PowerProfilesService& operator=(PowerProfilesService&&) = delete;

  [[nodiscard]] bool available() const { return available_; }
  [[nodiscard]] QString activeProfile() const { return active_profile_; }
  [[nodiscard]] bool hasPerformance() const { return has_performance_; }
  [[nodiscard]] bool hasBalanced() const { return has_balanced_; }
  [[nodiscard]] bool hasPowerSaver() const { return has_power_saver_; }

  void start();

  // Test seam / signal-driven appliers (mirrors BatteryService::applyStateUpdate).
  void applyActiveProfile(const QString& profile);
  void applyProfiles(const QStringList& names);

  Q_INVOKABLE void setProfile(const QString& profile);

 Q_SIGNALS:
  void availableChanged();
  void activeProfileChanged();
  void profilesChanged();

 private Q_SLOTS:
  void onPropertiesChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated);

 private:
  void initFromService();
  void setupWatcher();
  void disconnectPropertiesChanged();
  void setAvailable(bool value);
  void clearState();
  [[nodiscard]] static QStringList parseProfileNames(const QVariant& value);

  DbusPropertyClientPtr dbus_;
  QDBusServiceWatcher* watcher_ = nullptr;
  QString service_;  // active bus name (empty when daemon absent)
  QString path_;
  QString iface_;
  QString active_profile_;
  bool has_performance_{false};
  bool has_balanced_{false};
  bool has_power_saver_{false};
  bool available_{false};
  bool started_{false};
  bool properties_changed_connected_{false};
};
