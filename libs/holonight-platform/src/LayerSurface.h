#pragma once

#include "qwayland-wlr-layer-shell-unstable-v1.h"

#include <QObject>
#include <QPointer>
#include <QWindow>

// Wraps a zwlr_layer_surface_v1 and handles the configure → ack → resize → show sequence.
class LayerSurface : public QObject, public QtWayland::zwlr_layer_surface_v1 {
  Q_OBJECT
 public:
  explicit LayerSurface(::zwlr_layer_surface_v1* surface, ::wl_surface* wl_surface, QWindow* window,
                        QObject* parent = nullptr);
  ~LayerSurface() override;

  LayerSurface(const LayerSurface&) = delete;
  LayerSurface& operator=(const LayerSurface&) = delete;
  LayerSurface(LayerSurface&&) = delete;
  LayerSurface& operator=(LayerSurface&&) = delete;

 Q_SIGNALS:
  void closed();
  // Emitted after each compositor configure is acked (and the window mapped on first configure).
  // Lets owners apply post-map state — e.g. desktop widgets hiding a surface whose workspace is
  // already occupied, which must run after LayerSurface's first-configure auto-show.
  void configured();

 protected:
  void zwlr_layer_surface_v1_configure(uint32_t serial, uint32_t width, uint32_t height) override;
  void zwlr_layer_surface_v1_closed() override;

 private:
  ::wl_surface* wl_surface_;
  QPointer<QWindow> window_;
};
