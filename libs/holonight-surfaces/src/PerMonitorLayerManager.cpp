#include "PerMonitorLayerManager.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickView>
#include <QScreen>

#include <holonight/wayland/layershellcontext.h>
#include <utility>

using Holonight::Wayland::LayerSurfaceHost;
using Holonight::Wayland::LayerSurfaceSpec;

PerMonitorLayerManager::PerMonitorLayerManager(const char* log_tag, QObject* parent)
    : PerMonitorLayerManager(log_tag, [] { return std::make_unique<LayerSurfaceHost>(); }, parent) {}

PerMonitorLayerManager::PerMonitorLayerManager(const char* log_tag, HostFactory host_factory, QObject* parent)
    : QObject(parent), log_tag_(log_tag), host_factory_(std::move(host_factory)) {
  // Qt 6.11+: BypassWindowManagerHint only suppresses xdg_surface when this env var is set.
  qputenv("QT_WAYLAND_USE_BYPASSWINDOWMANAGERHINT", "1");
}

PerMonitorLayerManager::~PerMonitorLayerManager() { closeAllSurfaces(); }

void PerMonitorLayerManager::decorateEngine(QQmlEngine& /*engine*/) {}
void PerMonitorLayerManager::onHostConfigured(const QString& /*monitor_name*/) {}
bool PerMonitorLayerManager::openHost(LayerSurfaceHost& host, const LayerSurfaceSpec& spec) { return host.open(spec); }

void PerMonitorLayerManager::onScreenSetChanged() {}

bool PerMonitorLayerManager::shouldCreateSurface(QScreen* /*screen*/) const { return true; }

QQuickView* PerMonitorLayerManager::viewForMonitor(const QString& monitor_name) const {
  const auto surface = surfaces_.find(monitor_name);
  return surface == surfaces_.cend() ? nullptr : surface->second.host->view();
}

void PerMonitorLayerManager::start() {
  if (started_) {
    return;
  }
  started_ = true;

  connect(qGuiApp, &QGuiApplication::screenAdded, this, &PerMonitorLayerManager::handleScreenAdded);
  connect(qGuiApp, &QGuiApplication::screenRemoved, this, &PerMonitorLayerManager::handleScreenRemoved);
  connect(Holonight::Wayland::LayerShellContext::instance(),
          &Holonight::Wayland::LayerShellContext::availabilityChanged, this, [this]() {
            if (!Holonight::Wayland::LayerShellContext::instance()->isAvailable()) {
              closeAllSurfaces();
            }
          });

  for (QScreen* screen : QGuiApplication::screens()) {
    createSurface(screen);
  }
}

LayerSurfaceSpec PerMonitorLayerManager::surfaceSpec(QScreen* screen) {
  const LayerConfig cfg = layerConfig();
  const QmlSource source = qmlSource(screen);
  LayerSurfaceSpec spec{.output = screen,
                        .name_space = cfg.namespace_name,
                        .layer = cfg.layer,
                        .qml_url = source.url,
                        .initial_properties = source.initial_properties,
                        .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint | cfg.extra_flags,
                        .color = Qt::transparent,
                        .before_load = [this](QQmlEngine* engine) { decorateEngine(*engine); }};
  configureSurface(spec, screen);
  return spec;
}

void PerMonitorLayerManager::createSurface(QScreen* screen) {
  if (!shouldCreateSurface(screen)) {
    return;
  }
  const QString output_name = screen->name();
  auto host = host_factory_();
  LayerSurfaceHost* expected_host = host.get();
  const auto isCurrent = [this, output_name, expected_host] {
    const auto current = surfaces_.find(output_name);
    return current != surfaces_.cend() && current->second.host.get() == expected_host;
  };
  connect(
      host.get(), &LayerSurfaceHost::configured, this,
      [this, output_name, isCurrent]() {
        if (isCurrent()) {
          onHostConfigured(output_name);
        }
      },
      Qt::QueuedConnection);
  connect(
      host.get(), &LayerSurfaceHost::failed, this,
      [this, output_name, expected_host, isCurrent](const QString& diagnostic) {
        if (!isCurrent()) {
          return;
        }
        qCritical("%s: surface failed for screen '%s': %s", log_tag_, qPrintable(output_name), qPrintable(diagnostic));
        removeCurrentSurface(output_name, expected_host);
      },
      Qt::QueuedConnection);
  connect(
      host.get(), &LayerSurfaceHost::closed, this,
      [this, output_name, expected_host, isCurrent]() {
        if (isCurrent()) {
          removeCurrentSurface(output_name, expected_host);
        }
      },
      Qt::QueuedConnection);
  if (!openHost(*host, surfaceSpec(screen))) {
    qCritical("%s: failed to open surface for screen '%s': %s", log_tag_, qPrintable(screen->name()),
              qPrintable(host->diagnostic()));
    host->close();
    return;
  }
  if (auto old = surfaces_.find(output_name); old != surfaces_.end()) {
    old->second.host->close();
  }
  surfaces_.insert_or_assign(output_name, MonitorSurface{.screen = screen, .host = std::move(host)});
}

void PerMonitorLayerManager::handleScreenAdded(QScreen* screen) {
  if (surfaces_.contains(screen->name())) {
    return;
  }
  createSurface(screen);
  onScreenSetChanged();
}

void PerMonitorLayerManager::handleScreenRemoved(QScreen* screen) {
  auto surface = surfaces_.find(screen->name());
  if (surface != surfaces_.end() && surface->second.screen == screen) {
    surface->second.host->close();
    surfaces_.erase(surface);
  }
  onScreenSetChanged();
}

void PerMonitorLayerManager::closeAllSurfaces() {
  auto surfaces = std::move(surfaces_);
  surfaces_.clear();
  for (auto& [name, surface] : surfaces) {
    surface.host->close();
  }
}

void PerMonitorLayerManager::removeCurrentSurface(const QString& output_name, LayerSurfaceHost* expected_host) {
  const auto surface = surfaces_.find(output_name);
  if (surface == surfaces_.end() || surface->second.host.get() != expected_host) {
    return;
  }
  surface->second.host.release()->deleteLater();
  surfaces_.erase(surface);
}
