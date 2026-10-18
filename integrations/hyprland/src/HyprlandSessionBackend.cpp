#include "HyprlandSessionBackend.h"

#include "ProcessEnvironment.h"

HyprlandSessionBackend::HyprlandSessionBackend(const ProcessEnvironment* env, CommandRunner* runner)
    : SessionBackend(env, runner), env_(env) {}

SessionCommandResult HyprlandSessionBackend::logout() {
  const auto state = env_->managedSession();
  if (state == ProcessEnvironment::ManagedSession::Unverifiable) {
    return SessionCommandResult::failure(
        QStringLiteral("Cannot verify session ownership; run holonight-session-doctor"));
  }
  if (state == ProcessEnvironment::ManagedSession::Managed) {
    if (env_->findExecutable(QStringLiteral("uwsm")).isEmpty()) {
      return SessionCommandResult::failure(QStringLiteral("Session is managed by UWSM, but uwsm is unavailable"));
    }
    return run(QStringLiteral("uwsm"), {QStringLiteral("stop")});
  }
  return run(QStringLiteral("hyprctl"), {QStringLiteral("dispatch"), QStringLiteral("hl.dsp.exit()")});
}
