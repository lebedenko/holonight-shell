#pragma once

#include "PerMonitorLayerManager.h"

class QScreen;
class TrayModel;

// Creates one wlr-layer-shell bar per monitor on the TOP layer. Hotplug and lifecycle are handled by
// PerMonitorLayerManager; this subclass only supplies the bar-specific layer config, image providers,
// anchors/size/exclusive zone and QML source.
class LayerShellManager : public PerMonitorLayerManager {
  Q_OBJECT
 public:
  LayerShellManager(TrayModel* tray_model, QObject* parent = nullptr);

 protected:
  [[nodiscard]] LayerConfig layerConfig() const override;
  void decorateEngine(QQmlEngine& engine) override;
  void configureSurface(Holonight::Wayland::LayerSurfaceSpec& spec, QScreen* screen) override;
  [[nodiscard]] QmlSource qmlSource(QScreen* screen) override;

 private:
  TrayModel* tray_model_;
};
