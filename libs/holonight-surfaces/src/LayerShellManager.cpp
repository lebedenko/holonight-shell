#include "LayerShellManager.h"

#include "IconImageProvider.h"
#include "ShellConstants.h"
#include "TrayModel.h"

#include <QQmlEngine>
#include <QScreen>
#include <QUrl>

LayerShellManager::LayerShellManager(TrayModel* tray_model, QObject* parent)
    : PerMonitorLayerManager("LayerShellManager", parent), tray_model_(tray_model) {}

PerMonitorLayerManager::LayerConfig LayerShellManager::layerConfig() const {
  return {.layer = Holonight::Wayland::Layer::Top, .namespace_name = QStringLiteral("bar"), .extra_flags = {}};
}

void LayerShellManager::decorateEngine(QQmlEngine& engine) {
  engine.addImageProvider(QStringLiteral("icon"), new IconImageProvider());
  engine.addImageProvider(QStringLiteral("tray"), new TrayImageProvider(tray_model_));
}

void LayerShellManager::configureSurface(Holonight::Wayland::LayerSurfaceSpec& spec, QScreen* /*screen*/) {
  using enum Holonight::Wayland::Anchor;
  spec.anchors = Top | Left | Right;
  spec.height = kBarHeight;
  spec.exclusive_zone = kBarHeight;
}

PerMonitorLayerManager::QmlSource LayerShellManager::qmlSource(QScreen* screen) {
  return {.url = QUrl(QStringLiteral("qrc:/HolonightShell/Topbar/TopBar.qml")),
          .initial_properties = {{QStringLiteral("barMonitorName"), screen->name()}}};
}
