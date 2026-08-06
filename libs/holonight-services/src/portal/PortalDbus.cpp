#include "PortalDbus.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusVariant>

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace {
constexpr auto kPortalService = "org.freedesktop.portal.Desktop";
constexpr auto kPortalPath = "/org/freedesktop/portal/desktop";
constexpr auto kDbusService = "org.freedesktop.DBus";
constexpr auto kDbusPath = "/org/freedesktop/DBus";
constexpr auto kDbusIface = "org.freedesktop.DBus";
}  // namespace

// ─── SystemPortalDBus ────────────────────────────────────────────────────────

QDBusPendingCall SystemPortalDBus::nameHasOwner(const QString& service_name) {
  QDBusInterface iface(kDbusService, kDbusPath, kDbusIface, QDBusConnection::sessionBus());
  iface.setTimeout(probe_timeout_ms_);
  return iface.asyncCall(QStringLiteral("NameHasOwner"), service_name);
}

QDBusPendingCall SystemPortalDBus::introspectPortal() {
  QDBusInterface iface(portal_service_, kPortalPath, QStringLiteral("org.freedesktop.DBus.Introspectable"),
                       QDBusConnection::sessionBus());
  iface.setTimeout(probe_timeout_ms_);
  return iface.asyncCall(QStringLiteral("Introspect"));
}

QDBusPendingCall SystemPortalDBus::listNames() {
  QDBusInterface iface(kDbusService, kDbusPath, kDbusIface, QDBusConnection::sessionBus());
  iface.setTimeout(probe_timeout_ms_);
  return iface.asyncCall(QStringLiteral("ListNames"));
}

QDBusPendingCall SystemPortalDBus::readSetting(const QString& portal_namespace, const QString& key) {
  QDBusInterface iface(portal_service_, kPortalPath, QStringLiteral("org.freedesktop.portal.Settings"),
                       QDBusConnection::sessionBus());
  iface.setTimeout(probe_timeout_ms_);
  return iface.asyncCall(QStringLiteral("Read"), portal_namespace, key);
}

bool SystemPortalDBus::connectSettingChanged(QObject* receiver, const char* slot) {
  return QDBusConnection::sessionBus().connect(QLatin1String(kPortalService), QLatin1String(kPortalPath),
                                               QStringLiteral("org.freedesktop.portal.Settings"),
                                               QStringLiteral("SettingChanged"), receiver, slot);
}

bool SystemPortalDBus::connectNameOwnerChanged(QObject* receiver, const char* slot) {
  return QDBusConnection::sessionBus().connect(QLatin1String(kDbusService), QLatin1String(kDbusPath),
                                               QLatin1String(kDbusIface), QStringLiteral("NameOwnerChanged"), receiver,
                                               slot);
}

QDBusPendingCall SystemPortalDBus::openFile(const QString& window_handle, const QString& title,
                                            const QStringList& filters) {
  QDBusInterface iface(kPortalService, kPortalPath, QStringLiteral("org.freedesktop.portal.FileChooser"),
                       QDBusConnection::sessionBus());
  QVariantMap options;
  if (!filters.isEmpty()) {
    options.insert(QStringLiteral("filters"), filters);
  }
  return iface.asyncCall(QStringLiteral("OpenFile"), window_handle, title, options);
}

QDBusPendingCall SystemPortalDBus::openUri(const QString& uri) {
  QDBusInterface iface(kPortalService, kPortalPath, QStringLiteral("org.freedesktop.portal.OpenURI"),
                       QDBusConnection::sessionBus());
  const QVariantMap options;
  return iface.asyncCall(QStringLiteral("OpenURI"), QString{}, uri, options);
}

QDBusServiceWatcher* SystemPortalDBus::createServiceWatcher(const QString& service,
                                                            QDBusServiceWatcher::WatchModeFlag mode, QObject* parent) {
  return new QDBusServiceWatcher(service, QDBusConnection::sessionBus(), mode, parent);
}

// ─── NullPortalDBus ──────────────────────────────────────────────────────────

QString NullPortalDBus::buildIntrospectXml(const QStringList& ifaces) {
  QString xml = QStringLiteral(
      "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" "
      "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n<node>\n");
  for (const QString& iface : ifaces) {
    xml += QStringLiteral("  <interface name=\"") + iface + QStringLiteral("\"/>\n");
  }
  xml += QStringLiteral("</node>\n");
  return xml;
}

QDBusPendingCall NullPortalDBus::makeReply(const QVariantList& args) {
  const QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.fake"), QStringLiteral("/fake"),
                                                           QStringLiteral("org.fake.Fake"), QStringLiteral("Fake"));
  return QDBusPendingCall::fromCompletedCall(call.createReply(args));
}

QDBusPendingCall NullPortalDBus::makeErrorReply(const QString& error_name, const QString& msg) {
  const QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.fake"), QStringLiteral("/fake"),
                                                           QStringLiteral("org.fake.Fake"), QStringLiteral("Fake"));
  return QDBusPendingCall::fromCompletedCall(call.createErrorReply(error_name, msg));
}

void NullPortalDBus::setInterfaces(const QStringList& ifaces) { introspect_xml_ = buildIntrospectXml(ifaces); }

QDBusPendingCall NullPortalDBus::nameHasOwner(const QString& /*service_name*/) {
  return makeReply({QVariant(name_has_owner_)});
}

QDBusPendingCall NullPortalDBus::introspectPortal() { return makeReply({QVariant(introspect_xml_)}); }

QDBusPendingCall NullPortalDBus::listNames() { return makeReply({QVariant(names_)}); }

QDBusPendingCall NullPortalDBus::readSetting(const QString& /*portal_namespace*/, const QString& key) {
  if (setting_errors_.contains(key)) {
    return makeErrorReply(QStringLiteral("org.freedesktop.portal.Error.NotFound"), QStringLiteral("Key not found"));
  }
  if (settings_.contains(key)) {
    QDBusVariant dbv;
    dbv.setVariant(settings_.value(key));
    return makeReply({QVariant::fromValue(dbv)});
  }
  return makeErrorReply(QStringLiteral("org.freedesktop.portal.Error.NotFound"), QStringLiteral("Key not found"));
}

QDBusPendingCall NullPortalDBus::openFile(const QString& /*window_handle*/, const QString& /*title*/,
                                          const QStringList& filters) {
  ++open_file_calls_;
  last_open_file_filters_ = filters;
  return makeReply({});
}

QDBusPendingCall NullPortalDBus::openUri(const QString& /*uri*/) {
  ++open_uri_calls_;
  return makeReply({});
}

QDBusServiceWatcher* NullPortalDBus::createServiceWatcher(const QString& /*service*/,
                                                          QDBusServiceWatcher::WatchModeFlag mode, QObject* parent) {
  auto* watcher = new QDBusServiceWatcher(parent);
  watcher->setWatchMode(mode);
  return watcher;
}
