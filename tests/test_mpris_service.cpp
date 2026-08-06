#include "MprisDbus.h"
#include "MprisService.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QVariantMap>

#include <gtest/gtest.h>
#include <memory>

namespace {

constexpr auto kVlc = "org.mpris.MediaPlayer2.vlc";
constexpr auto kSpotify = "org.mpris.MediaPlayer2.spotify";
constexpr auto kRootInterface = "org.mpris.MediaPlayer2";
constexpr auto kPlayerInterface = "org.mpris.MediaPlayer2.Player";

QVariantMap metadataDict(const QString& title, const QStringList& artists, const QString& trackId) {
  return QVariantMap{
      {QStringLiteral("xesam:title"), title},
      {QStringLiteral("xesam:artist"), artists},
      {QStringLiteral("mpris:trackid"), trackId},
  };
}

QVariantMap playerProperties(const QString& status, const QString& title, const QStringList& artists,
                             const QString& trackId, const QString& identity, const QString& desktopEntry,
                             bool canControl = true, bool canPlay = true, bool canPause = true, bool canGoNext = true,
                             bool canGoPrevious = true) {
  return QVariantMap{
      {QStringLiteral("PlaybackStatus"), status},
      {QStringLiteral("Metadata"), metadataDict(title, artists, trackId)},
      {QStringLiteral("Identity"), identity},
      {QStringLiteral("DesktopEntry"), desktopEntry},
      {QStringLiteral("CanControl"), canControl},
      {QStringLiteral("CanPlay"), canPlay},
      {QStringLiteral("CanPause"), canPause},
      {QStringLiteral("CanGoNext"), canGoNext},
      {QStringLiteral("CanGoPrevious"), canGoPrevious},
  };
}

QVariantMap rootProperties(const QString& identity, const QString& desktopEntry) {
  return {
      {QStringLiteral("Identity"), identity},
      {QStringLiteral("DesktopEntry"), desktopEntry},
  };
}

// Position-tracking additions (REQ-F-021, REQ-F-022, REQ-F-024). `Position`/`Rate` are Player
// interface properties read via getAllPlayerProperties(), not part of the Metadata dict.
QVariantMap withPositionAndRate(QVariantMap props, qint64 positionUs, double rate) {
  props.insert(QStringLiteral("Position"), positionUs);
  props.insert(QStringLiteral("Rate"), rate);
  return props;
}

// Builds an MprisService over a FakeMprisDBus, returning both so tests can drive discovery/signal
// injection after construction. The FakeMprisDBus pointer is captured before ownership transfers,
// matching the pattern FakeQmlServices uses for its own injectable-seam singletons.
std::pair<std::unique_ptr<MprisService>, FakeMprisDBus*> makeService() {
  auto dbus_owned = std::make_unique<FakeMprisDBus>();
  FakeMprisDBus* dbus = dbus_owned.get();
  auto service = std::make_unique<MprisService>(std::move(dbus_owned));
  return {std::move(service), dbus};
}

}  // namespace

// --- Discovery (REQ-F-001, REQ-C-003) ---

TEST(MprisService, DiscoversPlayerAlreadyOnBusAtConstruction) {
  auto dbus_owned = std::make_unique<FakeMprisDBus>();
  dbus_owned->seedPlayer(QString::fromLatin1(kVlc),
                         playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {QStringLiteral("Artist")},
                                          QStringLiteral("t1"), QStringLiteral("VLC"), QStringLiteral("vlc")));

  const MprisService service(std::move(dbus_owned));

  EXPECT_TRUE(service.hasActivePlayer());
  EXPECT_EQ(service.activeTitle(), QStringLiteral("Song"));
}

TEST(MprisService, LoadsIdentityAndDesktopEntryFromRootInterface) {
  auto dbus_owned = std::make_unique<FakeMprisDBus>();
  dbus_owned->seedPlayer(
      QString::fromLatin1(kVlc), rootProperties(QStringLiteral("VLC Media Player"), QStringLiteral("vlc")),
      playerProperties(QStringLiteral("Playing"), QString(), {}, QStringLiteral("t1"), QString(), QString()));

  const MprisService service(std::move(dbus_owned));

  EXPECT_EQ(service.activeIdentity(), QStringLiteral("VLC Media Player"));
  EXPECT_EQ(service.activeDesktopEntry(), QStringLiteral("vlc"));
  EXPECT_EQ(service.activeTitle(), QStringLiteral("VLC Media Player"));
}

TEST(MprisService, SubscribesBeforeRequestingInitialProperties) {
  auto dbus_owned = std::make_unique<FakeMprisDBus>();
  FakeMprisDBus* dbus = dbus_owned.get();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));

  const MprisService service(std::move(dbus_owned));

  EXPECT_FALSE(dbus->propertiesRequestedBeforeSubscription());
}

TEST(MprisService, AcquiresPlayerAppearingAfterConstruction) {
  auto [service, dbus] = makeService();
  ASSERT_FALSE(service->hasActivePlayer());

  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {QStringLiteral("Artist")},
                                    QStringLiteral("t1"), QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  EXPECT_TRUE(service->hasActivePlayer());
  EXPECT_EQ(service->activeTitle(), QStringLiteral("Song"));
}

TEST(MprisService, ReleasesPlayerThatDisappears) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {QStringLiteral("Artist")},
                                    QStringLiteral("t1"), QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  ASSERT_TRUE(service->hasActivePlayer());

  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QStringLiteral(":1.1"), QString());

  EXPECT_FALSE(service->hasActivePlayer());
}

TEST(MprisService, IgnoresNonMprisNameOwnerChanges) {
  auto [service, dbus] = makeService();
  dbus->emitNameOwnerChanged(QStringLiteral("org.freedesktop.Notifications"), QString(), QStringLiteral(":1.1"));

  EXPECT_FALSE(service->hasActivePlayer());
}

TEST(MprisService, RetriesAcquisitionAfterSubscriptionFailure) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->setConnectPropertiesChangedResult(false);
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  EXPECT_FALSE(service->hasActivePlayer());

  dbus->setConnectPropertiesChangedResult(true);
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  EXPECT_TRUE(service->hasActivePlayer());
}

TEST(MprisService, ReacquiresPlayerWhenBusOwnerIsReplaced) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Old Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("New Song"), {}, QStringLiteral("t2"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QStringLiteral(":1.1"), QStringLiteral(":1.2"));

  EXPECT_EQ(service->activeTitle(), QStringLiteral("New Song"));
}

// --- Re-selection on state changes (REQ-F-008) ---

TEST(MprisService, ReselectsWhenActivePlayerPauses) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  ASSERT_TRUE(service->hasActivePlayer());

  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Paused")}});

  EXPECT_EQ(service->activePlaybackStatus(), QStringLiteral("Paused"));
}

TEST(MprisService, BecomesInactiveWhenSolePlayerStops) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Stopped")}});

  EXPECT_FALSE(service->hasActivePlayer());
}

TEST(MprisService, RefreshesInvalidatedPlaybackStatus) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  dbus->emitPropertiesChanged(QString::fromLatin1(kVlc), QString::fromLatin1(kPlayerInterface), {},
                              {QStringLiteral("PlaybackStatus")});

  EXPECT_FALSE(service->hasActivePlayer());
}

TEST(MprisService, IgnoresPropertiesFromUnrelatedInterface) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  dbus->emitPropertiesChanged(QString::fromLatin1(kVlc), QStringLiteral("org.example.Unrelated"),
                              {{QStringLiteral("PlaybackStatus"), QStringLiteral("Stopped")}});

  EXPECT_EQ(service->activePlaybackStatus(), QStringLiteral("Playing"));
}

TEST(MprisService, RootPropertyChangesCannotOverwritePlayerState) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  dbus->emitPropertiesChanged(QString::fromLatin1(kVlc), QString::fromLatin1(kRootInterface),
                              {{QStringLiteral("PlaybackStatus"), QStringLiteral("Stopped")}});

  EXPECT_EQ(service->activePlaybackStatus(), QStringLiteral("Playing"));
}

TEST(MprisService, RefreshesInvalidatedRootProperty) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QString(), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  dbus->emitPropertiesChanged(QString::fromLatin1(kVlc), QString::fromLatin1(kRootInterface), {},
                              {QStringLiteral("Identity")});

  EXPECT_EQ(service->activeIdentity(), QString());
  EXPECT_EQ(service->activeTitle(), QString());
}

// --- Fallback logic (REQ-F-011, REQ-F-012) ---

TEST(MprisService, EmptyTitleFallsBackToIdentity) {
  auto dbus_owned = std::make_unique<FakeMprisDBus>();
  dbus_owned->seedPlayer(QString::fromLatin1(kVlc),
                         playerProperties(QStringLiteral("Playing"), QString(), {}, QStringLiteral("t1"),
                                          QStringLiteral("VLC Media Player"), QStringLiteral("vlc")));
  const MprisService service(std::move(dbus_owned));

  EXPECT_EQ(service.activeTitle(), QStringLiteral("VLC Media Player"));
}

TEST(MprisService, EmptyArtistArrayYieldsEmptyString) {
  auto dbus_owned = std::make_unique<FakeMprisDBus>();
  dbus_owned->seedPlayer(QString::fromLatin1(kVlc),
                         playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                          QStringLiteral("VLC"), QStringLiteral("vlc")));
  const MprisService service(std::move(dbus_owned));

  EXPECT_EQ(service.activeArtist(), QString());
}

// --- NOTIFY signal discipline ---

TEST(MprisService, PlaybackStatusChangeEmitsSignalExactlyOnce) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  QSignalSpy spy(service.get(), &MprisService::activePlaybackStatusChanged);
  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Paused")}});

  EXPECT_EQ(spy.count(), 1);
}

TEST(MprisService, RepeatedIdenticalStatusDoesNotEmitSignal) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  QSignalSpy spy(service.get(), &MprisService::activePlaybackStatusChanged);
  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Playing")}});

  EXPECT_EQ(spy.count(), 0);
}

TEST(MprisService, InitialSnapshotEmitsEachChangedPropertyExactlyOnce) {
  auto [service, dbus] = makeService();
  QSignalSpy has_active_spy(service.get(), &MprisService::hasActivePlayerChanged);
  QSignalSpy title_spy(service.get(), &MprisService::activeTitleChanged);
  QSignalSpy artist_spy(service.get(), &MprisService::activeArtistChanged);
  QSignalSpy identity_spy(service.get(), &MprisService::activeIdentityChanged);
  QSignalSpy desktop_entry_spy(service.get(), &MprisService::activeDesktopEntryChanged);
  QSignalSpy status_spy(service.get(), &MprisService::activePlaybackStatusChanged);
  QSignalSpy next_spy(service.get(), &MprisService::canGoNextChanged);
  QSignalSpy previous_spy(service.get(), &MprisService::canGoPreviousChanged);
  QSignalSpy play_spy(service.get(), &MprisService::canPlayChanged);
  QSignalSpy pause_spy(service.get(), &MprisService::canPauseChanged);
  QSignalSpy control_spy(service.get(), &MprisService::canControlChanged);

  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {QStringLiteral("Artist")},
                                    QStringLiteral("t1"), QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  for (const QSignalSpy* spy : {&has_active_spy, &title_spy, &artist_spy, &identity_spy, &desktop_entry_spy,
                                &status_spy, &next_spy, &previous_spy, &play_spy, &pause_spy, &control_spy}) {
    EXPECT_EQ(spy->size(), 1);
  }
}

TEST(MprisService, CoalescedSnapshotSignalObservesCompleteState) {
  auto [service, dbus] = makeService();
  int snapshot_count = 0;
  QObject::connect(service.get(), &MprisService::activeSnapshotChanged, service.get(), [&]() {
    ++snapshot_count;
    EXPECT_TRUE(service->hasActivePlayer());
    EXPECT_EQ(service->activeTitle(), QStringLiteral("Song"));
    EXPECT_EQ(service->activeArtist(), QStringLiteral("Artist"));
    EXPECT_EQ(service->activePlaybackStatus(), QStringLiteral("Playing"));
  });
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {QStringLiteral("Artist")},
                                    QStringLiteral("t1"), QStringLiteral("VLC"), QStringLiteral("vlc")));

  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  EXPECT_EQ(snapshot_count, 1);
}

// --- Capability-gated commands (REQ-F-013, REQ-F-014, REQ-F-015) ---

TEST(MprisService, PlayPauseCallsDbusWhenCapable) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc"), /*canControl=*/true,
                                    /*canPlay=*/true, /*canPause=*/true));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  service->playPause();

  EXPECT_EQ(dbus->playPauseCalls(), QStringList{QString::fromLatin1(kVlc)});
}

TEST(MprisService, PlayPauseNoOpWhenPausedPlayerLacksCanPlay) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Paused"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc"), /*canControl=*/true,
                                    /*canPlay=*/false, /*canPause=*/true));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  service->playPause();

  EXPECT_TRUE(dbus->playPauseCalls().isEmpty());
}

TEST(MprisService, AllCommandsNoOpWhenCanControlFalse) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc"), /*canControl=*/false));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  service->playPause();
  service->next();
  service->previous();

  EXPECT_TRUE(dbus->playPauseCalls().isEmpty());
  EXPECT_TRUE(dbus->nextCalls().isEmpty());
  EXPECT_TRUE(dbus->previousCalls().isEmpty());
}

TEST(MprisService, NextGatedByCanGoNext) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc"), /*canControl=*/true, true, true,
                                    /*canGoNext=*/false));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  service->next();

  EXPECT_TRUE(dbus->nextCalls().isEmpty());
}

TEST(MprisService, CommandsDoNotChangeStateOptimistically) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Paused"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  ASSERT_EQ(service->activePlaybackStatus(), QStringLiteral("Paused"));

  QSignalSpy spy(service.get(), &MprisService::activePlaybackStatusChanged);
  service->playPause();

  EXPECT_EQ(service->activePlaybackStatus(), QStringLiteral("Paused"));
  EXPECT_EQ(spy.count(), 0);
}

// --- Multiple players / REQ-F-007 integration through the full pipeline ---

TEST(MprisService, SecondPlayerStartingPlayingBecomesActive) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Paused"), QStringLiteral("VLC Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  ASSERT_EQ(service->activeTitle(), QStringLiteral("VLC Song"));

  dbus->seedPlayer(QString::fromLatin1(kSpotify),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Spotify Song"), {}, QStringLiteral("t2"),
                                    QStringLiteral("Spotify"), QStringLiteral("spotify")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kSpotify), QString(), QStringLiteral(":1.2"));

  EXPECT_EQ(service->activeTitle(), QStringLiteral("Spotify Song"));
}

// --- Position tracking: extrapolation (REQ-F-021, REQ-F-024) ---

TEST(MprisService, ExtrapolatesPositionAtNormalRateWhileTrackingAndPlaying) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                           QStringLiteral("VLC"), QStringLiteral("vlc")),
                          1'000'000, 1.0));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  auto handle = service->acquirePositionTracking();
  ASSERT_EQ(service->activePosition(), 1'000'000);

  QElapsedTimer wall;
  wall.start();
  QTest::qSleep(150);
  const qint64 elapsed_ms = wall.elapsed();
  const qint64 delta_us = service->activePosition() - 1'000'000;
  const qint64 expected_us = elapsed_ms * 1000;

  EXPECT_GE(delta_us, expected_us / 2);
  EXPECT_LE(delta_us, (expected_us * 3) + 50'000);
}

TEST(MprisService, ExtrapolatesPositionFasterAtDoubleRate) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                           QStringLiteral("VLC"), QStringLiteral("vlc")),
                          1'000'000, 2.0));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  auto handle = service->acquirePositionTracking();

  QElapsedTimer wall;
  wall.start();
  QTest::qSleep(150);
  const qint64 elapsed_ms = wall.elapsed();
  const qint64 delta_us = service->activePosition() - 1'000'000;
  const qint64 expected_us = elapsed_ms * 2 * 1000;

  EXPECT_GE(delta_us, expected_us / 2);
  EXPECT_LE(delta_us, (expected_us * 3) + 50'000);
}

TEST(MprisService, ExtrapolatesPositionSlowerAtHalfRate) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                           QStringLiteral("VLC"), QStringLiteral("vlc")),
                          1'000'000, 0.5));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  auto handle = service->acquirePositionTracking();

  QElapsedTimer wall;
  wall.start();
  QTest::qSleep(150);
  const qint64 elapsed_ms = wall.elapsed();
  const qint64 delta_us = service->activePosition() - 1'000'000;
  const auto expected_us = static_cast<qint64>(static_cast<double>(elapsed_ms) * 0.5 * 1000);

  EXPECT_GE(delta_us, expected_us / 2);
  EXPECT_LE(delta_us, (expected_us * 3) + 50'000);
}

TEST(MprisService, DoesNotExtrapolateWithoutPositionTrackingInterest) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                           QStringLiteral("VLC"), QStringLiteral("vlc")),
                          1'000'000, 2.0));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  ASSERT_EQ(service->positionTrackingRefcountForTest(), 0);

  QTest::qSleep(150);

  EXPECT_EQ(service->activePosition(), 1'000'000);
}

TEST(MprisService, DoesNotExtrapolateWhilePaused) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Paused"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                           QStringLiteral("VLC"), QStringLiteral("vlc")),
                          1'000'000, 1.0));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  auto handle = service->acquirePositionTracking();

  QTest::qSleep(150);

  EXPECT_EQ(service->activePosition(), 1'000'000);
}

TEST(MprisService, LivePositionAndRateChangesReanchorExtrapolation) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                           QStringLiteral("VLC"), QStringLiteral("vlc")),
                          1'000'000, 1.0));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  auto handle = service->acquirePositionTracking();

  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("Position"), 3'000'000LL}, {QStringLiteral("Rate"), 2.0}});
  QTest::qSleep(60);

  EXPECT_GE(service->activePosition(), 3'080'000);
  EXPECT_LT(service->activePosition(), 3'300'000);
}

TEST(MprisService, PauseFreezesAndResumeDoesNotCountPausedTime) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                           QStringLiteral("VLC"), QStringLiteral("vlc")),
                          1'000'000, 1.0));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  auto handle = service->acquirePositionTracking();
  QTest::qSleep(60);
  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Paused")}});
  const qint64 frozen = service->activePosition();
  QTest::qSleep(80);
  EXPECT_EQ(service->activePosition(), frozen);

  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Playing")}});
  EXPECT_LT(service->activePosition() - frozen, 30'000);
}

TEST(MprisService, FailedReconciliationPreservesLastValidAnchorAndRate) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                           QStringLiteral("VLC"), QStringLiteral("vlc")),
                          2'000'000, 2.0));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  auto handle = service->acquirePositionTracking();
  dbus->setPlayerPropertiesReadFailure(true);
  const qint64 before = service->activePosition();
  service->reconcilePositionForTest();
  QTest::qSleep(50);

  EXPECT_GE(service->activePosition() - before, 70'000);
}

// --- Position tracking: Seeked correction (REQ-F-018, REQ-F-021(c)) ---

TEST(MprisService, SeekedReanchorsPositionOfActivePlayerImmediately) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                           QStringLiteral("VLC"), QStringLiteral("vlc")),
                          1'000'000, 1.0));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  auto handle = service->acquirePositionTracking();
  ASSERT_EQ(service->activePosition(), 1'000'000);

  QSignalSpy position_spy(service.get(), &MprisService::activePositionChanged);
  dbus->emitSeeked(QString::fromLatin1(kVlc), 5'000'000);

  EXPECT_EQ(position_spy.count(), 1);
  const qint64 pos = service->activePosition();
  EXPECT_GE(pos, 5'000'000);
  EXPECT_LE(pos, 5'050'000);  // corrected value + a small extrapolation slop, not stale/unclamped
}

TEST(MprisService, SeekedAtCurrentAnchorStillEmitsOneEffectiveCorrection) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                           QStringLiteral("VLC"), QStringLiteral("vlc")),
                          1'000'000, 1.0));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  auto handle = service->acquirePositionTracking();
  QTest::qSleep(30);
  QSignalSpy position_spy(service.get(), &MprisService::activePositionChanged);

  dbus->emitSeeked(QString::fromLatin1(kVlc), 1'000'000);

  EXPECT_EQ(position_spy.count(), 1);
}

TEST(MprisService, SeekedFromInactivePlayerIsIgnored) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Playing"), QStringLiteral("VLC Song"), {},
                                           QStringLiteral("t1"), QStringLiteral("VLC"), QStringLiteral("vlc")),
                          1'000'000, 1.0));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  dbus->seedPlayer(
      QString::fromLatin1(kSpotify),
      withPositionAndRate(playerProperties(QStringLiteral("Paused"), QStringLiteral("Spotify Song"), {},
                                           QStringLiteral("t2"), QStringLiteral("Spotify"), QStringLiteral("spotify")),
                          0, 1.0));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kSpotify), QString(), QStringLiteral(":1.2"));
  ASSERT_EQ(service->activeTitle(), QStringLiteral("VLC Song"));  // Playing beats Paused (REQ-F-007)

  dbus->emitSeeked(QString::fromLatin1(kSpotify), 9'999'999);

  EXPECT_EQ(service->activePosition(), 1'000'000);
}

// --- Reconciliation timer configuration and gating (REQ-F-022) ---
//
// reconcile_timer_ fires on a real 20 s interval, too slow to wait out in a unit test (and no test
// seam exists to shorten it — see MprisService::init()). These tests instead assert on the
// configured interval and the start/stop gating logic via the *ForTest() accessors, which is what
// is actually verifiable without a 20 s sleep.

TEST(MprisService, ReconcileTimerIntervalIsTwentySeconds) {
  auto [service, dbus] = makeService();

  EXPECT_EQ(service->reconcileTimerIntervalMsForTest(), 20000);
}

TEST(MprisService, ReconcileTimerStaysStoppedWithoutTrackingInterest) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  EXPECT_FALSE(service->isReconcileTimerActiveForTest());
}

TEST(MprisService, ReconcileTimerStartsOnInterestAndStopsOnRelease) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  auto handle = service->acquirePositionTracking();
  EXPECT_TRUE(service->isReconcileTimerActiveForTest());

  handle = {};  // explicit release, deterministic point (not scope-end)
  EXPECT_FALSE(service->isReconcileTimerActiveForTest());
}

TEST(MprisService, ReconcileTimerStopsWhenActivePlayerPausesEvenWithInterest) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  auto handle = service->acquirePositionTracking();
  ASSERT_TRUE(service->isReconcileTimerActiveForTest());

  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Paused")}});

  EXPECT_FALSE(service->isReconcileTimerActiveForTest());
}

// --- Track change / active-player switch resets position mark (REQ-F-021(e), REQ-F-042) ---

TEST(MprisService, TrackChangeReanchorsPositionMarkFromFreshDbusRead) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Playing"), QStringLiteral("Song 1"), {},
                                           QStringLiteral("t1"), QStringLiteral("VLC"), QStringLiteral("vlc")),
                          1'000'000, 1.0));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  auto handle = service->acquirePositionTracking();
  ASSERT_EQ(service->activePosition(), 1'000'000);

  // Simulate the new track starting from 0: re-seed the underlying property blob (as the real bus
  // would report on the next GetAll) before signalling the metadata change.
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      withPositionAndRate(playerProperties(QStringLiteral("Playing"), QStringLiteral("Song 2"), {},
                                           QStringLiteral("t2"), QStringLiteral("VLC"), QStringLiteral("vlc")),
                          0, 1.0));
  dbus->emitPlayerPropertiesChanged(
      QString::fromLatin1(kVlc),
      {{QStringLiteral("Metadata"), metadataDict(QStringLiteral("Song 2"), {}, QStringLiteral("t2"))}});

  EXPECT_LT(service->activePosition(), 50'000);  // reanchored near 0, not left at the old 1,000,000
}

TEST(MprisService, ActivePlayerSwitchStartsFreshPauseMarkNotInherited) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("VLC Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  dbus->seedPlayer(QString::fromLatin1(kSpotify),
                   playerProperties(QStringLiteral("Paused"), QStringLiteral("Spotify Song"), {}, QStringLiteral("t2"),
                                    QStringLiteral("Spotify"), QStringLiteral("spotify")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kSpotify), QString(), QStringLiteral(":1.2"));
  ASSERT_EQ(service->activeTitle(), QStringLiteral("VLC Song"));  // vlc Playing still wins

  // Spotify sits Paused-but-inactive for a while before the switch.
  QTest::qSleep(120);

  // vlc stops; Spotify (Paused) becomes active. REQ-F-042: this must start a brand-new mark, not
  // one inherited from however long Spotify was already sitting Paused-but-inactive.
  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Stopped")}});
  ASSERT_EQ(service->activeTitle(), QStringLiteral("Spotify Song"));

  EXPECT_LT(service->activePauseElapsedMs(), 100);
}

// --- Pause-elapsed-duration tracking (REQ-F-039...044) ---

TEST(MprisService, PauseElapsedIsZeroWhenNeverPaused) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  EXPECT_EQ(service->activePauseElapsedMs(), 0);
}

TEST(MprisService, PauseElapsedGrowsWhileActivePlayerIsPaused) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));

  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Paused")}});
  QTest::qSleep(100);

  EXPECT_GE(service->activePauseElapsedMs(), 50);
}

TEST(MprisService, PauseElapsedResetsWhenLeavingPaused) {
  auto [service, dbus] = makeService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                    QStringLiteral("VLC"), QStringLiteral("vlc")));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Paused")}});
  QTest::qSleep(50);
  ASSERT_GT(service->activePauseElapsedMs(), 0);

  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Playing")}});

  EXPECT_EQ(service->activePauseElapsedMs(), 0);
}

// --- Full state reset on track change / active-player change ---

TEST(MprisService, NewMprisFieldsResetToDefaultsWhenNoActivePlayerRemains) {
  auto dbus_owned = std::make_unique<FakeMprisDBus>();
  QVariantMap metadata = metadataDict(QStringLiteral("Song"), {}, QStringLiteral("t1"));
  metadata.insert(QStringLiteral("xesam:album"), QStringLiteral("Album"));
  metadata.insert(QStringLiteral("mpris:artUrl"), QStringLiteral("file:///cover.jpg"));
  metadata.insert(QStringLiteral("mpris:length"), 5'000'000);
  QVariantMap props = playerProperties(QStringLiteral("Playing"), QStringLiteral("Song"), {}, QStringLiteral("t1"),
                                       QStringLiteral("VLC"), QStringLiteral("vlc"));
  props.insert(QStringLiteral("Metadata"), metadata);
  props.insert(QStringLiteral("CanSeek"), true);
  dbus_owned->seedPlayer(QString::fromLatin1(kVlc), props);
  FakeMprisDBus* dbus = dbus_owned.get();
  auto service = std::make_unique<MprisService>(std::move(dbus_owned));
  ASSERT_EQ(service->activeAlbum(), QStringLiteral("Album"));
  ASSERT_EQ(service->activeArtUrl(), QStringLiteral("file:///cover.jpg"));
  ASSERT_EQ(service->activeLength(), 5'000'000);
  ASSERT_TRUE(service->activeCanSeek());

  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Stopped")}});

  EXPECT_FALSE(service->hasActivePlayer());
  EXPECT_EQ(service->activeAlbum(), QString());
  EXPECT_EQ(service->activeArtUrl(), QString());
  EXPECT_EQ(service->activeLength(), 0);
  EXPECT_FALSE(service->activeCanSeek());
}
