#include "StatusPopupGeometry.h"

#include "ShellConstants.h"

#include <algorithm>

namespace {
constexpr int kPopupNotchRadius = 8;
constexpr int kGlowPadding = 24;
constexpr int kTopPadding = 6;

QSize boundedDimension(const QSize& minimum, const QSize& preferred, const QSize& maximum, int available_width,
                       int available_height) {
  const int max_width = std::min(maximum.width(), std::max(1, available_width));
  const int max_height = std::min(maximum.height(), std::max(1, available_height));
  const int width = max_width < minimum.width() ? max_width : std::clamp(preferred.width(), minimum.width(), max_width);
  const int height =
      max_height < minimum.height() ? max_height : std::clamp(preferred.height(), minimum.height(), max_height);
  return {width, height};
}
}  // namespace

StatusPopupSizePolicy statusPopupSizePolicy(const QString& popup_id) {
  if (popup_id == QLatin1String("audio")) {
    return {.minimum_content_size = {600, 480},
            .preferred_content_size = {780, 820},
            .maximum_content_size = {780, 820},
            .overflow_mode = StatusPopupOverflowMode::InternalList};
  }
  if (popup_id == QLatin1String("network")) {
    return {.minimum_content_size = {480, 500},
            .preferred_content_size = {600, 866},
            .maximum_content_size = {600, 866},
            .overflow_mode = StatusPopupOverflowMode::InternalList};
  }
  if (popup_id == QLatin1String("weather")) {
    return {.minimum_content_size = {600, 640},
            .preferred_content_size = {760, 960},
            .maximum_content_size = {760, 960},
            .overflow_mode = StatusPopupOverflowMode::FixedContent};
  }
  if (popup_id == QLatin1String("battery")) {
    return {.minimum_content_size = {280, 280},
            .preferred_content_size = {300, 360},
            .maximum_content_size = {300, 360},
            .overflow_mode = StatusPopupOverflowMode::FixedContent};
  }
  return {.minimum_content_size = {320, 240},
          .preferred_content_size = {480, 320},
          .maximum_content_size = {480, 320},
          .overflow_mode = StatusPopupOverflowMode::FixedContent};
}

StatusPopupGeometry statusPopupGeometry(const QString& popup_id, const QRect& screen_geometry,
                                        const QRect& available_geometry, int anchor_x, int anchor_width) {
  const StatusPopupSizePolicy policy = statusPopupSizePolicy(popup_id);
  const int available_width = std::min(available_geometry.width(), screen_geometry.width());
  const int safe_surface_width = std::max(1, available_width - (2 * kScreenEdgeMargin));
  const int available_height = std::min(available_geometry.height(), screen_geometry.height() - kBarHeight);
  const int safe_surface_height = std::max(1, available_height - kStatusPopupTopGap - kScreenEdgeMargin);
  const QSize content_size =
      boundedDimension(policy.minimum_content_size, policy.preferred_content_size, policy.maximum_content_size,
                       safe_surface_width - (2 * kGlowPadding), safe_surface_height - kTopPadding - kGlowPadding);
  const int content_width = content_size.width();
  const int surface_width = content_width + (2 * kGlowPadding);
  const int surface_height = content_size.height() + kTopPadding + kGlowPadding;
  const int local_anchor_x = anchor_x - screen_geometry.x();
  const int anchor_center = local_anchor_x + (anchor_width / 2);
  const int min_panel_left = kGlowPadding + kScreenEdgeMargin;
  const int max_panel_left =
      std::max(min_panel_left, screen_geometry.width() - content_width - kGlowPadding - kScreenEdgeMargin);
  const int panel_left = std::clamp(anchor_center - (content_width / 2), min_panel_left, max_panel_left);
  const int left_margin = panel_left - kGlowPadding;
  const int raw_pointer_x = anchor_center - left_margin;
  const int min_pointer = kGlowPadding + kPopupNotchRadius + 1;
  const int max_pointer = kGlowPadding + content_width - kPopupNotchRadius - 1;

  return StatusPopupGeometry{
      .content_width = content_width,
      .content_height = content_size.height(),
      .surface_width = surface_width,
      .surface_height = surface_height,
      .left_margin = left_margin,
      .pointer_x = std::clamp(raw_pointer_x, min_pointer, max_pointer),
  };
}
