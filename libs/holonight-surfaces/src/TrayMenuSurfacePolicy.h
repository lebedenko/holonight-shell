#pragma once

#include <QRect>

struct TrayMenuPlacement {
  int x{0};
  int y{0};
  int width{244};
  int height{40};
  int column_count{1};
  int top_level_column{0};
  int column_step{1};
};

struct TrayMenuActiveGeometry {
  int surface_width{};
  int surface_height{};
  int margin_left{};
  int margin_top{};
  int padding_left{};
  int padding_right{};
  int padding_top{};
  int padding_bottom{};
  int column_count{1};
  int column_index{};
  int panel_height{};
};

inline constexpr int kTrayMenuWidth = 244;
inline constexpr int kTraySubmenuGap = 6;
inline constexpr int kTrayMenuMinHeight = 40;
inline constexpr int kTrayMenuMaxHeight = 480;

[[nodiscard]] TrayMenuPlacement trayMenuPlacement(const QRect& screen_geometry, int screen_x, int screen_y);
[[nodiscard]] TrayMenuActiveGeometry trayMenuActiveGeometry(const QRect& screen_geometry,
                                                            const TrayMenuPlacement& placement, int column_count,
                                                            int panel_height);
