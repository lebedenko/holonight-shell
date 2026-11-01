#include "StatusPopupGeometry.h"

#include "ShellConstants.h"

#include <algorithm>

QSize statusPopupMaximumSize(const QRect& screen_geometry, const QRect& available_geometry) {
  const QRect usable = screen_geometry.intersected(available_geometry);
  const int available_width = std::max(1, usable.width() - (2 * kScreenEdgeMargin));
  const int available_height = std::max(
      1, std::min(usable.height(), screen_geometry.height() - kBarHeight) - kStatusPopupTopGap - kScreenEdgeMargin);
  return {available_width, available_height};
}

StatusPopupSurfacePlacement statusPopupSurfacePlacement(const QRect& screen_geometry, const QRect& available_geometry,
                                                        int anchor_x, int anchor_width) {
  const QRect usable = screen_geometry.intersected(available_geometry);
  const int left_margin = usable.x() - screen_geometry.x() + kScreenEdgeMargin;
  return {
      .surface_size = statusPopupMaximumSize(screen_geometry, available_geometry),
      .left_margin = left_margin,
      .anchor_center = anchor_x - screen_geometry.x() + (anchor_width / 2) - left_margin,
  };
}

QRect statusPopupInputRect(const QSize& surface_size) {
  constexpr int kGlowPadding = 24;
  constexpr int kPanelTop = 15;
  return {kGlowPadding, kPanelTop, std::max(0, surface_size.width() - 2 * kGlowPadding),
          std::max(0, surface_size.height() - kPanelTop - kGlowPadding)};
}
