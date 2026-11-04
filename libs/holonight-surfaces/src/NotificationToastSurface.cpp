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
constexpr int kTopGap = 8;        // gap below the bar's reserved zone
constexpr int kRightMargin = 12;  // breathing room from the right screen edge
constexpr int kIdleHeight = 1;    // keep the empty surface mapped without capturing input
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
  return {
      .output = screen,
      .name_space = QStringLiteral("notifications"),
      .layer = Layer::Overlay,
      .anchors = Anchor::Top | Anchor::Right,
      .width = kSurfaceWidth,
      .height = kIdleHeight,
      .margin_top = kTopGap,
      .margin_right = kRightMargin,
      .exclusive_zone = 0,
      .keyboard_interactivity = KeyboardInteractivity::None,
      .input_region_policy = InputRegionPolicy::Empty,
      .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Notifications/ToastStack.qml")),
      .initial_properties = {{QStringLiteral("monitorName"), screen_name}},
      .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint | Qt::WindowDoesNotAcceptFocus,
      .color = Qt::transparent,
      .before_load =
          [](QQmlEngine* engine) { engine->addImageProvider(QStringLiteral("icon"), new IconImageProvider()); },
  };
}

void NotificationToastSurface::onSurfaceConfigured() {
  if (auto* root = qobject_cast<QQuickItem*>(rootObject())) {
    connect(root, SIGNAL(contentHeightChanged()), this, SLOT(updateSurfaceSize()), Qt::UniqueConnection);  // NOLINT
    connect(root, SIGNAL(hasToastsChanged()), this, SLOT(updateSurfaceSize()), Qt::UniqueConnection);      // NOLINT
  }
  if (view() != nullptr) {
    connect(view(), &QQuickWindow::afterAnimating, this, &NotificationToastSurface::updateSurfaceSize,
            Qt::UniqueConnection);
  }
  updateSurfaceSize();
}

void NotificationToastSurface::onSurfaceTerminated() {
  applied_height_ = -1;
  applied_input_ = false;
}

void NotificationToastSurface::updateSurfaceSize() {
  if (host() == nullptr) {
    return;
  }
  int height = kIdleHeight;
  bool accepts_input = false;
  if (auto* root = qobject_cast<QQuickItem*>(rootObject())) {
    accepts_input = root->property("hasToasts").toBool();
    const int reported = root->property("contentHeight").toInt();
    if (accepts_input && reported > 0) {
      height = reported;
    }
  }
  if (height == applied_height_ && accepts_input == applied_input_) {
    return;
  }
  host()->setSize(kSurfaceWidth, height);
  // Resizing can reinstall Qt's input region; restate the policy after each size change.
  host()->setInputRegion(accepts_input ? InputRegionPolicy::Default : InputRegionPolicy::Empty);
  applied_height_ = height;
  applied_input_ = accepts_input;
}

void NotificationToastSurface::destroySurface() {
  clearPendingSurface();
  closeSurface();
  current_screen_.clear();
  onSurfaceTerminated();
}
