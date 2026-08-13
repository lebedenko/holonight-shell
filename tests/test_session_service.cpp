#include "SessionService.h"
#include "session/CommandRunner.h"
#include "session/HyprlandSessionBackend.h"
#include "session/Locker.h"
#include "session/LogindSessionBackend.h"
#include "session/ProcessEnvironment.h"
#include "session/SwaySessionBackend.h"

#include <QHash>
#include <QSignalSpy>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>
#include <memory>

namespace {

// Controllable probe: tests declare exactly which daemons are "running" and which lockers exist
// on PATH, so every lock branch is reachable without a live session.
class FakeProcessEnvironment final : public ProcessEnvironment {
 public:
  void setRunning(const QString& name, bool running) { running_[name] = running; }
  void setExecutable(const QString& name, const QString& path) { paths_[name] = path; }
  void setUserServiceActive(const QString& name, bool active) { services_[name] = active; }

  [[nodiscard]] bool isRunning(const QString& process_name) const override {
    return running_.value(process_name, false);
  }
  [[nodiscard]] QString findExecutable(const QString& name) const override { return paths_.value(name, QString{}); }
  [[nodiscard]] bool isUserServiceActive(const QString& service_name) const override {
    return services_.value(service_name, false);
  }

 private:
  QHash<QString, bool> running_;
  QHash<QString, QString> paths_;
  QHash<QString, bool> services_;
};

// Records what would have been launched without launching anything.
class SpyCommandRunner final : public CommandRunner {
 public:
  bool run(const QString& program, const QStringList& args) override {
    ++call_count_;
    last_program_ = program;
    last_args_ = args;
    return !should_fail_;
  }

  void setShouldFail(bool should_fail) { should_fail_ = should_fail; }

  [[nodiscard]] int callCount() const { return call_count_; }
  [[nodiscard]] QString lastProgram() const { return last_program_; }
  [[nodiscard]] QStringList lastArgs() const { return last_args_; }

 private:
  int call_count_{0};
  QString last_program_;
  QStringList last_args_;
  bool should_fail_{false};
};

}  // namespace

// --- Locker: four-branch strategy (REQ-F-006..009) ---

TEST(LockerTest, IdleDaemonRunningUsesLoginctl) {
  FakeProcessEnvironment env;
  env.setRunning(QStringLiteral("hypridle"), true);
  env.setExecutable(QStringLiteral("hyprlock"), QStringLiteral("/usr/bin/hyprlock"));
  SpyCommandRunner runner;

  Locker locker(&env, &runner);
  const SessionCommandResult result = locker.lock();

  EXPECT_TRUE(result.ok);
  EXPECT_EQ(runner.callCount(), 1);
  EXPECT_EQ(runner.lastProgram(), QStringLiteral("loginctl"));
  EXPECT_EQ(runner.lastArgs(), QStringList{QStringLiteral("lock-session")});
}

TEST(LockerTest, ActiveIdleDaemonUserServiceUsesLoginctl) {
  FakeProcessEnvironment env;
  env.setUserServiceActive(QStringLiteral("hypridle.service"), true);
  env.setExecutable(QStringLiteral("hyprlock"), QStringLiteral("/usr/bin/hyprlock"));
  SpyCommandRunner runner;

  Locker locker(&env, &runner);
  const SessionCommandResult result = locker.lock();

  EXPECT_TRUE(result.ok);
  EXPECT_EQ(runner.callCount(), 1);
  EXPECT_EQ(runner.lastProgram(), QStringLiteral("loginctl"));
  EXPECT_EQ(runner.lastArgs(), QStringList{QStringLiteral("lock-session")});
  EXPECT_TRUE(locker.lockerAvailable());
  EXPECT_EQ(locker.lockerName(), QStringLiteral("hypridle"));
}

TEST(LockerTest, NoDaemonLockerOnPathSpawnsIt) {
  FakeProcessEnvironment env;
  env.setExecutable(QStringLiteral("hyprlock"), QStringLiteral("/usr/bin/hyprlock"));
  SpyCommandRunner runner;

  Locker locker(&env, &runner);
  const SessionCommandResult result = locker.lock();

  EXPECT_TRUE(result.ok);
  EXPECT_EQ(runner.callCount(), 1);
  EXPECT_EQ(runner.lastProgram(), QStringLiteral("/usr/bin/hyprlock"));
  EXPECT_TRUE(runner.lastArgs().isEmpty());
}

TEST(LockerTest, PriorityOrderPicksFirstAvailableLocker) {
  FakeProcessEnvironment env;
  // hyprlock absent; swaylock and gtklock present -> swaylock wins (higher priority).
  env.setExecutable(QStringLiteral("swaylock"), QStringLiteral("/usr/bin/swaylock"));
  env.setExecutable(QStringLiteral("gtklock"), QStringLiteral("/usr/bin/gtklock"));
  SpyCommandRunner runner;

  Locker locker(&env, &runner);
  const SessionCommandResult result = locker.lock();

  EXPECT_TRUE(result.ok);
  EXPECT_EQ(runner.lastProgram(), QStringLiteral("/usr/bin/swaylock"));
  EXPECT_EQ(locker.lockerName(), QStringLiteral("swaylock"));
}

TEST(LockerTest, LockerAlreadyRunningDoesNotSpawnAgain) {
  FakeProcessEnvironment env;
  env.setExecutable(QStringLiteral("hyprlock"), QStringLiteral("/usr/bin/hyprlock"));
  env.setRunning(QStringLiteral("hyprlock"), true);  // already locked
  SpyCommandRunner runner;

  Locker locker(&env, &runner);
  const SessionCommandResult result = locker.lock();

  EXPECT_TRUE(result.ok);
  EXPECT_EQ(runner.callCount(), 0);
}

TEST(LockerTest, NothingAvailableIsGracefulNoOp) {
  FakeProcessEnvironment env;  // no daemons running, no lockers on PATH
  SpyCommandRunner runner;

  Locker locker(&env, &runner);
  const SessionCommandResult result = locker.lock();  // must not crash

  EXPECT_TRUE(result.ok);
  EXPECT_EQ(runner.callCount(), 0);
  EXPECT_FALSE(locker.lockerAvailable());
  EXPECT_TRUE(locker.lockerName().isEmpty());
}

// --- Locker: availability snapshot (REQ-F-014, REQ-F-015) ---

TEST(LockerTest, AvailabilityReflectsRunningDaemon) {
  FakeProcessEnvironment env;
  env.setRunning(QStringLiteral("swayidle"), true);
  SpyCommandRunner runner;

  Locker locker(&env, &runner);

  EXPECT_TRUE(locker.lockerAvailable());
  EXPECT_EQ(locker.lockerName(), QStringLiteral("swayidle"));
}

TEST(LockerTest, AvailabilityReflectsLockerOnPath) {
  FakeProcessEnvironment env;
  env.setExecutable(QStringLiteral("hyprlock"), QStringLiteral("/usr/bin/hyprlock"));
  SpyCommandRunner runner;

  Locker locker(&env, &runner);

  EXPECT_TRUE(locker.lockerAvailable());
  EXPECT_EQ(locker.lockerName(), QStringLiteral("hyprlock"));
}

// --- SessionService facade delegation via a real backend over fakes ---

TEST(SessionServiceTest, HyprlandDelegatesLogoutToHyprctl) {
  FakeProcessEnvironment env;
  SpyCommandRunner runner;
  SessionService service(std::make_unique<HyprlandSessionBackend>(&env, &runner));

  service.logout();

  EXPECT_EQ(runner.lastProgram(), QStringLiteral("hyprctl"));
  EXPECT_EQ(runner.lastArgs(), (QStringList{QStringLiteral("dispatch"), QStringLiteral("exit")}));
}

TEST(SessionServiceTest, UwsmManagedHyprlandDelegatesLogoutToUwsm) {
  FakeProcessEnvironment env;
  env.setExecutable(QStringLiteral("uwsm"), QStringLiteral("/usr/bin/uwsm"));
  env.setUserServiceActive(QStringLiteral("wayland-wm@hyprland.desktop.service"), true);
  SpyCommandRunner runner;
  SessionService service(std::make_unique<HyprlandSessionBackend>(&env, &runner));

  service.logout();

  EXPECT_EQ(runner.lastProgram(), QStringLiteral("uwsm"));
  EXPECT_EQ(runner.lastArgs(), QStringList{QStringLiteral("stop")});
}

TEST(SessionServiceTest, UwsmUnavailableFallsBackToHyprctl) {
  FakeProcessEnvironment env;
  env.setUserServiceActive(QStringLiteral("wayland-wm@hyprland.desktop.service"), true);
  SpyCommandRunner runner;
  SessionService service(std::make_unique<HyprlandSessionBackend>(&env, &runner));

  service.logout();

  EXPECT_EQ(runner.lastProgram(), QStringLiteral("hyprctl"));
  EXPECT_EQ(runner.lastArgs(), (QStringList{QStringLiteral("dispatch"), QStringLiteral("exit")}));
}

TEST(SessionServiceTest, SwayDelegatesDirectLogoutToSwaymsg) {
  FakeProcessEnvironment env;
  SpyCommandRunner runner;
  SessionService service(std::make_unique<SwaySessionBackend>(&env, &runner));

  service.logout();

  EXPECT_EQ(runner.lastProgram(), QStringLiteral("swaymsg"));
  EXPECT_EQ(runner.lastArgs(), QStringList{QStringLiteral("exit")});
}

TEST(SessionServiceTest, UwsmManagedSwayDelegatesLogoutToUwsm) {
  FakeProcessEnvironment env;
  env.setExecutable(QStringLiteral("uwsm"), QStringLiteral("/usr/bin/uwsm"));
  env.setUserServiceActive(QStringLiteral("wayland-wm@sway.desktop.service"), true);
  SpyCommandRunner runner;
  SessionService service(std::make_unique<SwaySessionBackend>(&env, &runner));

  service.logout();

  EXPECT_EQ(runner.lastProgram(), QStringLiteral("uwsm"));
  EXPECT_EQ(runner.lastArgs(), QStringList{QStringLiteral("stop")});
}

TEST(SessionServiceTest, PowerActionsDelegateToSystemctl) {
  FakeProcessEnvironment env;
  SpyCommandRunner runner;
  SessionService service(std::make_unique<HyprlandSessionBackend>(&env, &runner));

  service.sleep();
  EXPECT_EQ(runner.lastProgram(), QStringLiteral("systemctl"));
  EXPECT_EQ(runner.lastArgs(), QStringList{QStringLiteral("suspend")});

  service.reboot();
  EXPECT_EQ(runner.lastArgs(), QStringList{QStringLiteral("reboot")});

  service.shutdown();
  EXPECT_EQ(runner.lastArgs(), QStringList{QStringLiteral("poweroff")});
}

TEST(SessionServiceTest, LockScreenDelegatesToLocker) {
  FakeProcessEnvironment env;
  env.setExecutable(QStringLiteral("hyprlock"), QStringLiteral("/usr/bin/hyprlock"));
  SpyCommandRunner runner;
  SessionService service(std::make_unique<HyprlandSessionBackend>(&env, &runner));

  service.lockScreen();

  EXPECT_EQ(runner.lastProgram(), QStringLiteral("/usr/bin/hyprlock"));
}

TEST(SessionServiceTest, HyprlandCapabilities) {
  FakeProcessEnvironment env;
  env.setExecutable(QStringLiteral("hyprlock"), QStringLiteral("/usr/bin/hyprlock"));
  SpyCommandRunner runner;
  SessionService service(std::make_unique<HyprlandSessionBackend>(&env, &runner));

  EXPECT_EQ(service.backendName(), QStringLiteral("hyprland"));
  EXPECT_TRUE(service.logoutSupported());
  EXPECT_TRUE(service.lockerAvailable());
  EXPECT_EQ(service.lockerName(), QStringLiteral("hyprlock"));
}

TEST(SessionServiceTest, DeclaredHyprlandDesktopSelectsHyprlandBeforeInstanceExists) {
  const QByteArray previous_desktop = qgetenv("XDG_CURRENT_DESKTOP");
  const QByteArray previous_instance = qgetenv("HYPRLAND_INSTANCE_SIGNATURE");
  qputenv("XDG_CURRENT_DESKTOP", "HoloNight:Hyprland");
  qunsetenv("HYPRLAND_INSTANCE_SIGNATURE");

  SessionService service;

  EXPECT_EQ(service.backendName(), QStringLiteral("hyprland"));
  qputenv("XDG_CURRENT_DESKTOP", previous_desktop);
  qputenv("HYPRLAND_INSTANCE_SIGNATURE", previous_instance);
}

TEST(SessionServiceTest, LogindBackendLogoutIsNoOpAndUnsupported) {
  FakeProcessEnvironment env;
  SpyCommandRunner runner;
  SessionService service(std::make_unique<LogindSessionBackend>(&env, &runner));

  service.logout();
  EXPECT_EQ(runner.callCount(), 0);  // no-op: nothing launched

  EXPECT_EQ(service.backendName(), QStringLiteral("logind"));
  EXPECT_FALSE(service.logoutSupported());
}

TEST(SessionServiceTest, LogindBackendStillPowersOff) {
  FakeProcessEnvironment env;
  SpyCommandRunner runner;
  SessionService service(std::make_unique<LogindSessionBackend>(&env, &runner));

  service.shutdown();

  EXPECT_EQ(runner.lastProgram(), QStringLiteral("systemctl"));
  EXPECT_EQ(runner.lastArgs(), QStringList{QStringLiteral("poweroff")});
}

// --- SessionService: command failure signaling (REQ-F-B.1..B.6) ---

TEST(SessionServiceTest, LockFailureEmitsCommandFailed) {
  FakeProcessEnvironment env;
  env.setRunning(QStringLiteral("hypridle"), true);  // daemon branch -> loginctl lock-session
  SpyCommandRunner runner;
  runner.setShouldFail(true);
  SessionService service(std::make_unique<HyprlandSessionBackend>(&env, &runner));
  QSignalSpy spy(&service, &SessionService::commandFailed);

  service.lockScreen();

  ASSERT_EQ(spy.count(), 1);
  const QList<QVariant> emitted = spy.takeFirst();
  EXPECT_EQ(emitted.at(0).toString(), QStringLiteral("lock"));
  EXPECT_FALSE(emitted.at(1).toString().isEmpty());
}

TEST(SessionServiceTest, LogoutFailureEmitsCommandFailed) {
  FakeProcessEnvironment env;
  SpyCommandRunner runner;
  runner.setShouldFail(true);
  SessionService service(std::make_unique<HyprlandSessionBackend>(&env, &runner));
  QSignalSpy spy(&service, &SessionService::commandFailed);

  service.logout();

  ASSERT_EQ(spy.count(), 1);
  const QList<QVariant> emitted = spy.takeFirst();
  EXPECT_EQ(emitted.at(0).toString(), QStringLiteral("logout"));
  EXPECT_FALSE(emitted.at(1).toString().isEmpty());
}

TEST(SessionServiceTest, SleepFailureEmitsCommandFailed) {
  FakeProcessEnvironment env;
  SpyCommandRunner runner;
  runner.setShouldFail(true);
  SessionService service(std::make_unique<HyprlandSessionBackend>(&env, &runner));
  QSignalSpy spy(&service, &SessionService::commandFailed);

  service.sleep();

  ASSERT_EQ(spy.count(), 1);
  const QList<QVariant> emitted = spy.takeFirst();
  EXPECT_EQ(emitted.at(0).toString(), QStringLiteral("sleep"));
  EXPECT_FALSE(emitted.at(1).toString().isEmpty());
}

TEST(SessionServiceTest, RebootFailureEmitsCommandFailed) {
  FakeProcessEnvironment env;
  SpyCommandRunner runner;
  runner.setShouldFail(true);
  SessionService service(std::make_unique<HyprlandSessionBackend>(&env, &runner));
  QSignalSpy spy(&service, &SessionService::commandFailed);

  service.reboot();

  ASSERT_EQ(spy.count(), 1);
  const QList<QVariant> emitted = spy.takeFirst();
  EXPECT_EQ(emitted.at(0).toString(), QStringLiteral("reboot"));
  EXPECT_FALSE(emitted.at(1).toString().isEmpty());
}

TEST(SessionServiceTest, ShutdownFailureEmitsCommandFailed) {
  FakeProcessEnvironment env;
  SpyCommandRunner runner;
  runner.setShouldFail(true);
  SessionService service(std::make_unique<HyprlandSessionBackend>(&env, &runner));
  QSignalSpy spy(&service, &SessionService::commandFailed);

  service.shutdown();

  ASSERT_EQ(spy.count(), 1);
  const QList<QVariant> emitted = spy.takeFirst();
  EXPECT_EQ(emitted.at(0).toString(), QStringLiteral("shutdown"));
  EXPECT_FALSE(emitted.at(1).toString().isEmpty());
}

TEST(SessionServiceTest, SuccessfulCommandsDoNotEmitCommandFailed) {
  FakeProcessEnvironment env;
  env.setExecutable(QStringLiteral("hyprlock"), QStringLiteral("/usr/bin/hyprlock"));
  SpyCommandRunner runner;  // should_fail_ defaults to false
  SessionService service(std::make_unique<HyprlandSessionBackend>(&env, &runner));
  QSignalSpy spy(&service, &SessionService::commandFailed);

  service.lockScreen();
  service.logout();
  service.sleep();
  service.reboot();
  service.shutdown();

  EXPECT_EQ(spy.count(), 0);
}
