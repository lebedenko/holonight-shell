#include "TrayModel.h"

#include <QDir>
#include <QFile>
#include <QIcon>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {

QImage imageWithColor(const QColor& color) {
  QImage image(2, 2, QImage::Format_ARGB32);
  image.fill(color);
  return image;
}

TrayItem makeItem(const QString& service, const QString& path, const QString& title = QStringLiteral("Title")) {
  return TrayItem{
      .service = service,
      .object_path = path,
      .icon_name = QStringLiteral("regular-icon"),
      .attention_icon_name = QStringLiteral("attention-icon"),
      .status = QStringLiteral("Active"),
      .title = title,
      .icon_pixmap_cache = imageWithColor(Qt::red),
      .attention_pixmap_cache = imageWithColor(Qt::blue),
  };
}

class ThemePathScope {
 public:
  ThemePathScope() : old_theme_name_(QIcon::themeName()), old_search_paths_(QIcon::themeSearchPaths()) {
    QDir theme_dir(temp_dir_.path() + QStringLiteral("/TestTheme"));
    theme_dir.mkpath(QStringLiteral("22x22/panel"));
    QFile index(theme_dir.filePath(QStringLiteral("index.theme")));
    if (index.open(QIODevice::WriteOnly | QIODevice::Text)) {
      index.write(
          "[Icon Theme]\nName=TestTheme\nDirectories=22x22/panel\n[22x22/panel]\nSize=22\nContext=Panel\nType=Fixed\n");
    }
    writeIcon(theme_dir.filePath(QStringLiteral("22x22/panel/slack-indicator.png")));
    writeIcon(theme_dir.filePath(QStringLiteral("22x22/panel/slack-indicator-attention.png")));
    QIcon::setThemeSearchPaths({temp_dir_.path()});
    QIcon::setThemeName(QStringLiteral("TestTheme"));
  }

  ~ThemePathScope() {
    QIcon::setThemeName(old_theme_name_);
    QIcon::setThemeSearchPaths(old_search_paths_);
  }

  ThemePathScope(const ThemePathScope&) = delete;
  ThemePathScope& operator=(const ThemePathScope&) = delete;
  ThemePathScope(ThemePathScope&&) = delete;
  ThemePathScope& operator=(ThemePathScope&&) = delete;

 private:
  static void writeIcon(const QString& path) {
    QImage image(22, 22, QImage::Format_ARGB32);
    image.fill(Qt::white);
    image.save(path, "PNG");
  }

  QTemporaryDir temp_dir_;
  QString old_theme_name_;
  QStringList old_search_paths_;
};

TrayIconOverridesConfig makeOverrideConfig(
    const QString& icon = QStringLiteral("slack-indicator"),
    const QString& attention_icon = QStringLiteral("slack-indicator-attention")) {
  TrayIconOverrideConfig override;
  override.name = QStringLiteral("slack");
  override.id = QStringLiteral("Slack_status_icon_1");
  override.icon = icon;
  override.attention_icon = attention_icon;
  return TrayIconOverridesConfig{.items = {override}};
}

// Mirrors the slot priority algorithm from TraySection.qml §4.
// Urgent (NeedsAttention) items claim leftmost visible positions; Passive items are excluded.
struct SlotEntry {
  QString key;
  int row{};
};

QList<SlotEntry> computeSlots(const TrayModel& model, int max_visible = 3) {
  QList<SlotEntry> urgent;
  QList<SlotEntry> non_urgent;
  for (int row = 0; row < model.rowCount(); ++row) {
    const QModelIndex idx = model.index(row, 0);
    const QString status = model.data(idx, TrayModel::StatusRole).toString();
    const QString key = model.data(idx, TrayModel::ItemKeyRole).toString();
    if (status == QStringLiteral("Passive")) {
      continue;
    }
    if (status == QStringLiteral("NeedsAttention")) {
      urgent.append({.key = key, .row = row});
    } else {
      non_urgent.append({.key = key, .row = row});
    }
  }
  QList<SlotEntry> visible = urgent.mid(0, max_visible);
  const int free_count = max_visible - static_cast<int>(visible.size());
  for (int i = 0; i < free_count && i < static_cast<int>(non_urgent.size()); ++i) {
    visible.append(non_urgent.at(i));
  }
  return visible;
}

}  // namespace

TEST(TrayModel, InsertsRowsAndExposesAllRoles) {
  TrayModel model;
  const QString key = QStringLiteral("org.example.StatusNotifierItem:/Item");
  model.addItem(makeItem(QStringLiteral("org.example.StatusNotifierItem"), QStringLiteral("/Item")), key);

  ASSERT_EQ(model.rowCount(), 1);
  const QModelIndex index = model.index(0, 0);
  EXPECT_EQ(model.data(index, TrayModel::ServiceRole).toString(), QStringLiteral("org.example.StatusNotifierItem"));
  EXPECT_EQ(model.data(index, TrayModel::ObjectPathRole).toString(), QStringLiteral("/Item"));
  EXPECT_EQ(model.data(index, TrayModel::IconNameRole).toString(), QStringLiteral("regular-icon"));
  EXPECT_EQ(model.data(index, TrayModel::AttentionIconNameRole).toString(), QStringLiteral("attention-icon"));
  EXPECT_EQ(model.data(index, TrayModel::StatusRole).toString(), QStringLiteral("Active"));
  EXPECT_EQ(model.data(index, TrayModel::IconPixmapUrlRole).toString(),
            QStringLiteral("image://tray/org.example.StatusNotifierItem:/Item?v=1"));
  EXPECT_EQ(model.data(index, TrayModel::AttentionPixmapUrlRole).toString(),
            QStringLiteral("image://tray/attn/org.example.StatusNotifierItem:/Item?v=1"));
  EXPECT_EQ(model.data(index, TrayModel::TitleRole).toString(), QStringLiteral("Title"));
  EXPECT_EQ(model.data(index, TrayModel::ItemKeyRole).toString(), key);
  EXPECT_FALSE(model.data(index, TrayModel::HasUnreadRole).toBool());

  const QHash<int, QByteArray> roles = model.roleNames();
  EXPECT_EQ(roles.value(TrayModel::ServiceRole), QByteArray("service"));
  EXPECT_EQ(roles.value(TrayModel::ObjectPathRole), QByteArray("objectPath"));
  EXPECT_EQ(roles.value(TrayModel::IconNameRole), QByteArray("iconName"));
  EXPECT_EQ(roles.value(TrayModel::AttentionIconNameRole), QByteArray("attentionIconName"));
  EXPECT_EQ(roles.value(TrayModel::StatusRole), QByteArray("status"));
  EXPECT_EQ(roles.value(TrayModel::IconPixmapUrlRole), QByteArray("iconPixmapUrl"));
  EXPECT_EQ(roles.value(TrayModel::AttentionPixmapUrlRole), QByteArray("attentionPixmapUrl"));
  EXPECT_EQ(roles.value(TrayModel::TitleRole), QByteArray("title"));
  EXPECT_EQ(roles.value(TrayModel::ItemKeyRole), QByteArray("itemKey"));
  EXPECT_EQ(roles.value(TrayModel::HasUnreadRole), QByteArray("hasUnread"));
}

TEST(TrayModel, InvalidIndexesAndParentIndexesAreEmpty) {
  TrayModel model;
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/one")), QStringLiteral("svc:/one"));

  EXPECT_EQ(model.rowCount(model.index(0, 0)), 0);
  EXPECT_FALSE(model.data({}).isValid());
  EXPECT_FALSE(model.data(model.index(-1, 0), TrayModel::ServiceRole).isValid());
  EXPECT_FALSE(model.data(model.index(1, 0), TrayModel::ServiceRole).isValid());
  EXPECT_FALSE(model.data(model.index(0, 0), Qt::DisplayRole).isValid());
}

TEST(TrayModel, AddingExistingKeyUpdatesExistingRow) {
  TrayModel model;
  QSignalSpy changed(&model, &TrayModel::dataChanged);
  const QString key = QStringLiteral("svc:/one");
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/one"), QStringLiteral("Old")), key);
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/one"), QStringLiteral("New")), key);

  ASSERT_EQ(model.rowCount(), 1);
  EXPECT_EQ(changed.count(), 1);
  EXPECT_EQ(model.data(model.index(0, 0), TrayModel::TitleRole).toString(), QStringLiteral("New"));
}

TEST(TrayModel, RemovesExistingRowsAndRemapsFollowingIndexes) {
  TrayModel model;
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/one"), QStringLiteral("One")),
                QStringLiteral("svc:/one"));
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/two"), QStringLiteral("Two")),
                QStringLiteral("svc:/two"));
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/three"), QStringLiteral("Three")),
                QStringLiteral("svc:/three"));

  model.removeItem(QStringLiteral("svc:/two"));
  model.removeItem(QStringLiteral("missing:/item"));

  ASSERT_EQ(model.rowCount(), 2);
  EXPECT_EQ(model.data(model.index(0, 0), TrayModel::ItemKeyRole).toString(), QStringLiteral("svc:/one"));
  EXPECT_EQ(model.data(model.index(1, 0), TrayModel::ItemKeyRole).toString(), QStringLiteral("svc:/three"));

  const TrayItem* remaining = model.itemForKey(QStringLiteral("svc:/three"));
  ASSERT_NE(remaining, nullptr);
  EXPECT_EQ(remaining->title, QStringLiteral("Three"));
}

TEST(TrayModel, ImageForKeyReturnsRegularAttentionAndNullUnknownImages) {
  TrayModel model;
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/one")), QStringLiteral("svc:/one"));

  EXPECT_EQ(model.imageForKey(QStringLiteral("svc:/one")).pixelColor(0, 0), QColor(Qt::red));
  EXPECT_EQ(model.imageForKey(QStringLiteral("svc:/one"), true).pixelColor(0, 0), QColor(Qt::blue));
  EXPECT_TRUE(model.imageForKey(QStringLiteral("missing:/item")).isNull());
}

TEST(TrayModel, TrayImageProviderParsesAttentionPrefixAndCacheBustSuffix) {
  TrayModel model;
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/one")), QStringLiteral("svc:/one"));
  TrayImageProvider provider(&model);

  QSize regular_size;
  const QImage regular = provider.requestImage(QStringLiteral("svc:/one?v=42"), &regular_size, {});
  EXPECT_EQ(regular_size, QSize(2, 2));
  EXPECT_EQ(regular.pixelColor(0, 0), QColor(Qt::red));

  QSize attention_size;
  const QImage attention = provider.requestImage(QStringLiteral("attn/svc:/one?v=43"), &attention_size, {});
  EXPECT_EQ(attention_size, QSize(2, 2));
  EXPECT_EQ(attention.pixelColor(0, 0), QColor(Qt::blue));

  QSize missing_size(10, 10);
  EXPECT_TRUE(provider.requestImage(QStringLiteral("missing:/item?v=44"), &missing_size, {}).isNull());
  EXPECT_EQ(missing_size, QSize(0, 0));
}

TEST(TrayModel, ValidTrayIconOverrideReplacesIconNameAndSuppressesPixmapUrl) {
  ThemePathScope theme;
  TrayModel model;
  model.updateTrayIconOverrides(makeOverrideConfig());

  TrayItem item = makeItem(QStringLiteral(":1.168"), QStringLiteral("/StatusNotifierItem"));
  item.id = QStringLiteral("Slack_status_icon_1");
  model.addItem(item, QStringLiteral(":1.168:/StatusNotifierItem"));

  const QModelIndex idx = model.index(0, 0);
  EXPECT_EQ(model.data(idx, TrayModel::IconNameRole).toString(), QStringLiteral("slack-indicator"));
  EXPECT_TRUE(model.data(idx, TrayModel::IconPixmapUrlRole).toString().isEmpty());
}

TEST(TrayModel, InvalidTrayIconOverrideFallsBackToDbusIconAndPixmap) {
  ThemePathScope theme;
  TrayModel model;
  model.updateTrayIconOverrides(makeOverrideConfig(QStringLiteral("missing-icon"), QString{}));

  TrayItem item = makeItem(QStringLiteral(":1.168"), QStringLiteral("/StatusNotifierItem"));
  item.id = QStringLiteral("Slack_status_icon_1");
  model.addItem(item, QStringLiteral(":1.168:/StatusNotifierItem"));

  const QModelIndex idx = model.index(0, 0);
  EXPECT_EQ(model.data(idx, TrayModel::IconNameRole).toString(), QStringLiteral("regular-icon"));
  EXPECT_FALSE(model.data(idx, TrayModel::IconPixmapUrlRole).toString().isEmpty());
}

TEST(TrayModel, AttentionTrayIconOverrideUsesAttentionIconWhenValid) {
  ThemePathScope theme;
  TrayModel model;
  model.updateTrayIconOverrides(makeOverrideConfig());

  TrayItem item = makeItem(QStringLiteral(":1.168"), QStringLiteral("/StatusNotifierItem"));
  item.id = QStringLiteral("Slack_status_icon_1");
  item.status = QStringLiteral("NeedsAttention");
  model.addItem(item, QStringLiteral(":1.168:/StatusNotifierItem"));

  const QModelIndex idx = model.index(0, 0);
  EXPECT_EQ(model.data(idx, TrayModel::AttentionIconNameRole).toString(), QStringLiteral("slack-indicator-attention"));
  EXPECT_TRUE(model.data(idx, TrayModel::AttentionPixmapUrlRole).toString().isEmpty());
}

TEST(TrayModel, TrayIconOverrideReloadUpdatesExistingRows) {
  ThemePathScope theme;
  TrayModel model;

  TrayItem item = makeItem(QStringLiteral(":1.168"), QStringLiteral("/StatusNotifierItem"));
  item.id = QStringLiteral("Slack_status_icon_1");
  model.addItem(item, QStringLiteral(":1.168:/StatusNotifierItem"));
  ASSERT_EQ(model.data(model.index(0, 0), TrayModel::IconNameRole).toString(), QStringLiteral("regular-icon"));

  QSignalSpy changed(&model, &TrayModel::dataChanged);
  model.updateTrayIconOverrides(makeOverrideConfig());

  EXPECT_EQ(changed.count(), 1);
  EXPECT_EQ(model.data(model.index(0, 0), TrayModel::IconNameRole).toString(), QStringLiteral("slack-indicator"));
}

TEST(TrayModel, SlotAlgorithmExcludesPassiveItems) {
  TrayModel model;
  TrayItem passive = makeItem(QStringLiteral("svc"), QStringLiteral("/p1"), QStringLiteral("P1"));
  passive.status = QStringLiteral("Passive");
  model.addItem(passive, QStringLiteral("svc:/p1"));
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/a1"), QStringLiteral("A1")),
                QStringLiteral("svc:/a1"));
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/a2"), QStringLiteral("A2")),
                QStringLiteral("svc:/a2"));

  const QList<SlotEntry> visible = computeSlots(model);
  ASSERT_EQ(visible.size(), 2);
  EXPECT_EQ(visible.at(0).key, QStringLiteral("svc:/a1"));
  EXPECT_EQ(visible.at(1).key, QStringLiteral("svc:/a2"));
}

TEST(TrayModel, SlotAlgorithmPlacesUrgentItemsFirst) {
  TrayModel model;
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/a1"), QStringLiteral("A1")),
                QStringLiteral("svc:/a1"));
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/a2"), QStringLiteral("A2")),
                QStringLiteral("svc:/a2"));
  TrayItem urgent = makeItem(QStringLiteral("svc"), QStringLiteral("/u1"), QStringLiteral("U1"));
  urgent.status = QStringLiteral("NeedsAttention");
  model.addItem(urgent, QStringLiteral("svc:/u1"));

  const QList<SlotEntry> visible = computeSlots(model);
  ASSERT_EQ(visible.size(), 3);
  EXPECT_EQ(visible.at(0).key, QStringLiteral("svc:/u1"));  // urgent -> leftmost
  EXPECT_EQ(visible.at(1).key, QStringLiteral("svc:/a1"));  // non-urgent in model order
  EXPECT_EQ(visible.at(2).key, QStringLiteral("svc:/a2"));
}

TEST(TrayModel, SlotAlgorithmCapsAtMaxVisible) {
  TrayModel model;
  for (int i = 1; i <= 4; ++i) {
    const QString path = QStringLiteral("/a%1").arg(i);
    model.addItem(makeItem(QStringLiteral("svc"), path), QStringLiteral("svc:") + path);
  }

  const QList<SlotEntry> visible = computeSlots(model, 3);
  ASSERT_EQ(visible.size(), 3);
  EXPECT_EQ(visible.at(0).key, QStringLiteral("svc:/a1"));
  EXPECT_EQ(visible.at(1).key, QStringLiteral("svc:/a2"));
  EXPECT_EQ(visible.at(2).key, QStringLiteral("svc:/a3"));
  // Fourth item is in overflow: total non-passive minus visible = 1
  EXPECT_EQ(model.rowCount() - static_cast<int>(visible.size()), 1);
}

TEST(TrayModel, UrgencyTransitionTriggersDataChangedAndUpdatesSlotOrder) {
  TrayModel model;
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/a1")), QStringLiteral("svc:/a1"));
  model.addItem(makeItem(QStringLiteral("svc"), QStringLiteral("/a2")), QStringLiteral("svc:/a2"));

  QSignalSpy changed(&model, &TrayModel::dataChanged);

  TrayItem now_urgent = makeItem(QStringLiteral("svc"), QStringLiteral("/a2"));
  now_urgent.status = QStringLiteral("NeedsAttention");
  model.updateItem(QStringLiteral("svc:/a2"), now_urgent);

  ASSERT_EQ(changed.count(), 1);
  EXPECT_EQ(model.data(model.index(1, 0), TrayModel::StatusRole).toString(), QStringLiteral("NeedsAttention"));

  const QList<SlotEntry> visible = computeSlots(model);
  ASSERT_EQ(visible.size(), 2);
  EXPECT_EQ(visible.at(0).key, QStringLiteral("svc:/a2"));  // promoted to front
  EXPECT_EQ(visible.at(1).key, QStringLiteral("svc:/a1"));
}
