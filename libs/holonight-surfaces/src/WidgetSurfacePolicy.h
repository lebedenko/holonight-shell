#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>

enum class WidgetPosition : std::uint8_t;

struct WidgetSurfacePlacement {
  std::uint32_t anchor_flags{};
  int width{};
  int height{};
  int top_margin{};
  int right_margin{};
  int bottom_margin{};
  int left_margin{};
};

// The nine-way position-to-layer-shell-anchor switch, public so it exists exactly once in the
// codebase (REQ-C-006). Surfaces that are not widgets -- the OSD -- need the anchor flags without
// widgetSurfacePlacement()'s widget-specific width, height and margins.
[[nodiscard]] std::uint32_t anchorFlagsForPosition(WidgetPosition position);

// MPRIS desktop widget size: provides transparent blur padding around its 256px content column,
// unlike the 460x200 Clock/TimeToEvent default below.
inline constexpr int kMprisWidgetWidth = 368;
inline constexpr int kMprisWidgetHeight = 456;

[[nodiscard]] WidgetSurfacePlacement widgetSurfacePlacement(WidgetPosition position, int margin);
[[nodiscard]] WidgetSurfacePlacement widgetSurfacePlacement(WidgetPosition position, int margin, int width, int height);
[[nodiscard]] bool widgetTargetsMonitor(const QStringList& configured_monitors, const QString& monitor_name);
[[nodiscard]] bool widgetBlockedOnMonitor(const QList<QStringList>& position_blockers, const QString& monitor_name);
[[nodiscard]] bool shouldCreateWidgetSurface(const QStringList& configured_monitors,
                                             const QList<QStringList>& position_blockers, const QString& monitor_name);
