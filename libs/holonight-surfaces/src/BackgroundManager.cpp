#include "BackgroundManager.h"

#include "ConfigService.h"

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>
#include <QStringList>
#include <QUrl>

QString BackgroundManager::imageUrlForScreen(QScreen* screen) const {
  const int index = static_cast<int>(QGuiApplication::screens().indexOf(screen));
  // Resolve the wallpaper for this monitor and convert to a file:// URL for QML Image.source. A bare
  // absolute path would be resolved against the component's qrc base URL and fail; empty stays empty
  // (the "no wallpaper" sentinel for Background.qml).
  const QString path =
      HoloNight::ShellConfig::BackgroundConfig::imageForMonitor(config_service_->background().images, index);
  return path.isEmpty() ? QString{} : QUrl::fromLocalFile(path).toString();
}

BackgroundManager::BackgroundManager(ConfigService* config_service, QObject* parent)
    : PerMonitorLayerManager("BackgroundManager", parent), config_service_(config_service) {
  connect(config_service_, &ConfigService::backgroundChanged, this, &BackgroundManager::onBackgroundChanged);
}

PerMonitorLayerManager::LayerConfig BackgroundManager::layerConfig() const {
  return {.layer = Holonight::Wayland::Layer::Background,
          .namespace_name = QStringLiteral("background"),
          // WindowTransparentForInput makes Qt maintain an empty input region and re-apply it on every
          // commit, so pointer/keyboard events fall through to the desktop below.
          .extra_flags = Qt::WindowTransparentForInput};
}

void BackgroundManager::configureSurface(Holonight::Wayland::LayerSurfaceSpec& spec, QScreen* /*screen*/) {
  using enum Holonight::Wayland::Anchor;
  spec.anchors = Top | Bottom | Left | Right;
  // -1 makes the surface span the full output and ignore other surfaces' exclusive zones, so the
  // wallpaper extends under the top bar. 0 would let the compositor displace it out of the bar's zone.
  spec.exclusive_zone = -1;
  spec.input_region_policy = Holonight::Wayland::InputRegionPolicy::Empty;
}

PerMonitorLayerManager::QmlSource BackgroundManager::qmlSource(QScreen* screen) {
  return {.url = QUrl(QStringLiteral("qrc:/HolonightShell/Background/Background.qml")),
          .initial_properties = {{QStringLiteral("imagePath"), imageUrlForScreen(screen)}}};
}

void BackgroundManager::onScreenSetChanged() {
  // A monitor was added or removed; remaining monitors may have shifted positional index, so every
  // surface re-resolves its wallpaper.
  refreshAllWallpapers();
}

void BackgroundManager::onBackgroundChanged() { refreshAllWallpapers(); }

void BackgroundManager::refreshAllWallpapers() {
  for (const auto& [monitor_name, monitor_surface] : surfaces()) {
    if (auto* root = qobject_cast<QQuickItem*>(monitor_surface.host->rootObject())) {
      root->setProperty("imagePath", imageUrlForScreen(monitor_surface.screen));
    }
  }
}
