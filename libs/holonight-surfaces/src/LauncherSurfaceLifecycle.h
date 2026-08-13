#pragma once

#include <QString>

#include <cstdint>
#include <optional>

enum class LauncherSurfaceCommandType : std::uint8_t {
  None,
  CreateSurface,
  DestroySurface,
  StartCloseAnimation,
};

struct LauncherSurfaceCommand {
  LauncherSurfaceCommandType type{LauncherSurfaceCommandType::None};
  QString screen_name;
  std::optional<bool> visible;
};

class LauncherSurfaceLifecycle {
 public:
  [[nodiscard]] bool visible() const { return visible_; }
  [[nodiscard]] bool closing() const { return closing_; }
  [[nodiscard]] bool pendingShow() const { return pending_show_; }
  [[nodiscard]] QString pendingScreen() const { return pending_screen_; }

  [[nodiscard]] LauncherSurfaceCommand toggle(const QString& screen_name, bool provider_available, bool has_view);
  [[nodiscard]] LauncherSurfaceCommand show(const QString& screen_name, bool provider_available);
  [[nodiscard]] LauncherSurfaceCommand hide(bool has_view);
  [[nodiscard]] LauncherSurfaceCommand notifyHideReady(bool has_view);
  [[nodiscard]] LauncherSurfaceCommand providerAvailabilityChanged(bool provider_available);
  [[nodiscard]] LauncherSurfaceCommand surfaceCreated(bool created);
  [[nodiscard]] LauncherSurfaceCommand surfaceClosed();

 private:
  bool pending_show_{false};
  bool visible_{false};
  bool closing_{false};
  QString pending_screen_;
};
