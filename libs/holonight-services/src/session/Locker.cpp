#include "Locker.h"

#include "CommandRunner.h"
#include "ProcessEnvironment.h"

#include <array>

namespace {
// Lock-handler daemons that respond to logind's Lock signal, checked in this order.
constexpr std::array kLockDaemons = {"hypridle", "swayidle", "xss-lock"};
// Lockers spawned directly when no daemon is wired, in priority order.
constexpr std::array kLockers = {"hyprlock", "swaylock", "gtklock", "waylock"};
}  // namespace

Locker::Locker(const ProcessEnvironment* env, CommandRunner* runner) : env_(env), runner_(runner) {
  const Resolution res = resolve();
  locker_available_ = res.mode != Mode::None;
  locker_name_ = res.name;
}

Locker::Resolution Locker::resolve() const {
  for (const char* daemon : kLockDaemons) {
    const QString name = QString::fromLatin1(daemon);
    if (env_->isRunning(name) || env_->isUserServiceActive(name + QStringLiteral(".service"))) {
      return {.mode = Mode::Daemon, .name = name, .path = {}};
    }
  }
  for (const char* locker : kLockers) {
    const QString name = QString::fromLatin1(locker);
    const QString path = env_->findExecutable(name);
    if (!path.isEmpty()) {
      return {.mode = Mode::Locker, .name = name, .path = path};
    }
  }
  return {};
}

SessionCommandResult Locker::lock() {
  const Resolution res = resolve();
  switch (res.mode) {
    case Mode::Daemon:
      if (!runner_->run(QStringLiteral("loginctl"), {QStringLiteral("lock-session")})) {
        return SessionCommandResult::failure(QStringLiteral("failed to launch loginctl lock-session"));
      }
      return SessionCommandResult::success();
    case Mode::Locker:
      if (env_->isRunning(res.name)) {
        return SessionCommandResult::success();  // already locked — do not stack a second instance
      }
      if (!runner_->run(res.path, {})) {
        return SessionCommandResult::failure(QStringLiteral("failed to launch %1").arg(res.name));
      }
      return SessionCommandResult::success();
    case Mode::None:
      return SessionCommandResult::success();  // nothing to do; graceful no-op
  }
  return SessionCommandResult::success();
}
