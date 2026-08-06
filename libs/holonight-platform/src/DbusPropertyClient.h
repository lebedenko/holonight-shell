#pragma once

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#include <memory>
#include <optional>

class DbusPropertyClient {
 public:
  virtual ~DbusPropertyClient() = default;

  DbusPropertyClient(const DbusPropertyClient&) = delete;
  DbusPropertyClient& operator=(const DbusPropertyClient&) = delete;
  DbusPropertyClient(DbusPropertyClient&&) = delete;
  DbusPropertyClient& operator=(DbusPropertyClient&&) = delete;

  [[nodiscard]] virtual bool systemBusConnected() const = 0;
  [[nodiscard]] virtual bool serviceRegistered(const QString& service) const = 0;
  [[nodiscard]] virtual std::optional<QVariant> property(const QString& service, const QString& path,
                                                         const QString& interface, const QString& name) const = 0;
  [[nodiscard]] virtual std::optional<QVariantMap> allProperties(const QString& service, const QString& path,
                                                                 const QString& interface) const = 0;
  // Writes a single property via org.freedesktop.DBus.Properties.Set. Non-pure with a default
  // no-op so existing fakes/clients that never write properties need no changes. Returns false
  // on failure (or when unimplemented). Used to set the writable PowerProfiles ActiveProfile.
  [[nodiscard]] virtual bool setProperty(const QString& /*service*/, const QString& /*path*/,
                                         const QString& /*interface*/, const QString& /*name*/,
                                         const QVariant& /*value*/) const {
    return false;
  }
  [[nodiscard]] virtual std::optional<QList<QDBusObjectPath>> upowerDevices() const = 0;
  virtual bool connectSignal(const QString& service, const QString& path, const QString& interface,
                             const QString& signal, QObject* receiver, const char* slot) const = 0;
  virtual bool disconnectSignal(const QString& service, const QString& path, const QString& interface,
                                const QString& signal, QObject* receiver, const char* slot) const = 0;

 protected:
  DbusPropertyClient() = default;
};

using DbusPropertyClientPtr = std::unique_ptr<DbusPropertyClient>;

class QtDbusPropertyClient final : public DbusPropertyClient {
 public:
  QtDbusPropertyClient() = default;

  static void setDbusConnection(const QDBusConnection& connection);
  static void resetDbusConnection();

  [[nodiscard]] bool systemBusConnected() const override;
  [[nodiscard]] bool serviceRegistered(const QString& service) const override;
  [[nodiscard]] std::optional<QVariant> property(const QString& service, const QString& path, const QString& interface,
                                                 const QString& name) const override;
  [[nodiscard]] std::optional<QVariantMap> allProperties(const QString& service, const QString& path,
                                                         const QString& interface) const override;
  [[nodiscard]] bool setProperty(const QString& service, const QString& path, const QString& interface,
                                 const QString& name, const QVariant& value) const override;
  [[nodiscard]] std::optional<QList<QDBusObjectPath>> upowerDevices() const override;
  bool connectSignal(const QString& service, const QString& path, const QString& interface, const QString& signal,
                     QObject* receiver, const char* slot) const override;
  bool disconnectSignal(const QString& service, const QString& path, const QString& interface, const QString& signal,
                        QObject* receiver, const char* slot) const override;
};
