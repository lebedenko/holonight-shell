#pragma once

#include "ConfigService.h"
#include "MprisService.h"
#include "PerMonitorLayerManager.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

class CompositorService;
class MprisArtworkCache;
class QQuickItem;
class QQuickView;
class QScreen;

// Manages the MPRIS "now playing" desktop widget across the monitors its WidgetDefinition targets
// (docs/sdd/mpris-desktop-widget/DESIGN.md §2.3). A sibling of WidgetManager rather than a third
// branch inside it (§5a) — MPRIS content is signal-driven and quiescent, the opposite of
// WidgetManager's tick-recomputes-a-string-from-scratch model. Holds NO local position/pause
// tracker of its own: MprisService centrally owns that state (§2.2/§5b) since there is exactly one
// process-global active player; this class only samples MprisService::activePosition()/
// activePauseElapsedMs() on its own 2 Hz tick and pushes the result into each visible surface.
//
// QML property contract pushed into MprisWidgetSurface.qml's root (§4.5, plus two presentation-
// state booleans DESIGN.md's prose names but its interface table omits — pinned down here):
//   title / artist / album / identity / desktopEntry / playbackStatus / lengthUs / canSeek / positionUs / artworkPath
//   contentVisible   — occupancy-driven; QML alone owns the root visible binding
//   pausedTimedOut   — true once activePauseElapsedMs() crosses this instance's pause_hide_minutes
//                       threshold while Paused (§3.2's "Paused-and-hidden-by-timeout" state);
//                       distinct from contentVisible, which the surface stays occupancy-visible
//                       under — pausedTimedOut is a presentation-only fade, not an occupancy hide.
class MprisWidgetManager : public PerMonitorLayerManager {
  Q_OBJECT

 public:
  MprisWidgetManager(HoloNight::ShellConfig::WidgetDefinition definition, int margin, int index,
                     QList<QStringList> position_blockers, CompositorService* compositor, MprisService* mpris,
                     MprisArtworkCache* artwork_cache, QObject* parent = nullptr);

  // Test-only accessors (mirrors MprisService's *ForTest() convention) for logic this class adds
  // beyond what WidgetSurfacePolicy/MprisService already unit-test — occupancy/timer/surface-push
  // behavior itself is covered through the persistent host seam.
  [[nodiscard]] QString currentTrackKeyForTest() const { return currentTrackKey(); }
  [[nodiscard]] bool pausedTimedOutForTest() const { return pausedTimedOutNow(); }
  [[nodiscard]] bool positionTimerActiveForTest() const { return position_tick_timer_.isActive(); }

 protected:
  [[nodiscard]] LayerConfig layerConfig() const override;
  // Registers the "icon" image provider on this surface's own QQmlEngine — each PerMonitorLayerManager
  // subclass owns a separate QQuickView/engine, so image://icon/ (used by the identity badge and the
  // artwork fallback, REQ-F-008/051) is invalid until registered here, the same as every other
  // surface manager that resolves app icons (LayerShellManager, LauncherSurface, TrayMenuSurface, ...).
  void decorateEngine(QQmlEngine& engine) override;
  void configureSurface(Holonight::Wayland::LayerSurfaceSpec& spec, QScreen* screen) override;
  void onHostConfigured(const QString& monitor_name) override;
  [[nodiscard]] QmlSource qmlSource(QScreen* screen) override;
  [[nodiscard]] bool shouldCreateSurface(QScreen* screen) const override;

 private Q_SLOTS:
  void onCompositorRevision();
  void onPositionTick();
  // Connected to every MprisService NOTIFY signal that carries per-track content (title, artist,
  // album, art URL, identity, desktop entry, length, canSeek, hasActivePlayer) — anything
  // resyncSurface() re-pulls. NOT connected to activePositionChanged (the 2 Hz tick already reads
  // it fresh every 500 ms; a Seeked correction needs no dedicated push, per §3.1) or the playback
  // control-capability properties (canGoNext etc. — irrelevant to a read-only ambient display).
  void onActiveMetadataChanged();
  // Separate from onActiveMetadataChanged() so a bare Play/Pause toggle doesn't re-trigger an
  // artwork-cache resolve() for unchanged art (§3.1's metadata/playback_status split).
  void onActivePlaybackStatusChanged();
  void onActivePositionChanged();

 private:
  // Show/hide the surface for one monitor to match current occupancy; on reveal, resyncs first
  // (REQ-F-062) so the surface never flashes stale content before disappearing again next tick.
  void applyVisibility(const QString& monitor_name);
  // REQ-F-062: pulls every fresh MPRIS field (metadata, status, position, length, canSeek) from
  // MprisService and pushes it to one monitor's QML root in a single frame, then kicks off artwork
  // resolution for the current track. Used for occupancy-reveal, initial surface creation, and any
  // metadata change on an already-visible monitor.
  void resyncSurface(const QString& monitor_name);
  // Starts/stops only the 2 Hz presentation timer. Position reconciliation remains acquired for
  // this manager's whole lifetime so hidden widgets cannot make the shared service go stale.
  void updateTimerState();
  [[nodiscard]] bool anySurfaceVisible() const;
  // Synthetic per-manager track identifier (title+artist+album+artUrl) used as MprisArtworkCache's
  // `track_id` and to detect "the track changed" locally (§3.1: "derived by comparing to last-seen
  // track_id per this manager") — MprisService does not expose its internal mpris:trackid publicly
  // (§4.1's interface list), so this manager derives its own composite key from what IS exposed.
  [[nodiscard]] QString currentTrackKey() const;
  // True once activePauseElapsedMs() has crossed this instance's configured pause_hide_minutes
  // while playback is Paused; shared by onPositionTick() and resyncSurface() to avoid duplicating
  // the comparison.
  [[nodiscard]] bool pausedTimedOutNow() const;
  [[nodiscard]] QString widgetLabel() const;

  HoloNight::ShellConfig::WidgetDefinition definition_;
  int margin_;
  int index_;
  QList<QStringList> position_blockers_;
  CompositorService* compositor_;
  MprisService* mpris_;
  MprisArtworkCache* artwork_cache_;
  QTimer position_tick_timer_;  // 2 Hz (REQ-F-023), runs only while anySurfaceVisible()
  MprisService::PositionTrackingHandle position_tracking_handle_;
  QString last_track_key_;  // currentTrackKey() as of the most recent metadata push
  QString requested_art_url_;
  quint64 artwork_generation_{0};
  QHash<QString, bool> content_visible_;
  mutable QSet<QString> warned_collisions_;
};
