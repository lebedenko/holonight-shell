#include "NotificationService.h"
#include "NotificationTypes.h"

#include <QSignalSpy>
#include <QString>

#include <cstdint>
#include <gtest/gtest.h>

// Tests for timeout-to-close-reason mapping, category/urgency-to-accent routing, and the
// hover-pause/resume timer behaviour (REQ-NF-041, REQ-F-035/036/037, REQ-F-017). These use real
// QTimers driven by the QGuiApplication event loop established in main.cpp; spy.wait() pumps the
// loop with generous margins so the timing assertions stay robust under load.

namespace {

NotificationData makeNotif(const QString& monitor, int timeout_ms, NotifUrgency urgency = NotifUrgency::Normal) {
  NotificationData data;
  data.monitor_name = monitor;
  data.urgency = urgency;
  data.expire_timeout_ms = timeout_ms;
  return data;
}

}  // namespace

// --- category / urgency -> accent (accentForData) ---

TEST(NotificationAccent, accentForData_critical) {
  NotificationData data;
  data.urgency = NotifUrgency::Critical;
  data.category = QStringLiteral("im.received");  // urgency wins over category
  EXPECT_EQ(accentForData(data), NotifAccentKind::Critical);
}

TEST(NotificationAccent, accentForData_categoryIm) {
  NotificationData data;
  data.category = QStringLiteral("im.received");
  EXPECT_EQ(accentForData(data), NotifAccentKind::Violet);
}

TEST(NotificationAccent, accentForData_categoryCall) {
  NotificationData data;
  data.category = QStringLiteral("call.incoming");
  EXPECT_EQ(accentForData(data), NotifAccentKind::Violet);
}

TEST(NotificationAccent, accentForData_categoryPresence) {
  NotificationData data;
  data.category = QStringLiteral("presence.online");
  EXPECT_EQ(accentForData(data), NotifAccentKind::Violet);
}

TEST(NotificationAccent, accentForData_default) {
  NotificationData plain;
  EXPECT_EQ(accentForData(plain), NotifAccentKind::Cyan);

  NotificationData unrouted;
  unrouted.category = QStringLiteral("mail.arrived");  // not a routed prefix
  EXPECT_EQ(accentForData(unrouted), NotifAccentKind::Cyan);
}

// --- close-reason mapping (NotifCloseReason 1/2/3) ---

TEST(NotificationReason, timeoutReason_expired) {
  NotificationService service(nullptr, nullptr);
  QSignalSpy closed(&service, &NotificationService::notificationClosed);

  const uint32_t notif_id = service.addOrReplace(makeNotif(QStringLiteral("DP-1"), 30));
  ASSERT_TRUE(closed.isEmpty());
  ASSERT_TRUE(closed.wait(2000));

  EXPECT_EQ(closed.count(), 1);
  EXPECT_EQ(closed.front().at(0).toUInt(), notif_id);
  EXPECT_EQ(closed.front().at(1).toUInt(), static_cast<uint>(NotifCloseReason::Expired));  // 1
}

TEST(NotificationReason, timeoutReason_dismissed) {
  NotificationService service(nullptr, nullptr);
  QSignalSpy closed(&service, &NotificationService::notificationClosed);

  const uint32_t notif_id = service.addOrReplace(makeNotif(QStringLiteral("DP-1"), 0));  // never auto-expires
  service.dismiss(notif_id);

  ASSERT_EQ(closed.count(), 1);
  EXPECT_EQ(closed.front().at(0).toUInt(), notif_id);
  EXPECT_EQ(closed.front().at(1).toUInt(), static_cast<uint>(NotifCloseReason::Dismissed));  // 2
}

TEST(NotificationReason, timeoutReason_closed) {
  NotificationService service(nullptr, nullptr);
  QSignalSpy closed(&service, &NotificationService::notificationClosed);

  const uint32_t notif_id = service.addOrReplace(makeNotif(QStringLiteral("DP-1"), 0));
  service.requestClose(notif_id);

  ASSERT_EQ(closed.count(), 1);
  EXPECT_EQ(closed.front().at(0).toUInt(), notif_id);
  EXPECT_EQ(closed.front().at(1).toUInt(), static_cast<uint>(NotifCloseReason::Closed));  // 3
}

// --- hover pauses and resumes the expiry timer (REQ-F-017) ---

TEST(NotificationHover, hoverPausesAndResumesTimer) {
  NotificationService service(nullptr, nullptr);
  QSignalSpy closed(&service, &NotificationService::notificationClosed);

  const uint32_t notif_id = service.addOrReplace(makeNotif(QStringLiteral("DP-1"), 200));

  // Pointer enters before expiry: timer is paused and must not fire while held.
  service.hoverEntered(notif_id);
  EXPECT_FALSE(closed.wait(400));  // waited well past the original 200ms — still alive
  EXPECT_TRUE(closed.isEmpty());

  // Pointer leaves: timer resumes with the remaining time and the notification expires.
  service.hoverLeft(notif_id);
  ASSERT_TRUE(closed.wait(2000));
  EXPECT_EQ(closed.count(), 1);
  EXPECT_EQ(closed.front().at(0).toUInt(), notif_id);
  EXPECT_EQ(closed.front().at(1).toUInt(), static_cast<uint>(NotifCloseReason::Expired));  // 1
}
