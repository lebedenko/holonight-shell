#pragma once

#include "SessionBackend.h"

class LabwcSessionBackend final : public SessionBackend {
 public:
  LabwcSessionBackend(const ProcessEnvironment* environment, CommandRunner* runner);
  [[nodiscard]] SessionCommandResult logout() override;
  [[nodiscard]] SessionCommandResult lockScreen() override { return runLocker(); }
  [[nodiscard]] bool logoutSupported() const override { return true; }
  [[nodiscard]] QString backendName() const override { return QStringLiteral("labwc"); }

 private:
  const ProcessEnvironment* environment_;
};
