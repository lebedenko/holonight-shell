#include "WindowSurface.h"

#include "IconImageProvider.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QScreen>

using namespace Holonight::Wayland;

void WindowSurface::toggle(const QString& screen) {
  if (hasSurface()) {
    hide();
    return;
  }
  mode_ = 0;
  target_.clear();
  choices_.clear();
  show(screen);
}
void WindowSurface::menu(const QString& identifier, const QString& screen) {
  hide();
  mode_ = 1;
  target_ = identifier;
  show(screen);
}
void WindowSurface::chooser(const QStringList& ids, const QString& screen) {
  hide();
  mode_ = 2;
  choices_ = ids;
  target_.clear();
  show(screen);
}
void WindowSurface::desktopMenu(const QString& screen) {
  hide();
  mode_ = 3;
  target_.clear();
  show(screen);
}
void WindowSurface::show(const QString& name) {
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
  emit opened();
  emit changed();
  const bool opened_surface = openSurface({
      .output = screen,
      .name_space = QStringLiteral("window-overview"),
      .layer = Layer::Overlay,
      .anchors = Anchor::Top | Anchor::Bottom | Anchor::Left | Anchor::Right,
      .width = 0,
      .height = 0,
      .exclusive_zone = 0,
      .keyboard_interactivity = KeyboardInteractivity::Exclusive,
      .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Windows/WindowOverlay.qml")),
      .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
      .color = Qt::transparent,
      .before_load =
          [](QQmlEngine* engine) { engine->addImageProvider(QStringLiteral("icon"), new IconImageProvider()); },
  });
  emit changed();
  if (!opened_surface) {
    emit dismissed();
  }
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
