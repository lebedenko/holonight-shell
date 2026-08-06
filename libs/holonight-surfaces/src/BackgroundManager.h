#pragma once

#include "PerMonitorLayerManager.h"

#include <QString>

class ConfigService;
class LayerShell;
class LayerSurface;
class QScreen;

// Creates one full-screen wlr-layer-shell surface per monitor on the BACKGROUND layer to host the
// configured wallpaper. Anchors all four edges, reserves no exclusive zone (-1 spans under the bar),
// and uses Qt::WindowTransparentForInput so input falls through to windows below. Wallpapers are mapped
// positionally to monitors; on hotplug or live config reload every surface re-resolves its image from
// its current index in QGuiApplication::screens().
class BackgroundManager : public PerMonitorLayerManager {
  Q_OBJECT
 public:
  BackgroundManager(LayerShell& shell, ConfigService* config_service, QObject* parent = nullptr);

 protected:
  [[nodiscard]] LayerConfig layerConfig() const override;
  void configureSurface(LayerSurface& surface, QScreen* screen) override;
  [[nodiscard]] QmlSource qmlSource(QScreen* screen) override;
  void onScreenSetChanged() override;

 private Q_SLOTS:
  void onBackgroundChanged();

 private:
  // file:// URL of the wallpaper for this screen's current positional index, or "" for none.
  [[nodiscard]] QString imageUrlForScreen(QScreen* screen) const;
  // Push the current positional wallpaper into every live surface (live reload + hotplug re-index).
  void refreshAllWallpapers();

  ConfigService* config_service_;
};
