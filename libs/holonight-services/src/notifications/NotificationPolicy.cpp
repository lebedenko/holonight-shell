#include "NotificationPolicy.h"

NotificationPlacementDecision placeNotification(QList<uint32_t>& visible, QList<uint32_t>& queue, uint32_t notif_id,
                                                NotifUrgency urgency,
                                                const QHash<uint32_t, NotifUrgency>& visible_urgencies,
                                                int max_visible) {
  if (visible.size() < max_visible) {
    visible.append(notif_id);
    return {.lifecycle = NotifLifecycle::Visible};
  }

  if (urgency == NotifUrgency::Critical) {
    int bump_index = -1;
    for (int idx = 0; idx < visible.size(); ++idx) {
      if (visible_urgencies.value(visible.at(idx), NotifUrgency::Normal) != NotifUrgency::Critical) {
        bump_index = idx;
        break;
      }
    }
    if (bump_index >= 0) {
      const uint32_t bumped_id = visible.at(bump_index);
      visible.removeAt(bump_index);
      queue.prepend(bumped_id);
      visible.append(notif_id);
      return {.lifecycle = NotifLifecycle::Visible, .bumped_id = bumped_id, .queue_changed = true};
    }
  }

  queue.append(notif_id);
  return {.lifecycle = NotifLifecycle::Queued, .queue_changed = true};
}

std::optional<uint32_t> promoteQueuedNotification(QList<uint32_t>& visible, QList<uint32_t>& queue, int max_visible) {
  if (queue.isEmpty() || visible.size() >= max_visible) {
    return std::nullopt;
  }
  const uint32_t promoted_id = queue.takeFirst();
  visible.append(promoted_id);
  return promoted_id;
}
