// Construction and MPRIS-specific state behavior live here. Persistent host policy and lifecycle
// are covered independently through PerMonitorLayerManager's host-facing seam.

#include "MprisArtworkCache.h"
#include "MprisDbus.h"
#include "MprisService.h"
#include "MprisWidgetManager.h"

#include <QString>
#include <QVariantMap>

#include <gtest/gtest.h>
#include <memory>

using namespace HoloNight::ShellConfig;

namespace {

constexpr auto kVlc = "org.mpris.MediaPlayer2.vlc";

QVariantMap metadataDict(const QString& title, const QString& artist, const QString& album, const QString& artUrl,
                         const QString& trackId) {
  return QVariantMap{
      {QStringLiteral("xesam:title"), title},     {QStringLiteral("xesam:artist"), QStringList{artist}},
      {QStringLiteral("xesam:album"), album},     {QStringLiteral("mpris:artUrl"), artUrl},
      {QStringLiteral("mpris:trackid"), trackId},
  };
}

QVariantMap playerProperties(const QString& status, const QVariantMap& metadata) {
  return QVariantMap{
      {QStringLiteral("PlaybackStatus"), status},
      {QStringLiteral("Metadata"), metadata},
      {QStringLiteral("Identity"), QStringLiteral("VLC")},
      {QStringLiteral("DesktopEntry"), QStringLiteral("vlc")},
      {QStringLiteral("CanControl"), true},
  };
}

std::pair<std::unique_ptr<MprisService>, FakeMprisDBus*> makeMprisService() {
  auto dbus_owned = std::make_unique<FakeMprisDBus>();
  FakeMprisDBus* dbus = dbus_owned.get();
  auto service = std::make_unique<MprisService>(std::move(dbus_owned));
  return {std::move(service), dbus};
}

WidgetDefinition mprisDefinition(int pause_hide_minutes) {
  WidgetDefinition def;
  def.type = WidgetType::Mpris;
  def.mpris.pause_hide_minutes = pause_hide_minutes;
  return def;
}

}  // namespace

TEST(MprisWidgetManagerConstruction, TolerateNullOptionalDependencies) {
  // occupancy/mpris/artwork_cache all default-nullable per the constructor signature; a widget
  // manager with no MprisService injected (e.g. a future headless test harness) must not crash.
  MprisWidgetManager manager(mprisDefinition(10), 32, 0, {}, nullptr, nullptr, nullptr);

  EXPECT_TRUE(manager.currentTrackKeyForTest().isEmpty());
  EXPECT_FALSE(manager.pausedTimedOutForTest());
}

TEST(MprisWidgetManagerConstruction, HoldsPositionTrackingForManagerLifetime) {
  auto [service, dbus] = makeMprisService();
  EXPECT_EQ(service->positionTrackingRefcountForTest(), 0);
  {
    MprisWidgetManager manager(mprisDefinition(10), 32, 0, {}, nullptr, service.get(), nullptr);
    EXPECT_EQ(service->positionTrackingRefcountForTest(), 1);
  }
  EXPECT_EQ(service->positionTrackingRefcountForTest(), 0);
}

TEST(MprisWidgetManagerTrackKey, ChangesWhenTitleArtistAlbumOrArtUrlChange) {
  auto [service, dbus] = makeMprisService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      playerProperties(QStringLiteral("Playing"),
                       metadataDict(QStringLiteral("Song A"), QStringLiteral("Artist"), QStringLiteral("Album"),
                                    QStringLiteral("file:///a.jpg"), QStringLiteral("t1"))));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  MprisArtworkCache artwork_cache(QStringLiteral("/tmp/holonight-mpris-widget-manager-test-unused/"));
  MprisWidgetManager manager(mprisDefinition(10), 32, 0, {}, nullptr, service.get(), &artwork_cache);

  const QString first_key = manager.currentTrackKeyForTest();
  EXPECT_FALSE(first_key.isEmpty());

  dbus->emitPlayerPropertiesChanged(
      QString::fromLatin1(kVlc),
      {{QStringLiteral("Metadata"),
        metadataDict(QStringLiteral("Song B"), QStringLiteral("Artist"), QStringLiteral("Album"),
                     QStringLiteral("file:///a.jpg"), QStringLiteral("t2"))}});

  const QString second_key = manager.currentTrackKeyForTest();
  EXPECT_NE(first_key, second_key) << "title change must produce a different composite key";
}

TEST(MprisWidgetManagerTrackKey, UnchangedWhenOnlyPlaybackStatusChanges) {
  auto [service, dbus] = makeMprisService();
  dbus->seedPlayer(
      QString::fromLatin1(kVlc),
      playerProperties(QStringLiteral("Playing"),
                       metadataDict(QStringLiteral("Song A"), QStringLiteral("Artist"), QStringLiteral("Album"),
                                    QStringLiteral("file:///a.jpg"), QStringLiteral("t1"))));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  MprisArtworkCache artwork_cache(QStringLiteral("/tmp/holonight-mpris-widget-manager-test-unused/"));
  MprisWidgetManager manager(mprisDefinition(10), 32, 0, {}, nullptr, service.get(), &artwork_cache);

  const QString before = manager.currentTrackKeyForTest();

  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Paused")}});

  EXPECT_EQ(manager.currentTrackKeyForTest(), before)
      << "a bare play/pause toggle must not look like a track change (avoids a redundant artwork re-resolve)";
}

TEST(MprisWidgetManagerPauseTimeout, FalseWhilePlaying) {
  auto [service, dbus] = makeMprisService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"),
                                    metadataDict(QStringLiteral("Song"), QStringLiteral("Artist"),
                                                 QStringLiteral("Album"), QString(), QStringLiteral("t1"))));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  MprisArtworkCache artwork_cache(QStringLiteral("/tmp/holonight-mpris-widget-manager-test-unused/"));
  MprisWidgetManager manager(mprisDefinition(1), 32, 0, {}, nullptr, service.get(), &artwork_cache);

  EXPECT_FALSE(manager.pausedTimedOutForTest());
}

TEST(MprisWidgetManagerPauseTimeout, FalseImmediatelyAfterPausingEvenWithOneMinuteThreshold) {
  auto [service, dbus] = makeMprisService();
  dbus->seedPlayer(QString::fromLatin1(kVlc),
                   playerProperties(QStringLiteral("Playing"),
                                    metadataDict(QStringLiteral("Song"), QStringLiteral("Artist"),
                                                 QStringLiteral("Album"), QString(), QStringLiteral("t1"))));
  dbus->emitNameOwnerChanged(QString::fromLatin1(kVlc), QString(), QStringLiteral(":1.1"));
  MprisArtworkCache artwork_cache(QStringLiteral("/tmp/holonight-mpris-widget-manager-test-unused/"));
  MprisWidgetManager manager(mprisDefinition(1), 32, 0, {}, nullptr, service.get(), &artwork_cache);

  dbus->emitPlayerPropertiesChanged(QString::fromLatin1(kVlc),
                                    {{QStringLiteral("PlaybackStatus"), QStringLiteral("Paused")}});

  // pause_hide_minutes = 1 (60'000ms); freshly paused elapsed time is a few ms at most.
  EXPECT_FALSE(manager.pausedTimedOutForTest());
}

// A "true" (threshold-crossed) case is not practically reachable in a fast unit test:
// pause_hide_minutes is minute-granularity (kMinPauseHideMinutes=1, REQ-C-002), so crossing it
// would require sleeping 60s+. The two boundary cases above (never-Paused, just-Paused) are the
// ones reachable without doing that; the threshold-crossed case is covered by the compositor
// smoke-check checklist (T-039) instead, same scoping rationale as the file header comment.
