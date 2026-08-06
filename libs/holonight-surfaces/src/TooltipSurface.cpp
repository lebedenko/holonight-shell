#include "TooltipSurface.h"

#include "QmlSourceLoader.h"
#include "TooltipGeometry.h"

#include <QGuiApplication>
#include <QScreen>
#include <qpa/qplatformwindow_p.h>
#include <qscreen_platform.h>

#include <wayland-client.h>

static constexpr int kTooltipGap = 4;
static constexpr int kTooltipHeight = 106;

TooltipSurface::TooltipSurface(QObject* parent) : QObject(parent) {
  connect(&shell_, &QWaylandClientExtension::activeChanged, this, [this]() {
    if (shell_.isActive() && pending_show_) {
      pending_show_ = false;
      const bool surface_created = ensureSurface(pending_screen_, pending_anchor_x_, pending_anchor_width_);
      setTooltipVisible(surface_created);
    }
  });
}

TooltipSurface::~TooltipSurface() { destroySurface(); }

void TooltipSurface::show(const QString& screen_name, int anchor_x, int anchor_width, const QString& title,
                          const QString& description, const QString& icon_name, int battery_percent, bool charging,
                          int signal_strength) {
  setContent(title, description, icon_name, battery_percent, charging, signal_strength);

  if (!shell_.isActive()) {
    pending_show_ = true;
    pending_screen_ = screen_name;
    pending_anchor_x_ = anchor_x;
    pending_anchor_width_ = anchor_width;
    return;
  }

  if (tooltip_visible_ && view_ != nullptr && surface_ != nullptr && current_screen_ == screen_name &&
      current_anchor_x_ == anchor_x && current_anchor_width_ == anchor_width) {
    return;
  }

  destroySurface();
  const bool surface_created = ensureSurface(screen_name, anchor_x, anchor_width);
  setTooltipVisible(surface_created);
}

void TooltipSurface::hide() {
  if (view_ != nullptr) {
    view_->hide();
  }
  setTooltipVisible(false);
  destroySurface();
}

bool TooltipSurface::ensureSurface(const QString& screen_name, int anchor_x, int anchor_width) {
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
    qCritical("TooltipSurface: no screen available");
    return false;
  }

  view_ = new QQuickView();
  view_->setScreen(screen);
  view_->setResizeMode(QQuickView::SizeRootObjectToView);
  view_->setFlags(view_->flags() | Qt::BypassWindowManagerHint);
  view_->setColor(Qt::transparent);
  view_->create();

  auto* waylandWindow = view_->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
  if (waylandWindow == nullptr) {
    qCritical("TooltipSurface: not running under Wayland");
    destroySurface();
    return false;
  }

  wl_surface* wlSurface = waylandWindow->surface();
  if (wlSurface == nullptr) {
    qCritical("TooltipSurface: wl_surface not available");
    destroySurface();
    return false;
  }

  wl_output* wlOutput = nullptr;
  if (auto* waylandScreen = screen->nativeInterface<QNativeInterface::QWaylandScreen>()) {
    wlOutput = waylandScreen->output();
  }

  auto* raw = shell_.get_layer_surface(wlSurface, wlOutput, QtWayland::zwlr_layer_shell_v1::layer_top,
                                       QStringLiteral("tooltip"));
  if (raw == nullptr) {
    qCritical("TooltipSurface: layer surface creation failed");
    destroySurface();
    return false;
  }

  const int screen_width = screen->geometry().width();
  const int left_margin = TooltipGeometry::leftMargin(screen_width, screen->geometry().x(), anchor_x, anchor_width);

  surface_ = new LayerSurface(raw, wlSurface, view_, this);
  connect(surface_, &LayerSurface::closed, this, [this]() { setTooltipVisible(false); });
  surface_->set_anchor(QtWayland::zwlr_layer_surface_v1::anchor_top | QtWayland::zwlr_layer_surface_v1::anchor_left);
  surface_->set_size(TooltipGeometry::kWidth, kTooltipHeight);
  surface_->set_exclusive_zone(0);
  surface_->set_margin(kTooltipGap, 0, 0, left_margin);

  if (!loadQmlSource(view_, QUrl(QStringLiteral("qrc:/HolonightShell/Popups/Tooltip/TooltipPopup.qml")),
                     "TooltipSurface")) {
    destroySurface();
    return false;
  }
  wl_surface_commit(wlSurface);
  current_screen_ = screen_name;
  current_anchor_x_ = anchor_x;
  current_anchor_width_ = anchor_width;
  return true;
}

void TooltipSurface::destroySurface() {
  delete surface_;
  surface_ = nullptr;
  delete view_;
  view_ = nullptr;
  current_screen_.clear();
  current_anchor_x_ = 0;
  current_anchor_width_ = 0;
}

void TooltipSurface::setTooltipVisible(bool visible) {
  if (tooltip_visible_ == visible) {
    return;
  }
  tooltip_visible_ = visible;
  Q_EMIT tooltipVisibleChanged();
}

void TooltipSurface::setContent(const QString& title, const QString& description, const QString& icon_name,
                                int battery_percent, bool charging, int signal_strength) {
  if (title_ == title && description_ == description && icon_name_ == icon_name &&
      battery_percent_ == battery_percent && charging_ == charging && signal_strength_ == signal_strength) {
    return;
  }
  title_ = title;
  description_ = description;
  icon_name_ = icon_name;
  battery_percent_ = battery_percent;
  charging_ = charging;
  signal_strength_ = signal_strength;
  Q_EMIT contentChanged();
}
