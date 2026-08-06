#include "NotificationServer.h"
#include "NotificationService.h"

#include <QString>
#include <QVariantMap>

#include <gtest/gtest.h>

namespace {

QString bigString(qsizetype approx_bytes, QChar fill = QLatin1Char('a')) { return {approx_bytes, fill}; }

}  // namespace

TEST(NotificationPayloadBounds, TruncateToMaxLengthLeavesShortStringUnchanged) {
  const QString value = QStringLiteral("short summary");
  EXPECT_EQ(truncateToMaxLength(value), value);
}

TEST(NotificationPayloadBounds, TruncateToMaxLengthBoundsAndMarksOversizedString) {
  const QString oversized = bigString(10 * 1024 * 1024);  // 10 MB
  const QString truncated = truncateToMaxLength(oversized);

  EXPECT_LE(truncated.size(), kMaxNotificationFieldLength);
  EXPECT_TRUE(truncated.endsWith(QLatin1String(kTruncationMarker)));
}

TEST(NotificationPayloadBounds, TruncateHintValuesBoundsOversizedStringHint) {
  QVariantMap hints;
  hints.insert(QStringLiteral("category"), bigString(10 * 1024 * 1024));

  const QVariantMap truncated = truncateHintValues(hints);

  const QString category = truncated.value(QStringLiteral("category")).toString();
  EXPECT_LE(category.size(), kMaxNotificationFieldLength);
  EXPECT_TRUE(category.endsWith(QLatin1String(kTruncationMarker)));
}

TEST(NotificationPayloadBounds, TruncateHintValuesLeavesUndersizedAndNonStringHintsUnmodified) {
  QVariantMap hints;
  hints.insert(QStringLiteral("category"), QStringLiteral("email.arrived"));
  hints.insert(QStringLiteral("urgency"), static_cast<uchar>(2));
  hints.insert(QStringLiteral("resident"), true);

  const QVariantMap truncated = truncateHintValues(hints);

  EXPECT_EQ(truncated.value(QStringLiteral("category")).toString(), QStringLiteral("email.arrived"));
  EXPECT_EQ(truncated.value(QStringLiteral("urgency")).toUInt(), 2U);
  EXPECT_EQ(truncated.value(QStringLiteral("resident")).toBool(), true);
}

TEST(NotificationPayloadBounds, NotifyBoundsOversizedSummaryEndToEnd) {
  NotificationService service(nullptr, nullptr);
  NotificationServer server(&service, nullptr);

  const QString oversized_summary = bigString(10 * 1024 * 1024);
  const uint notif_id =
      server.Notify(QStringLiteral("test-app"), 0, QString{}, oversized_summary, QString{}, {}, {}, 0);

  const QModelIndex idx = service.index(0);
  ASSERT_EQ(idx.data(NotificationService::NotifIdRole).toUInt(), notif_id);
  const QString stored_summary = idx.data(NotificationService::SummaryRole).toString();
  EXPECT_LE(stored_summary.size(), kMaxNotificationFieldLength);
  EXPECT_TRUE(stored_summary.endsWith(QLatin1String(kTruncationMarker)));
}

TEST(NotificationPayloadBounds, NotifyBoundsOversizedBodyEndToEnd) {
  NotificationService service(nullptr, nullptr);
  NotificationServer server(&service, nullptr);

  const QString oversized_body = bigString(10 * 1024 * 1024);
  const uint notif_id =
      server.Notify(QStringLiteral("test-app"), 0, QString{}, QStringLiteral("summary"), oversized_body, {}, {}, 0);

  const QModelIndex idx = service.index(0);
  ASSERT_EQ(idx.data(NotificationService::NotifIdRole).toUInt(), notif_id);
  const QString stored_body = idx.data(NotificationService::BodyRole).toString();
  EXPECT_LE(stored_body.size(), kMaxNotificationFieldLength);
  EXPECT_TRUE(stored_body.endsWith(QLatin1String(kTruncationMarker)));
}

TEST(NotificationPayloadBounds, NotifyBoundsOversizedHintDerivedAppIconEndToEnd) {
  NotificationService service(nullptr, nullptr);
  NotificationServer server(&service, nullptr);

  QVariantMap hints;
  hints.insert(QStringLiteral("image-path"), bigString(10 * 1024 * 1024));
  const uint notif_id =
      server.Notify(QStringLiteral("test-app"), 0, QString{}, QStringLiteral("summary"), QString{}, {}, hints, 0);

  const QModelIndex idx = service.index(0);
  ASSERT_EQ(idx.data(NotificationService::NotifIdRole).toUInt(), notif_id);
  const QString stored_icon = idx.data(NotificationService::AppIconRole).toString();
  EXPECT_LE(stored_icon.size(), kMaxNotificationFieldLength);
  EXPECT_TRUE(stored_icon.endsWith(QLatin1String(kTruncationMarker)));
}
