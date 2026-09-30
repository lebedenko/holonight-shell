#include "SessionBackend.h"

#include "CommandRunner.h"
#include "Locker.h"

SessionBackend::SessionBackend(const ProcessEnvironment* env, CommandRunner* runner)
    : runner_(runner), locker_(std::make_unique<Locker>(env, runner)) {}

SessionBackend::~SessionBackend() = default;

SessionCommandResult SessionBackend::sleep() { return run(QStringLiteral("systemctl"), {QStringLiteral("suspend")}); }

SessionCommandResult SessionBackend::reboot() { return run(QStringLiteral("systemctl"), {QStringLiteral("reboot")}); }

SessionCommandResult SessionBackend::shutdown() {
  return run(QStringLiteral("systemctl"), {QStringLiteral("poweroff")});
}

SessionCommandResult SessionBackend::runLocker() { return locker_->lock(); }

SessionCommandResult SessionBackend::run(const QString& program, const QStringList& args) {
  if (!runner_->run(program, args)) {
    return SessionCommandResult::failure(QStringLiteral("failed to launch %1 %2").arg(program, args.join(QChar(' '))));
  }
  return SessionCommandResult::success();
}

bool SessionBackend::lockerAvailable() const { return locker_->lockerAvailable(); }

QString SessionBackend::lockerName() const { return locker_->lockerName(); }
