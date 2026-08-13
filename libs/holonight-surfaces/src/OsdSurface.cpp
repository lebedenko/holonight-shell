#include "OsdSurface.h"

#include "ShellConstants.h"
#include "WidgetSurfacePolicy.h"

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>

namespace {
constexpr int kOsdMargin = 24;
// Used only between surface creation and the QML root's first layout pass. Roughly the size of a
// volume OSD, so the very first configure does not arrive at a wildly wrong geometry.
constexpr int kFallbackWidth = 220;
constexpr int kFallbackHeight = 96;
}  // namespace

using namespace Holonight::Wayland;

OsdSurface::OsdSurface(QObject* parent) : TransientSurfaceHost("OsdSurface", parent) {}

OsdSurface::~OsdSurface() { destroySurface(); }

void OsdSurface::setPosition(HoloNight::ShellConfig::WidgetPosition position) {
  if (position_ == position) {
    return;
  }
  position_ = position;
  applyPlacement();
}

void OsdSurface::ensureSurface(const QString& screen_name) {
  if (hasSurface() && current_screen_ == screen_name) {
    return;  // already on this monitor
  }
  if (hasSurface()) {
    destroySurface();  // monitor changed — rebuild
  }
  createSurface(screen_name);
}

void OsdSurface::showLevel(const QString& channel, int value, bool muted) {
  pending_is_level_ = true;
  pending_channel_ = channel;
  pending_value_ = value;
  pending_muted_ = muted;
  pushPendingContent();
}

void OsdSurface::showSelection(const QString& channel, const QString& short_label, const QString& full_label) {
  pending_is_level_ = false;
  pending_channel_ = channel;
  pending_short_label_ = short_label;
  pending_full_label_ = full_label;
  pushPendingContent();
}

void OsdSurface::hide() {
  if (!hasSurface()) {
    return;
  }
  if (auto* root = qobject_cast<QQuickItem*>(rootObject())) {
    root->setProperty("hiding", true);
  }
}

void OsdSurface::onHideAnimationFinished() {
  // Deliberately does not tear down here. The caller is a QML signal handler belonging to an
  // animation inside the very object tree destroySurface() deletes, so destroying synchronously
  // frees `view_` while that handler is still on the stack -- which Qt reports as fatal and aborts
  // on. Posting the teardown lets the handler return first.
  QMetaObject::invokeMethod(this, &OsdSurface::destroyAfterHide, Qt::QueuedConnection);
}

void OsdSurface::destroyAfterHide() {
  // A show arriving between the post above and this call has already cleared `hiding` and animated
  // the OSD back in (pushPendingContent), possibly onto a surface rebuilt for another monitor.
  // Tearing down on that stale request would kill a live OSD, so the decision is re-derived from
  // current state rather than trusted from when it was queued.
  if (hasSurface()) {
    const auto* root = qobject_cast<QQuickItem*>(rootObject());
    if (root != nullptr && !root->property("hiding").toBool()) {
      return;
    }
  }
  destroySurface();
}

bool OsdSurface::createSurface(const QString& screen_name) {
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
    qCritical("OsdSurface: no screen available");
    return false;
  }

  current_screen_ = screen_name;
  return openSurface(surfaceSpec(screen, screen_name));
}

LayerSurfaceSpec OsdSurface::surfaceSpec(QScreen* screen, const QString& screen_name) const {
  const int top = widgetPositionIsTopAnchored(position_) ? kBarHeight + kOsdMargin : kOsdMargin;
  return {.output = screen,
          .name_space = QStringLiteral("osd"),
          .layer = Layer::Overlay,
          .anchors = anchorsForPosition(position_),
          .width = kFallbackWidth,
          .height = kFallbackHeight,
          .margin_top = top,
          .margin_right = kOsdMargin,
          .margin_bottom = kOsdMargin,
          .margin_left = kOsdMargin,
          .exclusive_zone = 0,
          .keyboard_interactivity = KeyboardInteractivity::None,
          .input_region_policy = InputRegionPolicy::Empty,
          .input_region = {},
          .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Osd/OsdView.qml")),
          .initial_properties = {{QStringLiteral("monitorName"), screen_name}},
          .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
          .color = Qt::transparent};
}

void OsdSurface::onSurfaceConfigured() {
  applyInputRegion();
  if (auto* root = qobject_cast<QQuickItem*>(rootObject())) {
    root->setProperty("configured", true);
  }
  if (view() != nullptr) {
    connect(view(), &QQuickWindow::afterAnimating, this, &OsdSurface::updateSurfaceSize, Qt::UniqueConnection);
  }
  pushPendingContent();
}

void OsdSurface::onSurfaceTerminated() {}

void OsdSurface::applyPlacement() {
  if (host() == nullptr) {
    return;
  }
  // Top-anchored positions clear the bar's exclusive zone, the same rule desktop widgets follow.
  const int top = widgetPositionIsTopAnchored(position_) ? kBarHeight + kOsdMargin : kOsdMargin;
  host()->setAnchors(anchorsForPosition(position_));
  host()->setMargins(top, kOsdMargin, kOsdMargin, kOsdMargin);
  host()->setExclusiveZone(0);
}

void OsdSurface::applyInputRegion() {
  if (host() != nullptr) {
    host()->setInputRegion(InputRegionPolicy::Empty);
  }
}

void OsdSurface::pushPendingContent() {
  if (!hasSurface()) {
    return;
  }
  auto* root = qobject_cast<QQuickItem*>(rootObject());
  if (root == nullptr || !root->property("configured").toBool()) {
    return;  // replayed from the configured() handler instead
  }

  // Order matters: set the branch payload before `kind` switches the Loader synchronously, then
  // present only after the new renderer has been created with its final initial values.
  if (pending_is_level_) {
    root->setProperty("value", pending_value_);
    root->setProperty("muted", pending_muted_);
  } else {
    root->setProperty("shortLabel", pending_short_label_);
    root->setProperty("fullLabel", pending_full_label_);
  }
  root->setProperty("channel", pending_channel_);
  root->setProperty("kind", pending_is_level_ ? QStringLiteral("level") : QStringLiteral("selection"));
  // An explicit call, not a side effect of `channel` changing: two events in a row on one channel
  // leave that property equal to what it already held, so nothing would be shown at all.
  if (!QMetaObject::invokeMethod(root, "present")) {
    qCritical("OsdSurface: OsdView.present() is missing; the OSD cannot be shown");
  }
}

void OsdSurface::updateSurfaceSize() {
  if (host() == nullptr || view() == nullptr) {
    return;
  }
  int width = kFallbackWidth;
  int height = kFallbackHeight;
  if (auto* root = qobject_cast<QQuickItem*>(rootObject())) {
    // The QML root's implicit size already includes its glow margin, so it is used as-is.
    const int reported_width = static_cast<int>(root->implicitWidth());
    const int reported_height = static_cast<int>(root->implicitHeight());
    if (reported_width > 0 && reported_height > 0) {
      width = reported_width;
      height = reported_height;
    }
  }
  // Called every frame, so the request is issued only when the size actually moved -- otherwise the
  // OSD would flood the compositor with identical set_size/commit pairs for as long as it is up.
  if (width == applied_width_ && height == applied_height_) {
    return;
  }
  applied_width_ = width;
  applied_height_ = height;

  host()->setSize(width, height);
  // Qt reinstalls its own input region as the window resizes, so the empty one has to be restated
  // on every size change, not just at creation.
  applyInputRegion();
}

void OsdSurface::destroySurface() {
  clearPendingSurface();
  closeSurface();
  current_screen_.clear();
  // Reset with the surface: the next one starts at the fallback size, so a stale match here would
  // skip the first real resize and clip the card.
  applied_width_ = -1;
  applied_height_ = -1;
}
