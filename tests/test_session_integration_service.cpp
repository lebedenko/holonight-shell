#include "ApplicationCacheRebuilder.h"
#include "DesktopFileUtils.h"
#include "SessionIntegrationService.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QMutex>
#include <QProcessEnvironment>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QThreadPool>
#include <QVariantMap>

#include <algorithm>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <utility>

namespace {
class FakeCommandRunner final : public ISessionIntegrationCommandRunner {
 public:
  QSet<QString> executables;
  QHash<QString, SessionIntegrationCommandResult> results;
  mutable QStringList calls;
  mutable QMutex mutex;

  [[nodiscard]] bool executableExists(const QString& program) const override {
    const QMutexLocker locker(&mutex);
    return executables.contains(program);
  }

  [[nodiscard]] SessionIntegrationCommandResult run(const QString& program,
                                                    const QStringList& arguments) const override {
    const QMutexLocker locker(&mutex);
    calls.append(program + QLatin1Char(' ') + arguments.join(QLatin1Char(' ')));
    return results.value(program + QLatin1Char(' ') + arguments.join(QLatin1Char(' ')),
                         {.exit_code = -1, .stdout_text = {}, .stderr_text = QStringLiteral("missing fake result")});
  }
};

class FakeBusProbe final : public ISessionIntegrationBusProbe {
 public:
  bool connected{true};
  QHash<QString, QString> owners;
  QStringList names;

  [[nodiscard]] bool isConnected() const override { return connected; }
  [[nodiscard]] QString ownerForName(const QString& name) const override { return owners.value(name); }
  [[nodiscard]] QStringList registeredNames() const override { return names; }
};

QVariantMap findDiagnostic(const QVariantList& diagnostics, const QString& diagnostic_id) {
  for (const QVariant& diagnostic : diagnostics) {
    const QVariantMap row = diagnostic.toMap();
    if (row.value(QStringLiteral("id")).toString() == diagnostic_id) {
      return row;
    }
  }
  return {};
}

void writeFile(const QString& path, const QByteArray& content = "x") {
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  ASSERT_EQ(file.write(content), content.size());
}

void setMtime(const QString& path, const QDateTime& timestamp) {
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  ASSERT_TRUE(file.setFileTime(timestamp, QFileDevice::FileModificationTime));
}

QProcessEnvironment baseEnvironment(const QString& root) {
  QProcessEnvironment env;
  env.insert(QStringLiteral("WAYLAND_DISPLAY"), QStringLiteral("wayland-1"));
  env.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), QStringLiteral("Hyprland"));
  env.insert(QStringLiteral("XDG_SESSION_DESKTOP"), QStringLiteral("Hyprland"));
  env.insert(QStringLiteral("XDG_SESSION_TYPE"), QStringLiteral("wayland"));
  env.insert(QStringLiteral("XDG_MENU_PREFIX"), QStringLiteral("hyprland-"));
  env.insert(QStringLiteral("XDG_DATA_DIRS"), root + QStringLiteral("/data"));
  env.insert(QStringLiteral("XDG_CONFIG_HOME"), root + QStringLiteral("/config-home"));
  env.insert(QStringLiteral("XDG_CONFIG_DIRS"),
             root + QStringLiteral("/config-a:") + root + QStringLiteral("/config-b"));
  env.insert(QStringLiteral("XDG_CACHE_HOME"), root + QStringLiteral("/cache"));
  env.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=/tmp/bus"));
  return env;
}

SessionIntegrationService makeService(std::unique_ptr<FakeCommandRunner> runner, std::unique_ptr<FakeBusProbe> bus,
                                      const QProcessEnvironment& env, const QStringList& app_dirs) {
  return {std::move(runner), std::move(bus), env, app_dirs};
}

// refresh()/rebuildApplicationCaches() are synchronous today but will dispatch diagnostics
// collection onto QtConcurrent futures (see DESIGN.md Item 2), after which diagnosticsChanged()
// fires once the main-thread event loop processes the futures' completion instead of before the
// call returns. Constructing the spy before triggering the action and short-circuiting when it
// already fired keeps these helpers correct both before and after that change lands.
void refreshAndWait(SessionIntegrationService& service) {
  QSignalSpy spy(&service, &SessionIntegrationService::diagnosticsChanged);
  service.refresh();
  if (spy.isEmpty()) {
    ASSERT_TRUE(spy.wait(2000));
  }
}

void rebuildAndWait(SessionIntegrationService& service) {
  QSignalSpy spy(&service, &SessionIntegrationService::diagnosticsChanged);
  service.rebuildApplicationCaches();
  if (spy.isEmpty()) {
    ASSERT_TRUE(spy.wait(2000));
  }
}

// Records one [start, end] interval (relative to a shared QElapsedTimer) per calling thread, the
// first time that thread calls into either fake below, then sleeps `delay_ms`. Only 5 of the 7
// add*Diagnostics() methods make any external call (command runner or bus probe) — the other 2
// (process-environment, D-Bus-activation-address) read only in-process environment variables and
// cannot be delayed without touching production code — so "delay once per thread" (rather than once
// per call) keeps methods with multiple external calls (e.g. addMimeDiagnostics's 6 role checks)
// from accumulating 6x the delay while still giving each *diagnostic* (i.e. each QtConcurrent worker
// thread) exactly one measurable interval to compare for overlap.
class DelayTimeline {
 public:
  struct Interval {
    qint64 start_ms{};
    qint64 end_ms{};
  };

  DelayTimeline() { clock_.start(); }

  void delayOncePerThread(int delay_ms) {
    QThread* thread = QThread::currentThread();
    {
      const QMutexLocker locker(&mutex_);
      if (delayed_threads_.contains(thread)) {
        return;
      }
      delayed_threads_.insert(thread);
    }
    const qint64 start = clock_.elapsed();
    QThread::msleep(delay_ms);
    const qint64 end = clock_.elapsed();
    const QMutexLocker locker(&mutex_);
    intervals_.append({.start_ms = start, .end_ms = end});
  }

  [[nodiscard]] QVector<Interval> intervals() const {
    const QMutexLocker locker(&mutex_);
    return intervals_;
  }

 private:
  QElapsedTimer clock_;
  mutable QMutex mutex_;
  QVector<Interval> intervals_;
  QSet<QThread*> delayed_threads_;
};

bool anyIntervalsOverlap(const QVector<DelayTimeline::Interval>& intervals) {
  for (qsizetype i = 0; i < intervals.size(); ++i) {
    for (qsizetype j = i + 1; j < intervals.size(); ++j) {
      if (intervals.at(i).start_ms < intervals.at(j).end_ms && intervals.at(j).start_ms < intervals.at(i).end_ms) {
        return true;
      }
    }
  }
  return false;
}

class DelayingCommandRunner final : public ISessionIntegrationCommandRunner {
 public:
  DelayingCommandRunner(std::shared_ptr<DelayTimeline> timeline, int delay_ms)
      : timeline_(std::move(timeline)), delay_ms_(delay_ms) {}

  [[nodiscard]] bool executableExists(const QString& /*program*/) const override {
    timeline_->delayOncePerThread(delay_ms_);
    return true;
  }

  [[nodiscard]] SessionIntegrationCommandResult run(const QString& /*program*/,
                                                    const QStringList& /*arguments*/) const override {
    timeline_->delayOncePerThread(delay_ms_);
    return {.exit_code = 0, .stdout_text = QStringLiteral("ok"), .stderr_text = {}};
  }

 private:
  std::shared_ptr<DelayTimeline> timeline_;
  int delay_ms_;
};

class DelayingBusProbe final : public ISessionIntegrationBusProbe {
 public:
  DelayingBusProbe(std::shared_ptr<DelayTimeline> timeline, int delay_ms)
      : timeline_(std::move(timeline)), delay_ms_(delay_ms) {}

  [[nodiscard]] bool isConnected() const override {
    timeline_->delayOncePerThread(delay_ms_);
    return true;
  }

  [[nodiscard]] QString ownerForName(const QString& /*name*/) const override {
    timeline_->delayOncePerThread(delay_ms_);
    return QStringLiteral(":1.99");
  }

  [[nodiscard]] QStringList registeredNames() const override {
    timeline_->delayOncePerThread(delay_ms_);
    return {};
  }

 private:
  std::shared_ptr<DelayTimeline> timeline_;
  int delay_ms_;
};

// QtConcurrent::run() dispatches onto QThreadPool::globalInstance(); on a CI host with few cores its
// default maxThreadCount() could be lower than the 7 diagnostics dispatched by refresh(), making some
// futures queue and run sequentially rather than overlapping. Temporarily raising the cap (never
// lowering it) removes that source of flakiness; the destructor restores the original value
// unconditionally, including on early ASSERT_* returns.
class ThreadPoolCapacityGuard {
 public:
  explicit ThreadPoolCapacityGuard(int min_threads) : previous_(QThreadPool::globalInstance()->maxThreadCount()) {
    QThreadPool::globalInstance()->setMaxThreadCount(std::max(previous_, min_threads));
  }
  ~ThreadPoolCapacityGuard() { QThreadPool::globalInstance()->setMaxThreadCount(previous_); }

  ThreadPoolCapacityGuard(const ThreadPoolCapacityGuard&) = delete;
  ThreadPoolCapacityGuard& operator=(const ThreadPoolCapacityGuard&) = delete;
  ThreadPoolCapacityGuard(ThreadPoolCapacityGuard&&) = delete;
  ThreadPoolCapacityGuard& operator=(ThreadPoolCapacityGuard&&) = delete;

 private:
  int previous_;
};

// FakeCommandRunner's `calls` list is unguarded (fine for tests that trigger only one call site at a
// time), but rebuildApplicationCaches() now runs its trailing refresh() diagnostics on QtConcurrent
// worker threads that call run() concurrently with each other, so a call-count spy for REQ-F-006 needs
// its own mutex.
class ThreadSafeSpyCommandRunner final : public ISessionIntegrationCommandRunner {
 public:
  QSet<QString> executables;
  QHash<QString, SessionIntegrationCommandResult> results;
  mutable QMutex mutex;
  mutable QStringList calls;

  [[nodiscard]] bool executableExists(const QString& program) const override { return executables.contains(program); }

  [[nodiscard]] SessionIntegrationCommandResult run(const QString& program,
                                                    const QStringList& arguments) const override {
    const QString call = program + QLatin1Char(' ') + arguments.join(QLatin1Char(' '));
    {
      const QMutexLocker locker(&mutex);
      calls.append(call);
    }
    return results.value(call, {.exit_code = 0, .stdout_text = {}, .stderr_text = {}});
  }
};
}  // namespace

TEST(SessionIntegrationServiceTest, ReportsProcessEnvironmentRows) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  auto runner = std::make_unique<FakeCommandRunner>();
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service = makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {});

  refreshAndWait(service);

  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("process-env-wayland-display"))
                .value(QStringLiteral("observed"))
                .toString(),
            QStringLiteral("wayland-1"));
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("process-env-xdg-current-desktop"))
                .value(QStringLiteral("observed"))
                .toString(),
            QStringLiteral("Hyprland"));
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("process-env-dbus-session-bus-address"))
                .value(QStringLiteral("status"))
                .toString(),
            QStringLiteral("ok"));
}

TEST(SessionIntegrationServiceTest, ReportsMissingSystemdDesktopEnvironmentAsWarning) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  auto runner = std::make_unique<FakeCommandRunner>();
  runner->executables.insert(QStringLiteral("systemctl"));
  runner->results.insert(QStringLiteral("systemctl --user show-environment"),
                         {.exit_code = 0,
                          .stdout_text = QStringLiteral("WAYLAND_DISPLAY=wayland-1\nXDG_SESSION_TYPE=wayland\n"),
                          .stderr_text = {}});
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service = makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {});

  refreshAndWait(service);

  const QVariantMap row = findDiagnostic(service.diagnostics(), QStringLiteral("systemd-env-xdg-current-desktop"));
  EXPECT_EQ(row.value(QStringLiteral("status")).toString(), QStringLiteral("warning"));
  EXPECT_EQ(row.value(QStringLiteral("observed")).toString(), QStringLiteral("missing"));
}

TEST(SessionIntegrationServiceTest, ParsesSystemdEnvironmentValuesWithEquals) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  auto runner = std::make_unique<FakeCommandRunner>();
  runner->executables.insert(QStringLiteral("systemctl"));
  runner->results.insert(
      QStringLiteral("systemctl --user show-environment"),
      {.exit_code = 0,
       .stdout_text = QStringLiteral("IGNORED_LINE\n"
                                     "WAYLAND_DISPLAY=wayland-1\n"
                                     "XDG_CURRENT_DESKTOP=Hyprland\n"
                                     "XDG_SESSION_DESKTOP=Hyprland\n"
                                     "XDG_SESSION_TYPE=wayland\n"
                                     "XDG_MENU_PREFIX=hyprland-\n"
                                     "XDG_DATA_DIRS=/usr/local/share:/usr/share\n"
                                     "XDG_CONFIG_DIRS=/etc/xdg\n"
                                     "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus,guid=a=b=c\n"),
       .stderr_text = {}});
  auto bus = std::make_unique<FakeBusProbe>();
  QProcessEnvironment env = baseEnvironment(temp.path());
  env.insert(QStringLiteral("XDG_DATA_DIRS"), QStringLiteral("/usr/local/share:/usr/share"));
  env.insert(QStringLiteral("XDG_CONFIG_DIRS"), QStringLiteral("/etc/xdg"));
  env.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=/run/user/1000/bus,guid=a=b=c"));
  SessionIntegrationService service = makeService(std::move(runner), std::move(bus), env, {});

  refreshAndWait(service);

  const QVariantMap row = findDiagnostic(service.diagnostics(), QStringLiteral("systemd-env-dbus-session-bus-address"));
  EXPECT_EQ(row.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
  EXPECT_EQ(row.value(QStringLiteral("observed")).toString(),
            QStringLiteral("unix:path=/run/user/1000/bus,guid=a=b=c"));
}

TEST(SessionIntegrationServiceTest, ClassifiesDbusActivationEnvironment) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QProcessEnvironment env = baseEnvironment(temp.path());
  env.remove(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"));
  env.remove(QStringLiteral("XDG_SESSION_DESKTOP"));
  auto runner = std::make_unique<FakeCommandRunner>();
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service = makeService(std::move(runner), std::move(bus), env, {});

  refreshAndWait(service);

  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("dbus-activation-address"))
                .value(QStringLiteral("status"))
                .toString(),
            QStringLiteral("error"));
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("dbus-activation-desktop-env"))
                .value(QStringLiteral("status"))
                .toString(),
            QStringLiteral("warning"));
  EXPECT_EQ(service.overallStatus(), QStringLiteral("error"));
}

TEST(SessionIntegrationServiceTest, DiscoversXdgMenuSearchPathsAndWarnsForMissingSelectedMenu) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QDir root(temp.path());
  ASSERT_TRUE(root.mkpath(QStringLiteral("config-home/menus")));
  ASSERT_TRUE(root.mkpath(QStringLiteral("config-a/menus")));
  ASSERT_TRUE(root.mkpath(QStringLiteral("config-b/menus")));
  writeFile(root.filePath(QStringLiteral("config-home/menus/arch-applications.menu")));
  writeFile(root.filePath(QStringLiteral("config-b/menus/lxqt-applications.menu")));
  auto runner = std::make_unique<FakeCommandRunner>();
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service = makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {});

  refreshAndWait(service);

  const QVariantMap prefix = findDiagnostic(service.diagnostics(), QStringLiteral("xdg-menu-prefix"));
  const QVariantMap candidates = findDiagnostic(service.diagnostics(), QStringLiteral("xdg-menu-candidates"));
  EXPECT_EQ(prefix.value(QStringLiteral("status")).toString(), QStringLiteral("warning"));
  EXPECT_THAT(candidates.value(QStringLiteral("observed")).toString().toStdString(),
              testing::HasSubstr("arch-applications.menu"));
  EXPECT_THAT(candidates.value(QStringLiteral("observed")).toString().toStdString(),
              testing::HasSubstr("lxqt-applications.menu"));
}

TEST(SessionIntegrationServiceTest, ClassifiesExistingSelectedXdgMenuAsOk) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QDir root(temp.path());
  ASSERT_TRUE(root.mkpath(QStringLiteral("config-home/menus")));
  ASSERT_TRUE(root.mkpath(QStringLiteral("config-a/menus")));
  writeFile(root.filePath(QStringLiteral("config-home/menus/hyprland-applications.menu")));
  writeFile(root.filePath(QStringLiteral("config-a/menus/hyprland-applications.menu")));
  auto runner = std::make_unique<FakeCommandRunner>();
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service = makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {});

  refreshAndWait(service);

  const QVariantMap prefix = findDiagnostic(service.diagnostics(), QStringLiteral("xdg-menu-prefix"));
  const QVariantMap candidates = findDiagnostic(service.diagnostics(), QStringLiteral("xdg-menu-candidates"));
  EXPECT_EQ(prefix.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
  EXPECT_EQ(prefix.value(QStringLiteral("observed")).toString(), QStringLiteral("hyprland-"));
  EXPECT_THAT(candidates.value(QStringLiteral("observed")).toString().toStdString(),
              testing::HasSubstr("hyprland-applications.menu"));
}

TEST(SessionIntegrationServiceTest, ClassifiesMissingAndStaleKdeSycoca) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QDir root(temp.path());
  ASSERT_TRUE(root.mkpath(QStringLiteral("apps")));
  ASSERT_TRUE(root.mkpath(QStringLiteral("cache")));
  const QString app_dir = root.filePath(QStringLiteral("apps"));
  writeFile(QDir(app_dir).filePath(QStringLiteral("demo.desktop")));
  auto runner = std::make_unique<FakeCommandRunner>();
  runner->executables.insert(QStringLiteral("kbuildsycoca6"));
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService missing_service =
      makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {app_dir});

  refreshAndWait(missing_service);

  EXPECT_EQ(findDiagnostic(missing_service.diagnostics(), QStringLiteral("kde-sycoca"))
                .value(QStringLiteral("status"))
                .toString(),
            QStringLiteral("warning"));

  writeFile(root.filePath(QStringLiteral("cache/ksycoca6_en")));
  setMtime(root.filePath(QStringLiteral("cache/ksycoca6_en")), QDateTime::currentDateTimeUtc().addSecs(-60));
  setMtime(QDir(app_dir).filePath(QStringLiteral("demo.desktop")), QDateTime::currentDateTimeUtc());
  runner = std::make_unique<FakeCommandRunner>();
  runner->executables.insert(QStringLiteral("kbuildsycoca6"));
  bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService stale_service =
      makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {app_dir});

  refreshAndWait(stale_service);

  EXPECT_EQ(findDiagnostic(stale_service.diagnostics(), QStringLiteral("kde-sycoca"))
                .value(QStringLiteral("status"))
                .toString(),
            QStringLiteral("warning"));
}

TEST(SessionIntegrationServiceTest, ClassifiesCurrentKdeSycocaAsOk) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QDir root(temp.path());
  ASSERT_TRUE(root.mkpath(QStringLiteral("apps")));
  ASSERT_TRUE(root.mkpath(QStringLiteral("cache")));
  const QString app_dir = root.filePath(QStringLiteral("apps"));
  writeFile(QDir(app_dir).filePath(QStringLiteral("demo.desktop")));
  writeFile(QDir(app_dir).filePath(QStringLiteral("mimeinfo.cache")));
  writeFile(root.filePath(QStringLiteral("cache/ksycoca6_en")));
  setMtime(QDir(app_dir).filePath(QStringLiteral("demo.desktop")), QDateTime::currentDateTimeUtc().addSecs(-120));
  setMtime(QDir(app_dir).filePath(QStringLiteral("mimeinfo.cache")), QDateTime::currentDateTimeUtc().addSecs(-90));
  setMtime(root.filePath(QStringLiteral("cache/ksycoca6_en")), QDateTime::currentDateTimeUtc());
  auto runner = std::make_unique<FakeCommandRunner>();
  runner->executables.insert(QStringLiteral("kbuildsycoca6"));
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service =
      makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {app_dir});

  refreshAndWait(service);

  const QVariantMap row = findDiagnostic(service.diagnostics(), QStringLiteral("kde-sycoca"));
  EXPECT_EQ(row.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
  EXPECT_THAT(row.value(QStringLiteral("observed")).toString().toStdString(), testing::HasSubstr("mimeinfo="));
}

TEST(SessionIntegrationServiceTest, ReportsMimeRoleDefaultsFromCommands) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QDir root(temp.path());
  ASSERT_TRUE(root.mkpath(QStringLiteral("apps")));
  const QString app_dir = root.filePath(QStringLiteral("apps"));
  writeFile(QDir(app_dir).filePath(QStringLiteral("demo.desktop")));
  auto runner = std::make_unique<FakeCommandRunner>();
  runner->executables.insert(QStringLiteral("xdg-settings"));
  runner->executables.insert(QStringLiteral("xdg-mime"));
  runner->results.insert(QStringLiteral("xdg-settings get default-web-browser"),
                         {.exit_code = 0, .stdout_text = QStringLiteral("firefox.desktop"), .stderr_text = {}});
  runner->results.insert(QStringLiteral("xdg-mime query default inode/directory"),
                         {.exit_code = 0, .stdout_text = QStringLiteral("org.kde.dolphin.desktop"), .stderr_text = {}});
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service =
      makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {app_dir});

  refreshAndWait(service);

  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("mime-role-browser"))
                .value(QStringLiteral("observed"))
                .toString(),
            QStringLiteral("firefox.desktop"));
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("mime-role-file-manager"))
                .value(QStringLiteral("observed"))
                .toString(),
            QStringLiteral("org.kde.dolphin.desktop"));
  QString app_dir_id = app_dir;
  app_dir_id.replace(QLatin1Char('/'), QLatin1Char('-'));
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("mime-cache-") + app_dir_id)
                .value(QStringLiteral("status"))
                .toString(),
            QStringLiteral("warning"));
}

TEST(SessionIntegrationServiceTest, ClassifiesFailedMimeRoleCommandAsWarning) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  auto runner = std::make_unique<FakeCommandRunner>();
  runner->executables.insert(QStringLiteral("xdg-settings"));
  runner->results.insert(QStringLiteral("xdg-settings get default-web-browser"),
                         {.exit_code = 2, .stdout_text = {}, .stderr_text = QStringLiteral("no setting")});
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service = makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {});

  refreshAndWait(service);

  const QVariantMap row = findDiagnostic(service.diagnostics(), QStringLiteral("mime-role-browser"));
  EXPECT_EQ(row.value(QStringLiteral("status")).toString(), QStringLiteral("warning"));
  EXPECT_EQ(row.value(QStringLiteral("observed")).toString(), QStringLiteral("none"));
}

TEST(SessionIntegrationServiceTest, ReportsPortalAndDesktopDbusOwners) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  auto runner = std::make_unique<FakeCommandRunner>();
  auto bus = std::make_unique<FakeBusProbe>();
  bus->owners.insert(QStringLiteral("org.freedesktop.portal.Desktop"), QStringLiteral(":1.10"));
  bus->owners.insert(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral(":1.11"));
  bus->owners.insert(QStringLiteral("org.freedesktop.Notifications"), QStringLiteral(":1.12"));
  bus->owners.insert(QStringLiteral("org.kde.StatusNotifierWatcher"), QStringLiteral(":1.13"));
  bus->owners.insert(QStringLiteral("org.freedesktop.impl.portal.desktop.holonight"), QStringLiteral(":1.14"));
  bus->names = {QStringLiteral("org.freedesktop.impl.portal.desktop.hyprland"),
                QStringLiteral("org.freedesktop.impl.portal.desktop.gtk"),
                QStringLiteral("org.freedesktop.impl.portal.desktop.holonight")};
  SessionIntegrationService service = makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {});

  refreshAndWait(service);

  EXPECT_EQ(
      findDiagnostic(service.diagnostics(), QStringLiteral("portal-broker")).value(QStringLiteral("status")).toString(),
      QStringLiteral("ok"));
  EXPECT_THAT(findDiagnostic(service.diagnostics(), QStringLiteral("portal-backends"))
                  .value(QStringLiteral("observed"))
                  .toString()
                  .toStdString(),
              testing::HasSubstr("hyprland"));
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("dbus-service-screensaver"))
                .value(QStringLiteral("status"))
                .toString(),
            QStringLiteral("ok"));
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("holonight-settings-portal"))
                .value(QStringLiteral("status"))
                .toString(),
            QStringLiteral("ok"));
}

TEST(SessionIntegrationServiceTest, ReportsMissingHoloNightSettingsPortal) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  auto runner = std::make_unique<FakeCommandRunner>();
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service = makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {});

  refreshAndWait(service);

  const QVariantMap row = findDiagnostic(service.diagnostics(), QStringLiteral("holonight-settings-portal"));
  EXPECT_EQ(row.value(QStringLiteral("status")).toString(), QStringLiteral("warning"));
  EXPECT_EQ(row.value(QStringLiteral("observed")).toString(), QStringLiteral("missing"));
}

TEST(SessionIntegrationServiceTest, ReportsMatchingAndRestartRequiredCursorThemes) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QProcessEnvironment env = baseEnvironment(temp.path());
  env.insert(QStringLiteral("XCURSOR_THEME"), QStringLiteral("ActiveCursor"));
  auto runner = std::make_unique<FakeCommandRunner>();
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service = makeService(std::move(runner), std::move(bus), env, {});

  QSignalSpy first_refresh(&service, &SessionIntegrationService::diagnosticsChanged);
  service.setExpectedCursorTheme(QStringLiteral("ActiveCursor"));
  ASSERT_TRUE(first_refresh.wait(2000));
  EXPECT_EQ(
      findDiagnostic(service.diagnostics(), QStringLiteral("cursor-theme")).value(QStringLiteral("status")).toString(),
      QStringLiteral("ok"));

  QSignalSpy second_refresh(&service, &SessionIntegrationService::diagnosticsChanged);
  service.setExpectedCursorTheme(QStringLiteral("NextCursor"));
  ASSERT_TRUE(second_refresh.wait(2000));
  const QVariantMap mismatch = findDiagnostic(service.diagnostics(), QStringLiteral("cursor-theme"));
  EXPECT_EQ(mismatch.value(QStringLiteral("status")).toString(), QStringLiteral("warning"));
  EXPECT_EQ(mismatch.value(QStringLiteral("expected")).toString(), QStringLiteral("NextCursor"));
  EXPECT_THAT(mismatch.value(QStringLiteral("detail")).toString().toStdString(), testing::HasSubstr("session restart"));
}

TEST(SessionIntegrationServiceTest, CursorChangeQueuesFollowUpDuringRefresh) {
  constexpr int kDelayMs = 100;
  const ThreadPoolCapacityGuard pool_guard(7);
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  auto timeline = std::make_shared<DelayTimeline>();
  auto runner = std::make_unique<DelayingCommandRunner>(timeline, kDelayMs);
  auto bus = std::make_unique<DelayingBusProbe>(timeline, kDelayMs);
  SessionIntegrationService service(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {});

  QSignalSpy spy(&service, &SessionIntegrationService::diagnosticsChanged);
  service.refresh();
  service.setExpectedCursorTheme(QStringLiteral("QueuedCursor"));
  ASSERT_TRUE(spy.wait(2000));
  if (spy.count() < 2) {
    ASSERT_TRUE(spy.wait(2000));
  }
  EXPECT_GE(spy.count(), 2);
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("cursor-theme"))
                .value(QStringLiteral("expected"))
                .toString(),
            QStringLiteral("QueuedCursor"));
}

TEST(SessionIntegrationServiceTest, ClassifiesUnavailableSessionBusAsError) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  auto runner = std::make_unique<FakeCommandRunner>();
  auto bus = std::make_unique<FakeBusProbe>();
  bus->connected = false;
  SessionIntegrationService service = makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {});

  refreshAndWait(service);

  const QVariantMap row = findDiagnostic(service.diagnostics(), QStringLiteral("dbus-session-services"));
  EXPECT_EQ(row.value(QStringLiteral("status")).toString(), QStringLiteral("error"));
  EXPECT_EQ(row.value(QStringLiteral("observed")).toString(), QStringLiteral("session bus unavailable"));
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("portal-broker")), QVariantMap{});
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("holonight-settings-portal"))
                .value(QStringLiteral("observed"))
                .toString(),
            QStringLiteral("unavailable"));
  EXPECT_EQ(service.overallStatus(), QStringLiteral("error"));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(SessionIntegrationServiceTest, RefreshDispatchesDiagnosticsInParallelNotSequentially) {
  constexpr int kDelayMs = 200;
  const ThreadPoolCapacityGuard pool_guard(7);

  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QDir root(temp.path());
  ASSERT_TRUE(root.mkpath(QStringLiteral("apps")));
  const QString app_dir = root.filePath(QStringLiteral("apps"));
  writeFile(QDir(app_dir).filePath(QStringLiteral("demo.desktop")));

  auto timeline = std::make_shared<DelayTimeline>();
  auto runner = std::make_unique<DelayingCommandRunner>(timeline, kDelayMs);
  auto bus = std::make_unique<DelayingBusProbe>(timeline, kDelayMs);

  QProcessEnvironment env = baseEnvironment(temp.path());
  env.remove(QStringLiteral("XDG_MENU_PREFIX"));  // force addXdgMenuDiagnostics through executableExists()
  SessionIntegrationService service(std::move(runner), std::move(bus), env, {app_dir});

  QElapsedTimer wall_clock;
  wall_clock.start();
  QSignalSpy spy(&service, &SessionIntegrationService::diagnosticsChanged);
  service.refresh();
  ASSERT_TRUE(spy.wait(2000));
  const qint64 elapsed_ms = wall_clock.elapsed();

  // 5 of the 7 diagnostics (systemd-environment, xdg-menu, kde-sycoca, mime, portal/desktop-services)
  // each incur exactly one 200ms delay; a fully sequential dispatch of those 5 would take ~1000ms.
  // Parallel dispatch should land close to the single-diagnostic delay plus scheduling overhead.
  EXPECT_LT(elapsed_ms, 500);

  const QVariantList diagnostics = service.diagnostics();
  EXPECT_FALSE(findDiagnostic(diagnostics, QStringLiteral("process-env-wayland-display")).isEmpty());
  EXPECT_FALSE(findDiagnostic(diagnostics, QStringLiteral("dbus-activation-address")).isEmpty());
  EXPECT_FALSE(findDiagnostic(diagnostics, QStringLiteral("systemd-env-wayland-display")).isEmpty());
  EXPECT_FALSE(findDiagnostic(diagnostics, QStringLiteral("xdg-menu-prefix")).isEmpty());
  EXPECT_FALSE(findDiagnostic(diagnostics, QStringLiteral("kde-sycoca")).isEmpty());
  EXPECT_FALSE(findDiagnostic(diagnostics, QStringLiteral("mime-role-browser")).isEmpty());
  EXPECT_FALSE(findDiagnostic(diagnostics, QStringLiteral("portal-broker")).isEmpty());

  const QVector<DelayTimeline::Interval> intervals = timeline->intervals();
  ASSERT_GE(intervals.size(), 2);
  EXPECT_TRUE(anyIntervalsOverlap(intervals));
}

TEST(SessionIntegrationServiceTest, DestructionWaitsForActiveRefreshWorkers) {
  constexpr int kDelayMs = 100;
  const ThreadPoolCapacityGuard pool_guard(7);

  auto timeline = std::make_shared<DelayTimeline>();
  auto runner = std::make_unique<DelayingCommandRunner>(timeline, kDelayMs);
  auto bus = std::make_unique<DelayingBusProbe>(timeline, kDelayMs);
  auto service = std::make_unique<SessionIntegrationService>(std::move(runner), std::move(bus),
                                                             baseEnvironment(QStringLiteral("/tmp")), QStringList{});

  service->refresh();
  QElapsedTimer elapsed;
  elapsed.start();
  service.reset();

  EXPECT_GE(elapsed.elapsed(), kDelayMs / 2);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(SessionIntegrationServiceTest, RebuildApplicationCachesIssuesExactlyOneRebuildAndOneDiagnosticsPass) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QDir root(temp.path());
  ASSERT_TRUE(root.mkpath(QStringLiteral("apps")));
  const QString app_dir = root.filePath(QStringLiteral("apps"));
  writeFile(QDir(app_dir).filePath(QStringLiteral("demo.desktop")));

  auto runner = std::make_unique<ThreadSafeSpyCommandRunner>();
  ThreadSafeSpyCommandRunner* runner_ptr = runner.get();
  runner->executables = {QStringLiteral("update-desktop-database"), QStringLiteral("kbuildsycoca6"),
                         QStringLiteral("systemctl"), QStringLiteral("xdg-settings"), QStringLiteral("xdg-mime")};
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {app_dir});

  rebuildAndWait(service);

  const QStringList calls = runner_ptr->calls;

  // ApplicationCacheRebuilder::rebuild() runs synchronously, before rebuildApplicationCaches()'s
  // trailing refresh() is dispatched, so these first two calls have a fixed, deterministic order:
  // one update-desktop-database call per writable app dir with desktop files, then one
  // kbuildsycoca6 --noincremental call.
  ASSERT_GE(calls.size(), 2);
  EXPECT_EQ(calls.at(0), QStringLiteral("update-desktop-database ") + app_dir);
  EXPECT_EQ(calls.at(1), QStringLiteral("kbuildsycoca6 --noincremental"));

  // The trailing refresh() dispatches all 7 diagnostics in parallel; only addSystemdEnvironmentDiagnostics
  // (1 call) and addMimeDiagnostics (6 calls: 1 xdg-settings role + 5 xdg-mime roles) call run(), and
  // since they execute concurrently on separate worker threads their relative order is not fixed — but
  // the full multiset of remaining calls must match exactly this set of 7, once each. A regression that
  // reintroduced a second rebuild-and-refresh pass inside rebuildApplicationCaches() would double every
  // one of these counts (9 calls total becomes 18), which this exact-match assertion catches.
  QStringList diagnostic_calls = calls.mid(2);
  diagnostic_calls.sort();
  QStringList expected_diagnostic_calls{
      QStringLiteral("systemctl --user show-environment"),
      QStringLiteral("xdg-settings get default-web-browser"),
      QStringLiteral("xdg-mime query default application/x-terminal-emulator"),
      QStringLiteral("xdg-mime query default image/png"),
      QStringLiteral("xdg-mime query default inode/directory"),
      QStringLiteral("xdg-mime query default text/plain"),
      QStringLiteral("xdg-mime query default video/mp4"),
  };
  expected_diagnostic_calls.sort();
  EXPECT_EQ(diagnostic_calls, expected_diagnostic_calls);

  // One rebuild pass (2 calls) + one diagnostics pass (7 calls) = 9, not 18 (two of each).
  EXPECT_EQ(calls.size(), 9);
}

TEST(ApplicationCacheRebuilderTest, RunsUpdateDesktopDatabaseOnlyForWritableApplicationDirs) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QDir root(temp.path());
  ASSERT_TRUE(root.mkpath(QStringLiteral("writable")));
  ASSERT_TRUE(root.mkpath(QStringLiteral("read-only")));
  ASSERT_TRUE(root.mkpath(QStringLiteral("empty")));
  const QString writable_dir = root.filePath(QStringLiteral("writable"));
  const QString read_only_dir = root.filePath(QStringLiteral("read-only"));
  const QString empty_dir = root.filePath(QStringLiteral("empty"));
  writeFile(QDir(writable_dir).filePath(QStringLiteral("app.desktop")));
  writeFile(QDir(read_only_dir).filePath(QStringLiteral("app.desktop")));
  ASSERT_TRUE(QFile(read_only_dir).setPermissions(QFileDevice::ReadOwner | QFileDevice::ExeOwner));

  FakeCommandRunner runner;
  runner.executables.insert(QStringLiteral("update-desktop-database"));
  runner.results.insert(QStringLiteral("update-desktop-database ") + writable_dir,
                        {.exit_code = 0, .stdout_text = {}, .stderr_text = {}});
  const ApplicationCacheRebuilder rebuilder(runner);

  const QVector<ApplicationCacheRebuildStep> steps = rebuilder.rebuild({writable_dir, read_only_dir, empty_dir});

  ASSERT_EQ(steps.size(), 1);
  EXPECT_EQ(steps.at(0).commandLine(), QStringLiteral("update-desktop-database ") + writable_dir);
  EXPECT_EQ(runner.calls, QStringList({QStringLiteral("update-desktop-database ") + writable_dir}));

  QFile(read_only_dir).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

TEST(DesktopFileUtilsTest, FindsNestedDesktopFilesAndIgnoresNonMatchingFiles) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QDir root(temp.path());
  ASSERT_TRUE(root.mkpath(QStringLiteral("nested")));
  writeFile(root.filePath(QStringLiteral("nested/demo.desktop")));
  writeFile(root.filePath(QStringLiteral("near-miss.desktop.bak")));

  EXPECT_TRUE(DesktopFileUtils::containsDesktopFiles(temp.path()));

  QTemporaryDir no_desktop_files;
  ASSERT_TRUE(no_desktop_files.isValid());
  writeFile(QDir(no_desktop_files.path()).filePath(QStringLiteral("near-miss.desktop.bak")));

  EXPECT_FALSE(DesktopFileUtils::containsDesktopFiles(no_desktop_files.path()));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(SessionIntegrationServiceTest, RebuildApplicationCachesRunsSequentialCommandsAndRefreshesModelsOnSuccess) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QDir root(temp.path());
  ASSERT_TRUE(root.mkpath(QStringLiteral("apps")));
  const QString app_dir = root.filePath(QStringLiteral("apps"));
  writeFile(QDir(app_dir).filePath(QStringLiteral("demo.desktop")));
  auto runner = std::make_unique<FakeCommandRunner>();
  FakeCommandRunner* runner_ptr = runner.get();
  runner->executables.insert(QStringLiteral("update-desktop-database"));
  runner->executables.insert(QStringLiteral("kbuildsycoca6"));
  runner->results.insert(QStringLiteral("update-desktop-database ") + app_dir,
                         {.exit_code = 0, .stdout_text = {}, .stderr_text = {}});
  runner->results.insert(QStringLiteral("kbuildsycoca6 --noincremental"),
                         {.exit_code = 0, .stdout_text = {}, .stderr_text = {}});
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service =
      makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {app_dir});
  int mime_refreshes = 0;
  int launcher_refreshes = 0;
  service.setPostRebuildRefreshCallbacks([&mime_refreshes] { ++mime_refreshes; },
                                         [&launcher_refreshes] { ++launcher_refreshes; });
  QSignalSpy finished_spy(&service, &SessionIntegrationService::rebuildFinished);

  rebuildAndWait(service);

  EXPECT_EQ(runner_ptr->calls, QStringList({QStringLiteral("update-desktop-database ") + app_dir,
                                            QStringLiteral("kbuildsycoca6 --noincremental")}));
  ASSERT_EQ(finished_spy.count(), 1);
  EXPECT_TRUE(finished_spy.at(0).at(0).toBool());
  EXPECT_EQ(mime_refreshes, 1);
  EXPECT_EQ(launcher_refreshes, 1);
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("application-cache-rebuild-1"))
                .value(QStringLiteral("status"))
                .toString(),
            QStringLiteral("ok"));
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("application-cache-rebuild-2"))
                .value(QStringLiteral("command"))
                .toString(),
            QStringLiteral("kbuildsycoca6 --noincremental"));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(SessionIntegrationServiceTest, RebuildApplicationCachesDoesNotRefreshModelsOnFailure) {
  QTemporaryDir temp;
  ASSERT_TRUE(temp.isValid());
  QDir root(temp.path());
  ASSERT_TRUE(root.mkpath(QStringLiteral("apps")));
  const QString app_dir = root.filePath(QStringLiteral("apps"));
  writeFile(QDir(app_dir).filePath(QStringLiteral("demo.desktop")));
  auto runner = std::make_unique<FakeCommandRunner>();
  runner->executables.insert(QStringLiteral("update-desktop-database"));
  runner->results.insert(QStringLiteral("update-desktop-database ") + app_dir,
                         {.exit_code = 1, .stdout_text = {}, .stderr_text = QStringLiteral("failed")});
  auto bus = std::make_unique<FakeBusProbe>();
  SessionIntegrationService service =
      makeService(std::move(runner), std::move(bus), baseEnvironment(temp.path()), {app_dir});
  int mime_refreshes = 0;
  int launcher_refreshes = 0;
  service.setPostRebuildRefreshCallbacks([&mime_refreshes] { ++mime_refreshes; },
                                         [&launcher_refreshes] { ++launcher_refreshes; });
  QSignalSpy finished_spy(&service, &SessionIntegrationService::rebuildFinished);

  rebuildAndWait(service);

  ASSERT_EQ(finished_spy.count(), 1);
  EXPECT_FALSE(finished_spy.at(0).at(0).toBool());
  EXPECT_EQ(mime_refreshes, 0);
  EXPECT_EQ(launcher_refreshes, 0);
  EXPECT_EQ(findDiagnostic(service.diagnostics(), QStringLiteral("application-cache-rebuild-1"))
                .value(QStringLiteral("status"))
                .toString(),
            QStringLiteral("warning"));
}
