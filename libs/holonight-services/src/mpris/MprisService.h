#pragma once

#include "MprisPlayer.h"

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>
#include <QVariantMap>

#include <memory>

class IMprisDBus;

// QML singleton. Discovers org.mpris.MediaPlayer2.* players on the session bus, tracks their
// playback state/metadata/capabilities, deterministically selects one "active" player (REQ-F-007),
// and exposes its read-only snapshot + fire-and-forget commands to QML. Every monitor's TopBar.qml
// binds the same instance, so per-monitor consistency (REQ-F-026) falls out for free.
class MprisService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(bool hasActivePlayer READ hasActivePlayer NOTIFY hasActivePlayerChanged FINAL)
  Q_PROPERTY(QString activeTitle READ activeTitle NOTIFY activeTitleChanged FINAL)
  Q_PROPERTY(QString activeArtist READ activeArtist NOTIFY activeArtistChanged FINAL)
  Q_PROPERTY(QString activeIdentity READ activeIdentity NOTIFY activeIdentityChanged FINAL)
  Q_PROPERTY(QString activeDesktopEntry READ activeDesktopEntry NOTIFY activeDesktopEntryChanged FINAL)
  Q_PROPERTY(QString activePlaybackStatus READ activePlaybackStatus NOTIFY activePlaybackStatusChanged FINAL)
  Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY canGoNextChanged FINAL)
  Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY canGoPreviousChanged FINAL)
  Q_PROPERTY(bool canPlay READ canPlay NOTIFY canPlayChanged FINAL)
  Q_PROPERTY(bool canPause READ canPause NOTIFY canPauseChanged FINAL)
  Q_PROPERTY(bool canControl READ canControl NOTIFY canControlChanged FINAL)
  // mpris-desktop-widget additions (REQ-F-016...020)
  Q_PROPERTY(QString activeAlbum READ activeAlbum NOTIFY activeAlbumChanged FINAL)
  Q_PROPERTY(QString activeArtUrl READ activeArtUrl NOTIFY activeArtUrlChanged FINAL)
  Q_PROPERTY(qint64 activePosition READ activePosition NOTIFY activePositionChanged FINAL)
  Q_PROPERTY(qint64 activeLength READ activeLength NOTIFY activeLengthChanged FINAL)
  Q_PROPERTY(bool activeCanSeek READ activeCanSeek NOTIFY activeCanSeekChanged FINAL)

 public:
  explicit MprisService(QObject* parent = nullptr);                                    // production
  explicit MprisService(std::unique_ptr<IMprisDBus> dbus, QObject* parent = nullptr);  // test seam
  ~MprisService() override;

  MprisService(const MprisService&) = delete;
  MprisService& operator=(const MprisService&) = delete;
  MprisService(MprisService&&) = delete;
  MprisService& operator=(MprisService&&) = delete;

  // RAII interest handle for centralized position tracking (REQ-F-021, REQ-F-061). Move-only; an
  // engaged handle's destructor calls the private releasePositionTracking() exactly once. A
  // default-constructed (or moved-from) handle is a no-op on destruction. A guard type is used
  // (rather than plain acquire()/release() methods) so config-reload teardown of a
  // MprisWidgetManager holding one as an ordinary member can never leak outstanding interest.
  class PositionTrackingHandle {
   public:
    PositionTrackingHandle() noexcept = default;
    ~PositionTrackingHandle();

    PositionTrackingHandle(const PositionTrackingHandle&) = delete;
    PositionTrackingHandle& operator=(const PositionTrackingHandle&) = delete;
    PositionTrackingHandle(PositionTrackingHandle&& other) noexcept;
    PositionTrackingHandle& operator=(PositionTrackingHandle&& other) noexcept;

   private:
    friend class MprisService;
    explicit PositionTrackingHandle(MprisService* owner) noexcept;
    MprisService* owner_{nullptr};
  };

  // Registers interest in centralized position tracking (extrapolation + periodic
  // reconciliation). [[nodiscard]] because a discarded handle releases on the same statement it
  // was acquired on, silently defeating the whole mechanism.
  [[nodiscard]] PositionTrackingHandle acquirePositionTracking();

  // Milliseconds since PlaybackStatus last transitioned into "Paused"; 0 if not currently Paused
  // (REQ-F-039...044). Not refcounted/gated — always accurate, always cheap, unlike position
  // extrapolation. Not a Q_PROPERTY: only MprisWidgetManager C++ code polls this on its own tick;
  // no QML binding needs NOTIFY machinery for it.
  [[nodiscard]] qint64 activePauseElapsedMs() const;

  [[nodiscard]] bool hasActivePlayer() const { return has_active_player_; }
  [[nodiscard]] QString activeTitle() const { return active_title_; }
  [[nodiscard]] QString activeArtist() const { return active_artist_; }
  [[nodiscard]] QString activeIdentity() const { return active_identity_; }
  [[nodiscard]] QString activeDesktopEntry() const { return active_desktop_entry_; }
  [[nodiscard]] QString activePlaybackStatus() const { return active_playback_status_; }
  [[nodiscard]] bool canGoNext() const { return can_go_next_; }
  [[nodiscard]] bool canGoPrevious() const { return can_go_previous_; }
  [[nodiscard]] bool canPlay() const { return can_play_; }
  [[nodiscard]] bool canPause() const { return can_pause_; }
  [[nodiscard]] bool canControl() const { return can_control_; }
  [[nodiscard]] QString activeAlbum() const { return active_album_; }
  [[nodiscard]] QString activeArtUrl() const { return active_art_url_; }
  // Computed-on-read (REQ-F-021): extrapolated from position_mark_us_ while tracking interest
  // exists and playback is Playing; otherwise the last-known raw ground-truth value.
  [[nodiscard]] qint64 activePosition() const;
  [[nodiscard]] qint64 activeLength() const { return active_length_; }
  [[nodiscard]] bool activeCanSeek() const { return active_can_seek_; }

  Q_INVOKABLE void playPause();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();

  // Test-only accessors (REQ-F-021, REQ-F-022) — mirrors OsdController's *ForTest() convention.
  // reconcile_timer_ fires on a real 20 s QTimer that unit tests cannot afford to wait out; tests
  // instead assert on its configured interval/active-state directly.
  [[nodiscard]] bool isReconcileTimerActiveForTest() const { return reconcile_timer_->isActive(); }
  [[nodiscard]] int reconcileTimerIntervalMsForTest() const { return reconcile_timer_->interval(); }
  [[nodiscard]] int positionTrackingRefcountForTest() const { return position_tracking_refcount_; }
  void reconcilePositionForTest() { onReconcileTimerTick(); }

 Q_SIGNALS:
  void hasActivePlayerChanged();
  void activeTitleChanged();
  void activeArtistChanged();
  void activeIdentityChanged();
  void activeDesktopEntryChanged();
  void activePlaybackStatusChanged();
  void canGoNextChanged();
  void canGoPreviousChanged();
  void canPlayChanged();
  void canPauseChanged();
  void canControlChanged();
  void activeAlbumChanged();
  void activeArtUrlChanged();
  void activePositionChanged();
  void activeLengthChanged();
  void activeCanSeekChanged();
  void activeSnapshotChanged();

 private Q_SLOTS:
  void onNameOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);

 public:
  // Called by MprisPlayerPropWatcher (not itself a D-Bus slot; the watcher is).
  void onPlayerPropertiesChanged(const QString& service, const QString& interface_name, const QVariantMap& changed,
                                 const QStringList& invalidated);
  // Called by MprisPlayerPropWatcher on a Player.Seeked(x) signal (REQ-F-013, REQ-F-018,
  // REQ-F-021(c)). No-op if `service` is not the current active player.
  void onPlayerSeeked(const QString& service, qint64 positionUs);

 private:
  // Reachable only via PositionTrackingHandle's destructor.
  void releasePositionTracking();

  // Position-tracking mechanics (REQ-F-021, REQ-F-022, REQ-F-024).
  bool refreshPositionMarkFromDbus();  // successful fresh read -> update player + reanchor
  void reanchorPositionMark(qint64 fresh_position_us, double fresh_rate);
  void updateReconcileTimerState();  // starts/stops reconcile_timer_ per refcount + Playing status
  void onReconcileTimerTick();
  [[nodiscard]] static double sanitizeRate(const QVariant& raw);  // REQ-F-059

  void init();                                 // constructor common path
  void discoverExistingPlayers();              // REQ-C-003: listNames() + acquire() per match
  void acquirePlayer(const QString& service);  // GetAll -> MprisPlayer -> subscribe -> reselect
  void releasePlayer(const QString& service);  // unsubscribe -> erase -> reselect
  static void applyRootProperties(MprisPlayer& player, const QVariantMap& changed);
  static void applyPlayerProperties(MprisPlayer& player, const QVariantMap& changed);
  void refreshInvalidatedProperties(MprisPlayer& player, const QString& service, const QString& interface_name,
                                    const QStringList& invalidated);
  void reselectActivePlayer();
  // nullptr player => hasActivePlayer=false. active_player_changed distinguishes "the active
  // player switched to a different service" from "the same active player's fields changed" —
  // needed for REQ-F-042's pause-timer-reset-on-switch semantics.
  void applyActiveSnapshot(const MprisPlayer* player, bool active_player_changed);
  [[nodiscard]] int indexOfService(const QString& service) const;

  std::unique_ptr<IMprisDBus> dbus_;
  QList<MprisPlayer> players_;              // insertion-ordered; REQ-F-001 registry
  QHash<QString, QObject*> prop_watchers_;  // service -> MprisPlayerPropWatcher* (owned)
  QString active_service_;                  // "" if none

  bool has_active_player_{false};
  QString active_title_;
  QString active_artist_;
  QString active_identity_;
  QString active_desktop_entry_;
  QString active_playback_status_;
  bool can_go_next_{false};
  bool can_go_previous_{false};
  bool can_play_{false};
  bool can_pause_{false};
  bool can_control_{false};
  QString active_album_;
  QString active_art_url_;
  qint64 active_position_{0};
  qint64 active_length_{0};
  bool active_can_seek_{false};
  int position_tracking_refcount_{0};
  QString active_track_id_;
  qint64 position_mark_us_{0};
  QElapsedTimer position_mark_elapsed_;
  double active_rate_{1.0};
  bool active_position_known_{false};
  QTimer* reconcile_timer_{nullptr};
  QElapsedTimer paused_since_;
};
