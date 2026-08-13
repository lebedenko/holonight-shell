#include "PairedTransientSurfaceHost.h"

#include <QGuiApplication>
#include <QQuickView>
#include <QScreen>

#include <holonight/wayland/layershellcontext.h>
#include <utility>

using namespace Holonight::Wayland;

PairedTransientSurfaceHost::PairedTransientSurfaceHost(const char* log_tag, QObject* parent)
    : PairedTransientSurfaceHost(log_tag, [] { return std::make_unique<LayerSurfaceHost>(); }, parent) {}

PairedTransientSurfaceHost::PairedTransientSurfaceHost(const char* log_tag, HostFactory host_factory, QObject* parent)
    : QObject(parent), log_tag_(log_tag), host_factory_(std::move(host_factory)) {
  qputenv("QT_WAYLAND_USE_BYPASSWINDOWMANAGERHINT", "1");
  connect(LayerShellContext::instance(), &LayerShellContext::availabilityChanged, this,
          &PairedTransientSurfaceHost::providerAvailabilityChanged);
  connect(qGuiApp, &QGuiApplication::screenRemoved, this, &PairedTransientSurfaceHost::outputRemoved);
}

PairedTransientSurfaceHost::~PairedTransientSurfaceHost() { closePair(); }

QQuickView* PairedTransientSurfaceHost::contentView() const {
  return content_host_ != nullptr ? content_host_->view() : nullptr;
}

QObject* PairedTransientSurfaceHost::contentRootObject() const {
  return content_host_ != nullptr ? content_host_->rootObject() : nullptr;
}

bool PairedTransientSurfaceHost::openHost(LayerSurfaceHost& host, const LayerSurfaceSpec& spec) {
  return host.open(spec);
}

bool PairedTransientSurfaceHost::providerAvailable() const { return LayerShellContext::instance()->isAvailable(); }
void PairedTransientSurfaceHost::onPairOpened() {}
void PairedTransientSurfaceHost::onPairConfigured() {}
void PairedTransientSurfaceHost::onPairTerminated() {}

bool PairedTransientSurfaceHost::openPair(const PairedLayerSurfaceSpec& spec) {
  pending_spec_ = spec;
  requested_output_ = spec.content.output;
  pending_open_ = true;
  closePair();
  if (!providerAvailable()) {
    return false;
  }
  openPendingPair();
  return hasPair();
}

void PairedTransientSurfaceHost::openPendingPair() {
  if (!pending_open_ || requested_output_.isNull()) {
    return;
  }

  const quint64 next_generation = ++generation_;
  auto next_dismiss = host_factory_();
  connectTerminalSignals(*next_dismiss, next_generation);
  if (!openHost(*next_dismiss, pending_spec_.dismiss)) {
    qCritical("%s: failed to open dismiss surface: %s", log_tag_, qPrintable(next_dismiss->diagnostic()));
    next_dismiss->close();
    pending_open_ = false;
    onPairTerminated();
    return;
  }
  dismiss_host_ = std::move(next_dismiss);

  auto next_content = host_factory_();
  connectTerminalSignals(*next_content, next_generation);
  if (!openHost(*next_content, pending_spec_.content)) {
    qCritical("%s: failed to open content surface: %s", log_tag_, qPrintable(next_content->diagnostic()));
    next_content->close();
    pending_open_ = false;
    closePair();
    onPairTerminated();
    return;
  }
  content_host_ = std::move(next_content);
  pending_open_ = false;
  onPairOpened();
}

void PairedTransientSurfaceHost::connectTerminalSignals(LayerSurfaceHost& host, quint64 generation) {
  connect(
      &host, &LayerSurfaceHost::configured, this,
      [this, generation]() {
        if (generation == generation_ && hasPair()) {
          onPairConfigured();
        }
      },
      Qt::QueuedConnection);
  connect(
      &host, &LayerSurfaceHost::failed, this,
      [this, generation](const QString& diagnostic) { terminateGeneration(generation, diagnostic); },
      Qt::QueuedConnection);
  connect(
      &host, &LayerSurfaceHost::closed, this, [this, generation]() { terminateGeneration(generation); },
      Qt::QueuedConnection);
}

void PairedTransientSurfaceHost::terminateGeneration(quint64 generation, const QString& diagnostic) {
  if (generation != generation_ || !hasPair()) {
    return;
  }
  if (!diagnostic.isEmpty()) {
    qCritical("%s: paired surface failed: %s", log_tag_, qPrintable(diagnostic));
  }
  pending_open_ = false;
  closePair();
  onPairTerminated();
}

void PairedTransientSurfaceHost::closePair() {
  ++generation_;
  auto close = [](std::unique_ptr<LayerSurfaceHost>& host) {
    if (host == nullptr) {
      return;
    }
    LayerSurfaceHost* old = host.release();
    old->close();
    old->deleteLater();
  };
  close(content_host_);
  close(dismiss_host_);
}

void PairedTransientSurfaceHost::clearPendingPair() {
  pending_open_ = false;
  requested_output_.clear();
}

void PairedTransientSurfaceHost::providerAvailabilityChanged() {
  if (!providerAvailable()) {
    if (hasPair()) {
      pending_open_ = !requested_output_.isNull();
    }
    closePair();
    onPairTerminated();
    return;
  }
  openPendingPair();
}

void PairedTransientSurfaceHost::outputRemoved(QScreen* screen) {
  if (requested_output_ != screen) {
    return;
  }
  clearPendingPair();
  closePair();
  onPairTerminated();
}
