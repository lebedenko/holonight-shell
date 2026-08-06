#include "LayerSurface.h"

#include <wayland-client.h>

LayerSurface::LayerSurface(::zwlr_layer_surface_v1* surface, ::wl_surface* wl_surface, QWindow* window, QObject* parent)
    : QObject(parent), QtWayland::zwlr_layer_surface_v1(surface), wl_surface_(wl_surface), window_(window) {}

LayerSurface::~LayerSurface() { destroy(); }

void LayerSurface::zwlr_layer_surface_v1_configure(uint32_t serial, uint32_t width, uint32_t height) {
  ack_configure(serial);
  if (window_ == nullptr) {
    return;
  }
  if (width > 0 && height > 0) {
    window_->resize(static_cast<int>(width), static_cast<int>(height));
  }
  if (!window_->isVisible()) {
    window_->show();
  }
  Q_EMIT configured();
}

void LayerSurface::zwlr_layer_surface_v1_closed() {
  Q_EMIT closed();
  if (window_ != nullptr) {
    window_->hide();
  }
}
