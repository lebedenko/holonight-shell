#include "NotificationFilter.h"

#include <algorithm>

FilterDecision evaluateFilter(const NotificationData& data, bool dnd_enabled, const QList<AppNotificationRule>& rules) {
  // Critical urgency always passes — bypasses DND and per-app rules.
  if (data.urgency == NotifUrgency::Critical) {
    return FilterDecision::Allow;
  }

  if (dnd_enabled) {
    return FilterDecision::Suppress;
  }

  const auto rule_it =
      std::ranges::find_if(rules, [&](const AppNotificationRule& rule) { return rule.app_name == data.app_name; });
  if (rule_it != rules.cend()) {
    if (!rule_it->enabled) {
      return FilterDecision::Suppress;
    }
    const auto filter = rule_it->urgency_filter;
    if ((filter == UrgencyFilter::Low && data.urgency == NotifUrgency::Low) ||
        (filter == UrgencyFilter::Normal && data.urgency == NotifUrgency::Normal) ||
        (filter == UrgencyFilter::LowAndNormal &&
         (data.urgency == NotifUrgency::Low || data.urgency == NotifUrgency::Normal))) {
      return FilterDecision::Suppress;
    }
  }

  return FilterDecision::Allow;
}
