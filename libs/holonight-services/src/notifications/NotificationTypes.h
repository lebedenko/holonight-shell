#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

#include <cstdint>

// Plain data types for the notification daemon. Deliberately free of any QObject
// parent or Qt event-loop dependency so the model logic (accent routing, timeout
// policy, queue management) can be unit-tested with QCoreApplication only.

enum class NotifUrgency : uint8_t { Low = 0, Normal = 1, Critical = 2 };

// freedesktop close reason codes (notification-spec v1.2).
enum class NotifCloseReason : uint8_t { Expired = 1, Dismissed = 2, Closed = 3 };

enum class NotifAccentKind : uint8_t { Cyan, Violet, Critical };

enum class NotifLifecycle : uint8_t { Visible, Queued, Closed };

struct NotifAction {
  QString key;
  QString label;

  bool operator==(const NotifAction&) const = default;
};

struct NotificationData {
  uint32_t id{0};
  QString app_name;
  QString app_icon;  // raw hint value; resolved via IconImageProvider in QML
  QString summary;
  QString body;  // may contain markup; <img> stripped before QML binding
  QList<NotifAction> actions;
  bool has_default_action{false};  // "default" action is invoked by body click, never rendered as a button
  QVariantMap hints;               // full raw hints map kept for future use
  int expire_timeout_ms{-1};       // raw from Notify; policy applied by the service
  NotifUrgency urgency{NotifUrgency::Normal};
  bool is_resident{false};
  QString category;  // from hints["category"]
  NotifAccentKind accent{NotifAccentKind::Cyan};
  NotifLifecycle lifecycle{NotifLifecycle::Queued};
  QString monitor_name;  // assigned at arrival, immutable thereafter
  qint64 created_at_ms{0};

  // Runtime timer state, not part of the wire model.
  int effective_timeout_ms{-1};  // -1 = never expire
  int remaining_timeout_ms{-1};  // updated on hover pause/resume
};

struct NotificationHistoryItem {
  uint32_t id{0};
  QString app_name;
  QString summary;
  QString body;
  QString app_icon;
  QString app_class;  // from hints["desktop-entry"] at history-write time
  int urgency{1};
  qint64 timestamp_ms{0};
  int closed_reason{0};
  bool read{false};
  QList<NotifAction> actions;
};

// Encodes the three-way accent rule (REQ-F-035/036/037):
//   urgency == Critical                                  -> Critical
//   category begins with "im."/"call."/"presence."       -> Violet
//   otherwise                                            -> Cyan
[[nodiscard]] NotifAccentKind accentForData(const NotificationData& data);

// Applies the expire_timeout policy (REQ-F-014/015/016):
//   expire_timeout  > 0                       -> expire_timeout
//   expire_timeout == 0                       -> -1 (never)
//   expire_timeout == -1, urgency <  Critical -> default_timeout_ms
//   expire_timeout == -1, urgency == Critical -> -1 (never)
[[nodiscard]] int effectiveTimeoutMs(const NotificationData& data, int default_timeout_ms);
