#include "TrayMenuSurface.h"

#include "IconImageProvider.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QScreen>
#include <QTimer>

#include <algorithm>

using namespace Holonight::Wayland;

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

int calculateMenuHeight(DbusMenuModel* model) {
  if (model == nullptr) {
    return kTrayMenuMinHeight;
  }
  int height = 16;
  int visible_items = 0;
  for (int index = 0; index < model->rowCount(); ++index) {
    const QModelIndex model_index = model->index(index);
    if (!model->data(model_index, DbusMenuModel::VisibleRole).toBool()) {
      continue;
    }
    ++visible_items;
    height += model->data(model_index, DbusMenuModel::TypeRole).toString() == QLatin1String("separator") ? 10 : 30;
    ++height;
  }
  if (visible_items > 0) {
    --height;
  }
  return std::clamp(height, kTrayMenuMinHeight, kTrayMenuMaxHeight);
}
}  // namespace

TrayMenuSurface::TrayMenuSurface(QObject* parent) : PairedTransientSurfaceHost("TrayMenuSurface", parent) {}
TrayMenuSurface::~TrayMenuSurface() { destroySurface(); }

void TrayMenuSurface::show(const QString& screen_name, int screen_x, int screen_y, DbusMenuModel* model,
                           DbusMenuClient* client) {
  destroySurface();
  if (model != nullptr) {
    model->setParent(this);
  }
  active_model_ = model;
  active_client_ = client;
  setMenuVisible(ensureSurface(screen_name, screen_x, screen_y, model));
}

void TrayMenuSurface::hide() {
  destroySurface();
  setMenuVisible(false);
}

bool TrayMenuSurface::ensureSurface(const QString& screen_name, int screen_x, int screen_y, DbusMenuModel* model) {
  QScreen* screen = screenForMenuRequest(screen_name, screen_x, screen_y);
  if (screen == nullptr) {
    qCritical("TrayMenuSurface: no screen available");
    return false;
  }
  return openPair(surfaceSpec(screen, screen_x, screen_y, model));
}

PairedLayerSurfaceSpec TrayMenuSurface::surfaceSpec(QScreen* screen, int screen_x, int screen_y, DbusMenuModel* model) {
  current_screen_geometry_ = screen->geometry();
  current_placement_ = trayMenuPlacement(current_screen_geometry_, screen_x, screen_y);
  current_placement_.height = std::min(current_placement_.height, calculateMenuHeight(model));
  const TrayMenuActiveGeometry geometry = trayMenuActiveGeometry(
      current_screen_geometry_, current_placement_, current_placement_.column_count, current_placement_.height);
  applyGeometry(geometry);

  return {
      .dismiss = {.output = screen,
                  .name_space = QStringLiteral("tray-menu-dismiss"),
                  .layer = Layer::Top,
                  .anchors = Anchor::Top | Anchor::Bottom | Anchor::Left | Anchor::Right,
                  .width = 0,
                  .height = 0,
                  .exclusive_zone = 0,
                  .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Tray/TrayMenuDismissOverlay.qml")),
                  .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
                  .color = Qt::transparent},
      .content = {.output = screen,
                  .name_space = QStringLiteral("tray-menu"),
                  .layer = Layer::Top,
                  .anchors = Anchor::Top | Anchor::Left,
                  .width = geometry.surface_width,
                  .height = geometry.surface_height,
                  .margin_top = geometry.margin_top,
                  .margin_left = geometry.margin_left,
                  .exclusive_zone = 0,
                  .keyboard_interactivity = KeyboardInteractivity::OnDemand,
                  .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Popups/Tray/TrayMenuPopup.qml")),
                  .initial_properties = {{QStringLiteral("menuModel"), QVariant::fromValue(model)},
                                         {QStringLiteral("menuClient"),
                                          QVariant::fromValue(static_cast<QObject*>(this))},
                                         {QStringLiteral("columnWidth"), kTrayMenuWidth},
                                         {QStringLiteral("columnGap"), kTraySubmenuGap},
                                         {QStringLiteral("columnStep"), current_placement_.column_step}},
                  .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
                  .color = Qt::transparent,
                  .before_load =
                      [](QQmlEngine* engine) {
                        engine->addImageProvider(QStringLiteral("icon"), new IconImageProvider());
                      }},
  };
}

void TrayMenuSurface::updateActiveGeometry(int count, int panel_height) {
  if (contentHost() == nullptr) {
    return;
  }
  const TrayMenuActiveGeometry geometry =
      trayMenuActiveGeometry(current_screen_geometry_, current_placement_, count, panel_height);
  contentHost()->setSize(geometry.surface_width, geometry.surface_height);
  contentHost()->setMargins(geometry.margin_top, 0, 0, geometry.margin_left);
  applyGeometry(geometry);
}

void TrayMenuSurface::applyGeometry(const TrayMenuActiveGeometry& geometry) {
  padding_left_ = geometry.padding_left;
  padding_right_ = geometry.padding_right;
  padding_top_ = geometry.padding_top;
  padding_bottom_ = geometry.padding_bottom;
  column_count_ = geometry.column_count;
  column_index_ = geometry.column_index;
  max_panel_height_ = geometry.panel_height;
  Q_EMIT geometryChanged();
}

void TrayMenuSurface::setSubmenuModel(DbusMenuModel* submenu_model) {
  if (contentHost() == nullptr) {
    return;
  }
  updateActiveGeometry(submenu_model != nullptr ? 2 : 1,
                       std::max(calculateMenuHeight(active_model_), calculateMenuHeight(submenu_model)));
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

void TrayMenuSurface::onPairOpened() { setMenuVisible(true); }

void TrayMenuSurface::onPairTerminated() {
  setMenuVisible(false);
  if (hasPendingPair()) {
    return;
  }
  active_client_ = nullptr;
  delete active_model_;
  active_model_ = nullptr;
}

void TrayMenuSurface::destroySurface() {
  clearPendingPair();
  closePair();
  current_placement_ = {};
  current_screen_geometry_ = {};
  active_client_ = nullptr;
  delete active_model_;
  active_model_ = nullptr;
}

void TrayMenuSurface::setMenuVisible(bool visible) {
  if (menu_visible_ == visible) {
    return;
  }
  menu_visible_ = visible;
  Q_EMIT menuVisibleChanged();
}
