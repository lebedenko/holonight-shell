#include "NotificationServer.h"
#include "NotificationService.h"
#include "NotificationTypes.h"

#include <QAbstractItemModel>
#include <QList>
#include <QSignalSpy>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <gtest/gtest.h>

// Unit tests for NotificationService model logic (REQ-NF-041). The service is constructed with
// a null ConfigService (defaults: default_timeout_ms=5000, max_visible=3) and a null
// ActiveWindowService; every NotificationData carries an explicit monitor_name so placement does
// not depend on the live focused-monitor lookup. expire_timeout_ms is set to 0 (never expire) so
// no QTimer fires during these synchronous tests — timeout behaviour is covered separately in
// test_notification_timeout.cpp.

namespace {

NotificationData makeNotif(const QString& monitor, NotifUrgency urgency = NotifUrgency::Normal,
                           const QString& summary = QStringLiteral("summary")) {
  NotificationData data;
  data.monitor_name = monitor;
  data.urgency = urgency;
  data.summary = summary;
  data.expire_timeout_ms = 0;  // 0 → never expire → no timer armed
  return data;
}

QList<uint32_t> visibleIds(NotificationService& service, const QString& monitor) {
  QAbstractItemModel* model = service.visibleModelForMonitor(monitor);
  QList<uint32_t> ids;
  for (int row = 0; row < model->rowCount(); ++row) {
    ids.append(model->index(row, 0).data(NotificationService::NotifIdRole).toUInt());
  }
  return ids;
}

QString summaryForId(NotificationService& service, uint32_t notif_id) {
  for (int row = 0; row < service.rowCount(); ++row) {
    const QModelIndex idx = service.index(row);
    if (idx.data(NotificationService::NotifIdRole).toUInt() == notif_id) {
      return idx.data(NotificationService::SummaryRole).toString();
    }
  }
  return {};
}

QVariantList actionsForId(NotificationService& service, uint32_t notif_id) {
  for (int row = 0; row < service.rowCount(); ++row) {
    const QModelIndex idx = service.index(row);
    if (idx.data(NotificationService::NotifIdRole).toUInt() == notif_id) {
      return idx.data(NotificationService::ActionsRole).toList();
    }
  }
  return {};
}

bool hasDefaultActionForId(NotificationService& service, uint32_t notif_id) {
  for (int row = 0; row < service.rowCount(); ++row) {
    const QModelIndex idx = service.index(row);
    if (idx.data(NotificationService::NotifIdRole).toUInt() == notif_id) {
      return idx.data(NotificationService::HasDefaultActionRole).toBool();
    }
  }
  return false;
}

}  // namespace

TEST(NotificationService, idAllocation_monotonic) {
  NotificationService service(nullptr, nullptr);
  const uint32_t first = service.addOrReplace(makeNotif(QStringLiteral("DP-1")));
  const uint32_t second = service.addOrReplace(makeNotif(QStringLiteral("DP-1")));
  const uint32_t third = service.addOrReplace(makeNotif(QStringLiteral("DP-1")));

  EXPECT_LT(first, second);
  EXPECT_LT(second, third);
}

TEST(NotificationService, idAllocation_neverZero) {
  NotificationService service(nullptr, nullptr);
  for (int iteration = 0; iteration < 5; ++iteration) {
    const uint32_t notif_id = service.addOrReplace(makeNotif(QStringLiteral("DP-1")));
    EXPECT_GT(notif_id, 0U);
  }
}

TEST(NotificationService, replaceSemantics_existing) {
  NotificationService service(nullptr, nullptr);
  const uint32_t original =
      service.addOrReplace(makeNotif(QStringLiteral("DP-1"), NotifUrgency::Normal, QStringLiteral("before")));

  NotificationData replacement = makeNotif(QStringLiteral("DP-1"), NotifUrgency::Normal, QStringLiteral("after"));
  replacement.id = original;  // replaces_id
  const uint32_t returned = service.addOrReplace(replacement);

  EXPECT_EQ(returned, original);
  EXPECT_EQ(summaryForId(service, original), QStringLiteral("after"));
  EXPECT_EQ(service.rowCount(), 1);  // updated in place, not duplicated
}

TEST(NotificationService, replaceSemantics_unknown) {
  NotificationService service(nullptr, nullptr);
  NotificationData orphan = makeNotif(QStringLiteral("DP-1"));
  orphan.id = 999;  // no live notification with this id
  const uint32_t returned = service.addOrReplace(orphan);

  EXPECT_NE(returned, 999U);
  EXPECT_GT(returned, 0U);
  EXPECT_EQ(service.rowCount(), 1);
}

TEST(NotificationService, queueFifo_overflow) {
  NotificationService service(nullptr, nullptr);  // max_visible = 3
  const QString monitor = QStringLiteral("DP-1");
  QList<uint32_t> sent;
  for (int index = 0; index < 5; ++index) {
    sent.append(service.addOrReplace(makeNotif(monitor)));
  }

  // 3 visible, 2 queued.
  EXPECT_EQ(visibleIds(service, monitor).size(), 3);
  EXPECT_EQ(service.rowCount(), 5);
  EXPECT_TRUE(service.hasNotificationsForMonitor(monitor));

  // Closing the oldest visible promotes the front of the FIFO queue (the 4th sent).
  service.requestClose(sent.at(0));
  const QList<uint32_t> visible = visibleIds(service, monitor);
  EXPECT_EQ(visible.size(), 3);
  EXPECT_TRUE(visible.contains(sent.at(3)));   // promoted from queue front
  EXPECT_FALSE(visible.contains(sent.at(0)));  // closed
}

TEST(NotificationService, criticalPriorityJump) {
  NotificationService service(nullptr, nullptr);  // max_visible = 3
  const QString monitor = QStringLiteral("DP-1");
  QList<uint32_t> normal;
  for (int index = 0; index < 3; ++index) {
    normal.append(service.addOrReplace(makeNotif(monitor, NotifUrgency::Normal)));
  }

  const uint32_t critical = service.addOrReplace(makeNotif(monitor, NotifUrgency::Critical));

  const QList<uint32_t> visible = visibleIds(service, monitor);
  EXPECT_EQ(visible.size(), 3);
  EXPECT_TRUE(visible.contains(critical));       // critical shown immediately
  EXPECT_FALSE(visible.contains(normal.at(0)));  // oldest non-critical bumped to queue
  EXPECT_TRUE(visible.contains(normal.at(1)));
  EXPECT_TRUE(visible.contains(normal.at(2)));
}

TEST(NotificationService, perMonitorQueue_independent) {
  NotificationService service(nullptr, nullptr);  // max_visible = 3
  const QString primary = QStringLiteral("DP-1");
  const QString secondary = QStringLiteral("HDMI-1");

  // Saturate DP-1: 3 visible + 1 queued.
  for (int index = 0; index < 4; ++index) {
    service.addOrReplace(makeNotif(primary));
  }
  // A single notification on HDMI-1 must be visible despite DP-1's backlog.
  const uint32_t lone = service.addOrReplace(makeNotif(secondary));

  EXPECT_EQ(visibleIds(service, primary).size(), 3);
  const QList<uint32_t> secondaryVisible = visibleIds(service, secondary);
  EXPECT_EQ(secondaryVisible.size(), 1);
  EXPECT_TRUE(secondaryVisible.contains(lone));
}

TEST(NotificationService, timeoutEffectiveMs_policies) {
  NotificationData explicitTimeout;
  explicitTimeout.expire_timeout_ms = 2000;
  EXPECT_EQ(effectiveTimeoutMs(explicitTimeout, 5000), 2000);

  NotificationData neverTimeout;
  neverTimeout.expire_timeout_ms = 0;
  EXPECT_EQ(effectiveTimeoutMs(neverTimeout, 5000), -1);

  NotificationData defaultNormal;
  defaultNormal.expire_timeout_ms = -1;
  defaultNormal.urgency = NotifUrgency::Normal;
  EXPECT_EQ(effectiveTimeoutMs(defaultNormal, 5000), 5000);

  NotificationData defaultCritical;
  defaultCritical.expire_timeout_ms = -1;
  defaultCritical.urgency = NotifUrgency::Critical;
  EXPECT_EQ(effectiveTimeoutMs(defaultCritical, 5000), -1);
}

TEST(NotificationService, roleNamesExposeDefaultActionContract) {
  NotificationService service(nullptr, nullptr);

  EXPECT_EQ(service.roleNames().value(NotificationService::HasDefaultActionRole), "hasDefaultAction");
  EXPECT_EQ(service.roleNames().value(NotificationService::CreatedAtMsRole), "createdAtMs");
}

TEST(NotificationServer, notifyStoresDefaultActionForBodyClickOnly) {
  NotificationService service(nullptr, nullptr);
  NotificationServer server(&service);

  const uint32_t notif_id =
      server.Notify(QStringLiteral("app"), 0, QString(), QStringLiteral("summary"), QStringLiteral("body"),
                    QStringList{
                        QStringLiteral("default"),
                        QString(),
                        QStringLiteral("reply"),
                        QString(),
                        QStringLiteral("ignore"),
                        QStringLiteral("Ignore"),
                    },
                    QVariantMap{}, 0);

  ASSERT_GT(notif_id, 0U);
  EXPECT_TRUE(hasDefaultActionForId(service, notif_id));

  const QVariantList actions = actionsForId(service, notif_id);
  ASSERT_EQ(actions.size(), 1);
  const QVariantMap action = actions.front().toMap();
  EXPECT_EQ(action.value(QStringLiteral("key")).toString(), QStringLiteral("ignore"));
  EXPECT_EQ(action.value(QStringLiteral("label")).toString(), QStringLiteral("Ignore"));
}

TEST(NotificationService, defaultActionInvocationEmitsAndClosesNonResident) {
  NotificationService service(nullptr, nullptr);
  QSignalSpy invoked(&service, &NotificationService::actionInvoked);
  QSignalSpy closed(&service, &NotificationService::notificationClosed);

  NotificationData data = makeNotif(QStringLiteral("DP-1"));
  data.has_default_action = true;
  const uint32_t notif_id = service.addOrReplace(data);

  service.invokeAction(notif_id, QStringLiteral("default"));

  ASSERT_EQ(invoked.count(), 1);
  EXPECT_EQ(invoked.front().at(0).toUInt(), notif_id);
  EXPECT_EQ(invoked.front().at(1).toString(), QStringLiteral("default"));
  ASSERT_EQ(closed.count(), 1);
  EXPECT_EQ(closed.front().at(0).toUInt(), notif_id);
  EXPECT_EQ(closed.front().at(1).toUInt(), static_cast<uint>(NotifCloseReason::Closed));
  EXPECT_EQ(service.rowCount(), 0);
}
