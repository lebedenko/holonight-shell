#include "NotificationPolicy.h"

#include <gtest/gtest.h>

TEST(NotificationPolicy, ShowsImmediatelyWhenVisibleCapacityRemains) {
  QList<uint32_t> visible{1, 2};
  QList<uint32_t> queue;
  const QHash<uint32_t, NotifUrgency> urgencies{{1, NotifUrgency::Normal}, {2, NotifUrgency::Low}};

  const NotificationPlacementDecision decision =
      placeNotification(visible, queue, 3, NotifUrgency::Normal, urgencies, 3);

  EXPECT_EQ(decision.lifecycle, NotifLifecycle::Visible);
  EXPECT_EQ(decision.bumped_id, 0U);
  EXPECT_FALSE(decision.queue_changed);
  EXPECT_EQ(visible, (QList<uint32_t>{1, 2, 3}));
  EXPECT_TRUE(queue.isEmpty());
}

TEST(NotificationPolicy, EnqueuesNormalOverflowAtRear) {
  QList<uint32_t> visible{1, 2, 3};
  QList<uint32_t> queue{4};
  const QHash<uint32_t, NotifUrgency> urgencies{
      {1, NotifUrgency::Normal}, {2, NotifUrgency::Normal}, {3, NotifUrgency::Normal}};

  const NotificationPlacementDecision decision =
      placeNotification(visible, queue, 5, NotifUrgency::Normal, urgencies, 3);

  EXPECT_EQ(decision.lifecycle, NotifLifecycle::Queued);
  EXPECT_EQ(decision.bumped_id, 0U);
  EXPECT_TRUE(decision.queue_changed);
  EXPECT_EQ(visible, (QList<uint32_t>{1, 2, 3}));
  EXPECT_EQ(queue, (QList<uint32_t>{4, 5}));
}

TEST(NotificationPolicy, CriticalPreemptsOldestVisibleNonCriticalToQueueFront) {
  QList<uint32_t> visible{1, 2, 3};
  QList<uint32_t> queue{4};
  const QHash<uint32_t, NotifUrgency> urgencies{
      {1, NotifUrgency::Critical}, {2, NotifUrgency::Normal}, {3, NotifUrgency::Low}};

  const NotificationPlacementDecision decision =
      placeNotification(visible, queue, 5, NotifUrgency::Critical, urgencies, 3);

  EXPECT_EQ(decision.lifecycle, NotifLifecycle::Visible);
  EXPECT_EQ(decision.bumped_id, 2U);
  EXPECT_TRUE(decision.queue_changed);
  EXPECT_EQ(visible, (QList<uint32_t>{1, 3, 5}));
  EXPECT_EQ(queue, (QList<uint32_t>{2, 4}));
}

TEST(NotificationPolicy, CriticalOverflowEnqueuesWhenEveryVisibleNotificationIsCritical) {
  QList<uint32_t> visible{1, 2, 3};
  QList<uint32_t> queue;
  const QHash<uint32_t, NotifUrgency> urgencies{
      {1, NotifUrgency::Critical}, {2, NotifUrgency::Critical}, {3, NotifUrgency::Critical}};

  const NotificationPlacementDecision decision =
      placeNotification(visible, queue, 4, NotifUrgency::Critical, urgencies, 3);

  EXPECT_EQ(decision.lifecycle, NotifLifecycle::Queued);
  EXPECT_EQ(decision.bumped_id, 0U);
  EXPECT_TRUE(decision.queue_changed);
  EXPECT_EQ(visible, (QList<uint32_t>{1, 2, 3}));
  EXPECT_EQ(queue, (QList<uint32_t>{4}));
}

TEST(NotificationPolicy, PromotionMovesQueueFrontIntoVisibleSlot) {
  QList<uint32_t> visible{1, 2};
  QList<uint32_t> queue{3, 4};

  const std::optional<uint32_t> promoted = promoteQueuedNotification(visible, queue, 3);

  ASSERT_TRUE(promoted.has_value());
  EXPECT_EQ(*promoted, 3U);
  EXPECT_EQ(visible, (QList<uint32_t>{1, 2, 3}));
  EXPECT_EQ(queue, (QList<uint32_t>{4}));
}

TEST(NotificationPolicy, PromotionNoOpsWhenQueueEmptyOrVisibleFull) {
  QList<uint32_t> visible_full{1, 2, 3};
  QList<uint32_t> queued{4};
  EXPECT_FALSE(promoteQueuedNotification(visible_full, queued, 3).has_value());
  EXPECT_EQ(visible_full, (QList<uint32_t>{1, 2, 3}));
  EXPECT_EQ(queued, (QList<uint32_t>{4}));

  QList<uint32_t> visible_with_capacity{1};
  QList<uint32_t> empty_queue;
  EXPECT_FALSE(promoteQueuedNotification(visible_with_capacity, empty_queue, 3).has_value());
  EXPECT_EQ(visible_with_capacity, (QList<uint32_t>{1}));
  EXPECT_TRUE(empty_queue.isEmpty());
}
