#include "MprisWidgetManager.h"

#include "CompositorService.h"
#include "IconImageProvider.h"
#include "LayerSurface.h"
#include "MprisArtworkCache.h"
#include "WidgetSurfacePolicy.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>
#include <QUrl>

#include <algorithm>
#include <utility>

using namespace HoloNight::ShellConfig;

Q_LOGGING_CATEGORY(lcMprisWidget, "holonight.widgets.mpris")

namespace {
constexpr int kPositionTickIntervalMs = 500;  // 2 Hz, REQ-F-023
constexpr qint64 kMsPerMinute = 60'000;
}  // namespace

MprisWidgetManager::MprisWidgetManager(LayerShell& shell, WidgetDefinition definition, int margin, int index,
                                       QList<QStringList> position_blockers, CompositorService* compositor,
                                       MprisService* mpris, MprisArtworkCache* artwork_cache, QObject* parent)
    : PerMonitorLayerManager(shell, "MprisWidgetManager", parent),
      definition_(std::move(definition)),
      margin_(margin),
      index_(index),
      position_blockers_(std::move(position_blockers)),
      compositor_(compositor),
      mpris_(mpris),
      artwork_cache_(artwork_cache) {
  position_tick_timer_.setInterval(kPositionTickIntervalMs);
  connect(&position_tick_timer_, &QTimer::timeout, this, &MprisWidgetManager::onPositionTick);

  if (compositor_ != nullptr) {
    connect(compositor_, &CompositorService::revisionChanged, this, &MprisWidgetManager::onCompositorRevision);
  }
  if (mpris_ != nullptr) {
    position_tracking_handle_ = mpris_->acquirePositionTracking();
    connect(mpris_, &MprisService::activeSnapshotChanged, this, &MprisWidgetManager::onActiveMetadataChanged);
    connect(mpris_, &MprisService::activePositionChanged, this, &MprisWidgetManager::onActivePositionChanged);
    last_track_key_ = currentTrackKey();
  }
}

PerMonitorLayerManager::LayerConfig MprisWidgetManager::layerConfig() const {
  return {.layer = QtWayland::zwlr_layer_shell_v1::layer_bottom,
          .namespace_name = QStringLiteral("widget"),
          // Non-interactive: an empty input region lets clicks fall through (REQ-U-001).
          .extra_flags = Qt::WindowTransparentForInput};
}

void MprisWidgetManager::decorateEngine(QQmlEngine& engine) {
  engine.addImageProvider(QStringLiteral("icon"), new IconImageProvider());
}

void MprisWidgetManager::configureSurface(LayerSurface& surface, QScreen* screen) {
  const QString monitor = screen->name();
  const WidgetSurfacePlacement placement =
      widgetSurfacePlacement(definition_.position, margin_, kMprisWidgetWidth, kMprisWidgetHeight);
  surface.set_anchor(placement.anchor_flags);
  surface.set_size(placement.width, placement.height);
  // -1 keeps the widget out of every exclusive zone so it never displaces or is displaced by the bar.
  surface.set_exclusive_zone(-1);
  surface.set_margin(placement.top_margin, placement.right_margin, placement.bottom_margin, placement.left_margin);
  connect(&surface, &LayerSurface::configured, this, [this, monitor]() { applyVisibility(monitor); });
}

PerMonitorLayerManager::QmlSource MprisWidgetManager::qmlSource(QScreen* screen) {
  // Everything beyond barMonitorName arrives via live property pushes (resyncSurface/tick), not
  // setInitialProperties — MPRIS content changes continuously while mapped, unlike Clock/TTE's
  // seed-once-then-tick-push model (§2.3).
  return {.url = QUrl(QStringLiteral("qrc:/HolonightShell/Widgets/MprisWidgetSurface.qml")),
          .initial_properties = {{QStringLiteral("barMonitorName"), screen->name()}}};
}

bool MprisWidgetManager::shouldCreateSurface(QScreen* screen) const {
  const QString monitor = screen->name();
  if (!widgetTargetsMonitor(definition_.monitors, monitor)) {
    return false;
  }
  if (widgetBlockedOnMonitor(position_blockers_, monitor)) {
    if (!warned_collisions_.contains(monitor)) {
      warned_collisions_.insert(monitor);
      qCWarning(lcMprisWidget) << "MPRIS widget" << index_ << '(' << widgetLabel() << ") dropped on monitor" << monitor
                               << "at position" << widgetPositionToString(definition_.position)
                               << "— position already claimed by an earlier widget";
    }
    return false;
  }
  return true;
}

void MprisWidgetManager::onCompositorRevision() {
  for (QScreen* screen : QGuiApplication::screens()) {
    applyVisibility(screen->name());
  }
}

void MprisWidgetManager::applyVisibility(const QString& monitor_name) {
  QQuickView* view = viewForMonitor(monitor_name);
  if (view == nullptr) {
    return;
  }
  const bool visible = compositor_ != nullptr && compositor_->connected() && compositor_->hasOccupancy() &&
                       compositor_->isOutputEmpty(monitor_name);
  content_visible_.insert(monitor_name, visible);

  if (visible) {
    // REQ-F-062: fresh state before reveal, never a replay of whatever this surface last showed.
    resyncSurface(monitor_name);
  }
  if (QQuickItem* root = view->rootObject()) {
    root->setProperty("contentVisible", visible);
  }
  updateTimerState();
}

void MprisWidgetManager::updateTimerState() {
  if (!anySurfaceVisible()) {
    position_tick_timer_.stop();
    return;
  }
  if (!position_tick_timer_.isActive()) {
    position_tick_timer_.start();
  }
}

void MprisWidgetManager::onPositionTick() {
  if (mpris_ == nullptr) {
    return;
  }
  const qint64 position_us = mpris_->activePosition();
  const bool timed_out = pausedTimedOutNow();

  for (const auto& [screen, monitor_surface] : surfaces()) {
    const QString monitor = screen->name();
    if (!content_visible_.value(monitor, false)) {
      continue;
    }
    if (QQuickItem* root = monitor_surface.view->rootObject()) {
      root->setProperty("positionUs", position_us);
      root->setProperty("pausedTimedOut", timed_out);
    }
  }
}

void MprisWidgetManager::onActiveMetadataChanged() {
  const QString new_key = currentTrackKey();
  if (new_key != last_track_key_) {
    last_track_key_ = new_key;
  }
  for (const auto& [screen, monitor_surface] : surfaces()) {
    const QString monitor = screen->name();
    if (content_visible_.value(monitor, false)) {
      resyncSurface(monitor);
    }
  }
}

void MprisWidgetManager::onActivePositionChanged() { onPositionTick(); }

void MprisWidgetManager::onActivePlaybackStatusChanged() {
  if (mpris_ == nullptr) {
    return;
  }
  const QString status = mpris_->activePlaybackStatus();
  const bool timed_out = pausedTimedOutNow();
  for (const auto& [screen, monitor_surface] : surfaces()) {
    const QString monitor = screen->name();
    if (!content_visible_.value(monitor, false)) {
      continue;
    }
    if (QQuickItem* root = monitor_surface.view->rootObject()) {
      root->setProperty("playbackStatus", status);
      root->setProperty("pausedTimedOut", timed_out);
    }
  }
}

void MprisWidgetManager::resyncSurface(const QString& monitor_name) {
  if (mpris_ == nullptr) {
    return;
  }
  QQuickView* view = viewForMonitor(monitor_name);
  if (view == nullptr) {
    return;
  }
  QQuickItem* root = view->rootObject();
  if (root == nullptr) {
    return;
  }

  root->setProperty("title", mpris_->activeTitle());
  root->setProperty("artist", mpris_->activeArtist());
  root->setProperty("album", mpris_->activeAlbum());
  root->setProperty("identity", mpris_->activeIdentity());
  root->setProperty("desktopEntry", mpris_->activeDesktopEntry());
  root->setProperty("playbackStatus", mpris_->activePlaybackStatus());
  root->setProperty("lengthUs", mpris_->activeLength());
  root->setProperty("canSeek", mpris_->activeCanSeek());
  root->setProperty("positionUs", mpris_->activePosition());
  root->setProperty("pausedTimedOut", pausedTimedOutNow());

  const QString art_url = mpris_->activeArtUrl();
  if (requested_art_url_ != art_url) {
    requested_art_url_ = art_url;
    ++artwork_generation_;
  }
  const quint64 generation = artwork_generation_;
  if (art_url.isEmpty() || artwork_cache_ == nullptr) {
    root->setProperty("artworkPath", QString());
    return;
  }

  QPointer<MprisWidgetManager> self(this);
  artwork_cache_->resolve(art_url, [self, monitor_name, art_url, generation](const QString& local_path) {
    if (self.isNull()) {
      return;  // manager destroyed (e.g. config reload) before the fetch/decode finished
    }
    if (self->requested_art_url_ != art_url || self->artwork_generation_ != generation) {
      return;
    }
    if (!self->content_visible_.value(monitor_name, false)) {
      return;  // no longer visible; drop the result rather than pushing to a hidden surface
    }
    if (QQuickView* view = self->viewForMonitor(monitor_name)) {
      if (QQuickItem* root = view->rootObject()) {
        root->setProperty("artworkPath", local_path);
      }
    }
  });
}

bool MprisWidgetManager::anySurfaceVisible() const {
  return std::ranges::any_of(surfaces(), [this](const auto& pair) {
    const auto& [screen, monitor_surface] = pair;
    return content_visible_.value(screen->name(), false);
  });
}

QString MprisWidgetManager::currentTrackKey() const {
  if (mpris_ == nullptr) {
    return {};
  }
  static constexpr QChar kSep(0x1f);  // ASCII unit separator — never appears in MPRIS metadata text
  return mpris_->activeTitle() + kSep + mpris_->activeArtist() + kSep + mpris_->activeAlbum() + kSep +
         mpris_->activeArtUrl();
}

bool MprisWidgetManager::pausedTimedOutNow() const {
  if (mpris_ == nullptr || mpris_->activePlaybackStatus() != QLatin1String("Paused")) {
    return false;
  }
  const qint64 threshold_ms = static_cast<qint64>(definition_.mpris.pause_hide_minutes) * kMsPerMinute;
  return mpris_->activePauseElapsedMs() >= threshold_ms;
}

QString MprisWidgetManager::widgetLabel() const {
  return QStringLiteral("mpris@") + widgetPositionToString(definition_.position);
}
