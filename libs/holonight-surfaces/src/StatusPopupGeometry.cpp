#include "StatusPopupGeometry.h"

#include "ShellConstants.h"

#include <algorithm>

StatusPopupGeometry statusPopupGeometry(const QSize& requested_surface_size, const QRect& screen_geometry,
                                        const QRect& available_geometry, int anchor_x, int anchor_width) {
  constexpr int kGlowPadding = 24;
  constexpr int kTopPadding = 6;
  constexpr int kNotchCornerClearance = 9;
  const QRect usable = screen_geometry.intersected(available_geometry);
  const int available_width = std::max(1, usable.width() - (2 * kScreenEdgeMargin));
  const int available_height = std::max(
      1, std::min(usable.height(), screen_geometry.height() - kBarHeight) - kStatusPopupTopGap - kScreenEdgeMargin);
  const int surface_width = std::clamp(requested_surface_size.width(), 1, available_width);
  const int surface_height = std::clamp(requested_surface_size.height(), 1, available_height);
  const int content_width = std::max(0, surface_width - (2 * kGlowPadding));
  const int anchor_center = anchor_x - screen_geometry.x() + (anchor_width / 2);
  const int minimum_left = usable.x() - screen_geometry.x() + kScreenEdgeMargin;
  const int maximum_left =
      std::max(minimum_left, usable.right() + 1 - screen_geometry.x() - kScreenEdgeMargin - surface_width);
  const int left = std::clamp(anchor_center - (surface_width / 2), minimum_left, maximum_left);
  const int pointer_inset = std::min(surface_width / 2, kGlowPadding + kNotchCornerClearance);
  return {
      .content_width = content_width,
      .content_height = std::max(0, surface_height - kTopPadding - kGlowPadding),
      .surface_width = surface_width,
      .surface_height = surface_height,
      .left_margin = left,
      .pointer_x = std::clamp(anchor_center - left, pointer_inset, surface_width - pointer_inset),
  };
}
