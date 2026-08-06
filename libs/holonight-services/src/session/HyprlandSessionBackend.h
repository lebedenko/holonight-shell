#pragma once

#include "SessionBackend.h"

// Session backend for Hyprland. The only compositor-specific behaviour is logout, which is
// dispatched through hyprctl. Power actions and the lock strategy are inherited from the base.
class HyprlandSessionBackend final : public SessionBackend {
 public:
  HyprlandSessionBackend(const ProcessEnvironment* env, CommandRunner* runner);

  [[nodiscard]] SessionCommandResult logout() override;
  [[nodiscard]] SessionCommandResult lockScreen() override { return runLocker(); }

  [[nodiscard]] bool logoutSupported() const override { return true; }
  [[nodiscard]] QString backendName() const override { return QStringLiteral("hyprland"); }
};
