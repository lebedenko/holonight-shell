#pragma once

#include "SessionBackend.h"

class SwaySessionBackend final : public SessionBackend {
 public:
  SwaySessionBackend(const ProcessEnvironment* environment, CommandRunner* runner);
  [[nodiscard]] SessionCommandResult logout() override;
  [[nodiscard]] SessionCommandResult lockScreen() override { return runLocker(); }
  [[nodiscard]] bool logoutSupported() const override { return true; }
  [[nodiscard]] QString backendName() const override { return QStringLiteral("sway"); }

 private:
  const ProcessEnvironment* environment_;
};
