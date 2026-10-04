#pragma once

#include <QRect>
#include <QSize>

// Bootstrap allocation only; QML measurements replace it before the entrance animation.
inline constexpr QSize kStatusPopupInitialSize{480, 320};

struct StatusPopupGeometry {
  int content_width{};
  int content_height{};
  int surface_width{};
  int surface_height{};
  int left_margin{};
  int pointer_x{};
  bool operator==(const StatusPopupGeometry&) const = default;
};

[[nodiscard]] StatusPopupGeometry statusPopupGeometry(const QSize& requested_surface_size, const QRect& screen_geometry,
                                                      const QRect& available_geometry, int anchor_x, int anchor_width);
