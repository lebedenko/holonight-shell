#include "NotificationToastSurface.h"

#include "IconImageProvider.h"
#include "QmlSourceLoader.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>
#include <qpa/qplatformwindow_p.h>
#include <qscreen_platform.h>

#include <wayland-client.h>

namespace {
constexpr int kToastWidth = 430;
constexpr int kGlowMargin = 16;
constexpr int kSurfaceWidth = kToastWidth + (kGlowMargin * 2);
constexpr int kTopGap = 8;            // gap below the bar's reserved zone
constexpr int kRightMargin = 12;      // breathing room from the right screen edge
constexpr int kFallbackHeight = 120;  // used until the QML stack reports its content height
}  // namespace

NotificationToastSurface::NotificationToastSurface(QObject* parent) : QObject(parent) {
  connect(&shell_, &QWaylandClientExtension::activeChanged, this, [this]() {
    if (shell_.isActive() && pending_show_) {
      pending_show_ = false;
      createSurface(pending_screen_);
    }
  });
}

NotificationToastSurface::~NotificationToastSurface() { destroySurface(); }

void NotificationToastSurface::ensureSurface(const QString& screen_name) {
  if (view_ != nullptr && current_screen_ == screen_name) {
    return;  // already showing on this monitor
  }
  if (view_ != nullptr) {
    destroySurface();  // monitor changed — rebuild
  }
  if (!shell_.isActive()) {
    pending_show_ = true;
    pending_screen_ = screen_name;
    return;
  }
  createSurface(screen_name);
}

bool NotificationToastSurface::createSurface(const QString& screen_name) {
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
    qCritical("NotificationToastSurface: no screen available");
    return false;
  }

  view_ = new QQuickView();
  view_->setScreen(screen);
  view_->setResizeMode(QQuickView::SizeRootObjectToView);
  view_->setFlags(view_->flags() | Qt::BypassWindowManagerHint);
  view_->setColor(Qt::transparent);
  view_->create();

  auto* wayland_window = view_->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
  if (wayland_window == nullptr) {
    qCritical("NotificationToastSurface: not running under Wayland");
    destroySurface();
    return false;
  }

  wl_surface* wl_surface = wayland_window->surface();
  if (wl_surface == nullptr) {
    qCritical("NotificationToastSurface: wl_surface not available");
    destroySurface();
    return false;
  }

  wl_output* wl_output = nullptr;
  if (auto* wayland_screen = screen->nativeInterface<QNativeInterface::QWaylandScreen>()) {
    wl_output = wayland_screen->output();
  }

  auto* raw = shell_.get_layer_surface(wl_surface, wl_output, QtWayland::zwlr_layer_shell_v1::layer_overlay,
                                       QStringLiteral("notifications"));
  if (raw == nullptr) {
    qCritical("NotificationToastSurface: layer surface creation failed");
    destroySurface();
    return false;
  }

  wl_surface_ = wl_surface;
  surface_ = new LayerSurface(raw, wl_surface, view_, this);
  surface_->set_anchor(QtWayland::zwlr_layer_surface_v1::anchor_top | QtWayland::zwlr_layer_surface_v1::anchor_right);
  surface_->set_size(kSurfaceWidth, kFallbackHeight);
  surface_->set_exclusive_zone(0);
  surface_->set_margin(kTopGap, kRightMargin, 0, 0);

  view_->setInitialProperties({{QStringLiteral("monitorName"), screen_name}});
  // Toast app icons load via image://icon/ — register the provider on this view's engine.
  view_->engine()->addImageProvider(QStringLiteral("icon"), new IconImageProvider());
  if (!loadQmlSource(view_, QUrl(QStringLiteral("qrc:/HolonightShell/Notifications/ToastStack.qml")),
                     "NotificationToastSurface")) {
    destroySurface();
    return false;
  }

  if (QQuickItem* root = view_->rootObject()) {
    connect(root, SIGNAL(contentHeightChanged()), this, SLOT(updateSurfaceSize()));  // NOLINT
  }

  current_screen_ = screen_name;
  updateSurfaceSize();
  wl_surface_commit(wl_surface);
  return true;
}

void NotificationToastSurface::updateSurfaceSize() {
  if (surface_ == nullptr || view_ == nullptr || wl_surface_ == nullptr) {
    return;
  }
  int height = kFallbackHeight;
  if (QQuickItem* root = view_->rootObject()) {
    const int reported = root->property("contentHeight").toInt();
    if (reported > 0) {
      height = reported;
    }
  }
  surface_->set_size(kSurfaceWidth, height);
  wl_surface_commit(wl_surface_);
}

void NotificationToastSurface::destroySurface() {
  delete surface_;
  surface_ = nullptr;
  delete view_;
  view_ = nullptr;
  wl_surface_ = nullptr;
  current_screen_.clear();
}
