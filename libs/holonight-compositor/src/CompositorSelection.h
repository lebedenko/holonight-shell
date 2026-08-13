#pragma once

#include <QByteArray>

#include <cstdint>

enum class CompositorKind : std::uint8_t { Generic, Hyprland, Sway };

struct CompositorEnvironment {
  QByteArray current_desktop;
  QByteArray hyprland_instance_signature;
  QByteArray sway_socket;
};

[[nodiscard]] CompositorKind selectCompositor(const CompositorEnvironment& environment);
[[nodiscard]] CompositorEnvironment systemCompositorEnvironment();
[[nodiscard]] const char* compositorName(CompositorKind kind);
