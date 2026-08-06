#include "LogindSessionResolver.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QLoggingCategory>
#include <QProcess>

Q_LOGGING_CATEGORY(lcDbusSession, "holonight.dbus.session")

namespace {
constexpr auto kLogindService = "org.freedesktop.login1";
constexpr auto kLogindPath = "/org/freedesktop/login1";
constexpr auto kLogindManagerIface = "org.freedesktop.login1.Manager";
constexpr int kLoginctlTimeoutMs{2000};
}  // namespace

QString resolveActiveLogindSessionPath(const LogindSessionResolverHooks& hooks) {
  QString pid_session_path = hooks.session_path_for_pid();
  if (!pid_session_path.isEmpty()) {
    return pid_session_path;
  }

  const QString session_id = hooks.active_session_id();
  if (session_id.isEmpty()) {
    return {};
  }

  return hooks.session_path_for_id(session_id);
}

QString resolveActiveLogindSessionPath() {
  QDBusInterface manager(QLatin1String(kLogindService), QLatin1String(kLogindPath), QLatin1String(kLogindManagerIface),
                         QDBusConnection::systemBus());

  LogindSessionResolverHooks hooks;
  hooks.session_path_for_pid = [&manager] {
    // Try GetSessionByPID first. This works when the process is in a session-N.scope cgroup
    // (DM/TTY launch). It fails for processes started via systemd user services (UWSM, autostart)
    // because they live in user@UID.service/app.slice, not inside session-N.scope.
    const QDBusMessage pid_reply =
        manager.call(QStringLiteral("GetSessionByPID"), static_cast<quint32>(QCoreApplication::applicationPid()));
    if (pid_reply.type() == QDBusMessage::ReplyMessage && !pid_reply.arguments().isEmpty()) {
      return pid_reply.arguments().at(0).value<QDBusObjectPath>().path();
    }
    return QString{};
  };
  hooks.active_session_id = [] {
    // Fallback: Properties.Get("ActiveSession") v(so) and ListSessions a(susso) both corrupt the
    // QDBusArgument iterator or trigger a libdbus assertion on this Qt/libdbus build. Use loginctl
    // as a reliable out-of-process fallback; it is always co-installed with logind.
    QProcess loginctl;
    loginctl.start(QStringLiteral("loginctl"), {QStringLiteral("show-seat"), QStringLiteral("--property=ActiveSession"),
                                                QStringLiteral("--value"), QStringLiteral("seat0")});
    if (loginctl.waitForFinished(kLoginctlTimeoutMs) && loginctl.exitCode() == 0) {
      return QString::fromLatin1(loginctl.readAllStandardOutput()).trimmed();
    }
    return QString{};
  };
  hooks.session_path_for_id = [&manager](const QString& session_id) {
    const QDBusMessage sess_reply = manager.call(QStringLiteral("GetSession"), session_id);
    if (sess_reply.type() == QDBusMessage::ReplyMessage && !sess_reply.arguments().isEmpty()) {
      const QString session_path = sess_reply.arguments().at(0).value<QDBusObjectPath>().path();
      if (!session_path.isEmpty()) {
        qCInfo(lcDbusSession) << "GetSessionByPID unavailable (user service scope); resolved session via loginctl:"
                              << session_id;
      }
      return session_path;
    }
    return QString{};
  };

  return resolveActiveLogindSessionPath(hooks);
}
