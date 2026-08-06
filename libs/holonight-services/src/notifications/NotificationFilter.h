#pragma once

#include "NotificationTypes.h"

#include <QList>
#include <QString>

#include <cstdint>

// NOLINTNEXTLINE(cppcoreguidelines-use-enum-class, performance-enum-size)
enum class UrgencyFilter : uint8_t {
  None = 0,          // suppress nothing
  Low = 1,           // suppress urgency=0
  Normal = 2,        // suppress urgency=1
  LowAndNormal = 3,  // suppress urgency=0 and urgency=1
};

struct AppNotificationRule {
  QString app_name;
  QString desktop_entry;
  QString app_icon;
  bool enabled{true};
  UrgencyFilter urgency_filter{UrgencyFilter::None};
  qint64 last_seen_ms{0};
};

enum class FilterDecision : uint8_t { Allow, Suppress };

// Returns Allow or Suppress for the given notification against the current DND
// state and per-app rule list. Critical urgency (urgency==2) always returns Allow
// — it bypasses both DND and per-app rules unconditionally (REQ-F-NH16 > REQ-F-NH10).
[[nodiscard]] FilterDecision evaluateFilter(const NotificationData& data, bool dnd_enabled,
                                            const QList<AppNotificationRule>& rules);
