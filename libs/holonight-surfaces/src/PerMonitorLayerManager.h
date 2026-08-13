#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <Qt>

#include <cstdint>
#include <functional>
#include <holonight/wayland/layersurfacehost.h>
#include <map>
#include <memory>

class QQmlEngine;
class QQuickView;
class QScreen;

// Base for managers that maintain exactly one wlr-layer-shell surface per monitor (bars, wallpapers).
// Owns the shared per-monitor lifecycle: creating a persistent LayerSurfaceHost, tracking it by
// stable output name, and reacting to monitor hotplug
// (QGuiApplication::screenAdded / screenRemoved). ShellApplication calls start() once the shared
// LayerShellContext reports availability.
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

  // Enumerate the current monitors and wire hotplug. The application gates this on LayerShellContext.
  void start();

 protected:
  struct MonitorSurface {
    QScreen* screen{};
    std::unique_ptr<Holonight::Wayland::LayerSurfaceHost> host;
  };

  // Per-manager constants/behavior for one surface.
  struct LayerConfig {
    Holonight::Wayland::Layer layer;
    QString namespace_name;       // wlr namespace string ("bar", "background")
    Qt::WindowFlags extra_flags;  // added on top of BypassWindowManagerHint
  };

  // QML to load into the view for a given monitor.
  struct QmlSource {
    QUrl url;
    QVariantMap initial_properties;
  };

  PerMonitorLayerManager(const char* log_tag, QObject* parent = nullptr);
  using HostFactory = std::function<std::unique_ptr<Holonight::Wayland::LayerSurfaceHost>()>;
  PerMonitorLayerManager(const char* log_tag, HostFactory host_factory, QObject* parent = nullptr);

  // The live surfaces keyed by their monitor. Exposed so subclasses can push live updates (e.g. a
  // wallpaper change) into existing views without rebuilding them.
  [[nodiscard]] const std::map<QString, MonitorSurface>& surfaces() const { return surfaces_; }

  // Find a live view by its stable compositor monitor name without scanning every surface. Returns
  // null when this manager has no surface for the requested monitor.
  [[nodiscard]] QQuickView* viewForMonitor(const QString& monitor_name) const;

  // ---- Subclass hooks ----------------------------------------------------------------------------
  [[nodiscard]] virtual LayerConfig layerConfig() const = 0;
  // Add image providers etc. before the QML source is set. Default: nothing.
  virtual void decorateEngine(QQmlEngine& engine);
  virtual void onHostConfigured(const QString& monitor_name);
  virtual bool openHost(Holonight::Wayland::LayerSurfaceHost& host, const Holonight::Wayland::LayerSurfaceSpec& spec);
  // Set anchors, size and exclusive zone on a freshly created surface.
  virtual void configureSurface(Holonight::Wayland::LayerSurfaceSpec& spec, QScreen* screen) = 0;
  // The QML component + initial properties for this monitor.
  [[nodiscard]] virtual QmlSource qmlSource(QScreen* screen) = 0;
  // Gate surface creation per monitor. Default: create on every screen. Subclasses that target only
  // a subset of monitors (e.g. desktop widgets with a configured monitor list) override this so the
  // base skips non-targeted screens during both the initial enumeration and hotplug.
  [[nodiscard]] virtual bool shouldCreateSurface(QScreen* screen) const;
  // Called after the set of monitors changes (add or remove). Default: nothing. Backgrounds override
  // to re-resolve positional wallpaper assignments.
  virtual void onScreenSetChanged();
  [[nodiscard]] Holonight::Wayland::LayerSurfaceSpec surfaceSpec(QScreen* screen);

 private:
  void createSurface(QScreen* screen);
  void handleScreenAdded(QScreen* screen);
  void handleScreenRemoved(QScreen* screen);
  void closeAllSurfaces();
  void removeCurrentSurface(const QString& output_name, Holonight::Wayland::LayerSurfaceHost* expected_host);

  const char* log_tag_;
  HostFactory host_factory_;
  std::map<QString, MonitorSurface> surfaces_;
  bool started_ = false;
};
