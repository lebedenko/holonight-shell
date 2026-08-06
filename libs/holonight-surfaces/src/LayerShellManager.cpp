#include "LayerShellManager.h"

#include "IconImageProvider.h"
#include "LayerSurface.h"
#include "ShellConstants.h"
#include "TrayModel.h"

#include <QQmlEngine>
#include <QScreen>
#include <QUrl>

LayerShellManager::LayerShellManager(LayerShell& shell, TrayModel* tray_model, QObject* parent)
    : PerMonitorLayerManager(shell, "LayerShellManager", parent), tray_model_(tray_model) {}

PerMonitorLayerManager::LayerConfig LayerShellManager::layerConfig() const {
  return {
      .layer = QtWayland::zwlr_layer_shell_v1::layer_top, .namespace_name = QStringLiteral("bar"), .extra_flags = {}};
}

void LayerShellManager::decorateEngine(QQmlEngine& engine) {
  engine.addImageProvider(QStringLiteral("icon"), new IconImageProvider());
  engine.addImageProvider(QStringLiteral("tray"), new TrayImageProvider(tray_model_));
}

void LayerShellManager::configureSurface(LayerSurface& surface, QScreen* /*screen*/) {
  surface.set_anchor(QtWayland::zwlr_layer_surface_v1::anchor_top | QtWayland::zwlr_layer_surface_v1::anchor_left |
                     QtWayland::zwlr_layer_surface_v1::anchor_right);
  surface.set_size(0, kBarHeight);
  surface.set_exclusive_zone(kBarHeight);
}

PerMonitorLayerManager::QmlSource LayerShellManager::qmlSource(QScreen* screen) {
  return {.url = QUrl(QStringLiteral("qrc:/HolonightShell/Topbar/TopBar.qml")),
          .initial_properties = {{QStringLiteral("barMonitorName"), screen->name()}}};
}
