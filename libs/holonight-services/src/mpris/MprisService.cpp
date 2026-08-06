#include "MprisService.h"

#include "MprisActivityTimestamp.h"
#include "MprisDbus.h"
#include "MprisMetadata.h"
#include "MprisPlayerSelector.h"

#include <QDateTime>
#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcMprisService, "holonight.mpris")

namespace {
constexpr auto kMprisPrefix = "org.mpris.MediaPlayer2.";
constexpr auto kRootIface = "org.mpris.MediaPlayer2";
constexpr auto kPlayerIface = "org.mpris.MediaPlayer2.Player";
constexpr auto kStatusPlaying = "Playing";
constexpr auto kStatusPaused = "Paused";
}  // namespace

// Per-player PropertiesChanged receiver — stores the service name so the signal can route back to
// MprisService::onPlayerPropertiesChanged without needing QDBusContext. Mirrors TrayWatcher.cpp's
// ItemPropWatcher pattern exactly.
class MprisPlayerPropWatcher : public QObject {
  Q_OBJECT
 public:
  MprisPlayerPropWatcher(QString service, MprisService* owner, QObject* parent)
      : QObject(parent), service_(std::move(service)), owner_(owner) {}

 public Q_SLOTS:
  void onPropertiesChanged(const QString& interface_name, const QVariantMap& changed, const QStringList& invalidated) {
    owner_->onPlayerPropertiesChanged(service_, interface_name, changed, invalidated);
  }
  void onSeeked(qlonglong position) { owner_->onPlayerSeeked(service_, static_cast<qint64>(position)); }

 private:
  QString service_;
  MprisService* owner_;
};

// ─── Construction ────────────────────────────────────────────────────────────

MprisService::MprisService(QObject* parent) : QObject(parent), dbus_(std::make_unique<SystemMprisDBus>()) { init(); }

MprisService::MprisService(std::unique_ptr<IMprisDBus> dbus, QObject* parent)
    : QObject(parent), dbus_(std::move(dbus)) {
  init();
}

MprisService::~MprisService() = default;

// ─── Position-tracking interest (RAII handle) ────────────────────────────────

MprisService::PositionTrackingHandle::PositionTrackingHandle(MprisService* owner) noexcept : owner_(owner) {}

MprisService::PositionTrackingHandle::~PositionTrackingHandle() {
  if (owner_ != nullptr) {
    owner_->releasePositionTracking();
  }
}

MprisService::PositionTrackingHandle::PositionTrackingHandle(PositionTrackingHandle&& other) noexcept
    : owner_(other.owner_) {
  other.owner_ = nullptr;
}

MprisService::PositionTrackingHandle& MprisService::PositionTrackingHandle::operator=(
    PositionTrackingHandle&& other) noexcept {
  if (this != &other) {
    if (owner_ != nullptr) {
      owner_->releasePositionTracking();
    }
    owner_ = other.owner_;
    other.owner_ = nullptr;
  }
  return *this;
}

MprisService::PositionTrackingHandle MprisService::acquirePositionTracking() {
  const bool was_zero = position_tracking_refcount_ == 0;
  ++position_tracking_refcount_;
  if (was_zero) {
    // Re-anchor immediately so a caller that just started watching never extrapolates from a
    // stale mark (REQ-F-021).
    refreshPositionMarkFromDbus();
    updateReconcileTimerState();
  }
  return PositionTrackingHandle(this);
}

void MprisService::releasePositionTracking() {
  --position_tracking_refcount_;
  if (position_tracking_refcount_ == 0) {
    updateReconcileTimerState();
  }
}

// ─── Position-tracking mechanics (REQ-F-021, REQ-F-022, REQ-F-024) ──────────

double MprisService::sanitizeRate(const QVariant& raw) {
  bool valid = false;
  const double value = raw.toDouble(&valid);
  if (!valid || value < 0.0 || !std::isfinite(value)) {
    return 1.0;  // REQ-F-059
  }
  return value;
}

void MprisService::reanchorPositionMark(qint64 fresh_position_us, double fresh_rate) {
  const qint64 previous_position = activePosition();
  position_mark_us_ = fresh_position_us;
  position_mark_elapsed_.restart();
  active_rate_ = fresh_rate;
  active_position_known_ = true;
  if (fresh_position_us != previous_position) {
    active_position_ = fresh_position_us;
    emit activePositionChanged();
  }
}

bool MprisService::refreshPositionMarkFromDbus() {
  if (active_service_.isEmpty()) {
    reanchorPositionMark(0, 1.0);
    return true;
  }
  const std::optional<QVariantMap> props = dbus_->getAllPlayerProperties(active_service_);
  if (!props.has_value()) {
    return false;
  }
  const int player_index = indexOfService(active_service_);
  if (player_index < 0) {
    return false;
  }
  MprisPlayer& player = players_[player_index];
  applyPlayerProperties(player, *props);
  if (!player.position_known) {
    return false;
  }
  reanchorPositionMark(player.position, player.rate);
  return true;
}

void MprisService::updateReconcileTimerState() {
  const bool should_run = position_tracking_refcount_ > 0 && active_playback_status_ == QLatin1String(kStatusPlaying);
  if (should_run && !reconcile_timer_->isActive()) {
    reconcile_timer_->start();
  } else if (!should_run && reconcile_timer_->isActive()) {
    reconcile_timer_->stop();
  }
}

void MprisService::onReconcileTimerTick() { refreshPositionMarkFromDbus(); }

qint64 MprisService::activePosition() const {
  if (position_tracking_refcount_ > 0 && active_playback_status_ == QLatin1String(kStatusPlaying)) {
    const auto extrapolated_us =
        position_mark_us_ +
        static_cast<qint64>(static_cast<double>(position_mark_elapsed_.elapsed()) * active_rate_ * 1000.0);
    return active_length_ > 0 ? std::clamp<qint64>(extrapolated_us, 0, active_length_)
                              : std::max<qint64>(0, extrapolated_us);
  }
  return position_mark_us_;
}

qint64 MprisService::activePauseElapsedMs() const { return paused_since_.isValid() ? paused_since_.elapsed() : 0; }

void MprisService::init() {
  reconcile_timer_ = new QTimer(this);
  reconcile_timer_->setInterval(20000);  // REQ-F-022: midpoint of the 10-30s band
  reconcile_timer_->setSingleShot(false);
  connect(reconcile_timer_, &QTimer::timeout, this, &MprisService::onReconcileTimerTick);

  // Arm the live watcher before the blocking snapshot so a player appearing in the gap is not
  // silently missed (REQ-C-003; matches PortalService::init()'s "watcher first, probe second").
  dbus_->connectNameOwnerChanged(this, SLOT(onNameOwnerChanged(QString, QString, QString)));
  discoverExistingPlayers();
}

// ─── Discovery ───────────────────────────────────────────────────────────────

void MprisService::discoverExistingPlayers() {
  const QStringList names = dbus_->listNames();
  for (const QString& name : names) {
    if (name.startsWith(QLatin1String(kMprisPrefix))) {
      acquirePlayer(name);
    }
  }
}

void MprisService::acquirePlayer(const QString& service) {
  if (indexOfService(service) >= 0) {
    return;
  }

  MprisPlayer player;
  player.service_name = service;

  auto* watcher = new MprisPlayerPropWatcher(service, this, this);
  if (!dbus_->connectPropertiesChanged(service, watcher,
                                       SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)))) {
    qCWarning(lcMprisService) << "failed to subscribe to PropertiesChanged for" << service;
    watcher->deleteLater();
    return;
  }
  prop_watchers_.insert(service, watcher);
  // Non-fatal if unsupported/fails: Seeked is a correction-latency optimization (REQ-F-018), not
  // required for basic position tracking (track-change/acquire/reconciliation reads still work).
  if (!dbus_->connectSeeked(service, watcher, SLOT(onSeeked(qlonglong)))) {
    qCDebug(lcMprisService) << "failed to subscribe to Seeked for" << service;
  }

  applyRootProperties(player, dbus_->getAllRootProperties(service));
  if (const auto player_properties = dbus_->getAllPlayerProperties(service); player_properties.has_value()) {
    applyPlayerProperties(player, *player_properties);
  }
  MprisActivityTimestamp::update(player, /*previous_status=*/QString(), /*previous_track_id=*/QString(),
                                 /*is_discovery=*/true, QDateTime::currentMSecsSinceEpoch());
  players_.append(player);

  qCInfo(lcMprisService) << "acquired player" << service;
  reselectActivePlayer();
}

void MprisService::releasePlayer(const QString& service) {
  const int idx = indexOfService(service);
  if (idx < 0) {
    return;
  }

  if (auto* watcher = prop_watchers_.take(service); watcher != nullptr) {
    dbus_->disconnectPropertiesChanged(service, watcher, SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    dbus_->disconnectSeeked(service, watcher, SLOT(onSeeked(qlonglong)));
    watcher->deleteLater();
  }
  players_.removeAt(idx);

  qCInfo(lcMprisService) << "released player" << service;
  reselectActivePlayer();
}

// ─── D-Bus slots ─────────────────────────────────────────────────────────────

void MprisService::onNameOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner) {
  if (!name.startsWith(QLatin1String(kMprisPrefix))) {
    return;
  }
  if (oldOwner.isEmpty() && !newOwner.isEmpty()) {
    acquirePlayer(name);
  } else if (!oldOwner.isEmpty() && newOwner.isEmpty()) {
    releasePlayer(name);
  } else if (!oldOwner.isEmpty() && !newOwner.isEmpty() && oldOwner != newOwner) {
    releasePlayer(name);
    acquirePlayer(name);
  }
}

void MprisService::onPlayerPropertiesChanged(const QString& service, const QString& interface_name,
                                             const QVariantMap& changed, const QStringList& invalidated) {
  if (interface_name != QLatin1String(kRootIface) && interface_name != QLatin1String(kPlayerIface)) {
    return;
  }
  const int idx = indexOfService(service);
  if (idx < 0) {
    return;
  }

  MprisPlayer& player = players_[idx];
  const QString previous_status = player.playback_status;
  const QString previous_track_id = player.track_id;
  if (interface_name == QLatin1String(kRootIface)) {
    applyRootProperties(player, changed);
  } else {
    applyPlayerProperties(player, changed);
  }
  refreshInvalidatedProperties(player, service, interface_name, invalidated);
  MprisActivityTimestamp::update(player, previous_status, previous_track_id, /*is_discovery=*/false,
                                 QDateTime::currentMSecsSinceEpoch());
  reselectActivePlayer();
  if (service == active_service_ && interface_name == QLatin1String(kPlayerIface)) {
    if (changed.contains(QStringLiteral("Position"))) {
      reanchorPositionMark(player.position, player.rate);
    } else if (changed.contains(QStringLiteral("Rate"))) {
      reanchorPositionMark(activePosition(), player.rate);
    }
  }
}

void MprisService::onPlayerSeeked(const QString& service, qint64 positionUs) {
  if (service != active_service_) {
    return;  // only the active player's corrections matter (REQ-F-018/021(c))
  }
  // Unconditional (not gated by position_tracking_refcount_) — re-anchors the shared mark so
  // whoever next reads activePosition() (immediately, on the widget's own tick, or on reveal) sees
  // the correction; Rate is left untouched since Seeked carries only the corrected position.
  reanchorPositionMark(std::max<qint64>(0, positionUs), active_rate_);
}

void MprisService::refreshInvalidatedProperties(MprisPlayer& player, const QString& service,
                                                const QString& interface_name, const QStringList& invalidated) {
  if (invalidated.isEmpty()) {
    return;
  }

  QVariantMap snapshot;
  if (interface_name == QLatin1String(kRootIface)) {
    snapshot = dbus_->getAllRootProperties(service);
  } else {
    const auto player_snapshot = dbus_->getAllPlayerProperties(service);
    if (!player_snapshot.has_value()) {
      return;
    }
    snapshot = *player_snapshot;
  }
  QVariantMap refreshed;
  for (const QString& property : invalidated) {
    refreshed.insert(property, snapshot.value(property));
  }
  if (interface_name == QLatin1String(kRootIface)) {
    applyRootProperties(player, refreshed);
  } else {
    applyPlayerProperties(player, refreshed);
  }
}

// ─── Property merging ────────────────────────────────────────────────────────

void MprisService::applyRootProperties(MprisPlayer& player, const QVariantMap& changed) {
  if (changed.contains(QStringLiteral("Identity"))) {
    player.identity = changed.value(QStringLiteral("Identity")).toString();
  }
  if (changed.contains(QStringLiteral("DesktopEntry"))) {
    player.desktop_entry = changed.value(QStringLiteral("DesktopEntry")).toString();
  }
}

void MprisService::applyPlayerProperties(MprisPlayer& player, const QVariantMap& changed) {
  if (changed.contains(QStringLiteral("PlaybackStatus"))) {
    player.playback_status = changed.value(QStringLiteral("PlaybackStatus")).toString();
  }
  if (changed.contains(QStringLiteral("Metadata"))) {
    const QVariantMap metadata_dict = MprisMetadata::unwrapDict(changed.value(QStringLiteral("Metadata")));
    const MprisMetadata::Fields fields = MprisMetadata::extractFields(metadata_dict);
    player.title = fields.title;
    player.artists = fields.artists;
    if (player.track_id != fields.track_id) {
      player.position = 0;
      player.position_known = false;
    }
    player.track_id = fields.track_id;
    player.album = fields.album;
    player.art_url = fields.art_url;
    player.length = fields.length;
  }
  if (changed.contains(QStringLiteral("CanGoNext"))) {
    player.can_go_next = changed.value(QStringLiteral("CanGoNext")).toBool();
  }
  if (changed.contains(QStringLiteral("CanGoPrevious"))) {
    player.can_go_previous = changed.value(QStringLiteral("CanGoPrevious")).toBool();
  }
  if (changed.contains(QStringLiteral("CanPlay"))) {
    player.can_play = changed.value(QStringLiteral("CanPlay")).toBool();
  }
  if (changed.contains(QStringLiteral("CanPause"))) {
    player.can_pause = changed.value(QStringLiteral("CanPause")).toBool();
  }
  if (changed.contains(QStringLiteral("CanControl"))) {
    player.can_control = changed.value(QStringLiteral("CanControl")).toBool();
  }
  if (changed.contains(QStringLiteral("CanSeek"))) {
    player.can_seek = changed.value(QStringLiteral("CanSeek")).toBool();
  }
  if (changed.contains(QStringLiteral("Position"))) {
    player.position = std::max<qint64>(0, changed.value(QStringLiteral("Position")).toLongLong());
    player.position_known = true;
  }
  if (changed.contains(QStringLiteral("Rate"))) {
    player.rate = sanitizeRate(changed.value(QStringLiteral("Rate")));
  }
}

// ─── Selection + snapshot ────────────────────────────────────────────────────

int MprisService::indexOfService(const QString& service) const {
  for (int i = 0; i < players_.size(); ++i) {
    if (players_.at(i).service_name == service) {
      return i;
    }
  }
  return -1;
}

void MprisService::reselectActivePlayer() {
  const int selected = MprisPlayerSelector::selectActiveIndex(players_);
  const QString previous_active_service = active_service_;
  active_service_ = selected >= 0 ? players_.at(selected).service_name : QString();
  const bool active_player_changed = active_service_ != previous_active_service;
  if (selected >= 0 && (active_player_changed || players_.at(selected).track_id != active_track_id_)) {
    if (const auto snapshot = dbus_->getAllPlayerProperties(active_service_); snapshot.has_value()) {
      applyPlayerProperties(players_[selected], *snapshot);
    }
  }
  applyActiveSnapshot(selected >= 0 ? &players_.at(selected) : nullptr, active_player_changed);
}

void MprisService::applyActiveSnapshot(  // NOLINT(readability-function-cognitive-complexity)
    const MprisPlayer* player, bool active_player_changed) {
  const bool new_has_active_player = player != nullptr;
  QString new_title;
  if (player != nullptr) {
    new_title = player->title.isEmpty() ? player->identity : player->title;
  }
  const QString new_artist = player != nullptr && !player->artists.isEmpty() ? player->artists.first() : QString();
  const QString new_identity = player != nullptr ? player->identity : QString();
  const QString new_desktop_entry = player != nullptr ? player->desktop_entry : QString();
  const QString new_playback_status = player != nullptr ? player->playback_status : QString();
  const bool new_can_go_next = player != nullptr && player->can_go_next;
  const bool new_can_go_previous = player != nullptr && player->can_go_previous;
  const bool new_can_play = player != nullptr && player->can_play;
  const bool new_can_pause = player != nullptr && player->can_pause;
  const bool new_can_control = player != nullptr && player->can_control;
  const QString new_album = player != nullptr ? player->album : QString();
  const QString new_art_url = player != nullptr ? player->art_url : QString();
  const QString new_track_id = player != nullptr ? player->track_id : QString();
  const qint64 new_length = player != nullptr ? player->length : 0;
  const bool new_can_seek = player != nullptr && player->can_seek;
  const QString previous_playback_status = active_playback_status_;
  const bool has_changed = new_has_active_player != has_active_player_;
  const bool title_changed = new_title != active_title_;
  const bool artist_changed = new_artist != active_artist_;
  const bool identity_changed = new_identity != active_identity_;
  const bool desktop_changed = new_desktop_entry != active_desktop_entry_;
  const bool status_changed = new_playback_status != active_playback_status_;
  const bool next_changed = new_can_go_next != can_go_next_;
  const bool previous_changed = new_can_go_previous != can_go_previous_;
  const bool play_changed = new_can_play != can_play_;
  const bool pause_changed = new_can_pause != can_pause_;
  const bool control_changed = new_can_control != can_control_;
  const bool album_changed = new_album != active_album_;
  const bool art_changed = new_art_url != active_art_url_;
  const bool length_changed = new_length != active_length_;
  const bool seek_changed = new_can_seek != active_can_seek_;
  const bool track_changed = new_track_id != active_track_id_;
  const qint64 position_before = activePosition();

  has_active_player_ = new_has_active_player;
  active_title_ = new_title;
  active_artist_ = new_artist;
  active_identity_ = new_identity;
  active_desktop_entry_ = new_desktop_entry;
  active_playback_status_ = new_playback_status;
  can_go_next_ = new_can_go_next;
  can_go_previous_ = new_can_go_previous;
  can_play_ = new_can_play;
  can_pause_ = new_can_pause;
  can_control_ = new_can_control;
  active_album_ = new_album;
  active_art_url_ = new_art_url;
  active_length_ = new_length;
  active_can_seek_ = new_can_seek;
  active_track_id_ = new_track_id;

  if (active_player_changed || track_changed) {
    position_mark_us_ = player != nullptr && player->position_known ? player->position : 0;
    active_rate_ = player != nullptr ? player->rate : 1.0;
    active_position_known_ = player != nullptr && player->position_known;
    position_mark_elapsed_.restart();
  } else if (previous_playback_status == QLatin1String(kStatusPlaying) &&
             new_playback_status == QLatin1String(kStatusPaused)) {
    position_mark_us_ = position_before;
    position_mark_elapsed_.restart();
  } else if (previous_playback_status == QLatin1String(kStatusPaused) &&
             new_playback_status == QLatin1String(kStatusPlaying)) {
    position_mark_elapsed_.restart();
  }
  active_position_ = position_mark_us_;

  if (has_changed) emit hasActivePlayerChanged();
  if (title_changed) emit activeTitleChanged();
  if (artist_changed) emit activeArtistChanged();
  if (identity_changed) emit activeIdentityChanged();
  if (desktop_changed) emit activeDesktopEntryChanged();
  if (status_changed) emit activePlaybackStatusChanged();
  if (next_changed) emit canGoNextChanged();
  if (previous_changed) emit canGoPreviousChanged();
  if (play_changed) emit canPlayChanged();
  if (pause_changed) emit canPauseChanged();
  if (control_changed) emit canControlChanged();
  if (album_changed) emit activeAlbumChanged();
  if (art_changed) emit activeArtUrlChanged();
  if (length_changed) emit activeLengthChanged();
  if (seek_changed) emit activeCanSeekChanged();
  if (activePosition() != position_before) emit activePositionChanged();
  if (has_changed || title_changed || artist_changed || identity_changed || desktop_changed || status_changed ||
      next_changed || previous_changed || play_changed || pause_changed || control_changed || album_changed ||
      art_changed || length_changed || seek_changed || track_changed) {
    emit activeSnapshotChanged();
  }
  // Pause-elapsed-duration tracking (REQ-F-039...044). Unconditional (not gated by
  // position_tracking_refcount_) — see MprisService.h's comment on activePauseElapsedMs().
  if (active_player_changed) {
    // REQ-F-042: an active-player switch always starts a FRESH mark, never an inherited one,
    // even if the newly-active player is itself already Paused.
    paused_since_.invalidate();
    if (new_playback_status == QLatin1String(kStatusPaused)) {
      paused_since_.start();
    }
  } else if (new_playback_status == QLatin1String(kStatusPaused) &&
             previous_playback_status != QLatin1String(kStatusPaused)) {
    paused_since_.start();
  } else if (new_playback_status != QLatin1String(kStatusPaused) &&
             previous_playback_status == QLatin1String(kStatusPaused)) {
    paused_since_.invalidate();
  }
  // Playback-status changes and active-player switches both land here, and both can flip whether
  // the reconciliation timer should be running (REQ-F-022).
  updateReconcileTimerState();
}

// ─── Commands (REQ-F-013, REQ-F-014, REQ-F-015) ──────────────────────────────

void MprisService::playPause() {
  if (!can_control_) {
    return;
  }
  const bool needs_play = active_playback_status_ == QLatin1String(kStatusPaused);
  const bool needs_pause = active_playback_status_ == QLatin1String(kStatusPlaying);
  if ((needs_play && !can_play_) || (needs_pause && !can_pause_)) {
    return;
  }
  dbus_->asyncPlayPause(active_service_);
}

void MprisService::next() {
  if (!can_control_ || !can_go_next_) {
    return;
  }
  dbus_->asyncNext(active_service_);
}

void MprisService::previous() {
  if (!can_control_ || !can_go_previous_) {
    return;
  }
  dbus_->asyncPrevious(active_service_);
}

#include "MprisService.moc"
