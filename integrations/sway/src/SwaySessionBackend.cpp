#include "SwaySessionBackend.h"

#include "ProcessEnvironment.h"

SwaySessionBackend::SwaySessionBackend(const ProcessEnvironment* environment, CommandRunner* runner)
    : SessionBackend(environment, runner), environment_(environment) {}

SessionCommandResult SwaySessionBackend::logout() {
  if (environment_->isUserServiceActive(QStringLiteral("wayland-wm@sway.desktop.service")) &&
      !environment_->findExecutable(QStringLiteral("uwsm")).isEmpty()) {
    return run(QStringLiteral("uwsm"), {QStringLiteral("stop")});
  }
  return run(QStringLiteral("swaymsg"), {QStringLiteral("exit")});
}
