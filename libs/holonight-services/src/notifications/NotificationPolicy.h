#pragma once

#include "NotificationTypes.h"

#include <QHash>
#include <QList>

#include <cstdint>
#include <optional>

struct NotificationPlacementDecision {
  NotifLifecycle lifecycle{NotifLifecycle::Queued};
  uint32_t bumped_id{0};
  bool queue_changed{false};
};

// Mutates per-monitor visible/queue id lists using the toast placement policy:
// show immediately while capacity remains, enqueue overflow FIFO, and let a new
// critical notification preempt the oldest visible non-critical notification.
[[nodiscard]] NotificationPlacementDecision placeNotification(QList<uint32_t>& visible, QList<uint32_t>& queue,
                                                              uint32_t notif_id, NotifUrgency urgency,
                                                              const QHash<uint32_t, NotifUrgency>& visible_urgencies,
                                                              int max_visible);

// Promotes the oldest queued notification into the visible set if capacity exists.
[[nodiscard]] std::optional<uint32_t> promoteQueuedNotification(QList<uint32_t>& visible, QList<uint32_t>& queue,
                                                                int max_visible);
