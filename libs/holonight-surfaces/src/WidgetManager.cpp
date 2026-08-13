#include "WidgetManager.h"

#include "CompositorService.h"
#include "WidgetClock.h"
#include "WidgetCountdown.h"
#include "WidgetSurfacePolicy.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QLocale>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>
#include <QTime>
#include <QUrl>
#include <QVariantMap>

using namespace HoloNight::ShellConfig;

#include <utility>

Q_LOGGING_CATEGORY(lcWidgets, "holonight.widgets")

namespace {

constexpr int kSecsPerMinute = 60;

}  // namespace

WidgetManager::WidgetManager(WidgetDefinition definition, int margin, int index, QList<QStringList> position_blockers,
                             CompositorService* compositor, QObject* parent)
    : PerMonitorLayerManager("WidgetManager", parent),
      definition_(std::move(definition)),
      margin_(margin),
      index_(index),
      position_blockers_(std::move(position_blockers)),
      compositor_(compositor) {
  if (definition_.type == WidgetType::Clock) {
    recomputeClockStrings();
  } else {
    remaining_text_ = currentCountdownText();
  }
  tick_timer_.setSingleShot(true);
  connect(&tick_timer_, &QTimer::timeout, this, &WidgetManager::onTick);
  if (compositor_ != nullptr) {
    connect(compositor_, &CompositorService::revisionChanged, this, &WidgetManager::onCompositorRevision);
  }
}

PerMonitorLayerManager::LayerConfig WidgetManager::layerConfig() const {
  return {.layer = Holonight::Wayland::Layer::Bottom,
          .namespace_name = QStringLiteral("widget"),
          // Non-interactive in v1: an empty input region lets clicks fall through to windows below.
          .extra_flags = Qt::WindowTransparentForInput};
}

void WidgetManager::configureSurface(Holonight::Wayland::LayerSurfaceSpec& spec, QScreen* /*screen*/) {
  const WidgetSurfacePlacement placement = widgetSurfacePlacement(definition_.position, margin_);
  spec.anchors = placement.anchors;
  spec.width = placement.width;
  spec.height = placement.height;
  // -1 keeps the widget out of every exclusive zone so it never displaces or is displaced by the bar.
  spec.exclusive_zone = -1;
  spec.margin_top = placement.top_margin;
  spec.margin_right = placement.right_margin;
  spec.margin_bottom = placement.bottom_margin;
  spec.margin_left = placement.left_margin;
  spec.input_region_policy = Holonight::Wayland::InputRegionPolicy::Empty;
  // Apply occupancy-driven visibility once the compositor has configured (and first-time auto-shown)
  // the surface, so an initially-occupied workspace's widget ends up hidden.
}

void WidgetManager::onHostConfigured(const QString& monitor_name) { applyVisibility(monitor_name); }

PerMonitorLayerManager::QmlSource WidgetManager::qmlSource(QScreen* screen) {
  const QUrl url(QStringLiteral("qrc:/HolonightShell/Widgets/WidgetSurface.qml"));
  if (definition_.type == WidgetType::Clock) {
    return {.url = url,
            .initial_properties = {
                {QStringLiteral("widgetType"), QStringLiteral("clock")},
                {QStringLiteral("barMonitorName"), screen->name()},
                {QStringLiteral("timeText"), clock_time_text_},
                {QStringLiteral("secondsText"), clock_seconds_text_},
                {QStringLiteral("dateText"), clock_date_text_},
            }};
  }
  return {.url = url,
          .initial_properties = {
              {QStringLiteral("widgetType"), QStringLiteral("time-to-event")},
              {QStringLiteral("barMonitorName"), screen->name()},
              {QStringLiteral("titleText"), definition_.time_to_event.title},
              {QStringLiteral("remainingText"), remaining_text_},
              {QStringLiteral("deadlineLabelText"), deadlineLabelText()},
          }};
}

bool WidgetManager::shouldCreateSurface(QScreen* screen) const {
  const QString monitor = screen->name();
  if (!widgetTargetsMonitor(definition_.monitors, monitor)) {
    return false;
  }
  if (widgetBlockedOnMonitor(position_blockers_, monitor)) {
    if (!warned_collisions_.contains(monitor)) {
      warned_collisions_.insert(monitor);
      qCWarning(lcWidgets) << "Widget" << index_ << '(' << widgetLabel() << ") dropped on monitor" << monitor
                           << "at position" << widgetPositionToString(definition_.position)
                           << "— position already claimed by an earlier widget";
    }
    return false;
  }
  return true;
}

void WidgetManager::onCompositorRevision() {
  for (QScreen* screen : QGuiApplication::screens()) {
    applyVisibility(screen->name());
  }
}

void WidgetManager::applyVisibility(const QString& monitor_name) {
  QQuickView* view = viewForMonitor(monitor_name);
  if (view == nullptr) {
    return;
  }
  const bool visible = compositor_ != nullptr && compositor_->connected() && compositor_->hasOccupancy() &&
                       compositor_->isOutputEmpty(monitor_name);
  content_visible_.insert(monitor_name, visible);
  // Toggle the QML root rather than the QWindow: the layer surface stays mapped (its role survives),
  // while an invisible root renders nothing. Pushing the latest strings first avoids a stale frame
  // (REQ-F-016: a revealed clock shows the current wall-clock time, not the value it froze at).
  if (QQuickItem* root = view->rootObject()) {
    if (visible) {
      if (definition_.type == WidgetType::Clock) {
        recomputeClockStrings();
        pushClockStrings(root);
      } else {
        root->setProperty("remainingText", remaining_text_);
      }
    }
    root->setVisible(visible);
  }
  updateTimerState();
}

void WidgetManager::updateTimerState() {
  if (!anySurfaceVisible()) {
    tick_timer_.stop();
    return;
  }
  recomputeAndPropagate();
  // The clock never ends; the countdown stops once its deadline has passed.
  if (isPastDeadline()) {
    tick_timer_.stop();
  } else if (!tick_timer_.isActive()) {
    startTickTimer();
  }
}

void WidgetManager::onTick() {
  recomputeAndPropagate();
  if (!isPastDeadline() && anySurfaceVisible()) {
    startTickTimer();
  }
}

bool WidgetManager::isPastDeadline() const {
  if (definition_.type == WidgetType::Clock) {
    return false;
  }
  return QDateTime::currentDateTime() >= definition_.time_to_event.deadline;
}

void WidgetManager::recomputeAndPropagate() {
  if (definition_.type == WidgetType::Clock) {
    recomputeClockStrings();
    for (const auto& [monitor_name, monitor_surface] : surfaces()) {
      if (auto* root = qobject_cast<QQuickItem*>(monitor_surface.host->rootObject())) {
        pushClockStrings(root);
      }
    }
    return;
  }
  remaining_text_ = currentCountdownText();
  for (const auto& [monitor_name, monitor_surface] : surfaces()) {
    if (auto* root = qobject_cast<QQuickItem*>(monitor_surface.host->rootObject())) {
      root->setProperty("remainingText", remaining_text_);
    }
  }
}

void WidgetManager::startTickTimer() {
  if (definition_.type == WidgetType::Clock) {
    tick_timer_.start(clockTickIntervalMs(QDateTime::currentDateTime(), definition_.clock.show_seconds));
    return;
  }
  if (definition_.time_to_event.show_seconds) {
    tick_timer_.start(1000);
    return;
  }
  // Per-minute cadence aligned to the wall-clock minute boundary; the single-shot timer reschedules
  // itself each tick, so a freeze/resume re-derives alignment from the current time (no drift).
  const QTime now = QTime::currentTime();
  const int ms_to_next_minute = std::max(((kSecsPerMinute - now.second()) * 1000) - now.msec(), 1);
  tick_timer_.start(ms_to_next_minute);
}

bool WidgetManager::anySurfaceVisible() const {
  return std::ranges::any_of(surfaces(), [this](const auto& pair) {
    const auto& [monitor_name, monitor_surface] = pair;
    return content_visible_.value(monitor_name, false);
  });
}

QString WidgetManager::currentCountdownText() const {
  return formatCountdown(definition_.time_to_event.deadline, QDateTime::currentDateTime(),
                         definition_.time_to_event.show_seconds);
}

QString WidgetManager::deadlineLabelText() const {
  return formatDeadlineLabel(definition_.time_to_event.deadline, definition_.time_to_event.has_time);
}

void WidgetManager::recomputeClockStrings() {
  const QDateTime now = QDateTime::currentDateTime();
  const ClockConfig& clock = definition_.clock;

  bool locale_fell_back = false;
  const QLocale locale = resolveClockLocale(clock.locale, locale_fell_back);
  if (locale_fell_back && !locale_warned_) {
    locale_warned_ = true;
    qCWarning(lcWidgets) << "Widget" << index_ << '(' << widgetLabel() << ") has unrecognized locale" << clock.locale
                         << "— using system locale";
  }

  bool date_fell_back = false;
  clock_time_text_ = formatClockTime(now);
  clock_seconds_text_ = formatClockSeconds(now, clock.show_seconds);
  clock_date_text_ = formatClockDate(now, locale, clock.date_format, date_fell_back);
  if (date_fell_back && !date_format_warned_) {
    date_format_warned_ = true;
    qCWarning(lcWidgets) << "Widget" << index_ << '(' << widgetLabel() << ") date_format" << clock.date_format
                         << "produced no output — using default pattern";
  }
}

void WidgetManager::pushClockStrings(QQuickItem* root) const {
  if (root == nullptr) {
    return;
  }
  root->setProperty("timeText", clock_time_text_);
  root->setProperty("secondsText", clock_seconds_text_);
  root->setProperty("dateText", clock_date_text_);
}

QString WidgetManager::widgetLabel() const {
  if (definition_.type == WidgetType::Clock) {
    return QStringLiteral("clock@") + widgetPositionToString(definition_.position);
  }
  return definition_.time_to_event.title;
}
