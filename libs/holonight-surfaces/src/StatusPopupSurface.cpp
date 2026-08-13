#include "StatusPopupSurface.h"

#include "IconImageProvider.h"
#include "ShellConstants.h"
#include "StatusPopupGeometry.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QScreen>

using namespace Holonight::Wayland;

StatusPopupSurface::StatusPopupSurface(QObject* parent) : PairedTransientSurfaceHost("StatusPopupSurface", parent) {}
StatusPopupSurface::~StatusPopupSurface() { destroySurface(); }

void StatusPopupSurface::toggle(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width) {
  if (popup_visible_ && active_popup_id_ == popup_id) {
    hide();
    return;
  }
  show(popup_id, screen_name, anchor_x, anchor_width);
}

void StatusPopupSurface::show(const QString& popup_id, const QString& screen_name, int anchor_x, int anchor_width) {
  destroySurface();
  setPopupVisible(ensureSurface(popup_id, screen_name, anchor_x, anchor_width));
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

  const StatusPopupGeometry geometry =
      statusPopupGeometry(popup_id, screen->geometry(), screen->availableGeometry(), anchor_x, anchor_width);
  setPointerX(geometry.pointer_x);
  setActivePopupId(popup_id);
  return openPair(surfaceSpec(screen, popup_id, anchor_x, anchor_width));
}

PairedLayerSurfaceSpec StatusPopupSurface::surfaceSpec(QScreen* screen, const QString& popup_id, int anchor_x,
                                                       int anchor_width) {
  const StatusPopupGeometry geometry =
      statusPopupGeometry(popup_id, screen->geometry(), screen->availableGeometry(), anchor_x, anchor_width);
  return {
      .dismiss = {.output = screen,
                  .name_space = QStringLiteral("status-popup-dismiss"),
                  .layer = Layer::Top,
                  .anchors = Anchor::Top | Anchor::Bottom | Anchor::Left | Anchor::Right,
                  .width = 0,
                  .height = 0,
                  .exclusive_zone = 0,
                  .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Popups/Status/StatusPopupDismissOverlay.qml")),
                  .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
                  .color = Qt::transparent},
      .content = {.output = screen,
                  .name_space = QStringLiteral("status-popup"),
                  .layer = Layer::Top,
                  .anchors = Anchor::Top | Anchor::Left,
                  .width = geometry.surface_width,
                  .height = geometry.surface_height,
                  .margin_top = kStatusPopupTopGap,
                  .margin_left = geometry.left_margin,
                  .exclusive_zone = 0,
                  .keyboard_interactivity = KeyboardInteractivity::OnDemand,
                  .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Popups/Status/StatusPopup.qml")),
                  .initial_properties = {{QStringLiteral("popupId"), popup_id}},
                  .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
                  .color = Qt::transparent,
                  .before_load =
                      [](QQmlEngine* engine) {
                        engine->addImageProvider(QStringLiteral("icon"), new IconImageProvider());
                      }},
  };
}

void StatusPopupSurface::onPairOpened() { setPopupVisible(true); }

void StatusPopupSurface::onPairTerminated() {
  setPopupVisible(false);
  if (!hasPendingPair()) {
    setActivePopupId({});
  }
}

void StatusPopupSurface::destroySurface() {
  clearPendingPair();
  closePair();
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
