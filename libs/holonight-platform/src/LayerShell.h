#pragma once

#include "qwayland-wlr-layer-shell-unstable-v1.h"

#include <QtWaylandClient/QWaylandClientExtensionTemplate>

class LayerShell : public QWaylandClientExtensionTemplate<LayerShell>, public QtWayland::zwlr_layer_shell_v1 {
 public:
  LayerShell() : QWaylandClientExtensionTemplate(4) {}
};
