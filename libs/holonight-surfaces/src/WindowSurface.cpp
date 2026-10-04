#include "WindowSurface.h"

#include "IconImageProvider.h"
#include "ShellConstants.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QScreen>

using namespace Holonight::Wayland;

void WindowSurface::toggle(const QString& screen, const QRectF& anchor) {
  if (hasSurface()) {
    hide();
    return;
  }
  mode_ = 0;
  target_.clear();
  choices_.clear();
  beside_anchor_ = false;
  show(screen, anchor);
}
void WindowSurface::menu(const QString& identifier, const QString& screen, const QRectF& anchor, bool beside) {
  hide();
  mode_ = 1;
  target_ = identifier;
  beside_anchor_ = beside;
  show(screen, anchor);
}
void WindowSurface::chooser(const QStringList& ids, const QString& screen, const QRectF& anchor) {
  hide();
  mode_ = 2;
  choices_ = ids;
  target_.clear();
  beside_anchor_ = false;
  show(screen, anchor);
}
void WindowSurface::desktopMenu(const QString& screen, qreal position_x, qreal position_y) {
  hide();
  mode_ = 3;
  menu_position_ = QPointF(position_x, position_y);
  target_.clear();
  show(screen);
}
QRectF WindowSurface::localAnchor(const QRectF& global, const QRect& geometry) {
  return global.isValid() ? global.translated(-geometry.topLeft()) : QRectF(8, kBarHeight, 0, 0);
}
void WindowSurface::show(const QString& name, const QRectF& anchor) {
  auto* screen = QGuiApplication::primaryScreen();
  for (auto* candidate : QGuiApplication::screens()) {
    if (candidate->name() == name) {
      screen = candidate;
    }
  }
  if (screen == nullptr) {
    return;
  }
  screen_name_ = screen->name();
  anchor_ = localAnchor(anchor, screen->geometry());
  emit opened();
  emit changed();
  const bool opened_surface = openSurface(surfaceSpec(screen, mode_ == 3));
  emit changed();
  if (!opened_surface) {
    emit dismissed();
  }
}
LayerSurfaceSpec WindowSurface::surfaceSpec(QScreen* screen, bool /*desktop_menu*/) {
  return {
      .output = screen,
      .name_space = QStringLiteral("window-overview"),
      .layer = Layer::Overlay,
      .anchors = Anchor::Top | Anchor::Bottom | Anchor::Left | Anchor::Right,
      .width = 0,
      .height = 0,
      // All menu anchors use the full output coordinate space.
      .exclusive_zone = -1,
      .keyboard_interactivity = KeyboardInteractivity::Exclusive,
      .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Windows/WindowOverlay.qml")),
      .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
      .color = Qt::transparent,
      .before_load =
          [](QQmlEngine* engine) { engine->addImageProvider(QStringLiteral("icon"), new IconImageProvider()); },
  };
}
void WindowSurface::hide() {
  clearPendingSurface();
  closeSurface();
  emit changed();
  emit dismissed();
}
void WindowSurface::onSurfaceTerminated() {
  emit changed();
  emit dismissed();
}
