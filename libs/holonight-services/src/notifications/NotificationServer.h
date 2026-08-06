#pragma once

#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class NotificationService;

// Bounds a single untrusted D-Bus payload field (summary/body/string-typed hint) so a malicious
// or buggy client can't exhaust disk via unbounded log/model/history writes. Exposed here (not
// file-local) so tests can exercise the truncation boundary directly, without a live D-Bus call.
constexpr qsizetype kMaxNotificationFieldLength{4096};
constexpr auto kTruncationMarker = "...[truncated]";

[[nodiscard]] QString truncateToMaxLength(const QString& value, qsizetype max_length = kMaxNotificationFieldLength);
[[nodiscard]] QVariantMap truncateHintValues(const QVariantMap& hints);

// Pure org.freedesktop.Notifications (spec v1.2) D-Bus adapter. Owns no application state:
// it translates inbound D-Bus calls into NotificationService operations and relays the service's
// outbound signals back onto the bus. Mirrors the TrayWatcher protocol-adapter pattern — the
// Q_CLASSINFO is mandatory so Qt's introspection names the interface org.freedesktop.Notifications
// rather than local.NotificationServer.
class NotificationServer : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

 public:
  explicit NotificationServer(NotificationService* service, QObject* parent = nullptr);
  ~NotificationServer() override = default;

  NotificationServer(const NotificationServer&) = delete;
  NotificationServer& operator=(const NotificationServer&) = delete;
  NotificationServer(NotificationServer&&) = delete;
  NotificationServer& operator=(NotificationServer&&) = delete;

  // Claims the org.freedesktop.Notifications service name and exports this object on the
  // session bus. Wired into ShellApplication startup (T-010).
  void start();

 public Q_SLOTS:
  // Slot names are PascalCase per the freedesktop protocol — they cannot use the project camelCase.
  // NOLINTBEGIN(readability-identifier-naming, readability-convert-member-functions-to-static)
  uint Notify(const QString& app_name, uint replaces_id, const QString& app_icon, const QString& summary,
              const QString& body, const QStringList& actions, const QVariantMap& hints, int expire_timeout);
  void CloseNotification(uint notif_id);
  QStringList GetCapabilities();
  QString GetServerInformation(QString& vendor, QString& version, QString& spec_version);
  // NOLINTEND(readability-identifier-naming, readability-convert-member-functions-to-static)

 Q_SIGNALS:
  // Protocol-mandated PascalCase signal names.
  // NOLINTBEGIN(readability-identifier-naming)
  void NotificationClosed(uint notif_id, uint reason);
  void ActionInvoked(uint notif_id, const QString& action_key);
  // NOLINTEND(readability-identifier-naming)

 public:
  [[nodiscard]] bool conflictDetected() const { return conflict_detected_; }
  [[nodiscard]] QString conflictOwner() const { return conflict_owner_; }

 private:
  NotificationService* service_;
  bool started_{false};
  bool conflict_detected_{false};
  QString conflict_owner_;
};
