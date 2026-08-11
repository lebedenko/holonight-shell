#include "SessionService.h"

#include "session/CommandRunner.h"
#include "session/HyprlandSessionBackend.h"
#include "session/LogindSessionBackend.h"
#include "session/ProcessEnvironment.h"
#include "session/SessionBackend.h"

#include <algorithm>

namespace {
std::unique_ptr<SessionBackend> makeBackend(const ProcessEnvironment* env, CommandRunner* runner) {
  const QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QChar(':'), Qt::SkipEmptyParts);
  const bool declares_hyprland = std::ranges::any_of(desktops, [](const QString& desktop) {
    return desktop.compare(QStringLiteral("Hyprland"), Qt::CaseInsensitive) == 0;
  });
  if (!qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE") || declares_hyprland) {
    return std::make_unique<HyprlandSessionBackend>(env, runner);
  }
  return std::make_unique<LogindSessionBackend>(env, runner);
}
}  // namespace

SessionService::SessionService(QObject* parent)
    : QObject(parent),
      env_(std::make_unique<SystemProcessEnvironment>()),
      runner_(std::make_unique<DetachedCommandRunner>()),
      backend_(makeBackend(env_.get(), runner_.get())) {}

SessionService::SessionService(std::unique_ptr<SessionBackend> backend, QObject* parent)
    : QObject(parent), backend_(std::move(backend)) {}

SessionService::~SessionService() = default;

QString SessionService::backendName() const { return backend_->backendName(); }

bool SessionService::logoutSupported() const { return backend_->logoutSupported(); }

bool SessionService::lockerAvailable() const { return backend_->lockerAvailable(); }

QString SessionService::lockerName() const { return backend_->lockerName(); }

void SessionService::lockScreen() {
  const SessionCommandResult result = backend_->lockScreen();
  if (!result.ok) {
    emit commandFailed(QStringLiteral("lock"), result.reason);
  }
}

void SessionService::logout() {
  const SessionCommandResult result = backend_->logout();
  if (!result.ok) {
    emit commandFailed(QStringLiteral("logout"), result.reason);
  }
}

void SessionService::sleep() {
  const SessionCommandResult result = backend_->sleep();
  if (!result.ok) {
    emit commandFailed(QStringLiteral("sleep"), result.reason);
  }
}

void SessionService::reboot() {
  const SessionCommandResult result = backend_->reboot();
  if (!result.ok) {
    emit commandFailed(QStringLiteral("reboot"), result.reason);
  }
}

void SessionService::shutdown() {
  const SessionCommandResult result = backend_->shutdown();
  if (!result.ok) {
    emit commandFailed(QStringLiteral("shutdown"), result.reason);
  }
}
