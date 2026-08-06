#include "DbusPropertyClient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcDbusPropertyClient, "holonight.dbus.properties")

namespace {
constexpr auto kUPowerService = "org.freedesktop.UPower";
constexpr auto kUPowerPath = "/org/freedesktop/UPower";
constexpr auto kUPowerIface = "org.freedesktop.UPower";
constexpr auto kPropertiesIface = "org.freedesktop.DBus.Properties";

struct DbusConnectionOverride {
  bool has_custom{false};
  QDBusConnection custom_connection{QString()};
};

DbusConnectionOverride& dbusConnectionOverride() {
  static DbusConnectionOverride state;
  return state;
}

QDBusConnection dbusConnection() {
  const DbusConnectionOverride& state = dbusConnectionOverride();
  return state.has_custom ? state.custom_connection : QDBusConnection::systemBus();
}
}  // namespace

void QtDbusPropertyClient::setDbusConnection(const QDBusConnection& connection) {
  DbusConnectionOverride& state = dbusConnectionOverride();
  state.has_custom = true;
  state.custom_connection = connection;
}

void QtDbusPropertyClient::resetDbusConnection() {
  DbusConnectionOverride& state = dbusConnectionOverride();
  state.has_custom = false;
  state.custom_connection = QDBusConnection(QString());
}

bool QtDbusPropertyClient::systemBusConnected() const { return dbusConnection().isConnected(); }

bool QtDbusPropertyClient::serviceRegistered(const QString& service) const {
  QDBusConnection bus = dbusConnection();
  auto* bus_iface = bus.interface();
  if (bus_iface == nullptr) {
    qCWarning(lcDbusPropertyClient) << "D-Bus service registration interface is unavailable:" << service;
    return false;
  }

  const QDBusReply<bool> registered = bus_iface->isServiceRegistered(service);
  if (!registered.isValid()) {
    qCWarning(lcDbusPropertyClient) << "D-Bus service registration check failed:" << service
                                    << registered.error().message();
    return false;
  }
  return registered.value();
}

std::optional<QVariant> QtDbusPropertyClient::property(const QString& service, const QString& path,
                                                       const QString& interface, const QString& name) const {
  QDBusInterface props(service, path, kPropertiesIface, dbusConnection());
  const QDBusReply<QDBusVariant> reply = props.call(QStringLiteral("Get"), interface, name);
  if (!reply.isValid()) {
    qCWarning(lcDbusPropertyClient) << "D-Bus Get failed:" << service << path << interface << name
                                    << reply.error().message();
    return std::nullopt;
  }
  return reply.value().variant();
}

std::optional<QVariantMap> QtDbusPropertyClient::allProperties(const QString& service, const QString& path,
                                                               const QString& interface) const {
  QDBusInterface props(service, path, kPropertiesIface, dbusConnection());
  const QDBusReply<QVariantMap> reply = props.call(QStringLiteral("GetAll"), interface);
  if (!reply.isValid()) {
    qCWarning(lcDbusPropertyClient) << "D-Bus GetAll failed:" << service << path
                                    << interface << reply.error().message();
    return std::nullopt;
  }
  return reply.value();
}

bool QtDbusPropertyClient::setProperty(const QString& service, const QString& path, const QString& interface,
                                       const QString& name, const QVariant& value) const {
  QDBusInterface props(service, path, kPropertiesIface, dbusConnection());
  const QDBusReply<void> reply =
      props.call(QStringLiteral("Set"), interface, name, QVariant::fromValue(QDBusVariant(value)));
  if (!reply.isValid()) {
    qCWarning(lcDbusPropertyClient) << "D-Bus Set failed:" << service << path << interface << name
                                    << reply.error().message();
    return false;
  }
  return true;
}

std::optional<QList<QDBusObjectPath>> QtDbusPropertyClient::upowerDevices() const {
  QDBusInterface upower(kUPowerService, kUPowerPath, kUPowerIface, dbusConnection());
  const QDBusReply<QList<QDBusObjectPath>> reply = upower.call(QStringLiteral("EnumerateDevices"));
  if (!reply.isValid()) {
    qCWarning(lcDbusPropertyClient) << "UPower EnumerateDevices failed:" << reply.error().message();
    return std::nullopt;
  }
  return reply.value();
}

bool QtDbusPropertyClient::connectSignal(const QString& service, const QString& path, const QString& interface,
                                         const QString& signal, QObject* receiver, const char* slot) const {
  return dbusConnection().connect(service, path, interface, signal, receiver, slot);
}

bool QtDbusPropertyClient::disconnectSignal(const QString& service, const QString& path, const QString& interface,
                                            const QString& signal, QObject* receiver, const char* slot) const {
  return dbusConnection().disconnect(service, path, interface, signal, receiver, slot);
}
