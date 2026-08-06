#include "TrayItemProperties.h"

#include <QColor>
#include <QDBusMetaType>

#include <gtest/gtest.h>

namespace {

QByteArray argbBytes(QRgb pixel, int count) {
  QByteArray data;
  data.reserve(count * 4);
  for (int i = 0; i < count; ++i) {
    data.append(static_cast<char>(qAlpha(pixel)));
    data.append(static_cast<char>(qRed(pixel)));
    data.append(static_cast<char>(qGreen(pixel)));
    data.append(static_cast<char>(qBlue(pixel)));
  }
  return data;
}

SniIconPixel solidPixmap(int width, int height, QRgb color) {
  return SniIconPixel{.width = width, .height = height, .data = argbBytes(color, width * height)};
}

QVariant pixmapVariant(const SniIconPixmapList& pixmaps) {
  registerTrayMetaTypes();
  return QVariant::fromValue(pixmaps);
}

QVariant tooltipVariant(const QString& title) {
  registerTrayMetaTypes();
  SniToolTip tip;
  tip.title = title;
  return QVariant::fromValue(tip);
}

}  // namespace

TEST(TrayItemPropertiesTest, FullInsertDefaultsStatusToActive) {
  QVariantMap changed;
  changed.insert(QStringLiteral("IconName"), QStringLiteral("network-wireless"));
  changed.insert(QStringLiteral("Title"), QStringLiteral("Network"));

  TrayItem item =
      mergeTrayItemProperties(nullptr, QStringLiteral(":1.42"), QStringLiteral("/StatusNotifierItem"), changed);

  EXPECT_EQ(item.service, QStringLiteral(":1.42"));
  EXPECT_EQ(item.object_path, QStringLiteral("/StatusNotifierItem"));
  EXPECT_TRUE(item.id.isEmpty());
  EXPECT_EQ(item.icon_name, QStringLiteral("network-wireless"));
  EXPECT_EQ(item.title, QStringLiteral("Network"));
  EXPECT_EQ(item.status, QStringLiteral("Active"));
}

TEST(TrayItemPropertiesTest, StatusOnlyUpdatePreservesExistingFields) {
  TrayItem existing;
  existing.service = QStringLiteral(":1.42");
  existing.object_path = QStringLiteral("/StatusNotifierItem");
  existing.icon_name = QStringLiteral("audio-volume-high");
  existing.attention_icon_name = QStringLiteral("audio-volume-muted");
  existing.status = QStringLiteral("Active");
  existing.title = QStringLiteral("Volume");
  existing.icon_pixmap_cache = QImage(2, 2, QImage::Format_ARGB32);
  existing.attention_pixmap_cache = QImage(3, 3, QImage::Format_ARGB32);

  QVariantMap changed;
  changed.insert(QStringLiteral("Status"), QStringLiteral("NeedsAttention"));

  TrayItem item =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.42"), QStringLiteral("/StatusNotifierItem"), changed);

  EXPECT_EQ(item.icon_name, existing.icon_name);
  EXPECT_EQ(item.attention_icon_name, existing.attention_icon_name);
  EXPECT_EQ(item.title, existing.title);
  EXPECT_EQ(item.status, QStringLiteral("NeedsAttention"));
  EXPECT_EQ(item.icon_pixmap_cache.size(), QSize(2, 2));
  EXPECT_EQ(item.attention_pixmap_cache.size(), QSize(3, 3));
}

TEST(TrayItemPropertiesTest, IconOnlyUpdatePreservesTitleAndStatus) {
  TrayItem existing;
  existing.status = QStringLiteral("Passive");
  existing.title = QStringLiteral("Updater");

  QVariantMap changed;
  changed.insert(QStringLiteral("IconName"), QStringLiteral("software-update-available"));

  TrayItem item =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.9"), QStringLiteral("/StatusNotifierItem"), changed);

  EXPECT_EQ(item.icon_name, QStringLiteral("software-update-available"));
  EXPECT_EQ(item.status, QStringLiteral("Passive"));
  EXPECT_EQ(item.title, QStringLiteral("Updater"));
}

TEST(TrayItemPropertiesTest, EmptyStatusUpdateReturnsActive) {
  TrayItem existing;
  existing.status = QStringLiteral("NeedsAttention");

  QVariantMap changed;
  changed.insert(QStringLiteral("Status"), QString{});

  TrayItem item =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.10"), QStringLiteral("/StatusNotifierItem"), changed);

  EXPECT_EQ(item.status, QStringLiteral("Active"));
}

TEST(TrayItemPropertiesTest, IconPixmapUpdateReplacesExistingPixmapCache) {
  TrayItem existing;
  existing.icon_pixmap_cache = QImage(4, 4, QImage::Format_ARGB32);
  existing.icon_pixmap_cache.fill(QColor(1, 2, 3, 255));

  QVariantMap changed;
  changed.insert(QStringLiteral("IconPixmap"), pixmapVariant({solidPixmap(2, 2, qRgba(20, 30, 40, 255))}));

  TrayItem item =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.11"), QStringLiteral("/StatusNotifierItem"), changed);

  ASSERT_FALSE(item.icon_pixmap_cache.isNull());
  EXPECT_EQ(item.icon_pixmap_cache.size(), QSize(22, 22));
  EXPECT_EQ(item.icon_pixmap_cache.pixelColor(0, 0), QColor(20, 30, 40, 255));
  EXPECT_EQ(item.clean_icon_pixmap_cache.pixelColor(0, 0), QColor(20, 30, 40, 255));
}

TEST(TrayItemPropertiesTest, TeamsUnreadCounterRestoresCleanIconAndSetsUnreadFlag) {
  TrayItem existing;
  existing.tooltip.title = QStringLiteral("Microsoft Teams");
  existing.icon_pixmap_cache = QImage(2, 2, QImage::Format_ARGB32);
  existing.icon_pixmap_cache.fill(QColor(1, 2, 3, 255));
  existing.clean_icon_pixmap_cache = existing.icon_pixmap_cache;

  QVariantMap changed;
  changed.insert(QStringLiteral("IconPixmap"), pixmapVariant({solidPixmap(2, 2, qRgba(200, 10, 10, 255))}));

  TrayItem badged =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.101"), QStringLiteral("/StatusNotifierItem"), changed);
  EXPECT_FALSE(badged.has_unread);
  EXPECT_EQ(badged.icon_pixmap_cache.pixelColor(0, 0), QColor(200, 10, 10, 255));
  EXPECT_EQ(badged.previous_clean_icon_pixmap_cache.pixelColor(0, 0), QColor(1, 2, 3, 255));

  changed.clear();
  changed.insert(QStringLiteral("ToolTip"), tooltipVariant(QStringLiteral("Microsoft Teams (1)")));

  TrayItem unread =
      mergeTrayItemProperties(&badged, QStringLiteral(":1.101"), QStringLiteral("/StatusNotifierItem"), changed);
  EXPECT_TRUE(unread.has_unread);
  EXPECT_EQ(unread.icon_pixmap_cache.pixelColor(0, 0), QColor(1, 2, 3, 255));
  EXPECT_EQ(unread.clean_icon_pixmap_cache.pixelColor(0, 0), QColor(1, 2, 3, 255));
}

TEST(TrayItemPropertiesTest, TeamsTitleWithoutCounterClearsUnreadFlag) {
  TrayItem existing;
  existing.has_unread = true;
  existing.tooltip.title = QStringLiteral("Microsoft Teams (2)");

  QVariantMap changed;
  changed.insert(QStringLiteral("ToolTip"), tooltipVariant(QStringLiteral("Microsoft Teams")));

  TrayItem item =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.101"), QStringLiteral("/StatusNotifierItem"), changed);

  EXPECT_FALSE(item.has_unread);
}

TEST(TrayItemPropertiesTest, SlackNotificationTooltipRestoresCleanIconAndSetsUnreadFlag) {
  TrayItem existing;
  existing.id = QStringLiteral("Slack_status_icon_1");
  existing.icon_pixmap_cache = QImage(2, 2, QImage::Format_ARGB32);
  existing.icon_pixmap_cache.fill(QColor(1, 2, 3, 255));
  existing.clean_icon_pixmap_cache = existing.icon_pixmap_cache;

  QVariantMap changed;
  changed.insert(QStringLiteral("IconPixmap"), pixmapVariant({solidPixmap(2, 2, qRgba(200, 10, 10, 255))}));

  TrayItem badged =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.168"), QStringLiteral("/StatusNotifierItem"), changed);
  EXPECT_FALSE(badged.has_unread);
  EXPECT_EQ(badged.icon_pixmap_cache.pixelColor(0, 0), QColor(200, 10, 10, 255));
  EXPECT_EQ(badged.previous_clean_icon_pixmap_cache.pixelColor(0, 0), QColor(1, 2, 3, 255));

  changed.clear();
  changed.insert(QStringLiteral("ToolTip"), tooltipVariant(QStringLiteral("You have 1 notification")));

  TrayItem unread =
      mergeTrayItemProperties(&badged, QStringLiteral(":1.168"), QStringLiteral("/StatusNotifierItem"), changed);
  EXPECT_TRUE(unread.has_unread);
  EXPECT_EQ(unread.icon_pixmap_cache.pixelColor(0, 0), QColor(1, 2, 3, 255));
  EXPECT_EQ(unread.clean_icon_pixmap_cache.pixelColor(0, 0), QColor(1, 2, 3, 255));
}

TEST(TrayItemPropertiesTest, SlackTooltipWithoutNotificationClearsUnreadFlag) {
  TrayItem existing;
  existing.id = QStringLiteral("Slack_status_icon_1");
  existing.has_unread = true;
  existing.tooltip.title = QStringLiteral("You have 1 notification");

  QVariantMap changed;
  changed.insert(QStringLiteral("ToolTip"), tooltipVariant(QStringLiteral("Slack")));

  TrayItem item =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.168"), QStringLiteral("/StatusNotifierItem"), changed);

  EXPECT_FALSE(item.has_unread);
}

TEST(TrayItemPropertiesTest, GenericNotificationTooltipDoesNotSetUnreadForUnknownApp) {
  TrayItem existing;
  existing.id = QStringLiteral("other-app");

  QVariantMap changed;
  changed.insert(QStringLiteral("ToolTip"), tooltipVariant(QStringLiteral("You have 1 notification")));

  TrayItem item =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.169"), QStringLiteral("/StatusNotifierItem"), changed);

  EXPECT_FALSE(item.has_unread);
}

TEST(TrayItemPropertiesTest, AttentionIconPixmapUpdateReplacesExistingAttentionPixmapCache) {
  TrayItem existing;
  existing.attention_pixmap_cache = QImage(4, 4, QImage::Format_ARGB32);
  existing.attention_pixmap_cache.fill(QColor(1, 2, 3, 255));

  QVariantMap changed;
  changed.insert(QStringLiteral("AttentionIconPixmap"), pixmapVariant({solidPixmap(2, 2, qRgba(80, 90, 100, 255))}));

  TrayItem item =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.12"), QStringLiteral("/StatusNotifierItem"), changed);

  ASSERT_FALSE(item.attention_pixmap_cache.isNull());
  EXPECT_EQ(item.attention_pixmap_cache.size(), QSize(22, 22));
  EXPECT_EQ(item.attention_pixmap_cache.pixelColor(0, 0), QColor(80, 90, 100, 255));
}

TEST(TrayItemPropertiesTest, EmptyPixmapListsClearCorrespondingPixmapCaches) {
  TrayItem existing;
  existing.icon_pixmap_cache = QImage(2, 2, QImage::Format_ARGB32);
  existing.attention_pixmap_cache = QImage(3, 3, QImage::Format_ARGB32);

  QVariantMap changed;
  changed.insert(QStringLiteral("IconPixmap"), pixmapVariant({}));
  changed.insert(QStringLiteral("AttentionIconPixmap"), pixmapVariant({}));

  TrayItem item =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.13"), QStringLiteral("/StatusNotifierItem"), changed);

  EXPECT_TRUE(item.icon_pixmap_cache.isNull());
  EXPECT_TRUE(item.attention_pixmap_cache.isNull());
}

TEST(TrayItemPropertiesTest, InvalidPixmapVariantsPreserveExistingPixmapCaches) {
  TrayItem existing;
  existing.icon_pixmap_cache = QImage(2, 2, QImage::Format_ARGB32);
  existing.icon_pixmap_cache.fill(QColor(10, 20, 30, 255));
  existing.attention_pixmap_cache = QImage(3, 3, QImage::Format_ARGB32);
  existing.attention_pixmap_cache.fill(QColor(40, 50, 60, 255));

  QVariantMap changed;
  changed.insert(QStringLiteral("IconPixmap"), QStringLiteral("not-a-dbus-argument"));
  changed.insert(QStringLiteral("AttentionIconPixmap"), 42);

  TrayItem item =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.14"), QStringLiteral("/StatusNotifierItem"), changed);

  EXPECT_EQ(item.icon_pixmap_cache.size(), QSize(2, 2));
  EXPECT_EQ(item.icon_pixmap_cache.pixelColor(0, 0), QColor(10, 20, 30, 255));
  EXPECT_EQ(item.attention_pixmap_cache.size(), QSize(3, 3));
  EXPECT_EQ(item.attention_pixmap_cache.pixelColor(0, 0), QColor(40, 50, 60, 255));
}

TEST(TrayItemPropertiesTest, UnrelatedUpdatesPreservePixmapCachesAndIconNames) {
  TrayItem existing;
  existing.icon_name = QStringLiteral("normal-icon");
  existing.attention_icon_name = QStringLiteral("attention-icon");
  existing.icon_pixmap_cache = QImage(2, 2, QImage::Format_ARGB32);
  existing.icon_pixmap_cache.fill(QColor(10, 20, 30, 255));
  existing.attention_pixmap_cache = QImage(3, 3, QImage::Format_ARGB32);
  existing.attention_pixmap_cache.fill(QColor(40, 50, 60, 255));

  QVariantMap changed;
  changed.insert(QStringLiteral("Title"), QStringLiteral("Updated"));

  TrayItem item =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.15"), QStringLiteral("/StatusNotifierItem"), changed);

  EXPECT_EQ(item.title, QStringLiteral("Updated"));
  EXPECT_EQ(item.icon_name, QStringLiteral("normal-icon"));
  EXPECT_EQ(item.attention_icon_name, QStringLiteral("attention-icon"));
  EXPECT_EQ(item.icon_pixmap_cache.size(), QSize(2, 2));
  EXPECT_EQ(item.attention_pixmap_cache.size(), QSize(3, 3));
}

TEST(TrayItemPropertiesTest, IdUpdatePreservesAcrossPartialUpdates) {
  QVariantMap changed;
  changed.insert(QStringLiteral("Id"), QStringLiteral("Slack_status_icon_1"));

  TrayItem item =
      mergeTrayItemProperties(nullptr, QStringLiteral(":1.168"), QStringLiteral("/StatusNotifierItem"), changed);
  EXPECT_EQ(item.id, QStringLiteral("Slack_status_icon_1"));

  changed.clear();
  changed.insert(QStringLiteral("Status"), QStringLiteral("NeedsAttention"));

  TrayItem updated =
      mergeTrayItemProperties(&item, QStringLiteral(":1.168"), QStringLiteral("/StatusNotifierItem"), changed);
  EXPECT_EQ(updated.id, QStringLiteral("Slack_status_icon_1"));
  EXPECT_EQ(updated.status, QStringLiteral("NeedsAttention"));
}

TEST(TrayItemPropertiesTest, IconNameAndAttentionIconNameUpdateIndependently) {
  TrayItem existing;
  existing.icon_name = QStringLiteral("old-normal");
  existing.attention_icon_name = QStringLiteral("old-attention");

  QVariantMap changed;
  changed.insert(QStringLiteral("IconName"), QStringLiteral("new-normal"));

  TrayItem normal_update =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.16"), QStringLiteral("/StatusNotifierItem"), changed);

  EXPECT_EQ(normal_update.icon_name, QStringLiteral("new-normal"));
  EXPECT_EQ(normal_update.attention_icon_name, QStringLiteral("old-attention"));

  changed.clear();
  changed.insert(QStringLiteral("AttentionIconName"), QStringLiteral("new-attention"));

  TrayItem attention_update =
      mergeTrayItemProperties(&existing, QStringLiteral(":1.16"), QStringLiteral("/StatusNotifierItem"), changed);

  EXPECT_EQ(attention_update.icon_name, QStringLiteral("old-normal"));
  EXPECT_EQ(attention_update.attention_icon_name, QStringLiteral("new-attention"));
}
