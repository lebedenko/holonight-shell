#include "TooltipSurface.h"

#include "TooltipGeometry.h"

#include <QGuiApplication>
#include <QScreen>

#include <holonight/wayland/layershellcontext.h>

static constexpr int kTooltipGap = 4;
static constexpr int kTooltipHeight = 106;

using namespace Holonight::Wayland;

TooltipSurface::TooltipSurface(QObject* parent) : TransientSurfaceHost("TooltipSurface", parent) {}

TooltipSurface::~TooltipSurface() { destroySurface(); }

void TooltipSurface::show(const QString& screen_name, int anchor_x, int anchor_width, const QString& title,
                          const QString& description, const QString& icon_name, int battery_percent, bool charging,
                          int signal_strength) {
  setContent(title, description, icon_name, battery_percent, charging, signal_strength);

  if (tooltip_visible_ && hasSurface() && current_screen_ == screen_name && current_anchor_x_ == anchor_x &&
      current_anchor_width_ == anchor_width) {
    return;
  }

  destroySurface();
  const bool surface_created = ensureSurface(screen_name, anchor_x, anchor_width);
  setTooltipVisible(surface_created);
}

void TooltipSurface::hide() {
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

  current_screen_ = screen_name;
  current_anchor_x_ = anchor_x;
  current_anchor_width_ = anchor_width;
  return openSurface(surfaceSpec(screen, anchor_x, anchor_width));
}

LayerSurfaceSpec TooltipSurface::surfaceSpec(QScreen* screen, int anchor_x, int anchor_width) {
  const int left =
      TooltipGeometry::leftMargin(screen->geometry().width(), screen->geometry().x(), anchor_x, anchor_width);
  return {.output = screen,
          .name_space = QStringLiteral("tooltip"),
          .layer = Layer::Top,
          .anchors = Anchor::Top | Anchor::Left,
          .width = TooltipGeometry::kWidth,
          .height = kTooltipHeight,
          .margin_top = kTooltipGap,
          .margin_left = left,
          .exclusive_zone = 0,
          .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Popups/Tooltip/TooltipPopup.qml")),
          .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
          .color = Qt::transparent};
}

void TooltipSurface::onSurfaceTerminated() { setTooltipVisible(false); }
void TooltipSurface::onSurfaceConfigured() { setTooltipVisible(true); }

void TooltipSurface::destroySurface() {
  clearPendingSurface();
  closeSurface();
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
