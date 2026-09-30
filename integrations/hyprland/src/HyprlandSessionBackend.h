#pragma once

#include "SessionBackend.h"

// Session backend for Hyprland. UWSM owns logout when its compositor unit is active; direct
// sessions dispatch exit through hyprctl. Power actions and locking are inherited from the base.
class HyprlandSessionBackend final : public SessionBackend {
 public:
  HyprlandSessionBackend(const ProcessEnvironment* env, CommandRunner* runner);

  [[nodiscard]] SessionCommandResult logout() override;
  [[nodiscard]] SessionCommandResult lockScreen() override { return runLocker(); }

  [[nodiscard]] bool logoutSupported() const override { return true; }
  [[nodiscard]] QString backendName() const override { return QStringLiteral("hyprland"); }

 private:
  const ProcessEnvironment* env_;
};
