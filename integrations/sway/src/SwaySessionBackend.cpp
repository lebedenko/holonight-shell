#include "SwaySessionBackend.h"

#include "ProcessEnvironment.h"

SwaySessionBackend::SwaySessionBackend(const ProcessEnvironment* environment, CommandRunner* runner)
    : SessionBackend(environment, runner), environment_(environment) {}

SessionCommandResult SwaySessionBackend::logout() {
  const auto state = environment_->managedSession();
  if (state == ProcessEnvironment::ManagedSession::Unverifiable) {
    return SessionCommandResult::failure(
        QStringLiteral("Cannot verify session ownership; run holonight-session-doctor"));
  }
  if (state == ProcessEnvironment::ManagedSession::Managed) {
    if (environment_->findExecutable(QStringLiteral("uwsm")).isEmpty()) {
      return SessionCommandResult::failure(QStringLiteral("Session is managed by UWSM, but uwsm is unavailable"));
    }
    return run(QStringLiteral("uwsm"), {QStringLiteral("stop")});
  }
  return run(QStringLiteral("swaymsg"), {QStringLiteral("exit")});
}
