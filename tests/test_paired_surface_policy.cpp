#include "StatusPopupGeometry.h"
#include "StatusPopupSurface.h"
#include "TrayMenuSurface.h"
#include "TrayMenuSurfacePolicy.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QScreen>

#include <gtest/gtest.h>

using namespace Holonight::Wayland;

namespace {
void expectDismissPolicy(const LayerSurfaceSpec& spec, QScreen* screen, const QString& name, const QUrl& url) {
  EXPECT_EQ(spec.output, screen);
  EXPECT_EQ(spec.name_space, name);
  EXPECT_EQ(spec.layer, Layer::Top);
  EXPECT_EQ(spec.anchors, Anchor::Top | Anchor::Bottom | Anchor::Left | Anchor::Right);
  EXPECT_EQ(spec.width, 0);
  EXPECT_EQ(spec.height, 0);
  EXPECT_EQ(spec.exclusive_zone, 0);
  EXPECT_EQ(spec.qml_url, url);
  EXPECT_EQ(spec.color, QColor(Qt::transparent));
}
}  // namespace

TEST(PairedSurfacePolicy, DescribesStatusContentAndDismissSurfacesCompletely) {
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);
  const int anchor_x = screen->geometry().x() + 300;
  const PairedLayerSurfaceSpec pair = StatusPopupSurface::surfaceSpec(screen, QStringLiteral("audio"), anchor_x, 40);
  const StatusPopupGeometry geometry =
      statusPopupGeometry(QStringLiteral("audio"), screen->geometry(), screen->availableGeometry(), anchor_x, 40);
  expectDismissPolicy(pair.dismiss, screen, QStringLiteral("status-popup-dismiss"),
                      QUrl(QStringLiteral("qrc:/HolonightShell/Popups/Status/StatusPopupDismissOverlay.qml")));
  EXPECT_EQ(pair.content.name_space, QStringLiteral("status-popup"));
  EXPECT_EQ(pair.content.anchors, Anchor::Top | Anchor::Left);
  EXPECT_EQ(pair.content.width, geometry.surface_width);
  EXPECT_EQ(pair.content.height, geometry.surface_height);
  EXPECT_EQ(pair.content.margin_left, geometry.left_margin);
  EXPECT_EQ(pair.content.keyboard_interactivity, KeyboardInteractivity::OnDemand);
  EXPECT_EQ(pair.content.initial_properties.value(QStringLiteral("popupId")).toString(), QStringLiteral("audio"));
  EXPECT_TRUE(static_cast<bool>(pair.content.before_load));
  QQmlEngine engine;
  pair.content.before_load(&engine);
  EXPECT_NE(engine.imageProvider(QStringLiteral("icon")), nullptr);
}

TEST(PairedSurfacePolicy, ComputesTrayPlacementAndDynamicGeometryWithoutWayland) {
  const QRect screen(1920, 0, 1920, 1080);
  const TrayMenuPlacement left = trayMenuPlacement(screen, 1940, 50);
  EXPECT_EQ(left.x, 20);
  EXPECT_EQ(left.y, 52);
  EXPECT_EQ(left.column_count, 2);
  EXPECT_EQ(left.top_level_column, 0);

  const TrayMenuPlacement right = trayMenuPlacement(screen, 3800, 100);
  EXPECT_EQ(right.top_level_column, 1);
  EXPECT_EQ(right.column_step, -1);
  const TrayMenuActiveGeometry one = trayMenuActiveGeometry(screen, right, 1, 100);
  const TrayMenuActiveGeometry two = trayMenuActiveGeometry(screen, right, 2, 180);
  EXPECT_EQ(one.column_index, 0);
  EXPECT_EQ(two.column_index, 1);
  EXPECT_LT(one.surface_width, two.surface_width);
  EXPECT_LT(one.margin_left, right.x + kTrayMenuWidth + kTraySubmenuGap);
  EXPECT_EQ(two.panel_height, 180);
}

TEST(PairedSurfacePolicy, DescribesTrayContentPropertiesAndImageProvider) {
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);
  TrayMenuSurface tray;
  DbusMenuModel model({DbusMenuItem{.id = 1, .label = QStringLiteral("Item")}});
  const QPoint request = screen->geometry().topLeft() + QPoint(100, 100);
  const PairedLayerSurfaceSpec pair = tray.surfaceSpec(screen, request.x(), request.y(), &model);
  expectDismissPolicy(pair.dismiss, screen, QStringLiteral("tray-menu-dismiss"),
                      QUrl(QStringLiteral("qrc:/HolonightShell/Tray/TrayMenuDismissOverlay.qml")));
  EXPECT_EQ(pair.content.name_space, QStringLiteral("tray-menu"));
  EXPECT_EQ(pair.content.anchors, Anchor::Top | Anchor::Left);
  EXPECT_EQ(pair.content.keyboard_interactivity, KeyboardInteractivity::OnDemand);
  EXPECT_EQ(pair.content.initial_properties.value(QStringLiteral("menuModel")).value<DbusMenuModel*>(), &model);
  EXPECT_EQ(pair.content.initial_properties.value(QStringLiteral("menuClient")).value<QObject*>(), &tray);
  EXPECT_EQ(pair.content.initial_properties.value(QStringLiteral("columnWidth")).toInt(), kTrayMenuWidth);
  EXPECT_TRUE(static_cast<bool>(pair.content.before_load));
  QQmlEngine engine;
  pair.content.before_load(&engine);
  EXPECT_NE(engine.imageProvider(QStringLiteral("icon")), nullptr);
}
