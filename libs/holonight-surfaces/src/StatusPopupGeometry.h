#pragma once

#include <QRect>
#include <QSize>
#include <QString>

#include <cstdint>

enum class StatusPopupOverflowMode : std::uint8_t {
  FixedContent,
  InternalList,
};

struct StatusPopupSizePolicy {
  QSize minimum_content_size;
  QSize preferred_content_size;
  QSize maximum_content_size;
  StatusPopupOverflowMode overflow_mode{StatusPopupOverflowMode::FixedContent};
};

struct StatusPopupGeometry {
  int content_width{};
  int content_height{};
  int surface_width{};
  int surface_height{};
  int left_margin{};
  int pointer_x{};
};

[[nodiscard]] StatusPopupSizePolicy statusPopupSizePolicy(const QString& popup_id);
[[nodiscard]] StatusPopupGeometry statusPopupGeometry(const QString& popup_id, const QRect& screen_geometry,
                                                      const QRect& available_geometry, int anchor_x, int anchor_width);
