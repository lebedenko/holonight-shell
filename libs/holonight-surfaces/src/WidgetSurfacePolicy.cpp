#include "WidgetSurfacePolicy.h"

#include "ConfigService.h"
#include "ShellConstants.h"
#include "qwayland-wlr-layer-shell-unstable-v1.h"

#include <algorithm>

using namespace HoloNight::ShellConfig;

namespace {
constexpr int kWidgetWidth = 460;
constexpr int kWidgetHeight = 200;
}  // namespace

std::uint32_t anchorFlagsForPosition(WidgetPosition position) {
  using Surface = QtWayland::zwlr_layer_surface_v1;
  switch (position) {
    case WidgetPosition::LeftTop:
      return Surface::anchor_left | Surface::anchor_top;
    case WidgetPosition::CenterTop:
      return Surface::anchor_top;
    case WidgetPosition::RightTop:
      return Surface::anchor_right | Surface::anchor_top;
    case WidgetPosition::LeftCenter:
      return Surface::anchor_left;
    case WidgetPosition::CenterCenter:
      return 0;
    case WidgetPosition::RightCenter:
      return Surface::anchor_right;
    case WidgetPosition::LeftBottom:
      return Surface::anchor_left | Surface::anchor_bottom;
    case WidgetPosition::CenterBottom:
      return Surface::anchor_bottom;
    case WidgetPosition::RightBottom:
      return Surface::anchor_right | Surface::anchor_bottom;
  }
  return 0;
}

WidgetSurfacePlacement widgetSurfacePlacement(WidgetPosition position, int margin, int width, int height) {
  const int top_margin = widgetPositionIsTopAnchored(position) ? kBarHeight + margin : margin;
  return WidgetSurfacePlacement{
      .anchor_flags = anchorFlagsForPosition(position),
      .width = width,
      .height = height,
      .top_margin = top_margin,
      .right_margin = margin,
      .bottom_margin = margin,
      .left_margin = margin,
  };
}

WidgetSurfacePlacement widgetSurfacePlacement(WidgetPosition position, int margin) {
  return widgetSurfacePlacement(position, margin, kWidgetWidth, kWidgetHeight);
}

bool widgetTargetsMonitor(const QStringList& configured_monitors, const QString& monitor_name) {
  return configured_monitors.isEmpty() || configured_monitors.contains(monitor_name);
}

bool widgetBlockedOnMonitor(const QList<QStringList>& position_blockers, const QString& monitor_name) {
  return std::ranges::any_of(
      position_blockers, [&](const QStringList& filter) { return filter.isEmpty() || filter.contains(monitor_name); });
}

bool shouldCreateWidgetSurface(const QStringList& configured_monitors, const QList<QStringList>& position_blockers,
                               const QString& monitor_name) {
  return widgetTargetsMonitor(configured_monitors, monitor_name) &&
         !widgetBlockedOnMonitor(position_blockers, monitor_name);
}
