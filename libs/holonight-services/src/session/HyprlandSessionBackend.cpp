#include "HyprlandSessionBackend.h"

HyprlandSessionBackend::HyprlandSessionBackend(const ProcessEnvironment* env, CommandRunner* runner)
    : SessionBackend(env, runner) {}

SessionCommandResult HyprlandSessionBackend::logout() {
  return run(QStringLiteral("hyprctl"), {QStringLiteral("dispatch"), QStringLiteral("exit")});
}
