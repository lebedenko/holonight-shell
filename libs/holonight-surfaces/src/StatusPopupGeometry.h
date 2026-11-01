#pragma once

#include <QRect>
#include <QSize>

// Fixed layer-surface allocation for one popup session. Resizing a layer surface is not atomic:
// the compositor can stretch a stale buffer into the new size, so the panel moves inside this
// constant surface instead (see StatusPopup.qml).
struct StatusPopupSurfacePlacement {
  QSize surface_size;
  int left_margin{};
  int anchor_center{};  // Horizontal anchor center in surface coordinates.
  bool operator==(const StatusPopupSurfacePlacement&) const = default;
};

[[nodiscard]] QSize statusPopupMaximumSize(const QRect& screen_geometry, const QRect& available_geometry);

[[nodiscard]] StatusPopupSurfacePlacement statusPopupSurfacePlacement(const QRect& screen_geometry,
                                                                      const QRect& available_geometry, int anchor_x,
                                                                      int anchor_width);

[[nodiscard]] QRect statusPopupInputRect(const QSize& surface_size);
