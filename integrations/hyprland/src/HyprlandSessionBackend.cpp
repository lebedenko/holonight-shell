#include "HyprlandSessionBackend.h"

#include "ProcessEnvironment.h"

HyprlandSessionBackend::HyprlandSessionBackend(const ProcessEnvironment* env, CommandRunner* runner)
    : SessionBackend(env, runner), env_(env) {}

SessionCommandResult HyprlandSessionBackend::logout() {
  if (env_->isUserServiceActive(QStringLiteral("wayland-wm@hyprland.desktop.service")) &&
      !env_->findExecutable(QStringLiteral("uwsm")).isEmpty()) {
    return run(QStringLiteral("uwsm"), {QStringLiteral("stop")});
  }
  return run(QStringLiteral("hyprctl"), {QStringLiteral("dispatch"), QStringLiteral("exit")});
}
