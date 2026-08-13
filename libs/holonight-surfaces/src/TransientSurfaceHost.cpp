#include "TransientSurfaceHost.h"

#include <QGuiApplication>
#include <QQuickView>
#include <QScreen>

#include <holonight/wayland/layershellcontext.h>
#include <utility>

using Holonight::Wayland::LayerShellContext;
using Holonight::Wayland::LayerSurfaceHost;
using Holonight::Wayland::LayerSurfaceSpec;

TransientSurfaceHost::TransientSurfaceHost(const char* log_tag, QObject* parent)
    : TransientSurfaceHost(log_tag, [] { return std::make_unique<LayerSurfaceHost>(); }, parent) {}

TransientSurfaceHost::TransientSurfaceHost(const char* log_tag, HostFactory host_factory, QObject* parent)
    : QObject(parent), log_tag_(log_tag), host_factory_(std::move(host_factory)) {
  qputenv("QT_WAYLAND_USE_BYPASSWINDOWMANAGERHINT", "1");
  connect(LayerShellContext::instance(), &LayerShellContext::availabilityChanged, this,
          &TransientSurfaceHost::handleAvailabilityChanged);
  connect(qGuiApp, &QGuiApplication::screenRemoved, this, &TransientSurfaceHost::handleScreenRemoved);
}

TransientSurfaceHost::~TransientSurfaceHost() { closeSurface(); }

QQuickView* TransientSurfaceHost::view() const { return host_ != nullptr ? host_->view() : nullptr; }
QObject* TransientSurfaceHost::rootObject() const { return host_ != nullptr ? host_->rootObject() : nullptr; }

bool TransientSurfaceHost::openHost(LayerSurfaceHost& host, const LayerSurfaceSpec& spec) { return host.open(spec); }
bool TransientSurfaceHost::providerAvailable() const { return LayerShellContext::instance()->isAvailable(); }
void TransientSurfaceHost::onSurfaceConfigured() {}
void TransientSurfaceHost::onSurfaceTerminated() {}

bool TransientSurfaceHost::openSurface(const LayerSurfaceSpec& spec) {
  pending_spec_ = spec;
  requested_output_ = spec.output;
  pending_open_ = true;
  closeSurface();
  if (!providerAvailable()) {
    return false;
  }
  openPendingSurface();
  return host_ != nullptr;
}

void TransientSurfaceHost::openPendingSurface() {
  if (!pending_open_ || requested_output_.isNull()) {
    return;
  }
  auto next_host = host_factory_();
  LayerSurfaceHost* expected_host = next_host.get();
  const auto is_current = [this, expected_host] { return host_.get() == expected_host; };
  connect(
      next_host.get(), &LayerSurfaceHost::configured, this,
      [this, is_current]() {
        if (is_current()) {
          onSurfaceConfigured();
        }
      },
      Qt::QueuedConnection);
  connect(
      next_host.get(), &LayerSurfaceHost::failed, this,
      [this, expected_host, is_current](const QString& diagnostic) {
        if (!is_current()) {
          return;
        }
        qCritical("%s: surface failed: %s", log_tag_, qPrintable(diagnostic));
        removeCurrentHost(expected_host);
      },
      Qt::QueuedConnection);
  connect(
      next_host.get(), &LayerSurfaceHost::closed, this,
      [this, expected_host, is_current]() {
        if (is_current()) {
          removeCurrentHost(expected_host);
        }
      },
      Qt::QueuedConnection);
  pending_open_ = false;
  if (!openHost(*next_host, pending_spec_)) {
    qCritical("%s: failed to open surface: %s", log_tag_, qPrintable(next_host->diagnostic()));
    next_host->close();
    onSurfaceTerminated();
    return;
  }
  host_ = std::move(next_host);
}

void TransientSurfaceHost::closeSurface() {
  if (host_ == nullptr) {
    return;
  }
  LayerSurfaceHost* old = host_.release();
  old->close();
  old->deleteLater();
}

void TransientSurfaceHost::clearPendingSurface() {
  pending_open_ = false;
  requested_output_.clear();
}

void TransientSurfaceHost::removeCurrentHost(LayerSurfaceHost* expected_host) {
  if (host_.get() != expected_host) {
    return;
  }
  host_.release()->deleteLater();
  onSurfaceTerminated();
}

void TransientSurfaceHost::handleAvailabilityChanged() {
  if (!providerAvailable()) {
    if (host_ != nullptr) {
      pending_open_ = !requested_output_.isNull();
    }
    closeSurface();
    onSurfaceTerminated();
    return;
  }
  openPendingSurface();
}

void TransientSurfaceHost::handleScreenRemoved(QScreen* screen) {
  if (requested_output_ != screen) {
    return;
  }
  clearPendingSurface();
  closeSurface();
  onSurfaceTerminated();
}
