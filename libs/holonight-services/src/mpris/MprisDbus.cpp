#include "MprisDbus.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QMetaObject>
#include <QObject>

namespace {
constexpr auto kMprisPath = "/org/mpris/MediaPlayer2";
constexpr auto kRootIface = "org.mpris.MediaPlayer2";
constexpr auto kPlayerIface = "org.mpris.MediaPlayer2.Player";
constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";
constexpr auto kDbusService = "org.freedesktop.DBus";
constexpr auto kDbusPath = "/org/freedesktop/DBus";
constexpr auto kDbusIface = "org.freedesktop.DBus";

// Extracts the bare member name from a SLOT()-macro-decorated string ("1onFoo(QString)" ->
// "onFoo") so FakeMprisDBus can drive a recorded receiver/slot pair via QMetaObject::invokeMethod
// without requiring a real D-Bus connection.
QByteArray slotMemberName(const char* slot) {
  if (slot == nullptr) {
    return {};
  }
  const QByteArray raw(slot);
  if (raw.isEmpty()) {
    return {};
  }
  const qsizetype paren = raw.indexOf('(');
  return paren > 1 ? raw.mid(1, static_cast<int>(paren) - 1) : raw.mid(1);
}
}  // namespace

// ─── SystemMprisDBus ─────────────────────────────────────────────────────────

QStringList SystemMprisDBus::listNames() {
  auto* iface = QDBusConnection::sessionBus().interface();
  if (iface == nullptr) {
    return {};
  }
  const QDBusReply<QStringList> reply = iface->registeredServiceNames();
  return reply.isValid() ? reply.value() : QStringList{};
}

bool SystemMprisDBus::connectNameOwnerChanged(QObject* receiver, const char* slot) {
  return QDBusConnection::sessionBus().connect(QLatin1String(kDbusService), QLatin1String(kDbusPath),
                                               QLatin1String(kDbusIface), QStringLiteral("NameOwnerChanged"), receiver,
                                               slot);
}

namespace {
std::optional<QVariantMap> getAllProperties(const QString& service, const char* interface_name) {
  QDBusInterface iface(service, QLatin1String(kMprisPath), QLatin1String(kPropsIface), QDBusConnection::sessionBus());
  iface.setTimeout(SystemMprisDBus::kCallTimeoutMs);
  const QDBusReply<QVariantMap> reply = iface.call(QStringLiteral("GetAll"), QLatin1String(interface_name));
  return reply.isValid() ? std::optional<QVariantMap>(reply.value()) : std::nullopt;
}
}  // namespace

QVariantMap SystemMprisDBus::getAllRootProperties(const QString& service) {
  return getAllProperties(service, kRootIface).value_or(QVariantMap{});
}

std::optional<QVariantMap> SystemMprisDBus::getAllPlayerProperties(const QString& service) {
  return getAllProperties(service, kPlayerIface);
}

bool SystemMprisDBus::connectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) {
  return QDBusConnection::sessionBus().connect(service, QLatin1String(kMprisPath), QLatin1String(kPropsIface),
                                               QStringLiteral("PropertiesChanged"), receiver, slot);
}

void SystemMprisDBus::disconnectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) {
  QDBusConnection::sessionBus().disconnect(service, QLatin1String(kMprisPath), QLatin1String(kPropsIface),
                                           QStringLiteral("PropertiesChanged"), receiver, slot);
}

bool SystemMprisDBus::connectSeeked(const QString& service, QObject* receiver, const char* slot) {
  return QDBusConnection::sessionBus().connect(service, QLatin1String(kMprisPath), QLatin1String(kPlayerIface),
                                               QStringLiteral("Seeked"), receiver, slot);
}

void SystemMprisDBus::disconnectSeeked(const QString& service, QObject* receiver, const char* slot) {
  QDBusConnection::sessionBus().disconnect(service, QLatin1String(kMprisPath), QLatin1String(kPlayerIface),
                                           QStringLiteral("Seeked"), receiver, slot);
}

void SystemMprisDBus::asyncPlayPause(const QString& service) {
  QDBusInterface iface(service, QLatin1String(kMprisPath), QLatin1String(kPlayerIface), QDBusConnection::sessionBus());
  iface.setTimeout(kCallTimeoutMs);
  iface.asyncCall(QStringLiteral("PlayPause"));
}

void SystemMprisDBus::asyncNext(const QString& service) {
  QDBusInterface iface(service, QLatin1String(kMprisPath), QLatin1String(kPlayerIface), QDBusConnection::sessionBus());
  iface.setTimeout(kCallTimeoutMs);
  iface.asyncCall(QStringLiteral("Next"));
}

void SystemMprisDBus::asyncPrevious(const QString& service) {
  QDBusInterface iface(service, QLatin1String(kMprisPath), QLatin1String(kPlayerIface), QDBusConnection::sessionBus());
  iface.setTimeout(kCallTimeoutMs);
  iface.asyncCall(QStringLiteral("Previous"));
}

// ─── FakeMprisDBus ───────────────────────────────────────────────────────────

void FakeMprisDBus::seedPlayer(const QString& service, const QVariantMap& root_properties,
                               const QVariantMap& player_properties) {
  root_properties_[service] = root_properties;
  player_properties_[service] = player_properties;
}

void FakeMprisDBus::seedPlayer(const QString& service, const QVariantMap& combined_properties) {
  QVariantMap root_properties;
  QVariantMap player_properties = combined_properties;
  for (const QString& property : {QStringLiteral("Identity"), QStringLiteral("DesktopEntry")}) {
    if (player_properties.contains(property)) {
      root_properties.insert(property, player_properties.take(property));
    }
  }
  seedPlayer(service, root_properties, player_properties);
}

void FakeMprisDBus::removePlayer(const QString& service) {
  root_properties_.remove(service);
  player_properties_.remove(service);
}

void FakeMprisDBus::emitNameOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner) {
  if (name_owner_receiver_ == nullptr) {
    return;
  }
  const QByteArray member = slotMemberName(name_owner_slot_);
  QMetaObject::invokeMethod(name_owner_receiver_, member.constData(), Qt::DirectConnection, Q_ARG(QString, name),
                            Q_ARG(QString, oldOwner), Q_ARG(QString, newOwner));
}

void FakeMprisDBus::emitPropertiesChanged(const QString& service, const QString& interface_name,
                                          const QVariantMap& changed, const QStringList& invalidated) {
  QVariantMap* stored = nullptr;
  if (interface_name == QLatin1String(kRootIface)) {
    stored = &root_properties_[service];
  } else if (interface_name == QLatin1String(kPlayerIface)) {
    stored = &player_properties_[service];
  }
  if (stored != nullptr) {
    for (auto entry = changed.constBegin(); entry != changed.constEnd(); ++entry) {
      stored->insert(entry.key(), entry.value());
    }
    for (const QString& property : invalidated) {
      stored->remove(property);
    }
  }

  const auto entry = prop_receivers_.constFind(service);
  if (entry == prop_receivers_.constEnd()) {
    return;
  }
  const QByteArray member = slotMemberName(entry.value().second);
  QMetaObject::invokeMethod(entry.value().first, member.constData(), Qt::DirectConnection,
                            Q_ARG(QString, interface_name), Q_ARG(QVariantMap, changed),
                            Q_ARG(QStringList, invalidated));
}

void FakeMprisDBus::emitPlayerPropertiesChanged(const QString& service, const QVariantMap& changed,
                                                const QStringList& invalidated) {
  emitPropertiesChanged(service, QLatin1String(kPlayerIface), changed, invalidated);
}

QStringList FakeMprisDBus::listNames() { return player_properties_.keys(); }

bool FakeMprisDBus::connectNameOwnerChanged(QObject* receiver, const char* slot) {
  name_owner_receiver_ = receiver;
  name_owner_slot_ = slot;
  return true;
}

QVariantMap FakeMprisDBus::getAllRootProperties(const QString& service) {
  properties_requested_before_subscription_ =
      properties_requested_before_subscription_ || !prop_receivers_.contains(service);
  return root_properties_.value(service);
}

std::optional<QVariantMap> FakeMprisDBus::getAllPlayerProperties(const QString& service) {
  properties_requested_before_subscription_ =
      properties_requested_before_subscription_ || !prop_receivers_.contains(service);
  ++get_all_player_properties_call_count_;
  if (player_properties_read_failure_) {
    return std::nullopt;
  }
  return player_properties_.value(service);
}

bool FakeMprisDBus::connectPropertiesChanged(const QString& service, QObject* receiver, const char* slot) {
  if (!connect_properties_changed_result_) {
    return false;
  }
  prop_receivers_.insert(service, {receiver, slot});
  return true;
}

void FakeMprisDBus::disconnectPropertiesChanged(const QString& service, QObject* /*receiver*/, const char* /*slot*/) {
  prop_receivers_.remove(service);
}

bool FakeMprisDBus::connectSeeked(const QString& service, QObject* receiver, const char* slot) {
  seeked_receivers_.insert(service, {receiver, slot});
  return true;
}

void FakeMprisDBus::disconnectSeeked(const QString& service, QObject* /*receiver*/, const char* /*slot*/) {
  seeked_receivers_.remove(service);
}

void FakeMprisDBus::emitSeeked(const QString& service, qint64 positionUs) {
  const auto entry = seeked_receivers_.constFind(service);
  if (entry == seeked_receivers_.constEnd()) {
    return;
  }
  const QByteArray member = slotMemberName(entry.value().second);
  QMetaObject::invokeMethod(entry.value().first, member.constData(), Qt::DirectConnection,
                            Q_ARG(qlonglong, positionUs));
}
