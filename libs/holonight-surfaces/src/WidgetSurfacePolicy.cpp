#include "WidgetSurfacePolicy.h"

#include "ConfigService.h"
#include "ShellConstants.h"

#include <algorithm>

using namespace HoloNight::ShellConfig;

namespace {
constexpr int kWidgetWidth = 460;
constexpr int kWidgetHeight = 200;
}  // namespace

Holonight::Wayland::Anchors anchorsForPosition(WidgetPosition position) {
  using enum Holonight::Wayland::Anchor;
  switch (position) {
    case WidgetPosition::LeftTop:
      return Left | Top;
    case WidgetPosition::CenterTop:
      return Top;
    case WidgetPosition::RightTop:
      return Right | Top;
    case WidgetPosition::LeftCenter:
      return Left;
    case WidgetPosition::CenterCenter:
      return {};
    case WidgetPosition::RightCenter:
      return Right;
    case WidgetPosition::LeftBottom:
      return Left | Bottom;
    case WidgetPosition::CenterBottom:
      return Bottom;
    case WidgetPosition::RightBottom:
      return Right | Bottom;
  }
  return {};
}

std::uint32_t anchorFlagsForPosition(WidgetPosition position) {
  return static_cast<std::uint32_t>(anchorsForPosition(position).toInt());
}

WidgetSurfacePlacement widgetSurfacePlacement(WidgetPosition position, int margin, int width, int height) {
  const int top_margin = widgetPositionIsTopAnchored(position) ? kBarHeight + margin : margin;
  return WidgetSurfacePlacement{
      .anchors = anchorsForPosition(position),
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
