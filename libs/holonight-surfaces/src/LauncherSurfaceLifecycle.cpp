#include "LauncherSurfaceLifecycle.h"

LauncherSurfaceCommand LauncherSurfaceLifecycle::toggle(const QString& screen_name, bool shell_active, bool has_view) {
  if (visible_ && !closing_) {
    return hide(has_view);
  }
  return show(screen_name, shell_active);
}

LauncherSurfaceCommand LauncherSurfaceLifecycle::show(const QString& screen_name, bool shell_active) {
  if (!shell_active) {
    pending_show_ = true;
    pending_screen_ = screen_name;
    return {};
  }

  closing_ = false;
  return {
      .type = LauncherSurfaceCommandType::CreateSurface,
      .screen_name = screen_name,
  };
}

LauncherSurfaceCommand LauncherSurfaceLifecycle::hide(bool has_view) {
  if (!has_view || !visible_ || closing_) {
    return {};
  }

  closing_ = true;
  return {.type = LauncherSurfaceCommandType::StartCloseAnimation};
}

LauncherSurfaceCommand LauncherSurfaceLifecycle::notifyHideReady(bool has_view) {
  if (!has_view) {
    return {};
  }

  closing_ = false;
  visible_ = false;
  return {
      .type = LauncherSurfaceCommandType::DestroySurface,
      .visible = false,
  };
}

LauncherSurfaceCommand LauncherSurfaceLifecycle::shellActivated(bool shell_active) {
  if (!shell_active || !pending_show_) {
    return {};
  }

  pending_show_ = false;
  const QString screen = pending_screen_;
  pending_screen_.clear();
  return show(screen, true);
}

LauncherSurfaceCommand LauncherSurfaceLifecycle::surfaceCreated(bool created) {
  visible_ = created;
  return {.visible = created};
}

LauncherSurfaceCommand LauncherSurfaceLifecycle::surfaceClosed() {
  closing_ = false;
  visible_ = false;
  return {
      .type = LauncherSurfaceCommandType::DestroySurface,
      .visible = false,
  };
}
