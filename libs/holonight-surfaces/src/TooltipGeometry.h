#pragma once

namespace TooltipGeometry {

inline constexpr int kWidth = 324;

[[nodiscard]] int leftMargin(int screen_width, int screen_origin_x, int anchor_x, int anchor_width);

}  // namespace TooltipGeometry
