#include "NotificationServer.h"

#include "NotificationService.h"
#include "NotificationTypes.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QLoggingCategory>
#include <QStringList>

#include <algorithm>
#include <utility>

QString truncateToMaxLength(const QString& value, qsizetype max_length) {
  if (value.size() <= max_length) {
    return value;
  }
  const qsizetype marker_length = QLatin1String(kTruncationMarker).size();
  const qsizetype prefix_length = std::max<qsizetype>(0, max_length - marker_length);
  return value.left(static_cast<int>(prefix_length)) + QLatin1String(kTruncationMarker);
}

QVariantMap truncateHintValues(const QVariantMap& hints) {
  QVariantMap truncated;
  for (auto it = hints.constBegin(); it != hints.constEnd(); ++it) {
    if (it.value().typeId() == QMetaType::QString) {
      truncated.insert(it.key(), truncateToMaxLength(it.value().toString()));
    } else {
      truncated.insert(it.key(), it.value());
    }
  }
  return truncated;
}

namespace {

Q_LOGGING_CATEGORY(lcNotificationServer, "holonight.notifications.server")

constexpr auto kServiceName = "org.freedesktop.Notifications";
constexpr auto kObjectPath = "/org/freedesktop/Notifications";

constexpr uint kUrgencyMax = 2;  // 0 = Low, 1 = Normal, 2 = Critical

// Translates the eight Notify() D-Bus arguments into a NotificationData. Hints are decoded per
// the freedesktop spec: urgency arrives as a uchar variant (read via toUInt, matching the
// NetworkManagerBackend signal-strength pattern), category/resident are plain typed hints, and
// the actions array is a flat list of alternating key/label pairs. The special "default" key is
// retained as body-click state, but never rendered as a button because clients commonly send it
// with an empty label.
NotificationData buildNotificationData(const QString& app_name, uint replaces_id, const QString& app_icon,
                                       const QString& summary, const QString& body, const QStringList& actions,
                                       const QVariantMap& hints, int expire_timeout) {
  NotificationData data;
  data.id = replaces_id;  // 0 → new notification; non-zero → replace candidate
  data.app_name = app_name;
  data.summary = truncateToMaxLength(summary);
  data.body = truncateToMaxLength(body);
  data.hints = truncateHintValues(hints);
  data.app_icon = truncateToMaxLength(app_icon);
  if (data.app_icon.isEmpty()) {
    if (data.hints.contains(QStringLiteral("image-path"))) {
      data.app_icon = data.hints.value(QStringLiteral("image-path")).toString();
    } else if (data.hints.contains(QStringLiteral("image_path"))) {
      data.app_icon = data.hints.value(QStringLiteral("image_path")).toString();
    } else if (data.hints.contains(QStringLiteral("desktop-entry"))) {
      data.app_icon = data.hints.value(QStringLiteral("desktop-entry")).toString();
    }
  }
  data.expire_timeout_ms = expire_timeout;

  for (int idx = 0; idx + 1 < actions.size(); idx += 2) {
    if (actions.at(idx) == QLatin1String("default")) {
      data.has_default_action = true;
      continue;
    }
    if (actions.at(idx + 1).trimmed().isEmpty()) {
      continue;
    }
    data.actions.append(NotifAction{.key = actions.at(idx), .label = actions.at(idx + 1)});
  }

  if (hints.contains(QStringLiteral("urgency"))) {
    const uint urgency = qBound(0U, hints.value(QStringLiteral("urgency")).toUInt(), kUrgencyMax);
    data.urgency = static_cast<NotifUrgency>(urgency);
  }
  data.category = data.hints.value(QStringLiteral("category")).toString();
  data.is_resident = hints.value(QStringLiteral("resident")).toBool();

  return data;
}

}  // namespace

NotificationServer::NotificationServer(NotificationService* service, QObject* parent)
    : QObject(parent), service_(service) {
  // Relay the service's outbound lifecycle signals onto the bus as the spec-mandated
  // NotificationClosed / ActionInvoked signals. Wired here (not in start()) so the connection
  // exists for the object's whole lifetime; the signals only reach peers once registerObject runs.
  if (service_ != nullptr) {
    connect(service_, &NotificationService::notificationClosed, this, &NotificationServer::NotificationClosed);
    connect(service_, &NotificationService::actionInvoked, this, &NotificationServer::ActionInvoked);
  }
}

void NotificationServer::start() {
  if (started_) {
    return;
  }
  started_ = true;

  QDBusConnection bus = QDBusConnection::sessionBus();

  // Probe ownership before attempting to claim the name (REQ-C-NH05).
  const QDBusReply<QString> owner_reply = bus.interface()->serviceOwner(QString::fromUtf8(kServiceName));
  if (owner_reply.isValid()) {
    conflict_detected_ = true;
    conflict_owner_ = owner_reply.value();
    qCWarning(lcNotificationServer) << "daemon conflict:" << conflict_owner_ << "already owns" << kServiceName
                                    << "— skipping registration";
    return;
  }

  if (!bus.registerService(QString::fromUtf8(kServiceName))) {
    qCWarning(lcNotificationServer) << "failed to claim" << kServiceName;
    return;
  }
  if (!bus.registerObject(QString::fromUtf8(kObjectPath), this,
                          QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
    qCWarning(lcNotificationServer) << "failed to export object at" << kObjectPath;
    bus.unregisterService(QString::fromUtf8(kServiceName));
    return;
  }
  qCInfo(lcNotificationServer) << "registered as" << kServiceName;
}

uint NotificationServer::Notify(const QString& app_name, uint replaces_id, const QString& app_icon,
                                const QString& summary, const QString& body, const QStringList& actions,
                                const QVariantMap& hints, int expire_timeout) {
  if (service_ == nullptr) {
    return 0;
  }
  NotificationData data =
      buildNotificationData(app_name, replaces_id, app_icon, summary, body, actions, hints, expire_timeout);

  qCInfo(lcNotificationServer).nospace() << "Notify received - app_name: \"" << app_name
                                         << "\", replaces_id: " << replaces_id << ", app_icon: \"" << app_icon
                                         << "\", summary: \"" << data.summary << "\", body: \"" << data.body
                                         << "\", actions: " << actions << ", hints: " << data.hints
                                         << ", expire_timeout: " << expire_timeout;

  return service_->addOrReplace(std::move(data));
}

void NotificationServer::CloseNotification(uint notif_id) {
  if (service_ != nullptr) {
    service_->requestClose(notif_id);
  }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): must stay a slot for D-Bus export.
QStringList NotificationServer::GetCapabilities() {
  return {
      QStringLiteral("body"),
      QStringLiteral("body-markup"),
      QStringLiteral("actions"),
      QStringLiteral("icon-static"),
  };
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): must stay a slot for D-Bus export.
QString NotificationServer::GetServerInformation(QString& vendor, QString& version, QString& spec_version) {
  vendor = QStringLiteral("holonight");
  version = QStringLiteral("1.0");
  spec_version = QStringLiteral("1.2");
  return QStringLiteral("holonight-shell");
}
