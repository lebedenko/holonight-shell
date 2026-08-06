#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <Qt>

#include <cstdint>
#include <memory>
#include <unordered_map>

class LayerShell;
class LayerSurface;
class QQmlEngine;
class QQuickView;
class QScreen;

// Base for managers that maintain exactly one wlr-layer-shell surface per monitor (bars, wallpapers).
// Owns the shared per-monitor lifecycle: creating the QQuickView + LayerSurface, tracking them keyed by
// QScreen*, tearing them down in Wayland-safe order, and reacting to monitor hotplug
// (QGuiApplication::screenAdded / screenRemoved). The layer-shell readiness gate lives in
// ShellApplication, which calls start() once the shared LayerShell global is bound.
//
// Subclasses supply the per-monitor specifics through the small virtuals below (layer, namespace,
// flags, anchors/size/zone, QML source). The base never decides those.
class PerMonitorLayerManager : public QObject {
  Q_OBJECT
 public:
  ~PerMonitorLayerManager() override;

  PerMonitorLayerManager(const PerMonitorLayerManager&) = delete;
  PerMonitorLayerManager& operator=(const PerMonitorLayerManager&) = delete;
  PerMonitorLayerManager(PerMonitorLayerManager&&) = delete;
  PerMonitorLayerManager& operator=(PerMonitorLayerManager&&) = delete;

  // Enumerate the current monitors and wire hotplug. Must only be called once the shared LayerShell
  // global is active (ShellApplication gates this). Idempotent.
  void start();

 protected:
  // surface must be destroyed before view to preserve Wayland protocol order; member declaration
  // order is load-bearing — do NOT reorder (members destruct in reverse).
  struct MonitorSurface {
    std::unique_ptr<QQuickView> view;
    std::unique_ptr<LayerSurface> surface;
  };

  // Per-manager constants/behavior for one surface.
  struct LayerConfig {
    std::uint32_t layer;          // QtWayland::zwlr_layer_shell_v1::layer_*
    QString namespace_name;       // wlr namespace string ("bar", "background")
    Qt::WindowFlags extra_flags;  // added on top of BypassWindowManagerHint
  };

  // QML to load into the view for a given monitor.
  struct QmlSource {
    QUrl url;
    QVariantMap initial_properties;
  };

  PerMonitorLayerManager(LayerShell& shell, const char* log_tag, QObject* parent = nullptr);

  // The live surfaces keyed by their monitor. Exposed so subclasses can push live updates (e.g. a
  // wallpaper change) into existing views without rebuilding them.
  const std::unordered_map<QScreen*, MonitorSurface>& surfaces() const { return surfaces_; }

  // Find a live view by its stable compositor monitor name without scanning every surface. Returns
  // null when this manager has no surface for the requested monitor.
  [[nodiscard]] QQuickView* viewForMonitor(const QString& monitor_name) const;

  // ---- Subclass hooks ----------------------------------------------------------------------------
  [[nodiscard]] virtual LayerConfig layerConfig() const = 0;
  // Add image providers etc. before the QML source is set. Default: nothing.
  virtual void decorateEngine(QQmlEngine& engine);
  // Set anchors, size and exclusive zone on a freshly created surface.
  virtual void configureSurface(LayerSurface& surface, QScreen* screen) = 0;
  // The QML component + initial properties for this monitor.
  [[nodiscard]] virtual QmlSource qmlSource(QScreen* screen) = 0;
  // Gate surface creation per monitor. Default: create on every screen. Subclasses that target only
  // a subset of monitors (e.g. desktop widgets with a configured monitor list) override this so the
  // base skips non-targeted screens during both the initial enumeration and hotplug.
  [[nodiscard]] virtual bool shouldCreateSurface(QScreen* screen) const;
  // Called after the set of monitors changes (add or remove). Default: nothing. Backgrounds override
  // to re-resolve positional wallpaper assignments.
  virtual void onScreenSetChanged();

 private:
  void createSurface(QScreen* screen);
  void handleScreenAdded(QScreen* screen);
  void handleScreenRemoved(QScreen* screen);

  LayerShell& shell_;
  const char* log_tag_;
  std::unordered_map<QScreen*, MonitorSurface> surfaces_;
  QHash<QString, QScreen*> screens_by_name_;
  bool started_ = false;
};
