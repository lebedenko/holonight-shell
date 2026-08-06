#include "PerMonitorLayerManager.h"

#include "LayerShell.h"
#include "LayerSurface.h"
#include "QmlSourceLoader.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickView>
#include <QScreen>
#include <qpa/qplatformwindow_p.h>
#include <qscreen_platform.h>

#include <utility>
#include <wayland-client.h>

PerMonitorLayerManager::PerMonitorLayerManager(LayerShell& shell, const char* log_tag, QObject* parent)
    : QObject(parent), shell_(shell), log_tag_(log_tag) {
  // Qt 6.11+: BypassWindowManagerHint only suppresses xdg_surface when this env var is set.
  qputenv("QT_WAYLAND_USE_BYPASSWINDOWMANAGERHINT", "1");
}

// Defaulted: each MonitorSurface destructs its surface before its view (reverse member declaration
// order), preserving Wayland protocol destruction order. See struct MonitorSurface.
PerMonitorLayerManager::~PerMonitorLayerManager() = default;

void PerMonitorLayerManager::decorateEngine(QQmlEngine& /*engine*/) {}

void PerMonitorLayerManager::onScreenSetChanged() {}

bool PerMonitorLayerManager::shouldCreateSurface(QScreen* /*screen*/) const { return true; }

QQuickView* PerMonitorLayerManager::viewForMonitor(const QString& monitor_name) const {
  const auto screen = screens_by_name_.constFind(monitor_name);
  if (screen == screens_by_name_.cend()) {
    return nullptr;
  }
  const auto surface = surfaces_.find(*screen);
  return surface == surfaces_.cend() ? nullptr : surface->second.view.get();
}

void PerMonitorLayerManager::start() {
  if (started_) {
    return;
  }
  started_ = true;

  connect(qGuiApp, &QGuiApplication::screenAdded, this, &PerMonitorLayerManager::handleScreenAdded);
  connect(qGuiApp, &QGuiApplication::screenRemoved, this, &PerMonitorLayerManager::handleScreenRemoved);

  for (QScreen* screen : QGuiApplication::screens()) {
    createSurface(screen);
  }
}

void PerMonitorLayerManager::createSurface(QScreen* screen) {
  if (!shouldCreateSurface(screen)) {
    return;
  }
  const LayerConfig cfg = layerConfig();

  auto view = std::make_unique<QQuickView>();
  decorateEngine(*view->engine());
  view->setScreen(screen);
  view->setResizeMode(QQuickView::SizeRootObjectToView);
  view->setColor(Qt::transparent);
  view->setFlags(view->flags() | Qt::BypassWindowManagerHint | cfg.extra_flags);
  view->create();

  auto* waylandWindow = view->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
  if (waylandWindow == nullptr) {
    qCritical("%s: not running under a Wayland compositor", log_tag_);
    return;
  }

  wl_surface* wlSurface = waylandWindow->surface();
  if (wlSurface == nullptr) {
    qCritical("%s: wl_surface not available for screen '%s'", log_tag_, qPrintable(screen->name()));
    return;
  }

  wl_output* wlOutput = nullptr;
  if (auto* waylandScreen = screen->nativeInterface<QNativeInterface::QWaylandScreen>()) {
    wlOutput = waylandScreen->output();
  }

  auto* rawSurface = shell_.get_layer_surface(wlSurface, wlOutput, cfg.layer, cfg.namespace_name);
  auto layerSurface = std::make_unique<LayerSurface>(rawSurface, wlSurface, view.get());
  configureSurface(*layerSurface, screen);

  const QmlSource source = qmlSource(screen);
  if (!source.initial_properties.isEmpty()) {
    view->setInitialProperties(source.initial_properties);
  }
  if (!loadQmlSource(view.get(), source.url, log_tag_)) {
    return;
  }
  wl_surface_commit(wlSurface);

  surfaces_.insert_or_assign(screen, MonitorSurface{.view = std::move(view), .surface = std::move(layerSurface)});
  screens_by_name_.insert(screen->name(), screen);
}

void PerMonitorLayerManager::handleScreenAdded(QScreen* screen) {
  if (surfaces_.contains(screen)) {
    return;
  }
  createSurface(screen);
  onScreenSetChanged();
}

void PerMonitorLayerManager::handleScreenRemoved(QScreen* screen) {
  // Erase before Qt deletes the QScreen so the LayerSurface/QQuickView tear down while the pointer is
  // still valid. The unique_ptr member order destroys the layer surface before the view.
  screens_by_name_.remove(screen->name());
  surfaces_.erase(screen);
  onScreenSetChanged();
}
