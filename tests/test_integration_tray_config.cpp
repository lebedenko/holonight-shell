// Integration test (T-030): TrayModel.maxVisible reads from ConfigService on
// startup and updates on live-reload; the number of visible slots (the QML
// slot-slice cap) tracks the configured max_items.
#include "ConfigService.h"
#include "TrayItem.h"
#include "TrayModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <algorithm>
#include <gtest/gtest.h>

namespace {

QString setTempXdg(const QTemporaryDir& tmp) {
  qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
  return tmp.path() + "/holonight/config.toml";
}

void writeConfig(const QString& path, const QByteArray& content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write(content);
}

QImage swatch() {
  QImage image(2, 2, QImage::Format_ARGB32);
  image.fill(Qt::red);
  return image;
}

TrayItem makeActiveItem(const QString& service) {
  return TrayItem{
      .service = service,
      .object_path = QStringLiteral("/StatusNotifierItem"),
      .icon_name = QStringLiteral("regular-icon"),
      .status = QStringLiteral("Active"),
      .title = service,
      .icon_pixmap_cache = swatch(),
  };
}

// Mirrors TraySection.qml: slotItems = orderedItems.slice(0, maxVisible).
int visibleSlotCount(const TrayModel& model) {
  int active = 0;
  for (int row = 0; row < model.rowCount(); ++row) {
    const QModelIndex idx = model.index(row, 0);
    if (model.data(idx, TrayModel::StatusRole).toString() != QStringLiteral("Passive")) {
      ++active;
    }
  }
  return std::min(active, model.maxVisible());
}

}  // namespace

class TrayConfigIntegrationTest : public ::testing::Test {
 protected:
  QTemporaryDir tmp;    // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  QString config_path;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

  void SetUp() override { config_path = setTempXdg(tmp); }
  void TearDown() override { qunsetenv("XDG_CONFIG_HOME"); }
};

// REQ-F-019, REQ-C-005: maxVisible mirrors the configured max_items at startup.
TEST_F(TrayConfigIntegrationTest, StartupMaxVisibleMatchesConfig) {
  writeConfig(config_path,
              "[bar.systemtray]\n"
              "max_items = 4\n");

  ConfigService config;
  TrayModel model(&config);

  EXPECT_EQ(model.maxVisible(), 4);
  EXPECT_EQ(model.maxVisible(), config.barSystemTray().max_items);
}

// REQ-F-019: a live max_items change updates maxVisible and fires maxVisibleChanged.
TEST_F(TrayConfigIntegrationTest, LiveReloadUpdatesMaxVisibleAndEmits) {
  writeConfig(config_path,
              "[bar.systemtray]\n"
              "max_items = 3\n");

  ConfigService config;
  TrayModel model(&config);
  ASSERT_EQ(model.maxVisible(), 3);

  QSignalSpy spy(&model, &TrayModel::maxVisibleChanged);

  writeConfig(config_path,
              "[bar.systemtray]\n"
              "max_items = 5\n");
  QMetaObject::invokeMethod(&config, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(model.maxVisible(), 5);
  EXPECT_EQ(spy.count(), 1);
}

// REQ-F-019: the visible slot count (truncation threshold) tracks the new value.
TEST_F(TrayConfigIntegrationTest, VisibleSlotCountTracksMaxItems) {
  writeConfig(config_path,
              "[bar.systemtray]\n"
              "max_items = 3\n");

  ConfigService config;
  TrayModel model(&config);

  // Five active items: with max_items=3 only three slots are visible.
  model.addItem(makeActiveItem(QStringLiteral(":1.1")), QStringLiteral("k1"));
  model.addItem(makeActiveItem(QStringLiteral(":1.2")), QStringLiteral("k2"));
  model.addItem(makeActiveItem(QStringLiteral(":1.3")), QStringLiteral("k3"));
  model.addItem(makeActiveItem(QStringLiteral(":1.4")), QStringLiteral("k4"));
  model.addItem(makeActiveItem(QStringLiteral(":1.5")), QStringLiteral("k5"));
  EXPECT_EQ(visibleSlotCount(model), 3);

  // Raise the cap to 5 via live-reload: all five become visible.
  writeConfig(config_path,
              "[bar.systemtray]\n"
              "max_items = 5\n");
  QMetaObject::invokeMethod(&config, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(model.maxVisible(), 5);
  EXPECT_EQ(visibleSlotCount(model), 5);
}
