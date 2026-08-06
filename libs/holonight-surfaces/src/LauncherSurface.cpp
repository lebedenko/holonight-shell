#include "LauncherSurface.h"

#include "IconImageProvider.h"
#include "QmlSourceLoader.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QScreen>
#include <qpa/qplatformwindow_p.h>
#include <qscreen_platform.h>

#include <wayland-client.h>

Q_LOGGING_CATEGORY(lcLauncher, "holonight.launcher")

namespace {
QQuickView* makeLauncherView(QScreen* screen, wl_surface*& wl_surface_out, wl_output*& wl_output_out) {
  auto* view = new QQuickView();
  view->setScreen(screen);
  view->setResizeMode(QQuickView::SizeRootObjectToView);
  view->setFlags(view->flags() | Qt::BypassWindowManagerHint);
  view->setColor(Qt::transparent);
  view->create();

  auto* wayland_window = view->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
  if (wayland_window == nullptr) {
    qWarning("LauncherSurface: not running under Wayland");
    delete view;
    return nullptr;
  }

  wl_surface* wl_surf = wayland_window->surface();
  if (wl_surf == nullptr) {
    qWarning("LauncherSurface: wl_surface not available");
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

LauncherSurface::LauncherSurface(QObject* parent) : QObject(parent) {
  connect(&shell_, &QWaylandClientExtension::activeChanged, this,
          [this]() { executeCommand(lifecycle_.shellActivated(shell_.isActive())); });
}

LauncherSurface::~LauncherSurface() { destroySurface(); }

void LauncherSurface::toggle(const QString& screen_name) {
  qCInfo(lcLauncher) << "toggle" << screen_name << "visible" << visible_ << "closing" << lifecycle_.closing();
  executeCommand(lifecycle_.toggle(screen_name, shell_.isActive(), view_ != nullptr));
}

void LauncherSurface::show(const QString& screen_name) {
  qCInfo(lcLauncher) << "show requested" << screen_name << "shell active" << shell_.isActive() << "visible" << visible_
                     << "closing" << lifecycle_.closing() << "view" << view_;
  executeCommand(lifecycle_.show(screen_name, shell_.isActive()));
}

void LauncherSurface::hide() {
  qCInfo(lcLauncher) << "hide requested"
                     << "view" << view_ << "visible" << visible_ << "closing" << lifecycle_.closing();
  executeCommand(lifecycle_.hide(view_ != nullptr));
}

void LauncherSurface::notifyHideReady() {
  qCInfo(lcLauncher) << "notifyHideReady"
                     << "view" << view_ << "wl_surface" << wl_surface_ << "visible" << visible_ << "closing"
                     << lifecycle_.closing();
  executeCommand(lifecycle_.notifyHideReady(view_ != nullptr));
}

bool LauncherSurface::ensureSurface(const QString& screen_name) {
  qCInfo(lcLauncher) << "ensureSurface" << screen_name;
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
    qCritical("LauncherSurface: no screen available");
    return false;
  }

  wl_output* launcher_out = nullptr;
  view_ = makeLauncherView(screen, wl_surface_, launcher_out);
  if (view_ == nullptr) {
    return false;
  }

  auto* raw_surface = shell_.get_layer_surface(wl_surface_, launcher_out, QtWayland::zwlr_layer_shell_v1::layer_top,
                                               QStringLiteral("launcher"));
  if (raw_surface == nullptr) {
    qCritical("LauncherSurface: layer surface creation failed");
    destroySurface();
    return false;
  }

  surface_ = new LayerSurface(raw_surface, wl_surface_, view_, this);
  connect(surface_, &LayerSurface::closed, this, [this]() { executeCommand(lifecycle_.surfaceClosed()); });

  surface_->set_anchor(QtWayland::zwlr_layer_surface_v1::anchor_top | QtWayland::zwlr_layer_surface_v1::anchor_bottom |
                       QtWayland::zwlr_layer_surface_v1::anchor_left | QtWayland::zwlr_layer_surface_v1::anchor_right);
  surface_->set_size(0, 0);
  surface_->set_exclusive_zone(0);
  surface_->set_keyboard_interactivity(QtWayland::zwlr_layer_surface_v1::keyboard_interactivity_exclusive);

  view_->engine()->addImageProvider(QStringLiteral("icon"), new IconImageProvider());
  if (!loadQmlSource(view_, QUrl(QStringLiteral("qrc:/HolonightShell/Launcher/Launcher.qml")), "LauncherSurface")) {
    destroySurface();
    return false;
  }

  wl_surface_commit(wl_surface_);
  return true;
}

void LauncherSurface::destroySurface() {
  qCInfo(lcLauncher) << "destroySurface"
                     << "view" << view_ << "surface" << surface_ << "wl_surface" << wl_surface_;
  wl_surface_ = nullptr;
  if (surface_ != nullptr) {
    surface_->deleteLater();
    surface_ = nullptr;
  }
  if (view_ != nullptr) {
    view_->hide();
    view_->deleteLater();
    view_ = nullptr;
  }
}

void LauncherSurface::executeCommand(const LauncherSurfaceCommand& command) {
  switch (command.type) {
    case LauncherSurfaceCommandType::None:
      break;
    case LauncherSurfaceCommandType::CreateSurface: {
      destroySurface();
      executeCommand(lifecycle_.surfaceCreated(ensureSurface(command.screen_name)));
      break;
    }
    case LauncherSurfaceCommandType::DestroySurface:
      destroySurface();
      break;
    case LauncherSurfaceCommandType::StartCloseAnimation:
      if (QQuickItem* root = view_ != nullptr ? view_->rootObject() : nullptr) {
        QMetaObject::invokeMethod(root, "startClose");
      }
      break;
  }

  if (command.visible.has_value()) {
    setVisible(*command.visible);
  }
}

void LauncherSurface::setVisible(bool visible) {
  if (visible_ == visible) {
    return;
  }
  visible_ = visible;
  emit visibleChanged();
}
