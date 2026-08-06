#include "TooltipGeometry.h"

#include "ShellConstants.h"

#include <algorithm>

namespace TooltipGeometry {

int leftMargin(int screen_width, int screen_origin_x, int anchor_x, int anchor_width) {
  const int local_anchor_x = anchor_x - screen_origin_x;
  const int centered_left = local_anchor_x + (anchor_width / 2) - (kWidth / 2);
  const int max_left = std::max(kScreenEdgeMargin, screen_width - kWidth - kScreenEdgeMargin);
  return std::clamp(centered_left, kScreenEdgeMargin, max_left);
}

}  // namespace TooltipGeometry
