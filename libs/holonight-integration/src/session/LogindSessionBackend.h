#pragma once

#include "SessionBackend.h"

// Generic fallback backend for compositors with no dedicated adapter. Power actions and the lock
// strategy work through logind/systemctl; logout is unsupported because there is no portable,
// compositor-agnostic way to end the graphical session, so it is reported as such and is a no-op.
class LogindSessionBackend final : public SessionBackend {
 public:
  LogindSessionBackend(const ProcessEnvironment* env, CommandRunner* runner);

  // unsupported: no portable way to exit an arbitrary compositor; a documented capability
  // limitation surfaced via logoutSupported(), not a dispatch failure.
  [[nodiscard]] SessionCommandResult logout() override { return SessionCommandResult::success(); }
  [[nodiscard]] SessionCommandResult lockScreen() override { return runLocker(); }

  [[nodiscard]] bool logoutSupported() const override { return false; }
  [[nodiscard]] QString backendName() const override { return QStringLiteral("logind"); }
};
