#include "TrayMenuSurface.h"

#include "IconImageProvider.h"
#include "QmlSourceLoader.h"
#include "ShellConstants.h"

#include <QGuiApplication>
#include <QQuickItem>
#include <QScreen>
#include <QTimer>
#include <qpa/qplatformwindow_p.h>
#include <qscreen_platform.h>

#include <wayland-client.h>

static constexpr int kMenuGap = 2;
static constexpr int kMenuWidth = 244;
static constexpr int kSubmenuGap = 6;
static constexpr int kMenuMaxColumns = 2;
static constexpr int kMenuMinHeight = 40;
static constexpr int kMenuMaxHeight = 480;
static constexpr int kShadowPadding = 12;

namespace {
QScreen* screenForMenuRequest(const QString& screen_name, int screen_x, int screen_y) {
  if (!screen_name.isEmpty()) {
    for (QScreen* candidate : QGuiApplication::screens()) {
      if (candidate->name() == screen_name) {
        return candidate;
      }
    }
  }

  for (QScreen* candidate : QGuiApplication::screens()) {
    if (candidate->geometry().contains(screen_x, screen_y)) {
      return candidate;
    }
  }
  return QGuiApplication::primaryScreen();
}

TrayMenuSurface::MenuPlacement menuPlacement(const QRect& screen_geometry, int screen_x, int screen_y) {
  const int screen_local_request_x = screen_x - screen_geometry.x();
  const int available_width = qMax(kMenuWidth, screen_geometry.width() - (kScreenEdgeMargin * 2));
  const int top_level_x = qBound(kScreenEdgeMargin, screen_local_request_x,
                                 qMax(kScreenEdgeMargin, screen_geometry.width() - kMenuWidth - kScreenEdgeMargin));
  const bool can_fit_second_column = available_width >= (kMenuWidth * kMenuMaxColumns) + kSubmenuGap;
  const bool can_open_right = can_fit_second_column && top_level_x + (kMenuWidth * 2) + kSubmenuGap <=
                                                           screen_geometry.width() - kScreenEdgeMargin;
  const bool can_open_left = can_fit_second_column && top_level_x - kMenuWidth - kSubmenuGap >= kScreenEdgeMargin;

  TrayMenuSurface::MenuPlacement placement;
  placement.column_count = (can_open_right || can_open_left) ? kMenuMaxColumns : 1;
  placement.width = (placement.column_count * kMenuWidth) + ((placement.column_count - 1) * kSubmenuGap);
  placement.top_level_column = can_open_left && !can_open_right ? 1 : 0;
  placement.column_step = placement.top_level_column == 1 ? -1 : 1;

  const int requested_surface_x =
      placement.top_level_column == 1 ? top_level_x - kMenuWidth - kSubmenuGap : top_level_x;
  placement.x = qBound(kScreenEdgeMargin, requested_surface_x,
                       qMax(kScreenEdgeMargin, screen_geometry.width() - placement.width - kScreenEdgeMargin));
  const int requested_y = screen_y - screen_geometry.y() + kMenuGap;
  placement.y = qBound(0, requested_y, qMax(0, screen_geometry.height() - kMenuMinHeight));
  const int available_height = qMax(kMenuMinHeight, screen_geometry.height() - placement.y - kScreenEdgeMargin);
  placement.height = qMin(kMenuMaxHeight, available_height);
  return placement;
}

int calculateMenuHeight(DbusMenuModel* model) {
  if (model == nullptr) {
    return kMenuMinHeight;
  }
  int height = 16;
  int visible_items = 0;
  for (int i = 0; i < model->rowCount(); ++i) {
    QModelIndex idx = model->index(i);
    bool visible = model->data(idx, DbusMenuModel::VisibleRole).toBool();
    if (!visible) {
      continue;
    }
    visible_items++;
    QString type = model->data(idx, DbusMenuModel::TypeRole).toString();
    if (type == QLatin1String("separator")) {
      height += 10;
    } else {
      height += 30;
    }
    height += 1;
  }
  if (visible_items > 0) {
    height -= 1;
  }
  return qBound(kMenuMinHeight, height, kMenuMaxHeight);
}
}  // namespace

TrayMenuSurface::TrayMenuSurface(QObject* parent) : QObject(parent) {
  connect(&shell_, &QWaylandClientExtension::activeChanged, this, [this]() {
    if (shell_.isActive() && pending_show_) {
      pending_show_ = false;
      active_client_ = pending_client_;
      const bool created = ensureSurface(pending_screen_, pending_x_, pending_y_, pending_model_);
      pending_model_ = nullptr;
      pending_client_ = nullptr;
      setMenuVisible(created);
    }
  });
}

TrayMenuSurface::~TrayMenuSurface() { destroySurface(); }

void TrayMenuSurface::show(const QString& screen_name, int screen_x, int screen_y, DbusMenuModel* model,
                           DbusMenuClient* client) {
  destroySurface();
  if (model != nullptr) {
    model->setParent(this);
  }

  if (!shell_.isActive()) {
    pending_show_ = true;
    pending_screen_ = screen_name;
    pending_x_ = screen_x;
    pending_y_ = screen_y;
    pending_model_ = model;
    pending_client_ = client;
    return;
  }

  active_client_ = client;
  const bool created = ensureSurface(screen_name, screen_x, screen_y, model);
  setMenuVisible(created);
}

void TrayMenuSurface::hide() {
  if (view_ != nullptr) {
    view_->hide();
  }
  destroySurface();
  setMenuVisible(false);
}

void TrayMenuSurface::updateActiveGeometry(int count, int panel_height) {
  if (surface_ == nullptr || view_ == nullptr || wl_surface_ == nullptr) {
    return;
  }

  int active_width = (count * kMenuWidth) + ((count - 1) * kSubmenuGap);

  int active_x = current_placement_.x;
  if (current_placement_.top_level_column == 1 && count == 1) {
    active_x += kMenuWidth + kSubmenuGap;
  }

  const int margin_left = qMax(0, active_x - kShadowPadding);
  const int margin_top = qMax(0, current_placement_.y - kShadowPadding);

  const int padding_left = active_x - margin_left;
  const int padding_top = current_placement_.y - margin_top;

  const int padding_right = qMin(kShadowPadding, current_screen_geometry_.width() - active_x - active_width);
  const int padding_bottom =
      qMin(kShadowPadding, current_screen_geometry_.height() - current_placement_.y - panel_height);

  const int surface_width = active_width + padding_left + padding_right;
  const int surface_height = panel_height + padding_top + padding_bottom;

  surface_->set_size(surface_width, surface_height);
  surface_->set_margin(margin_top, 0, 0, margin_left);
  wl_surface_commit(wl_surface_);

  padding_left_ = padding_left;
  padding_right_ = padding_right;
  padding_top_ = padding_top;
  padding_bottom_ = padding_bottom;
  column_count_ = count;
  max_panel_height_ = panel_height;
  column_index_ = (current_placement_.top_level_column == 1 && count == 2) ? 1 : 0;

  Q_EMIT geometryChanged();
}

void TrayMenuSurface::setSubmenuModel(DbusMenuModel* submenu_model) {
  if (surface_ == nullptr || view_ == nullptr) {
    return;
  }
  int count = (submenu_model != nullptr) ? 2 : 1;
  int hash1 = calculateMenuHeight(active_model_);
  int hash2 = calculateMenuHeight(submenu_model);
  int max_h = qMax(hash1, hash2);
  updateActiveGeometry(count, max_h);
}

void TrayMenuSurface::activateItem(int item_id) {
  if (active_client_ != nullptr) {
    active_client_->activateItem(item_id);
  }
}

void TrayMenuSurface::close() {
  if (active_client_ != nullptr) {
    QTimer::singleShot(0, active_client_, &DbusMenuClient::close);
    return;
  }
  QTimer::singleShot(0, this, &TrayMenuSurface::hide);
}

bool TrayMenuSurface::ensureSurface(const QString& screen_name, int screen_x, int screen_y, DbusMenuModel* model) {
  active_model_ = model;
  QScreen* screen = screenForMenuRequest(screen_name, screen_x, screen_y);
  if (screen == nullptr) {
    qCritical("TrayMenuSurface: no screen available");
    return false;
  }

  dismiss_view_ = new QQuickView();
  dismiss_view_->setScreen(screen);
  dismiss_view_->setResizeMode(QQuickView::SizeRootObjectToView);
  dismiss_view_->setFlags(dismiss_view_->flags() | Qt::BypassWindowManagerHint);
  dismiss_view_->setColor(Qt::transparent);
  dismiss_view_->create();

  auto* dismissWaylandWindow = dismiss_view_->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
  if (dismissWaylandWindow == nullptr) {
    qCritical("TrayMenuSurface: dismiss surface is not running under Wayland");
    destroySurface();
    return false;
  }

  wl_surface* dismissWlSurface = dismissWaylandWindow->surface();
  if (dismissWlSurface == nullptr) {
    qCritical("TrayMenuSurface: dismiss wl_surface not available");
    destroySurface();
    return false;
  }

  wl_output* dismissWlOutput = nullptr;
  if (auto* waylandScreen = screen->nativeInterface<QNativeInterface::QWaylandScreen>()) {
    dismissWlOutput = waylandScreen->output();
  }

  auto* dismissRaw =
      shell_.get_layer_surface(dismissWlSurface, dismissWlOutput, QtWayland::zwlr_layer_shell_v1::layer_top,
                               QStringLiteral("tray-menu-dismiss"));
  if (dismissRaw == nullptr) {
    qCritical("TrayMenuSurface: dismiss layer surface creation failed");
    destroySurface();
    return false;
  }

  dismiss_surface_ = new LayerSurface(dismissRaw, dismissWlSurface, dismiss_view_, this);
  dismiss_surface_->set_anchor(
      QtWayland::zwlr_layer_surface_v1::anchor_top | QtWayland::zwlr_layer_surface_v1::anchor_bottom |
      QtWayland::zwlr_layer_surface_v1::anchor_left | QtWayland::zwlr_layer_surface_v1::anchor_right);
  dismiss_surface_->set_size(0, 0);
  dismiss_surface_->set_exclusive_zone(0);
  dismiss_surface_->set_margin(0, 0, 0, 0);
  if (!loadQmlSource(dismiss_view_, QUrl(QStringLiteral("qrc:/HolonightShell/Tray/TrayMenuDismissOverlay.qml")),
                     "TrayMenuSurface dismiss overlay")) {
    destroySurface();
    return false;
  }
  wl_surface_commit(dismissWlSurface);

  const QRect screen_geometry = screen->geometry();
  TrayMenuSurface::MenuPlacement placement = menuPlacement(screen_geometry, screen_x, screen_y);
  const int initial_height = calculateMenuHeight(model);
  placement.height = qMin(placement.height, initial_height);

  view_ = new QQuickView();
  view_->engine()->addImageProvider(QStringLiteral("icon"), new IconImageProvider());
  view_->setScreen(screen);
  view_->setResizeMode(QQuickView::SizeRootObjectToView);
  view_->setFlags(view_->flags() | Qt::BypassWindowManagerHint);
  view_->setColor(Qt::transparent);
  view_->create();

  auto* waylandWindow = view_->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
  if (waylandWindow == nullptr) {
    qCritical("TrayMenuSurface: not running under Wayland");
    destroySurface();
    return false;
  }

  wl_surface* wlSurface = waylandWindow->surface();
  if (wlSurface == nullptr) {
    qCritical("TrayMenuSurface: wl_surface not available");
    destroySurface();
    return false;
  }

  wl_surface_ = wlSurface;
  current_placement_ = placement;
  current_screen_geometry_ = screen_geometry;

  wl_output* wlOutput = nullptr;
  if (auto* waylandScreen = screen->nativeInterface<QNativeInterface::QWaylandScreen>()) {
    wlOutput = waylandScreen->output();
  }

  auto* raw = shell_.get_layer_surface(wlSurface, wlOutput, QtWayland::zwlr_layer_shell_v1::layer_top,
                                       QStringLiteral("tray-menu"));
  if (raw == nullptr) {
    qCritical("TrayMenuSurface: layer surface creation failed");
    destroySurface();
    return false;
  }

  surface_ = new LayerSurface(raw, wlSurface, view_, this);
  connect(surface_, &LayerSurface::closed, this, [this]() { setMenuVisible(false); });

  const int margin_left = qMax(0, placement.x - kShadowPadding);
  const int margin_top = qMax(0, placement.y - kShadowPadding);
  const int padding_left = placement.x - margin_left;
  const int padding_top = placement.y - margin_top;

  const int padding_right = qMin(kShadowPadding, screen_geometry.width() - placement.x - placement.width);
  const int padding_bottom = qMin(kShadowPadding, screen_geometry.height() - placement.y - placement.height);

  const int surface_width = placement.width + padding_left + padding_right;
  const int surface_height = placement.height + padding_top + padding_bottom;

  surface_->set_anchor(QtWayland::zwlr_layer_surface_v1::anchor_top | QtWayland::zwlr_layer_surface_v1::anchor_left);
  surface_->set_size(surface_width, surface_height);
  surface_->set_exclusive_zone(0);
  surface_->set_margin(margin_top, 0, 0, margin_left);
  surface_->set_keyboard_interactivity(QtWayland::zwlr_layer_surface_v1::keyboard_interactivity_on_demand);
  padding_left_ = padding_left;
  padding_right_ = padding_right;
  padding_top_ = padding_top;
  padding_bottom_ = padding_bottom;
  column_count_ = placement.column_count;
  column_index_ = placement.top_level_column;
  max_panel_height_ = placement.height;
  Q_EMIT geometryChanged();

  view_->setInitialProperties({{QStringLiteral("menuModel"), QVariant::fromValue(model)},
                               {QStringLiteral("menuClient"), QVariant::fromValue(static_cast<QObject*>(this))},
                               {QStringLiteral("columnWidth"), kMenuWidth},
                               {QStringLiteral("columnGap"), kSubmenuGap},
                               {QStringLiteral("columnStep"), placement.column_step}});
  if (!loadQmlSource(view_, QUrl(QStringLiteral("qrc:/HolonightShell/Popups/Tray/TrayMenuPopup.qml")),
                     "TrayMenuSurface")) {
    destroySurface();
    return false;
  }
  wl_surface_commit(wlSurface);
  return true;
}

void TrayMenuSurface::destroySurface() {
  wl_surface_ = nullptr;
  current_placement_ = {};
  current_screen_geometry_ = {};
  active_client_ = nullptr;
  delete dismiss_surface_;
  dismiss_surface_ = nullptr;
  delete dismiss_view_;
  dismiss_view_ = nullptr;
  delete surface_;
  surface_ = nullptr;
  delete view_;
  view_ = nullptr;
  delete active_model_;
  active_model_ = nullptr;
  if (pending_show_) {
    delete pending_model_;
    pending_model_ = nullptr;
    pending_client_ = nullptr;
    pending_show_ = false;
  }
}

void TrayMenuSurface::setMenuVisible(bool visible) {
  if (menu_visible_ == visible) {
    return;
  }
  menu_visible_ = visible;
  Q_EMIT menuVisibleChanged();
}
