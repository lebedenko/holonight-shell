#include "LabwcSessionBackend.h"

#include "ProcessEnvironment.h"

#include <QRegularExpression>

#include <limits>

LabwcSessionBackend::LabwcSessionBackend(const ProcessEnvironment* environment, CommandRunner* runner)
    : SessionBackend(environment, runner), environment_(environment) {}

SessionCommandResult LabwcSessionBackend::logout() {
  if (environment_->isUserServiceActive(QStringLiteral("wayland-wm@labwc.desktop.service"))) {
    if (environment_->findExecutable(QStringLiteral("uwsm")).isEmpty()) {
      return SessionCommandResult::failure(QStringLiteral("labwc session is managed by UWSM, but uwsm is unavailable"));
    }
    return run(QStringLiteral("uwsm"), {QStringLiteral("stop")});
  }
  // labwc --exit signals the inherited LABWC_PID. Reject process-group and invalid IDs.
  const auto value = qEnvironmentVariable("LABWC_PID");
  bool valid = false;
  const auto pid = value.toLongLong(&valid);
  static const QRegularExpression digits(QStringLiteral("^[0-9]+$"));
  if (!valid || !digits.match(value).hasMatch() || pid <= 1 || pid > std::numeric_limits<int>::max()) {
    return SessionCommandResult::failure(QStringLiteral("cannot identify labwc session: valid LABWC_PID is required"));
  }
  return run(QStringLiteral("labwc"), {QStringLiteral("--exit")});
}
