#include "NotificationToastSurface.h"

#include "IconImageProvider.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>

#include <holonight/wayland/layershellcontext.h>

namespace {
constexpr int kToastWidth = 430;
constexpr int kGlowMargin = 16;
constexpr int kSurfaceWidth = kToastWidth + (kGlowMargin * 2);
constexpr int kTopGap = 8;            // gap below the bar's reserved zone
constexpr int kRightMargin = 12;      // breathing room from the right screen edge
constexpr int kFallbackHeight = 120;  // used until the QML stack reports its content height
}  // namespace

using namespace Holonight::Wayland;

NotificationToastSurface::NotificationToastSurface(QObject* parent)
    : TransientSurfaceHost("NotificationToastSurface", parent) {}

NotificationToastSurface::~NotificationToastSurface() { destroySurface(); }

void NotificationToastSurface::ensureSurface(const QString& screen_name) {
  if (hasSurface() && current_screen_ == screen_name) {
    return;  // already showing on this monitor
  }
  if (hasSurface()) {
    destroySurface();  // monitor changed — rebuild
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

  current_screen_ = screen_name;
  return openSurface(surfaceSpec(screen, screen_name));
}

LayerSurfaceSpec NotificationToastSurface::surfaceSpec(QScreen* screen, const QString& screen_name) {
  return {.output = screen,
          .name_space = QStringLiteral("notifications"),
          .layer = Layer::Overlay,
          .anchors = Anchor::Top | Anchor::Right,
          .width = kSurfaceWidth,
          .height = kFallbackHeight,
          .margin_top = kTopGap,
          .margin_right = kRightMargin,
          .exclusive_zone = 0,
          .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Notifications/ToastStack.qml")),
          .initial_properties = {{QStringLiteral("monitorName"), screen_name}},
          .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
          .color = Qt::transparent,
          .before_load = [](QQmlEngine* engine) {
            engine->addImageProvider(QStringLiteral("icon"), new IconImageProvider());
          }};
}

void NotificationToastSurface::onSurfaceConfigured() {
  if (auto* root = qobject_cast<QQuickItem*>(rootObject())) {
    connect(root, SIGNAL(contentHeightChanged()), this, SLOT(updateSurfaceSize()), Qt::UniqueConnection);  // NOLINT
  }
  updateSurfaceSize();
}

void NotificationToastSurface::onSurfaceTerminated() {}

void NotificationToastSurface::updateSurfaceSize() {
  if (host() == nullptr) {
    return;
  }
  int height = kFallbackHeight;
  if (auto* root = qobject_cast<QQuickItem*>(rootObject())) {
    const int reported = root->property("contentHeight").toInt();
    if (reported > 0) {
      height = reported;
    }
  }
  host()->setSize(kSurfaceWidth, height);
}

void NotificationToastSurface::destroySurface() {
  clearPendingSurface();
  closeSurface();
  current_screen_.clear();
}
