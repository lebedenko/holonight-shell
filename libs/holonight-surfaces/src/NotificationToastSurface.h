#pragma once

#include "TransientSurfaceHost.h"

#include <QObject>
#include <QString>

class QScreen;

// Layer-shell surface manager for toast notifications on a single monitor. Modeled on
// TooltipSurface / StatusPopupSurface: owns one QQuickView whose root is ToastStack.qml, placed
// at layer_overlay (above everything), anchored to the output's top-right corner with
// exclusive_zone = 0 so it never pushes other panels.
//
// The surface is sized to the QML root's contentHeight and resized whenever the visible toast
// set changes, so only the toasts (plus a small glow margin) capture pointer input — the rest of
// the screen edge stays click-through. One instance per active monitor; owned by
// NotificationManager, which creates it on the first notification and destroys it when the
// monitor's visible set and queue both empty.
class NotificationToastSurface : public TransientSurfaceHost {
  Q_OBJECT

 public:
  explicit NotificationToastSurface(QObject* parent = nullptr);
  ~NotificationToastSurface() override;

  NotificationToastSurface(const NotificationToastSurface&) = delete;
  NotificationToastSurface& operator=(const NotificationToastSurface&) = delete;
  NotificationToastSurface(NotificationToastSurface&&) = delete;
  NotificationToastSurface& operator=(NotificationToastSurface&&) = delete;

  // Creates (or keeps) the toast surface for the given output. Deferred until the layer-shell
  // global is bound if it is not yet active. Safe to call repeatedly for the same monitor.
  void ensureSurface(const QString& screen_name);
  void destroySurface();

  [[nodiscard]] bool isActive() const { return hasSurface(); }
  [[nodiscard]] static Holonight::Wayland::LayerSurfaceSpec surfaceSpec(QScreen* screen, const QString& screen_name);

 private Q_SLOTS:
  // Resizes the layer surface to the current QML contentHeight (toasts + glow margin).
  void updateSurfaceSize();

 private:
  bool createSurface(const QString& screen_name);
  void onSurfaceConfigured() override;
  void onSurfaceTerminated() override;

  QString current_screen_;
  bool pending_show_ = false;
  QString pending_screen_;
};
