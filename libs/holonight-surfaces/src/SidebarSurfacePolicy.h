#pragma once

#include <holonight/wayland/layersurfacespec.h>

class QScreen;

[[nodiscard]] int sidebarDefaultHeight();
[[nodiscard]] int boundedSidebarHeight(int requested_height, int screen_height);
[[nodiscard]] Holonight::Wayland::LayerSurfaceSpec sidebarSurfaceSpec(QScreen* screen, const QString& monitor_name,
                                                                      int panel_height, int current_tab);
