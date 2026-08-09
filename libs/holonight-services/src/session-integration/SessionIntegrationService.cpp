#include "SessionIntegrationService.h"

#include "ApplicationCacheRebuilder.h"
#include "DesktopEntryScanner.h"
#include "DesktopFileUtils.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFuture>
#include <QFutureWatcher>
#include <QHash>
#include <QProcess>
#include <QStandardPaths>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

namespace {
constexpr std::array kRequiredEnvironment{
    "WAYLAND_DISPLAY", "XDG_CURRENT_DESKTOP", "XDG_SESSION_DESKTOP", "XDG_SESSION_TYPE",
    "XDG_MENU_PREFIX", "XDG_DATA_DIRS",       "XDG_CONFIG_DIRS",     "DBUS_SESSION_BUS_ADDRESS",
};

constexpr auto kHoloNightPortalService = "org.freedesktop.impl.portal.desktop.holonight";

constexpr std::array kDesktopEnvironment{
    "XDG_CURRENT_DESKTOP",
    "XDG_SESSION_DESKTOP",
    "XDG_SESSION_TYPE",
    "XDG_MENU_PREFIX",
};

class ProcessCommandRunner final : public ISessionIntegrationCommandRunner {
 public:
  [[nodiscard]] bool executableExists(const QString& program) const override {
    return !QStandardPaths::findExecutable(program).isEmpty();
  }

  [[nodiscard]] SessionIntegrationCommandResult run(const QString& program,
                                                    const QStringList& arguments) const override {
    QProcess proc;
    proc.setProgram(program);
    proc.setArguments(arguments);
    proc.start();
    if (!proc.waitForStarted(1000) || !proc.waitForFinished(5000)) {
      proc.kill();
      proc.waitForFinished(1000);
      return {.exit_code = -1, .stdout_text = {}, .stderr_text = proc.errorString()};
    }
    return {.exit_code = proc.exitCode(),
            .stdout_text = QString::fromUtf8(proc.readAllStandardOutput()).trimmed(),
            .stderr_text = QString::fromUtf8(proc.readAllStandardError()).trimmed()};
  }
};

class SessionBusProbe final : public ISessionIntegrationBusProbe {
 public:
  [[nodiscard]] bool isConnected() const override { return QDBusConnection::sessionBus().isConnected(); }

  [[nodiscard]] QString ownerForName(const QString& name) const override {
    auto* iface = QDBusConnection::sessionBus().interface();
    if (iface == nullptr) {
      return {};
    }
    const QDBusReply<QString> reply = iface->serviceOwner(name);
    return reply.isValid() ? reply.value() : QString{};
  }

  [[nodiscard]] QStringList registeredNames() const override {
    auto* iface = QDBusConnection::sessionBus().interface();
    if (iface == nullptr) {
      return {};
    }
    const QDBusReply<QStringList> reply = iface->registeredServiceNames();
    return reply.isValid() ? reply.value() : QStringList{};
  }
};

QHash<QString, QString> parseEnvironmentOutput(const QString& text) {
  QHash<QString, QString> parsed;
  for (const QString& raw_line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
    const qsizetype separator = raw_line.indexOf(QLatin1Char('='));
    if (separator <= 0) {
      continue;
    }
    parsed.insert(raw_line.left(separator), raw_line.mid(separator + 1));
  }
  return parsed;
}

void addUniqueCleanPath(QStringList* paths, const QString& path) {
  if (path.isEmpty()) {
    return;
  }
  const QString clean_path = QDir::cleanPath(path);
  if (!paths->contains(clean_path)) {
    paths->append(clean_path);
  }
}

QDateTime newestMatchingMtime(const QStringList& dirs, const QStringList& name_filters) {
  QDateTime newest;
  for (const QString& dir_path : dirs) {
    QDirIterator iter(dir_path, name_filters, QDir::Files, QDirIterator::Subdirectories);
    while (iter.hasNext()) {
      const QFileInfo info(iter.next());
      const QDateTime modified = info.lastModified();
      if (!newest.isValid() || modified > newest) {
        newest = modified;
      }
    }
  }
  return newest;
}

QString observedMtime(const QDateTime& timestamp) {
  return timestamp.isValid() ? timestamp.toString(Qt::ISODate) : QStringLiteral("missing");
}

}  // namespace

SessionIntegrationService::SessionIntegrationService(QObject* parent)
    : SessionIntegrationService(std::make_unique<ProcessCommandRunner>(), std::make_unique<SessionBusProbe>(),
                                QProcessEnvironment::systemEnvironment(), DesktopEntryScanner::defaultApplicationDirs(),
                                parent) {}

SessionIntegrationService::SessionIntegrationService(std::unique_ptr<ISessionIntegrationCommandRunner> command_runner,
                                                     std::unique_ptr<ISessionIntegrationBusProbe> bus_probe,
                                                     const QProcessEnvironment& environment,
                                                     QStringList application_dirs, QObject* parent)
    : QObject(parent),
      command_runner_(std::move(command_runner)),
      bus_probe_(std::move(bus_probe)),
      environment_(environment),
      application_dirs_(std::move(application_dirs)) {}

SessionIntegrationService::~SessionIntegrationService() {
  if (refresh_watcher_ != nullptr) {
    refresh_watcher_->future().waitForFinished();
  }
}

void SessionIntegrationService::refresh() {
  if (refresh_in_progress_) {
    refresh_pending_ = true;
    return;
  }
  setRefreshInProgress(true);

  using DiagFn = QVariantList (SessionIntegrationService::*)() const;
  static constexpr std::array<DiagFn, 7> kDiagnosticFns{
      &SessionIntegrationService::addProcessEnvironmentDiagnostics,
      &SessionIntegrationService::addSystemdEnvironmentDiagnostics,
      &SessionIntegrationService::addDbusActivationDiagnostics,
      &SessionIntegrationService::addXdgMenuDiagnostics,
      &SessionIntegrationService::addKdeCacheDiagnostics,
      &SessionIntegrationService::addMimeDiagnostics,
      &SessionIntegrationService::addPortalAndDesktopServiceDiagnostics,
  };

  // QtFuture::whenAll(begin, end) over QFuture<QVariantList> returns
  // QFuture<QList<QFuture<QVariantList>>> — a future of "the same futures, now all finished, in
  // input order" (not a QFuture<QVariantList> directly) — the watcher's template argument must
  // match that wrapped-list type, not the per-diagnostic result type.
  using CombinedFuture = QList<QFuture<QVariantList>>;
  auto* watcher = new QFutureWatcher<CombinedFuture>(this);
  refresh_watcher_ = watcher;
  QList<QFuture<QVariantList>> futures;
  futures.reserve(kDiagnosticFns.size());
  for (DiagFn diag_fn : kDiagnosticFns) {
    futures.append(QtConcurrent::run([this, diag_fn]() { return (this->*diag_fn)(); }));
  }
  watcher->setFuture(QtFuture::whenAll(futures.begin(), futures.end()));
  connect(watcher, &QFutureWatcher<CombinedFuture>::finished, this, [this, watcher]() {
    diagnostics_.clear();
    for (const QFuture<QVariantList>& future : watcher->result()) {
      diagnostics_.append(future.result());  // input order preserved by whenAll — deterministic row order
    }
    addLastRebuildDiagnostics();  // unchanged: reads last_rebuild_steps_, main-thread only
    overall_status_ = deriveOverallStatus();
    refresh_watcher_ = nullptr;
    watcher->deleteLater();
    setRefreshInProgress(false);
    emit diagnosticsChanged();
    if (std::exchange(refresh_pending_, false)) {
      refresh();
    }
  });
}

void SessionIntegrationService::setExpectedCursorTheme(QString cursor_theme) {
  {
    const QMutexLocker locker(&expected_cursor_mutex_);
    if (expected_cursor_theme_ == cursor_theme) {
      return;
    }
    expected_cursor_theme_ = std::move(cursor_theme);
  }
  refresh();
}

void SessionIntegrationService::setPostRebuildRefreshCallbacks(std::function<void()> refresh_mime_roles,
                                                               std::function<void()> refresh_launcher) {
  refresh_mime_roles_ = std::move(refresh_mime_roles);
  refresh_launcher_ = std::move(refresh_launcher);
}

void SessionIntegrationService::rebuildApplicationCaches() {
  if (rebuild_in_progress_) {
    return;
  }

  setRebuildInProgress(true);
  const ApplicationCacheRebuilder rebuilder(*command_runner_);
  last_rebuild_steps_ = rebuilder.rebuild(application_dirs_);
  const bool success =
      std::ranges::all_of(last_rebuild_steps_, [](const ApplicationCacheRebuildStep& step) { return step.success; });
  if (success) {
    if (refresh_mime_roles_) {
      refresh_mime_roles_();
    }
    if (refresh_launcher_) {
      refresh_launcher_();
    }
  }
  setRebuildInProgress(false);
  emit rebuildFinished(success);
  refresh();
}

QVariantMap SessionIntegrationService::addDiagnostic(const QString& diagnostic_id, const QString& title,
                                                     const QString& status, const QString& observed,
                                                     const QString& expected, const QString& detail,
                                                     const QString& command) {
  QVariantMap row;
  row.insert(QStringLiteral("id"), diagnostic_id);
  row.insert(QStringLiteral("title"), title);
  row.insert(QStringLiteral("status"), status);
  row.insert(QStringLiteral("observed"), observed);
  row.insert(QStringLiteral("expected"), expected);
  row.insert(QStringLiteral("detail"), detail);
  if (!command.isEmpty()) {
    row.insert(QStringLiteral("command"), command);
  }
  return row;
}

QVariantList SessionIntegrationService::addProcessEnvironmentDiagnostics() const {
  QVariantList rows;
  for (const auto* variable : kRequiredEnvironment) {
    const QString name = QString::fromLatin1(variable);
    const QString value = envValue(name);
    QString status = QStringLiteral("ok");
    if (value.isEmpty()) {
      status = name == QStringLiteral("DBUS_SESSION_BUS_ADDRESS") ? QStringLiteral("error") : QStringLiteral("warning");
    }
    rows.append(addDiagnostic(
        QStringLiteral("process-env-%1").arg(name.toLower().replace(QLatin1Char('_'), QLatin1Char('-'))),
        QStringLiteral("Process %1").arg(name), status, value.isEmpty() ? QStringLiteral("missing") : value,
        QStringLiteral("set in shell process environment"),
        value.isEmpty() ? QStringLiteral("The shell process is missing this desktop-session variable.")
                        : QStringLiteral("The shell process has this desktop-session variable.")));
  }
  const QString active_cursor = envValue(QStringLiteral("XCURSOR_THEME"));
  QString expected_cursor;
  {
    const QMutexLocker locker(&expected_cursor_mutex_);
    expected_cursor = expected_cursor_theme_;
  }
  const bool cursor_matches = !expected_cursor.isEmpty() && active_cursor == expected_cursor;
  rows.append(
      addDiagnostic(QStringLiteral("cursor-theme"), QStringLiteral("Session cursor theme"),
                    cursor_matches ? QStringLiteral("ok") : QStringLiteral("warning"),
                    active_cursor.isEmpty() ? QStringLiteral("native fallback") : active_cursor,
                    expected_cursor.isEmpty() ? QStringLiteral("canonical cursor unavailable") : expected_cursor,
                    cursor_matches ? QStringLiteral("The active session cursor matches canonical appearance.")
                                   : QStringLiteral("The canonical cursor will apply after a session restart.")));
  return rows;
}

QVariantList SessionIntegrationService::addSystemdEnvironmentDiagnostics() const {
  QVariantList rows;
  if (!command_runner_->executableExists(QStringLiteral("systemctl"))) {
    rows.append(addDiagnostic(QStringLiteral("systemd-user-env"), QStringLiteral("systemd user environment"),
                              QStringLiteral("info"), QStringLiteral("systemctl missing"), QStringLiteral("optional"),
                              QStringLiteral("systemd user manager environment could not be queried.")));
    return rows;
  }

  const SessionIntegrationCommandResult result =
      command_runner_->run(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("show-environment")});
  if (result.exit_code != 0) {
    rows.append(addDiagnostic(
        QStringLiteral("systemd-user-env"), QStringLiteral("systemd user environment"), QStringLiteral("info"),
        result.stderr_text.isEmpty() ? QStringLiteral("unavailable") : result.stderr_text, QStringLiteral("optional"),
        QStringLiteral("systemd user manager environment could not be queried."),
        QStringLiteral("systemctl --user show-environment")));
    return rows;
  }

  const QHash<QString, QString> systemd_env = parseEnvironmentOutput(result.stdout_text);
  for (const auto* variable : kRequiredEnvironment) {
    const QString name = QString::fromLatin1(variable);
    const QString process_value = envValue(name);
    const QString systemd_value = systemd_env.value(name);
    const bool mismatch = !process_value.isEmpty() && systemd_value != process_value;
    rows.append(addDiagnostic(
        QStringLiteral("systemd-env-%1").arg(name.toLower().replace(QLatin1Char('_'), QLatin1Char('-'))),
        QStringLiteral("systemd %1").arg(name), mismatch ? QStringLiteral("warning") : QStringLiteral("ok"),
        systemd_value.isEmpty() ? QStringLiteral("missing") : systemd_value,
        process_value.isEmpty() ? QStringLiteral("matches process environment") : process_value,
        mismatch ? QStringLiteral("D-Bus activated applications may inherit an incomplete desktop environment.")
                 : QStringLiteral("systemd user environment matches the process value."),
        QStringLiteral("systemctl --user show-environment")));
  }
  return rows;
}

QVariantList SessionIntegrationService::addDbusActivationDiagnostics() const {
  QVariantList rows;
  const QString bus_address = envValue(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"));
  rows.append(addDiagnostic(QStringLiteral("dbus-activation-address"), QStringLiteral("D-Bus session bus"),
                            bus_address.isEmpty() ? QStringLiteral("error") : QStringLiteral("ok"),
                            bus_address.isEmpty() ? QStringLiteral("missing") : bus_address,
                            QStringLiteral("DBUS_SESSION_BUS_ADDRESS is set"),
                            bus_address.isEmpty()
                                ? QStringLiteral("D-Bus activated applications cannot inherit a session bus.")
                                : QStringLiteral("D-Bus activation can see the session bus address."),
                            QStringLiteral("dbus-update-activation-environment --systemd DBUS_SESSION_BUS_ADDRESS")));

  QStringList missing;
  for (const auto* variable : kDesktopEnvironment) {
    const QString name = QString::fromLatin1(variable);
    if (envValue(name).isEmpty()) {
      missing.append(name);
    }
  }
  rows.append(addDiagnostic(
      QStringLiteral("dbus-activation-desktop-env"), QStringLiteral("D-Bus desktop environment"),
      missing.isEmpty() ? QStringLiteral("ok") : QStringLiteral("warning"),
      missing.isEmpty() ? QStringLiteral("complete") : missing.join(QStringLiteral(", ")),
      QStringLiteral("desktop variables are available for activation"),
      missing.isEmpty() ? QStringLiteral("Desktop variables are available to import for D-Bus activation.")
                        : QStringLiteral("Import desktop variables with the session bootstrap command."),
      QStringLiteral("dbus-update-activation-environment --systemd XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP "
                     "XDG_SESSION_TYPE XDG_MENU_PREFIX")));
  return rows;
}

QVariantList SessionIntegrationService::addXdgMenuDiagnostics() const {
  const QString prefix = envValue(QStringLiteral("XDG_MENU_PREFIX"));
  const QString selected = prefix + QStringLiteral("applications.menu");
  const QStringList menu_dirs = xdgMenuSearchPaths();
  const QStringList menus = discoverApplicationMenus(menu_dirs);
  const bool selected_exists = menus.contains(selected);

  QString status = QStringLiteral("ok");
  QString detail = QStringLiteral("Selected XDG application menu exists.");
  if (prefix.isEmpty()) {
    status = command_runner_->executableExists(QStringLiteral("kbuildsycoca6")) ? QStringLiteral("warning")
                                                                                : QStringLiteral("info");
    detail = QStringLiteral("XDG_MENU_PREFIX is empty; KDE frameworks may build an incomplete application menu.");
  } else if (!selected_exists) {
    status = QStringLiteral("warning");
    detail = QStringLiteral("No matching menu file was found in XDG menu paths.");
  }

  QVariantList rows;
  rows.append(addDiagnostic(QStringLiteral("xdg-menu-prefix"), QStringLiteral("XDG menu prefix"), status,
                            prefix.isEmpty() ? QStringLiteral("empty") : prefix, selected + QStringLiteral(" exists"),
                            detail, QStringLiteral("find ~/.config/menus /etc/xdg/menus -name '*applications.menu'")));
  rows.append(addDiagnostic(QStringLiteral("xdg-menu-candidates"), QStringLiteral("Available XDG menus"),
                            QStringLiteral("info"),
                            menus.isEmpty() ? QStringLiteral("none") : menus.join(QStringLiteral(", ")),
                            QStringLiteral("*applications.menu files in XDG menu paths"),
                            QStringLiteral("Discovered application menu files.")));
  return rows;
}

QVariantList SessionIntegrationService::addKdeCacheDiagnostics() const {
  if (!command_runner_->executableExists(QStringLiteral("kbuildsycoca6"))) {
    return {addDiagnostic(QStringLiteral("kde-sycoca"), QStringLiteral("KDE application cache"), QStringLiteral("info"),
                          QStringLiteral("kbuildsycoca6 missing"), QStringLiteral("optional"),
                          QStringLiteral("KDE sycoca checks are skipped because kbuildsycoca6 is not installed."))};
  }

  const QStringList cache_dirs{xdgCacheHome()};
  const QDateTime newest_sycoca = newestMatchingMtime(cache_dirs, {QStringLiteral("ksycoca6*")});
  const QDateTime newest_desktop = newestMatchingMtime(application_dirs_, {QStringLiteral("*.desktop")});
  const QDateTime newest_mime_cache = newestMatchingMtime(application_dirs_, {QStringLiteral("mimeinfo.cache")});
  const QDateTime newest_input = std::max(newest_desktop, newest_mime_cache);

  QString status = QStringLiteral("ok");
  QString detail = QStringLiteral("KDE sycoca cache exists and is current.");
  if (!newest_sycoca.isValid()) {
    status = QStringLiteral("warning");
    detail = QStringLiteral("No ksycoca6 cache was found. Rebuild application caches.");
  } else if (newest_input.isValid() && newest_input > newest_sycoca) {
    status = QStringLiteral("warning");
    detail = QStringLiteral("A desktop entry or MIME cache is newer than KDE sycoca. Rebuild application caches.");
  }

  return {addDiagnostic(
      QStringLiteral("kde-sycoca"), QStringLiteral("KDE application cache"), status,
      QStringLiteral("sycoca=%1; desktop=%2; mimeinfo=%3")
          .arg(observedMtime(newest_sycoca), observedMtime(newest_desktop), observedMtime(newest_mime_cache)),
      QStringLiteral("ksycoca6 is present and newer than application metadata"), detail,
      QStringLiteral("kbuildsycoca6 --noincremental"))};
}

QVariantList SessionIntegrationService::addMimeDiagnostics() const {
  QVariantList rows;
  struct RoleCheck {
    QString id;
    QString title;
    QString program;
    QStringList arguments;
  };
  const QList<RoleCheck> roles{
      {.id = QStringLiteral("browser"),
       .title = QStringLiteral("Default browser"),
       .program = QStringLiteral("xdg-settings"),
       .arguments = {QStringLiteral("get"), QStringLiteral("default-web-browser")}},
      {.id = QStringLiteral("terminal"),
       .title = QStringLiteral("Default terminal"),
       .program = QStringLiteral("xdg-mime"),
       .arguments = {QStringLiteral("query"), QStringLiteral("default"),
                     QStringLiteral("application/x-terminal-emulator")}},
      {.id = QStringLiteral("file-manager"),
       .title = QStringLiteral("Default file manager"),
       .program = QStringLiteral("xdg-mime"),
       .arguments = {QStringLiteral("query"), QStringLiteral("default"), QStringLiteral("inode/directory")}},
      {.id = QStringLiteral("image-viewer"),
       .title = QStringLiteral("Default image viewer"),
       .program = QStringLiteral("xdg-mime"),
       .arguments = {QStringLiteral("query"), QStringLiteral("default"), QStringLiteral("image/png")}},
      {.id = QStringLiteral("text-editor"),
       .title = QStringLiteral("Default text editor"),
       .program = QStringLiteral("xdg-mime"),
       .arguments = {QStringLiteral("query"), QStringLiteral("default"), QStringLiteral("text/plain")}},
      {.id = QStringLiteral("video-player"),
       .title = QStringLiteral("Default video player"),
       .program = QStringLiteral("xdg-mime"),
       .arguments = {QStringLiteral("query"), QStringLiteral("default"), QStringLiteral("video/mp4")}},
  };

  for (const RoleCheck& role : roles) {
    if (!command_runner_->executableExists(role.program)) {
      rows.append(addDiagnostic(QStringLiteral("mime-role-%1").arg(role.id), role.title, QStringLiteral("info"),
                                role.program + QStringLiteral(" missing"), QStringLiteral("optional"),
                                QStringLiteral("Default application role could not be queried.")));
      continue;
    }
    const SessionIntegrationCommandResult result = command_runner_->run(role.program, role.arguments);
    rows.append(addDiagnostic(QStringLiteral("mime-role-%1").arg(role.id), role.title,
                              result.exit_code == 0 ? QStringLiteral("ok") : QStringLiteral("warning"),
                              result.stdout_text.isEmpty() ? QStringLiteral("none") : result.stdout_text,
                              QStringLiteral("desktop file returned by xdg tools"),
                              result.exit_code == 0 ? QStringLiteral("Default application role was queried.")
                                                    : QStringLiteral("Default application role query failed."),
                              role.program + QLatin1Char(' ') + role.arguments.join(QLatin1Char(' '))));
  }

  for (const QString& dir_path : application_dirs_) {
    const QFileInfo dir_info(dir_path);
    if (!dir_info.exists() || !dir_info.isDir() || !DesktopFileUtils::containsDesktopFiles(dir_path)) {
      continue;
    }
    const QFileInfo cache_info(QDir(dir_path).filePath(QStringLiteral("mimeinfo.cache")));
    const bool writable = dir_info.isWritable();
    QString status = QStringLiteral("ok");
    if (!cache_info.exists()) {
      status = writable ? QStringLiteral("warning") : QStringLiteral("info");
    }
    rows.append(addDiagnostic(
        QStringLiteral("mime-cache-%1").arg(QString(dir_path).replace(QLatin1Char('/'), QLatin1Char('-'))),
        QStringLiteral("MIME cache %1").arg(dir_path), status,
        cache_info.exists() ? cache_info.absoluteFilePath() : QStringLiteral("missing"),
        QStringLiteral("mimeinfo.cache exists when desktop files are present"),
        cache_info.exists() ? QStringLiteral("Application directory has a MIME cache.")
                            : QStringLiteral("Application directory has desktop files but no MIME cache.")));
  }
  return rows;
}

QVariantList SessionIntegrationService::addPortalAndDesktopServiceDiagnostics() const {
  if (!bus_probe_->isConnected()) {
    return {
        addDiagnostic(QStringLiteral("dbus-session-services"), QStringLiteral("D-Bus desktop services"),
                      QStringLiteral("error"), QStringLiteral("session bus unavailable"),
                      QStringLiteral("session bus connected"),
                      QStringLiteral("Portal and desktop service ownership could not be queried.")),
        addDiagnostic(QStringLiteral("holonight-settings-portal"), QStringLiteral("HoloNight Settings portal"),
                      QStringLiteral("warning"), QStringLiteral("unavailable"),
                      QStringLiteral("org.freedesktop.impl.portal.desktop.holonight is owned by HoloNight Shell"),
                      QStringLiteral("Session bus is disconnected; native toolkit appearance remains available.")),
    };
  }

  QVariantList rows;
  const QString portal_owner = bus_probe_->ownerForName(QStringLiteral("org.freedesktop.portal.Desktop"));
  rows.append(addDiagnostic(QStringLiteral("portal-broker"), QStringLiteral("Portal broker"),
                            portal_owner.isEmpty() ? QStringLiteral("error") : QStringLiteral("ok"),
                            portal_owner.isEmpty() ? QStringLiteral("unowned") : portal_owner,
                            QStringLiteral("org.freedesktop.portal.Desktop is owned"),
                            portal_owner.isEmpty()
                                ? QStringLiteral("xdg-desktop-portal is not available on the session bus.")
                                : QStringLiteral("Portal broker is owned on the session bus.")));

  const QStringList names = bus_probe_->registeredNames();
  const QString holonight_portal_owner = bus_probe_->ownerForName(QString::fromLatin1(kHoloNightPortalService));
  const bool holonight_portal_registered = names.contains(QString::fromLatin1(kHoloNightPortalService));
  QString holonight_portal_observed = QStringLiteral("missing");
  if (!holonight_portal_owner.isEmpty()) {
    holonight_portal_observed = QStringLiteral("owned by %1").arg(holonight_portal_owner);
  } else if (holonight_portal_registered) {
    holonight_portal_observed = QStringLiteral("unavailable");
  }
  rows.append(addDiagnostic(
      QStringLiteral("holonight-settings-portal"), QStringLiteral("HoloNight Settings portal"),
      !holonight_portal_owner.isEmpty() ? QStringLiteral("ok") : QStringLiteral("warning"), holonight_portal_observed,
      QStringLiteral("org.freedesktop.impl.portal.desktop.holonight is owned by HoloNight Shell"),
      !holonight_portal_owner.isEmpty()
          ? QStringLiteral("HoloNight Shell is publishing the Settings portal.")
          : QStringLiteral("Start HoloNight Shell; native toolkit appearance remains available as fallback.")));

  QStringList portal_backends;
  for (const QString& name : names) {
    if (name.startsWith(QStringLiteral("org.freedesktop.impl.portal."))) {
      portal_backends.append(name);
    }
  }
  portal_backends.sort();
  rows.append(
      addDiagnostic(QStringLiteral("portal-backends"), QStringLiteral("Portal implementations"),
                    portal_backends.isEmpty() ? QStringLiteral("warning") : QStringLiteral("ok"),
                    portal_backends.isEmpty() ? QStringLiteral("none") : portal_backends.join(QStringLiteral(", ")),
                    QStringLiteral("Hyprland plus toolkit portal backend"),
                    portal_backends.isEmpty() ? QStringLiteral("No active portal implementation names were found.")
                                              : QStringLiteral("Active portal implementation names were found.")));

  const QList<QPair<QString, QString>> desktop_services{
      {QStringLiteral("screensaver"), QStringLiteral("org.freedesktop.ScreenSaver")},
      {QStringLiteral("notifications"), QStringLiteral("org.freedesktop.Notifications")},
      {QStringLiteral("status-notifier"), QStringLiteral("org.kde.StatusNotifierWatcher")},
  };
  for (const auto& service : desktop_services) {
    const QString owner = bus_probe_->ownerForName(service.second);
    rows.append(addDiagnostic(QStringLiteral("dbus-service-%1").arg(service.first), service.second,
                              owner.isEmpty() ? QStringLiteral("warning") : QStringLiteral("ok"),
                              owner.isEmpty() ? QStringLiteral("unowned") : owner,
                              QStringLiteral("owned on session bus"),
                              owner.isEmpty() ? QStringLiteral("Desktop service is not currently owned.")
                                              : QStringLiteral("Desktop service is owned on the session bus.")));
  }
  return rows;
}

void SessionIntegrationService::addLastRebuildDiagnostics() {
  if (last_rebuild_steps_.isEmpty()) {
    return;
  }

  for (qsizetype i = 0; i < last_rebuild_steps_.size(); ++i) {
    const ApplicationCacheRebuildStep& step = last_rebuild_steps_.at(i);
    QString detail = QStringLiteral("Cache rebuild command completed.");
    if (!step.success) {
      detail = step.stderr_text.isEmpty() ? QStringLiteral("Cache rebuild command failed.") : step.stderr_text;
    }
    diagnostics_.append(addDiagnostic(QStringLiteral("application-cache-rebuild-%1").arg(i + 1),
                                      QStringLiteral("Application cache rebuild step"),
                                      step.success ? QStringLiteral("ok") : QStringLiteral("warning"),
                                      QStringLiteral("%1 exited %2").arg(step.commandLine()).arg(step.exit_code),
                                      QStringLiteral("cache rebuild command succeeds"), detail, step.commandLine()));
  }
}

QString SessionIntegrationService::envValue(const QString& name) const { return environment_.value(name).trimmed(); }

QStringList SessionIntegrationService::xdgMenuSearchPaths() const {
  QStringList paths;
  const QString config_home = envValue(QStringLiteral("XDG_CONFIG_HOME")).isEmpty()
                                  ? QDir::homePath() + QStringLiteral("/.config")
                                  : envValue(QStringLiteral("XDG_CONFIG_HOME"));
  addUniqueCleanPath(&paths, config_home + QStringLiteral("/menus"));

  const QString config_dirs_value = envValue(QStringLiteral("XDG_CONFIG_DIRS"));
  const QStringList config_dirs = config_dirs_value.isEmpty()
                                      ? QStringList{QStringLiteral("/etc/xdg")}
                                      : config_dirs_value.split(QLatin1Char(':'), Qt::SkipEmptyParts);
  for (const QString& config_dir : config_dirs) {
    addUniqueCleanPath(&paths, config_dir + QStringLiteral("/menus"));
  }
  addUniqueCleanPath(&paths, QStringLiteral("/etc/xdg/menus"));
  return paths;
}

QStringList SessionIntegrationService::discoverApplicationMenus(const QStringList& menu_dirs) {
  QStringList menus;
  for (const QString& dir_path : menu_dirs) {
    QDir dir(dir_path);
    for (const QString& name : dir.entryList({QStringLiteral("*applications.menu")}, QDir::Files, QDir::Name)) {
      if (!menus.contains(name)) {
        menus.append(name);
      }
    }
  }
  menus.sort();
  return menus;
}

QString SessionIntegrationService::xdgCacheHome() const {
  const QString cache_home = envValue(QStringLiteral("XDG_CACHE_HOME"));
  return cache_home.isEmpty() ? QDir::homePath() + QStringLiteral("/.cache") : cache_home;
}

QString SessionIntegrationService::deriveOverallStatus() const {
  bool has_warning = false;
  for (const QVariant& diagnostic : diagnostics_) {
    const QString status = diagnostic.toMap().value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("error")) {
      return QStringLiteral("error");
    }
    has_warning = has_warning || status == QStringLiteral("warning");
  }
  return has_warning ? QStringLiteral("warning") : QStringLiteral("ok");
}

void SessionIntegrationService::setRefreshInProgress(bool val) {
  if (refresh_in_progress_ == val) {
    return;
  }
  refresh_in_progress_ = val;
  emit refreshInProgressChanged();
}

void SessionIntegrationService::setRebuildInProgress(bool val) {
  if (rebuild_in_progress_ == val) {
    return;
  }
  rebuild_in_progress_ = val;
  emit rebuildInProgressChanged();
}
