#include "NotificationTypes.h"

NotifAccentKind accentForData(const NotificationData& data) {
  if (data.urgency == NotifUrgency::Critical) {
    return NotifAccentKind::Critical;
  }
  if (data.category.startsWith(QStringLiteral("im.")) || data.category.startsWith(QStringLiteral("call.")) ||
      data.category.startsWith(QStringLiteral("presence."))) {
    return NotifAccentKind::Violet;
  }
  return NotifAccentKind::Cyan;
}

int effectiveTimeoutMs(const NotificationData& data, int default_timeout_ms) {
  if (data.expire_timeout_ms > 0) {
    return data.expire_timeout_ms;
  }
  if (data.expire_timeout_ms == 0) {
    return -1;  // never expire
  }
  // expire_timeout_ms == -1 (server default): honour urgency.
  if (data.urgency == NotifUrgency::Critical) {
    return -1;  // critical never auto-expires
  }
  return default_timeout_ms;
}
