#include "TrayMenuSurfacePolicy.h"

#include "ShellConstants.h"

#include <algorithm>

namespace {
constexpr int kMenuGap = 2;
constexpr int kMenuMaxColumns = 2;
constexpr int kShadowPadding = 12;
}  // namespace

TrayMenuPlacement trayMenuPlacement(const QRect& screen, int screen_x, int screen_y) {
  const int local_x = screen_x - screen.x();
  const int available_width = std::max(kTrayMenuWidth, screen.width() - (kScreenEdgeMargin * 2));
  const int top_x = std::clamp(local_x, kScreenEdgeMargin,
                               std::max(kScreenEdgeMargin, screen.width() - kTrayMenuWidth - kScreenEdgeMargin));
  const bool fits_two = available_width >= (kTrayMenuWidth * kMenuMaxColumns) + kTraySubmenuGap;
  const bool opens_right =
      fits_two && top_x + (kTrayMenuWidth * 2) + kTraySubmenuGap <= screen.width() - kScreenEdgeMargin;
  const bool opens_left = fits_two && top_x - kTrayMenuWidth - kTraySubmenuGap >= kScreenEdgeMargin;

  TrayMenuPlacement result;
  result.column_count = (opens_right || opens_left) ? kMenuMaxColumns : 1;
  result.width = (result.column_count * kTrayMenuWidth) + ((result.column_count - 1) * kTraySubmenuGap);
  result.top_level_column = opens_left && !opens_right ? 1 : 0;
  result.column_step = result.top_level_column == 1 ? -1 : 1;
  const int requested_x = result.top_level_column == 1 ? top_x - kTrayMenuWidth - kTraySubmenuGap : top_x;
  result.x = std::clamp(requested_x, kScreenEdgeMargin,
                        std::max(kScreenEdgeMargin, screen.width() - result.width - kScreenEdgeMargin));
  result.y = std::clamp(screen_y - screen.y() + kMenuGap, 0, std::max(0, screen.height() - kTrayMenuMinHeight));
  result.height =
      std::min(kTrayMenuMaxHeight, std::max(kTrayMenuMinHeight, screen.height() - result.y - kScreenEdgeMargin));
  return result;
}

TrayMenuActiveGeometry trayMenuActiveGeometry(const QRect& screen, const TrayMenuPlacement& placement, int column_count,
                                              int panel_height) {
  const int active_width = (column_count * kTrayMenuWidth) + ((column_count - 1) * kTraySubmenuGap);
  const int active_x =
      placement.x + ((placement.top_level_column == 1 && column_count == 1) ? kTrayMenuWidth + kTraySubmenuGap : 0);
  TrayMenuActiveGeometry result;
  result.margin_left = std::max(0, active_x - kShadowPadding);
  result.margin_top = std::max(0, placement.y - kShadowPadding);
  result.padding_left = active_x - result.margin_left;
  result.padding_top = placement.y - result.margin_top;
  result.padding_right = std::min(kShadowPadding, screen.width() - active_x - active_width);
  result.padding_bottom = std::min(kShadowPadding, screen.height() - placement.y - panel_height);
  result.surface_width = active_width + result.padding_left + result.padding_right;
  result.surface_height = panel_height + result.padding_top + result.padding_bottom;
  result.column_count = column_count;
  result.column_index = (placement.top_level_column == 1 && column_count == 2) ? 1 : 0;
  result.panel_height = panel_height;
  return result;
}
