#include "OsdSurface.h"

#include "QmlSourceLoader.h"
#include "ShellConstants.h"
#include "WidgetSurfacePolicy.h"

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>
#include <QtGui/qguiapplication_platform.h>
#include <qpa/qplatformwindow_p.h>
#include <qscreen_platform.h>

#include <wayland-client.h>

namespace {
constexpr int kOsdMargin = 24;
// Used only between surface creation and the QML root's first layout pass. Roughly the size of a
// volume OSD, so the very first configure does not arrive at a wildly wrong geometry.
constexpr int kFallbackWidth = 220;
constexpr int kFallbackHeight = 96;
}  // namespace

OsdSurface::OsdSurface(QObject* parent) : QObject(parent) {
  connect(&shell_, &QWaylandClientExtension::activeChanged, this, [this]() {
    if (shell_.isActive() && pending_show_) {
      pending_show_ = false;
      createSurface(pending_screen_);
    }
  });
}

OsdSurface::~OsdSurface() { destroySurface(); }

void OsdSurface::setPosition(HoloNight::ShellConfig::WidgetPosition position) {
  if (position_ == position) {
    return;
  }
  position_ = position;
  applyPlacement();
}

void OsdSurface::ensureSurface(const QString& screen_name) {
  if (view_ != nullptr && current_screen_ == screen_name) {
    return;  // already on this monitor
  }
  if (view_ != nullptr) {
    destroySurface();  // monitor changed — rebuild
  }
  if (!shell_.isActive()) {
    pending_show_ = true;
    pending_screen_ = screen_name;
    return;
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
  if (view_ == nullptr) {
    return;
  }
  if (QQuickItem* root = view_->rootObject()) {
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
  if (view_ != nullptr) {
    const QQuickItem* root = view_->rootObject();
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

  view_ = new QQuickView();
  view_->setScreen(screen);
  view_->setResizeMode(QQuickView::SizeRootObjectToView);
  view_->setFlags(view_->flags() | Qt::BypassWindowManagerHint);
  view_->setColor(Qt::transparent);
  view_->create();

  auto* wayland_window = view_->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
  if (wayland_window == nullptr) {
    qCritical("OsdSurface: not running under Wayland");
    destroySurface();
    return false;
  }

  wl_surface* wl_surface = wayland_window->surface();
  if (wl_surface == nullptr) {
    qCritical("OsdSurface: wl_surface not available");
    destroySurface();
    return false;
  }

  wl_output* wl_output = nullptr;
  if (auto* wayland_screen = screen->nativeInterface<QNativeInterface::QWaylandScreen>()) {
    wl_output = wayland_screen->output();
  }

  auto* raw = shell_.get_layer_surface(wl_surface, wl_output, QtWayland::zwlr_layer_shell_v1::layer_overlay,
                                       QStringLiteral("osd"));
  if (raw == nullptr) {
    qCritical("OsdSurface: layer surface creation failed");
    destroySurface();
    return false;
  }

  wl_surface_ = wl_surface;
  surface_ = new LayerSurface(raw, wl_surface, view_, this);
  surface_->set_size(kFallbackWidth, kFallbackHeight);
  // REQ-F-024: the OSD is never focusable. `none` is already the protocol default, but say so
  // explicitly -- this surface sits on the overlay layer, where silently taking keyboard focus
  // would steal input from the focused application.
  surface_->set_keyboard_interactivity(QtWayland::zwlr_layer_surface_v1::keyboard_interactivity_none);
  applyPlacement();
  applyInputRegion();

  // REQ-NF-009: nothing renders until the compositor has configured the surface, so the entrance
  // animation never plays against a not-yet-mapped surface. Guarded per the documented
  // SingleShotConnection race: the configure event can arrive after a monitor-change rebuild has
  // already destroyed this view.
  connect(
      surface_, &LayerSurface::configured, this,
      [this]() {
        if (!isActive()) {
          return;
        }
        // Re-applied post-map: Qt owns the same wl_surface and sets its own input region while
        // mapping the window, which would otherwise overwrite the empty one installed at creation.
        applyInputRegion();
        wl_surface_commit(wl_surface_);
        if (QQuickItem* root = view_->rootObject()) {
          root->setProperty("configured", true);
        }
        // Content is pushed only now, not at creation: OsdView.qml starts its entrance from
        // the channel property changing, and it ignores that change while `configured` is false.
        // Pushing earlier would leave the OSD permanently at opacity 0.
        pushPendingContent();
      },
      Qt::SingleShotConnection);

  view_->setInitialProperties({{QStringLiteral("monitorName"), screen_name}});
  if (!loadQmlSource(view_, QUrl(QStringLiteral("qrc:/HolonightShell/Osd/OsdView.qml")), "OsdSurface")) {
    destroySurface();
    return false;
  }

  // Sampled once per frame rather than from implicitWidth/HeightChanged: the renderers are built
  // from QtQuick.Layouts, whose implicit size is recomputed during the polish phase and not at all
  // while the root is still invisible, so those signals can fire before the card has its final
  // geometry and then never again -- leaving the surface at the fallback size with the card
  // overflowing and clipped. afterAnimating runs after polishItems() on the GUI thread, so the
  // implicit size read here is always the one about to be rendered.
  connect(view_, &QQuickWindow::afterAnimating, this, &OsdSurface::updateSurfaceSize);

  current_screen_ = screen_name;
  updateSurfaceSize();
  wl_surface_commit(wl_surface);
  return true;
}

void OsdSurface::applyPlacement() {
  if (surface_ == nullptr || wl_surface_ == nullptr) {
    return;
  }
  // Top-anchored positions clear the bar's exclusive zone, the same rule desktop widgets follow.
  const int top = widgetPositionIsTopAnchored(position_) ? kBarHeight + kOsdMargin : kOsdMargin;
  surface_->set_anchor(anchorFlagsForPosition(position_));
  surface_->set_margin(top, kOsdMargin, kOsdMargin, kOsdMargin);
  surface_->set_exclusive_zone(0);  // the OSD never reserves space
  wl_surface_commit(wl_surface_);
}

void OsdSurface::applyInputRegion() {
  if (wl_surface_ == nullptr) {
    return;
  }
  const auto* wayland_app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
  wl_compositor* compositor = wayland_app != nullptr ? wayland_app->compositor() : nullptr;
  if (compositor == nullptr) {
    qCritical("OsdSurface: no wl_compositor available; the OSD would swallow clicks");
    return;
  }
  wl_region* region = wl_compositor_create_region(compositor);
  if (region == nullptr) {
    qCritical("OsdSurface: wl_region creation failed; the OSD would swallow clicks");
    return;
  }
  // Deliberately empty -- no wl_region_add() -- so every pointer and touch event lands on whatever
  // is behind the OSD instead of on this surface.
  wl_surface_set_input_region(wl_surface_, region);
  wl_region_destroy(region);
}

void OsdSurface::pushPendingContent() {
  if (view_ == nullptr) {
    return;
  }
  QQuickItem* root = view_->rootObject();
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
  if (surface_ == nullptr || view_ == nullptr || wl_surface_ == nullptr) {
    return;
  }
  int width = kFallbackWidth;
  int height = kFallbackHeight;
  if (QQuickItem* root = view_->rootObject()) {
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

  surface_->set_size(width, height);
  // Qt reinstalls its own input region as the window resizes, so the empty one has to be restated
  // on every size change, not just at creation.
  applyInputRegion();
  wl_surface_commit(wl_surface_);
}

void OsdSurface::destroySurface() {
  delete surface_;
  surface_ = nullptr;
  delete view_;
  view_ = nullptr;
  wl_surface_ = nullptr;
  current_screen_.clear();
  // Reset with the surface: the next one starts at the fallback size, so a stale match here would
  // skip the first real resize and clip the card.
  applied_width_ = -1;
  applied_height_ = -1;
}
