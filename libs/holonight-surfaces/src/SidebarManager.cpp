#include "SidebarManager.h"

#include "IconImageProvider.h"
#include "LayerSurface.h"
#include "QmlSourceLoader.h"
#include "SidebarSurfacePolicy.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>
#include <qpa/qplatformwindow_p.h>
#include <qscreen_platform.h>

#include <wayland-client.h>

Q_LOGGING_CATEGORY(lcSidebar, "holonight.sidebar")

namespace {

QQuickView* makeSidebarView(QScreen* screen, wl_surface*& wl_surface_out, wl_output*& wl_output_out) {
  auto* view = new QQuickView();
  view->setScreen(screen);
  view->setResizeMode(QQuickView::SizeRootObjectToView);
  view->setFlags(view->flags() | Qt::BypassWindowManagerHint);
  view->setColor(Qt::transparent);
  view->create();

  auto* wayland_window = view->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
  if (wayland_window == nullptr) {
    qCWarning(lcSidebar, "not running under Wayland");
    delete view;
    return nullptr;
  }

  wl_surface* wl_surface = wayland_window->surface();
  if (wl_surface == nullptr) {
    qCWarning(lcSidebar, "wl_surface not available");
    delete view;
    return nullptr;
  }

  wl_output* wl_output = nullptr;
  if (auto* wayland_screen = screen->nativeInterface<QNativeInterface::QWaylandScreen>()) {
    wl_output = wayland_screen->output();
  }

  wl_surface_out = wl_surface;
  wl_output_out = wl_output;
  return view;
}

}  // namespace

SidebarManager::SidebarManager(LayerShell& shell, QObject* parent) : QObject(parent), shell_(shell) {
  // Qt 6.11+: BypassWindowManagerHint only suppresses xdg_surface when this env var is set.
  qputenv("QT_WAYLAND_USE_BYPASSWINDOWMANAGERHINT", "1");
}

SidebarManager::~SidebarManager() {
  const QList<QString> keys = surfaces_.keys();
  for (const QString& key : keys) {
    destroySurface(key);
  }
}

void SidebarManager::start() {
  if (started_) {
    return;
  }
  started_ = true;

  connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen* screen) {
    if (screen == nullptr) {
      return;
    }
    const QString monitor_name = screen->name();
    destroySurface(monitor_name);
    open_state_.remove(monitor_name);
    stored_heights_.remove(monitor_name);
    current_tabs_.remove(monitor_name);
  });
}

bool SidebarManager::isOpen(const QString& monitor_name) const { return open_state_.value(monitor_name, false); }

bool SidebarManager::isKnownMonitor(const QString& monitor_name) {
  // An empty name must never match: some QPA platforms (e.g. offscreen, used by the test
  // harness) expose a QScreen with an empty name(), which would otherwise let an empty/malformed
  // control-socket command slip through as "known".
  return !monitor_name.isEmpty() && findScreen(monitor_name) != nullptr;
}

void SidebarManager::toggle(const QString& monitor_name) {
  qCInfo(lcSidebar) << "toggle" << monitor_name << "open" << open_state_.value(monitor_name, false);
  if (!isKnownMonitor(monitor_name)) {
    qCWarning(lcSidebar) << "toggle: rejecting unknown monitor" << monitor_name;
    return;
  }
  if (open_state_.value(monitor_name, false)) {
    closeOnMonitor(monitor_name);
    return;
  }
  closeAll();
  openOnMonitor(monitor_name);
}

void SidebarManager::close(const QString& monitor_name) {
  qCInfo(lcSidebar) << "close" << monitor_name << "open" << open_state_.value(monitor_name, false);
  if (!open_state_.value(monitor_name, false)) {
    return;
  }
  closeOnMonitor(monitor_name);
}

void SidebarManager::closeAll() {
  const QList<QString> keys = open_state_.keys();
  for (const QString& key : keys) {
    if (open_state_.value(key, false)) {
      closeOnMonitor(key);
    }
  }
}

void SidebarManager::onClosingAnimationFinished(const QString& monitor_name) {
  qCInfo(lcSidebar) << "closing animation finished" << monitor_name << "open" << open_state_.value(monitor_name, false);
  if (open_state_.value(monitor_name, false)) {
    return;
  }
  destroySurface(monitor_name);
}

void SidebarManager::onContentHeightChanged(const QString& monitor_name, int height) {
  const int clamped = boundedHeight(monitor_name, height);
  stored_heights_.insert(monitor_name, clamped);

  QQuickView* view = findView(monitor_name);
  if (view != nullptr) {
    if (QQuickItem* root = view->rootObject()) {
      root->setProperty("panelHeight", clamped);
    }
  }
}

void SidebarManager::onCurrentTabChanged(const QString& monitor_name, int tab_index) {
  current_tabs_.insert(monitor_name, tab_index);
  Q_EMIT currentTabChanged(monitor_name, tab_index);
}

int SidebarManager::currentTabForMonitor(const QString& monitor_name) const {
  return current_tabs_.value(monitor_name, 0);
}

void SidebarManager::openOnMonitor(const QString& monitor_name) {
  qCInfo(lcSidebar) << "openOnMonitor" << monitor_name;
  if (!createSurface(monitor_name)) {
    return;
  }

  QQuickView* view = findView(monitor_name);
  if (view == nullptr) {
    return;
  }
  QQuickItem* root = view->rootObject();
  if (root == nullptr) {
    return;
  }

  open_state_.insert(monitor_name, true);

  const int stored_height = stored_heights_.value(monitor_name, 0);
  root->setProperty("panelHeight",
                    boundedHeight(monitor_name, stored_height > 0 ? stored_height : sidebarDefaultHeight()));
  root->setProperty("currentTab", currentTabForMonitor(monitor_name));
  root->setVisible(true);
  root->setProperty("active", true);
  root->forceActiveFocus();

  Q_EMIT sidebarOpened(monitor_name);
}

void SidebarManager::closeOnMonitor(const QString& monitor_name) {
  qCInfo(lcSidebar) << "closeOnMonitor" << monitor_name;
  open_state_.insert(monitor_name, false);

  QQuickView* view = findView(monitor_name);
  if (view == nullptr) {
    destroySurface(monitor_name);
    return;
  }
  if (QQuickItem* root = view->rootObject()) {
    root->setProperty("active", false);
  }

  Q_EMIT sidebarClosed(monitor_name);
}

bool SidebarManager::createSurface(const QString& monitor_name) {
  if (surfaces_.contains(monitor_name)) {
    return true;
  }

  QScreen* screen = findScreen(monitor_name);
  if (screen == nullptr) {
    qCWarning(lcSidebar) << "createSurface: no screen for monitor" << monitor_name;
    return false;
  }

  wl_surface* wl_surface = nullptr;
  wl_output* wl_output = nullptr;
  QQuickView* view = makeSidebarView(screen, wl_surface, wl_output);
  if (view == nullptr) {
    return false;
  }

  auto* raw_surface = shell_.get_layer_surface(wl_surface, wl_output, QtWayland::zwlr_layer_shell_v1::layer_top,
                                               QStringLiteral("sidebar"));
  if (raw_surface == nullptr) {
    qCWarning(lcSidebar) << "createSurface: layer surface creation failed for" << monitor_name;
    delete view;
    return false;
  }

  auto* layer_surface = new LayerSurface(raw_surface, wl_surface, view, this);
  layer_surface->set_anchor(
      QtWayland::zwlr_layer_surface_v1::anchor_top | QtWayland::zwlr_layer_surface_v1::anchor_bottom |
      QtWayland::zwlr_layer_surface_v1::anchor_left | QtWayland::zwlr_layer_surface_v1::anchor_right);
  layer_surface->set_size(0, 0);
  layer_surface->set_exclusive_zone(0);
  layer_surface->set_margin(0, 0, 0, 0);
  layer_surface->set_keyboard_interactivity(QtWayland::zwlr_layer_surface_v1::keyboard_interactivity_exclusive);

  configureEngine(*view->engine());
  const int stored_height = stored_heights_.value(monitor_name, 0);
  view->setInitialProperties({
      {QStringLiteral("barMonitorName"), monitor_name},
      {QStringLiteral("active"), false},
      {QStringLiteral("currentTab"), currentTabForMonitor(monitor_name)},
      {QStringLiteral("panelHeight"),
       boundedHeight(monitor_name, stored_height > 0 ? stored_height : sidebarDefaultHeight())},
  });
  if (!loadQmlSource(view, QUrl(QStringLiteral("qrc:/HolonightShell/RightSidebar/RightSidebar.qml")),
                     "SidebarManager")) {
    delete layer_surface;
    delete view;
    return false;
  }

  surfaces_.insert(monitor_name, SidebarSurface{.view = view, .surface = layer_surface, .wl_surface_ptr = wl_surface});
  wl_surface_commit(wl_surface);
  return true;
}

void SidebarManager::destroySurface(const QString& monitor_name) {
  auto iter = surfaces_.find(monitor_name);
  if (iter == surfaces_.end()) {
    return;
  }

  SidebarSurface surface = iter.value();
  surfaces_.erase(iter);

  if (surface.surface != nullptr) {
    surface.surface->deleteLater();
  }
  if (surface.view != nullptr) {
    surface.view->hide();
    surface.view->deleteLater();
  }
}

void SidebarManager::configureEngine(QQmlEngine& engine) {
  engine.addImageProvider(QStringLiteral("icon"), new IconImageProvider());
}

int SidebarManager::boundedHeight(const QString& monitor_name, int requested_height) {
  const QScreen* screen = findScreen(monitor_name);
  return boundedSidebarHeight(requested_height, screen == nullptr ? 0 : screen->geometry().height());
}

QScreen* SidebarManager::findScreen(const QString& monitor_name) {
  for (QScreen* screen : QGuiApplication::screens()) {
    if (screen->name() == monitor_name) {
      return screen;
    }
  }
  return nullptr;
}

QQuickView* SidebarManager::findView(const QString& monitor_name) const {
  const auto iter = surfaces_.constFind(monitor_name);
  if (iter == surfaces_.constEnd()) {
    return nullptr;
  }
  return iter->view;
}
