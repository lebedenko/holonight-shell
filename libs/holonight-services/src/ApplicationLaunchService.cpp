#include "ApplicationLaunchService.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDeadlineTimer>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QtConcurrent>

#include <memory>

namespace {
struct Property {
  QString name;
  QDBusVariant value;
};
using Properties = QList<Property>;
struct Exec {
  QString path;
  QStringList argv;
  bool ignore;
};
using Execs = QList<Exec>;
struct Auxiliary {
  QString name;
  Properties properties;
};
using Auxiliaries = QList<Auxiliary>;
QDBusArgument& operator<<(QDBusArgument& a, const Property& p) {
  a.beginStructure();
  a << p.name << p.value;
  a.endStructure();
  return a;
}
const QDBusArgument& operator>>(const QDBusArgument& a, Property& p) {
  a.beginStructure();
  a >> p.name >> p.value;
  a.endStructure();
  return a;
}
QDBusArgument& operator<<(QDBusArgument& a, const Exec& p) {
  a.beginStructure();
  a << p.path << p.argv << p.ignore;
  a.endStructure();
  return a;
}
const QDBusArgument& operator>>(const QDBusArgument& a, Exec& p) {
  a.beginStructure();
  a >> p.path >> p.argv >> p.ignore;
  a.endStructure();
  return a;
}
QDBusArgument& operator<<(QDBusArgument& a, const Auxiliary& p) {
  a.beginStructure();
  a << p.name << p.properties;
  a.endStructure();
  return a;
}
const QDBusArgument& operator>>(const QDBusArgument& a, Auxiliary& p) {
  a.beginStructure();
  a >> p.name >> p.properties;
  a.endStructure();
  return a;
}
ApplicationLaunchService::Capabilities probeCapabilities(const QDeadlineTimer& deadline) {
  ApplicationLaunchService::Capabilities capabilities;
  capabilities.runningAsService =
      qEnvironmentVariableIsSet("INVOCATION_ID") || qEnvironmentVariableIsSet("SYSTEMD_EXEC_PID");
  auto bus = QDBusConnection::sessionBus();
  QDBusInterface manager("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager",
                         bus);
  manager.setTimeout(static_cast<int>(qMax<qint64>(1, deadline.remainingTime())));
  capabilities.managerAvailable = manager.isValid();
  if (!capabilities.managerAvailable) return capabilities;
  const auto units = manager.call("ListUnitsByPatterns", QStringList{"active"}, QStringList{"wayland-wm@*.service"});
  if (units.type() == QDBusMessage::ErrorMessage) {
    capabilities.error = units.errorMessage();
    return capabilities;
  }
  if (!units.arguments().isEmpty()) {
    const auto array = qvariant_cast<QDBusArgument>(units.arguments().first());
    array.beginArray();
    capabilities.uwsmActive = !array.atEnd();
    array.endArray();
  }
  return capabilities;
}
QString execute(ApplicationLaunchService::Backend backend, LauncherCommand command, const DesktopEntry* entry,
                const QString& action, const QDeadlineTimer& deadline) {
  if (deadline.hasExpired()) return "Application startup timed out; outcome uncertain";
  if (backend == ApplicationLaunchService::Backend::Uwsm) {
    QProcess helper;
    helper.setWorkingDirectory(command.working_dir);
    QStringList args{"app", "-t", "service", "--"};
    if (entry)
      args << (entry->desktop_file + (action.isEmpty() ? QString() : ":" + action));
    else {
      args << command.program;
      args << command.arguments;
    }

    helper.start("uwsm", args);
    if (!helper.waitForStarted(static_cast<int>(qMax<qint64>(0, deadline.remainingTime()))))
      return helper.errorString();
    if (!helper.waitForFinished(static_cast<int>(qMax<qint64>(0, deadline.remainingTime())))) {
      helper.kill();
      helper.waitForFinished(1000);
      return "Application startup timed out; outcome uncertain";
    }
    const QString error = QString::fromLocal8Bit(helper.readAllStandardError()).trimmed();
    return helper.exitStatus() == QProcess::NormalExit && helper.exitCode() == 0
               ? QString()
               : (error.isEmpty() ? QString("UWSM application launch failed") : error);
  }
  if (entry) {
    DesktopEntry selected = *entry;
    if (!action.isEmpty()) {
      bool found = false;
      for (const auto& item : entry->actions) {
        if (item.id == action) {
          selected.exec = item.exec;
          found = true;
          break;
        }
      }
      if (!found) return "Desktop action was not found";
    }
    QString terminal;
    if (selected.terminal) {
      QStringList candidates{QString::fromLocal8Bit(qgetenv("TERMINAL")).trimmed(),
                             "foot",
                             "kitty",
                             "alacritty",
                             "wezterm",
                             "konsole",
                             "gnome-terminal",
                             "xfce4-terminal",
                             "xterm"};
      for (const auto& candidate : candidates)
        if (!candidate.isEmpty() && !QStandardPaths::findExecutable(candidate).isEmpty()) {
          terminal = candidate;
          break;
        }
      if (terminal.isEmpty()) return "No terminal emulator is available";
    }
    const QString workingDirectory = command.working_dir;
    command = commandForDesktopEntry(selected, terminal);
    if (command.working_dir.isEmpty()) command.working_dir = workingDirectory;
  }
  command.program = QStandardPaths::findExecutable(command.program);
  if (command.program.isEmpty()) return "Application executable was not found";
  if (deadline.hasExpired()) return "Application startup timed out; outcome uncertain";
  if (backend == ApplicationLaunchService::Backend::Detached) {
    return QProcess::startDetached(command.program, command.arguments, command.working_dir)
               ? QString()
               : QString("Could not start application");
  }
  auto bus = QDBusConnection::sessionBus();
  QDBusInterface manager("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager",
                         bus);
  manager.setTimeout(static_cast<int>(qMax<qint64>(1, deadline.remainingTime())));
  if (deadline.hasExpired()) return "Application startup timed out; outcome uncertain";
  if (!manager.isValid()) return "Systemd user manager became inaccessible";
  Properties properties;
  auto add = [&properties](QString name, QVariant value) { properties.append({name, QDBusVariant(value)}); };
  add("Type", "exec");
  add("Slice", "app.slice");
  add("CollectMode", "inactive-or-failed");
  add("Restart", "no");
  add("StandardOutput", "journal");
  add("StandardError", "journal");
  const auto environment = ApplicationLaunchService::nativeEnvironment(QProcessEnvironment::systemEnvironment());
  add("Environment", environment.toStringList());
  if (!command.working_dir.isEmpty()) add("WorkingDirectory", command.working_dir);
  QDBusReply<QDBusObjectPath> target = manager.call("GetUnit", "graphical-session.target");
  if (target.isValid()) {
    QDBusInterface state("org.freedesktop.systemd1", target.value().path(), "org.freedesktop.systemd1.Unit", bus);
    if (state.property("ActiveState").toString() == "active") {
      add("PartOf", QStringList{"graphical-session.target"});
      add("After", QStringList{"graphical-session.target"});
    }
  }
  QStringList argv{command.program};
  argv << command.arguments;
  add("ExecStart", QVariant::fromValue(Execs{{command.program, argv, false}}));
  const QString unit = ApplicationLaunchService::nativeUnitName(command.program);
  ApplicationLaunchJobObserver observer;
  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&observer, &ApplicationLaunchJobObserver::finished, &loop, &QEventLoop::quit);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  if (!bus.connect("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager",
                   "JobRemoved", &observer, SLOT(jobRemoved(uint, QDBusObjectPath, QString, QString))))
    return "Could not monitor application startup";
  const auto subscription = manager.call("Subscribe");
  if (subscription.type() == QDBusMessage::ErrorMessage && !subscription.errorName().endsWith("AlreadySubscribed"))
    return subscription.errorMessage();
  if (deadline.hasExpired()) return "Application startup timed out; outcome uncertain";
  timeout.setTimerType(Qt::PreciseTimer);
  timeout.start(static_cast<int>(deadline.remainingTime()));
  manager.setTimeout(static_cast<int>(qMax<qint64>(1, deadline.remainingTime())));
  QDBusReply<QDBusObjectPath> reply = manager.call("StartTransientUnit", unit, "fail", QVariant::fromValue(properties),
                                                   QVariant::fromValue(Auxiliaries{}));
  if (!reply.isValid()) {
    if (reply.error().type() == QDBusError::NoReply || reply.error().type() == QDBusError::Timeout) {
      manager.call("StopUnit", unit, "replace");
      return "Application startup timed out; outcome uncertain";
    }
    return reply.error().message();
  }
  observer.jobPath = reply.value().path();
  observer.result = observer.results.value(observer.jobPath);
  if (observer.result.isEmpty() && timeout.isActive()) loop.exec();
  if (observer.result == "done" && !deadline.hasExpired()) return {};
  if (!observer.result.isEmpty() && observer.result != "done")
    return "Application startup job failed: " + observer.result;
  manager.call("StopUnit", unit, "replace");
  return "Application startup timed out; outcome uncertain";
}
}  // namespace
Q_DECLARE_METATYPE(Property)
Q_DECLARE_METATYPE(Properties)
Q_DECLARE_METATYPE(Exec)
Q_DECLARE_METATYPE(Execs)
Q_DECLARE_METATYPE(Auxiliary)
Q_DECLARE_METATYPE(Auxiliaries)
ApplicationLaunchService::ApplicationLaunchService(QObject* parent, Transport transport, Probe probe)
    : QObject(parent), transport_(std::move(transport)), probe_(std::move(probe)) {
  qDBusRegisterMetaType<Property>();
  qDBusRegisterMetaType<Properties>();
  qDBusRegisterMetaType<Exec>();
  qDBusRegisterMetaType<Execs>();
  qDBusRegisterMetaType<Auxiliary>();
  qDBusRegisterMetaType<Auxiliaries>();
}
QString ApplicationLaunchService::launch(LauncherCommand command, QObject* context, Completion completion) {
  return submit(command, {}, {}, context, std::move(completion));
}
QString ApplicationLaunchService::launchDesktop(DesktopEntry entry, QString action, QObject* context,
                                                Completion completion) {
  return submit({.working_dir = entry.path}, entry, action, context, std::move(completion));
}
QString ApplicationLaunchService::submit(LauncherCommand command, DesktopEntry entry, QString action, QObject* context,
                                         Completion completion) {
  if (command.working_dir.isEmpty()) command.working_dir = QDir::currentPath();
  const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const QDeadlineTimer deadline(15000, Qt::PreciseTimer);
  auto* watcher = new QFutureWatcher<QString>(context);
  auto* timeout = new QTimer(watcher);
  timeout->setSingleShot(true);
  timeout->setTimerType(Qt::PreciseTimer);
  auto completed = std::make_shared<bool>(false);
  auto callback = std::make_shared<Completion>(std::move(completion));
  connect(timeout, &QTimer::timeout, context, [watcher, id, completed, callback] {
    if (*completed) return;
    *completed = true;
    // A completed future can precede delivery of its queued finished signal.
    const QString result =
        watcher->isFinished() ? watcher->result() : QString("Application startup timed out; outcome uncertain");
    (*callback)(id, result);
  });
  connect(watcher, &QFutureWatcher<QString>::finished, context, [watcher, timeout, id, completed, callback] {
    timeout->stop();
    const QString result = watcher->result();
    watcher->deleteLater();
    if (*completed) return;
    *completed = true;
    (*callback)(id, result);
  });
  timeout->start(15000);
  watcher->setFuture(QtConcurrent::run([command, entry, action, transport = transport_, probe = probe_, deadline] {
    if (deadline.hasExpired()) return QString("Application startup timed out; outcome uncertain");
    const auto* desktop = entry.desktop_file.isEmpty() ? nullptr : &entry;
    const auto capabilities = probe ? probe() : probeCapabilities(deadline);
    if (deadline.hasExpired()) return QString("Application startup timed out; outcome uncertain");
    const auto backend = backendFor(capabilities);
    if (backend == Backend::Unavailable)
      return capabilities.error.isEmpty()
                 ? QString("Systemd user manager is inaccessible; refusing to launch in the shell service")
                 : capabilities.error;
    return transport ? transport(backend, command, desktop, action)
                     : execute(backend, command, desktop, action, deadline);
  }));
  return id;
}

ApplicationLaunchService::Backend ApplicationLaunchService::backendFor(const Capabilities& capabilities) {
  if (!capabilities.error.isEmpty()) return Backend::Unavailable;
  if (capabilities.managerAvailable) return capabilities.uwsmActive ? Backend::Uwsm : Backend::Systemd;
  return capabilities.runningAsService ? Backend::Unavailable : Backend::Detached;
}

QString ApplicationLaunchService::nativeUnitName(const QString& executable) {
  QString app = QFileInfo(executable).fileName();
  app.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "-");
  return "app-holonight-" + app + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".service";
}
QProcessEnvironment ApplicationLaunchService::nativeEnvironment(QProcessEnvironment environment) {
  for (const auto& key : environment.keys())
    if (key == "NOTIFY_SOCKET" || key == "INVOCATION_ID" || key == "SYSTEMD_EXEC_PID" || key.startsWith("LISTEN_"))
      environment.remove(key);
  return environment;
}
