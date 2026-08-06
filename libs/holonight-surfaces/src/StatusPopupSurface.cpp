#include "StatusPopupSurface.h"

#include "IconImageProvider.h"
#include "QmlSourceLoader.h"
#include "ShellConstants.h"
#include "StatusPopupGeometry.h"

#include <QGuiApplication>
#include <QScreen>
#include <qpa/qplatformwindow_p.h>
#include <qscreen_platform.h>

#include <wayland-client.h>

namespace {
// Builds a transparent, WM-bypassing QQuickView on the given screen and hands back the
// associated wl_surface / wl_output. Returns nullptr (and leaves out-params untouched)
// if the platform is not Wayland or the surface is unavailable.
QQuickView* makeWaylandView(QScreen* screen, wl_surface*& wl_surface_out, wl_output*& wl_output_out) {
  auto* view = new QQuickView();
  view->setScreen(screen);
  view->setResizeMode(QQuickView::SizeRootObjectToView);
  view->setFlags(view->flags() | Qt::BypassWindowManagerHint);
  view->setColor(Qt::transparent);
  view->create();

  auto* wayland_window = view->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
  if (wayland_window == nullptr) {
    qWarning("StatusPopupSurface: not running under Wayland");
    delete view;
    return nullptr;
  }

  wl_surface* wl_surf = wayland_window->surface();
  if (wl_surf == nullptr) {
    qWarning("StatusPopupSurface: wl_surface not available");
    delete view;
    return nullptr;
  }

  wl_output* wl_out = nullptr;
  if (auto* wayland_screen = screen->nativeInterface<QNativeInterface::QWaylandScreen>()) {
    wl_out = wayland_screen->output();
  }

  wl_surface_out = wl_surf;
  wl_output_out = wl_out;
  return view;
}
}  // namespace

StatusPopupSurface::StatusPopupSurface(QObject* parent) : QObject(parent) {
  connect(&shell_, &QWaylandClientExtension::activeChanged, this, [this]() {
    if (shell_.isActive() && pending_show_) {
      pending_show_ = false;
      const bool created = ensureSurface(pending_popup_id_, pending_screen_, pending_anchor_x_, pending_anchor_width_);
      setPopupVisible(created);
    }
  });
}

StatusPopupSurface::~StatusPopupSurface() { destroySurface(); }

void StatusPopupSurface::toggle(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width) {
  if (popup_visible_ && active_popup_id_ == popup_id) {
    hide();
    return;
  }
  show(popup_id, screen_name, anchor_x, anchor_width);
}

void StatusPopupSurface::show(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width) {
  if (!shell_.isActive()) {
    pending_show_ = true;
    pending_popup_id_ = popup_id;
    pending_screen_ = screen_name;
    pending_anchor_x_ = anchor_x;
    pending_anchor_width_ = anchor_width;
    return;
  }

  // Always remap so the compositor sends a fresh configure cycle.
  destroySurface();
  const bool created = ensureSurface(popup_id, screen_name, anchor_x, anchor_width);
  setPopupVisible(created);
}

void StatusPopupSurface::hide() {
  destroySurface();
  setPopupVisible(false);
  setActivePopupId({});
}

bool StatusPopupSurface::ensureSurface(const QString& popup_id, const QString& screen_name, int anchor_x,
                                       int anchor_width) {
  QScreen* screen = QGuiApplication::primaryScreen();
  if (!screen_name.isEmpty()) {
    for (QScreen* candidate : QGuiApplication::screens()) {
      if (candidate->name() == screen_name) {
        screen = candidate;
        break;
      }
    }
  }
  if (screen == nullptr) {
    qCritical("StatusPopupSurface: no screen available");
    return false;
  }

  // Fullscreen dismiss overlay (created first so the popup stacks above it). It lives on
  // layer_top with exclusive_zone 0, so the compositor displaces it below the bar — it
  // covers all content but leaves the bar itself clickable for widget toggle / switch.
  {
    wl_surface* dismiss_wl = nullptr;
    wl_output* dismiss_out = nullptr;
    dismiss_view_ = makeWaylandView(screen, dismiss_wl, dismiss_out);
    if (dismiss_view_ == nullptr) {
      destroySurface();
      return false;
    }
    auto* dismiss_raw = shell_.get_layer_surface(dismiss_wl, dismiss_out, QtWayland::zwlr_layer_shell_v1::layer_top,
                                                 QStringLiteral("status-popup-dismiss"));
    if (dismiss_raw == nullptr) {
      qCritical("StatusPopupSurface: dismiss layer surface creation failed");
      destroySurface();
      return false;
    }
    dismiss_surface_ = new LayerSurface(dismiss_raw, dismiss_wl, dismiss_view_, this);
    dismiss_surface_->set_anchor(
        QtWayland::zwlr_layer_surface_v1::anchor_top | QtWayland::zwlr_layer_surface_v1::anchor_bottom |
        QtWayland::zwlr_layer_surface_v1::anchor_left | QtWayland::zwlr_layer_surface_v1::anchor_right);
    dismiss_surface_->set_size(0, 0);  // stretch to fill the non-exclusive area
    dismiss_surface_->set_exclusive_zone(0);
    dismiss_surface_->set_margin(0, 0, 0, 0);
    if (!loadQmlSource(dismiss_view_,
                       QUrl(QStringLiteral("qrc:/HolonightShell/Popups/Status/StatusPopupDismissOverlay.qml")),
                       "StatusPopupSurface dismiss overlay")) {
      destroySurface();
      return false;
    }
    wl_surface_commit(dismiss_wl);
  }

  // Popup surface: sized per id, anchored top|left, positioned under the triggering widget.
  wl_surface* popup_wl = nullptr;
  wl_output* popup_out = nullptr;
  view_ = makeWaylandView(screen, popup_wl, popup_out);
  if (view_ == nullptr) {
    destroySurface();
    return false;
  }
  auto* popup_raw = shell_.get_layer_surface(popup_wl, popup_out, QtWayland::zwlr_layer_shell_v1::layer_top,
                                             QStringLiteral("status-popup"));
  if (popup_raw == nullptr) {
    qCritical("StatusPopupSurface: popup layer surface creation failed");
    destroySurface();
    return false;
  }

  const StatusPopupGeometry geometry =
      statusPopupGeometry(popup_id, screen->geometry(), screen->availableGeometry(), anchor_x, anchor_width);
  setPointerX(geometry.pointer_x);

  surface_ = new LayerSurface(popup_raw, popup_wl, view_, this);
  connect(surface_, &LayerSurface::closed, this, [this]() { setPopupVisible(false); });
  surface_->set_anchor(QtWayland::zwlr_layer_surface_v1::anchor_top | QtWayland::zwlr_layer_surface_v1::anchor_left);
  surface_->set_size(geometry.surface_width, geometry.surface_height);
  surface_->set_exclusive_zone(0);
  surface_->set_margin(kStatusPopupTopGap, 0, 0, geometry.left_margin);
  // Request keyboard focus so the popup receives Esc; released when the surface is destroyed.
  surface_->set_keyboard_interactivity(QtWayland::zwlr_layer_surface_v1::keyboard_interactivity_on_demand);

  view_->setInitialProperties({{QStringLiteral("popupId"), popup_id}});
  // Stream rows in the audio popup load app icons via image://icon/ — register the provider
  // on this view's engine (the bar views register it separately in LayerShellManager).
  view_->engine()->addImageProvider(QStringLiteral("icon"), new IconImageProvider());
  if (!loadQmlSource(view_, QUrl(QStringLiteral("qrc:/HolonightShell/Popups/Status/StatusPopup.qml")),
                     "StatusPopupSurface")) {
    destroySurface();
    return false;
  }
  wl_surface_commit(popup_wl);

  setActivePopupId(popup_id);
  return true;
}

void StatusPopupSurface::destroySurface() {
  // Hide immediately, but defer deletion: hide() can be invoked from a QML signal handler
  // running inside one of these views (Esc handler, dismiss-overlay click). Deleting the
  // QQuickView synchronously from there crashes ("object destroyed while a signal handler is
  // in progress"), so tear down on the next event-loop pass via deleteLater().
  if (surface_ != nullptr) {
    surface_->deleteLater();
    surface_ = nullptr;
  }
  if (view_ != nullptr) {
    view_->hide();
    view_->deleteLater();
    view_ = nullptr;
  }
  if (dismiss_surface_ != nullptr) {
    dismiss_surface_->deleteLater();
    dismiss_surface_ = nullptr;
  }
  if (dismiss_view_ != nullptr) {
    dismiss_view_->hide();
    dismiss_view_->deleteLater();
    dismiss_view_ = nullptr;
  }
}

void StatusPopupSurface::setPopupVisible(bool visible) {
  if (popup_visible_ == visible) {
    return;
  }
  popup_visible_ = visible;
  Q_EMIT popupVisibleChanged();
}

void StatusPopupSurface::setActivePopupId(const QString& popup_id) {
  if (active_popup_id_ == popup_id) {
    return;
  }
  active_popup_id_ = popup_id;
  Q_EMIT activePopupChanged();
}

void StatusPopupSurface::setPointerX(int value) {
  if (pointer_x_ == value) {
    return;
  }
  pointer_x_ = value;
  Q_EMIT geometryChanged();
}
