#pragma once

// Shared geometry constants for layer-shell surfaces.

// Height of the top bar in logical pixels. The bar reserves this as its exclusive zone, so any
// surface that must sit clear of the bar (e.g. top-anchored desktop widgets) offsets by it.
inline constexpr int kBarHeight = 64;
inline constexpr int kStatusPopupTopGap = 4;

inline constexpr int kSidebarTabBarWidth = 64;
inline constexpr int kSidebarRightMargin = 24;
inline constexpr int kSidebarBottomMargin = 24;
inline constexpr int kSidebarTopMargin = kBarHeight + 8;
inline constexpr int kSidebarMaxContentWidth = 400;
// Sidebar height policy: open at the default, then snap to content height.
// 64 px brand + 12 px tab inset + (6 × 48 px tabs) + (5 × 4 px gaps) + 12 px protected
// separation + 1 px divider + 64 px profile area keeps the complete tab rail usable.
inline constexpr int kSidebarMinHeight = 461;
inline constexpr int kSidebarDefaultHeight = 600;  // first-open fallback before QML reports preferredHeight

// Minimum clearance kept between a popup/menu surface and the screen edge.
inline constexpr int kScreenEdgeMargin = 8;
