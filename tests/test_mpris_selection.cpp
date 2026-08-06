#include "MprisActivityTimestamp.h"
#include "MprisPlayer.h"
#include "MprisPlayerSelector.h"

#include <QList>
#include <QString>

#include <gtest/gtest.h>

namespace {

MprisPlayer makePlayer(const QString& service_name, const QString& playback_status, qint64 last_activity_ts) {
  MprisPlayer player;
  player.service_name = service_name;
  player.playback_status = playback_status;
  player.last_activity_timestamp_ms = last_activity_ts;
  return player;
}

}  // namespace

// --- MprisPlayerSelector::selectActiveIndex (REQ-F-007) ---

TEST(MprisPlayerSelector, PlayingByHighestRecencyWins) {
  const QList<MprisPlayer> players{
      makePlayer(QStringLiteral("A"), QStringLiteral("Playing"), 100),
      makePlayer(QStringLiteral("B"), QStringLiteral("Playing"), 50),
      makePlayer(QStringLiteral("C"), QStringLiteral("Paused"), 150),
  };

  EXPECT_EQ(MprisPlayerSelector::selectActiveIndex(players), 0);  // Player-A
}

TEST(MprisPlayerSelector, PausedByHighestRecencyWinsWhenNoPlayingExists) {
  const QList<MprisPlayer> players{
      makePlayer(QStringLiteral("A"), QStringLiteral("Paused"), 100),
      makePlayer(QStringLiteral("C"), QStringLiteral("Paused"), 150),
  };

  EXPECT_EQ(MprisPlayerSelector::selectActiveIndex(players), 1);  // Player-C
}

TEST(MprisPlayerSelector, ReturnsNegativeOneWhenAllStopped) {
  const QList<MprisPlayer> players{
      makePlayer(QStringLiteral("A"), QStringLiteral("Stopped"), 100),
      makePlayer(QStringLiteral("B"), QStringLiteral("Stopped"), 200),
  };

  EXPECT_EQ(MprisPlayerSelector::selectActiveIndex(players), -1);
}

TEST(MprisPlayerSelector, ReturnsNegativeOneForEmptyRegistry) {
  EXPECT_EQ(MprisPlayerSelector::selectActiveIndex({}), -1);
}

TEST(MprisPlayerSelector, SelectsSolitaryPlayerWithUnsetTimestamp) {
  // A player discovered already Paused (never transitioned through Playing) keeps its
  // last_activity_timestamp_ms at the -1 "unset" sentinel (REQ-F-005). It must still be
  // selectable — regression test for a bug where the selector's internal "no candidate yet"
  // sentinel also used -1, so a lone unset player was never selected at all.
  const QList<MprisPlayer> players{makePlayer(QStringLiteral("A"), QStringLiteral("Paused"), -1)};

  EXPECT_EQ(MprisPlayerSelector::selectActiveIndex(players), 0);
}

TEST(MprisPlayerSelector, SelectsSolitaryPlayingPlayerWithUnsetTimestamp) {
  const QList<MprisPlayer> players{makePlayer(QStringLiteral("A"), QStringLiteral("Playing"), -1)};

  EXPECT_EQ(MprisPlayerSelector::selectActiveIndex(players), 0);
}

TEST(MprisPlayerSelector, TieBreaksToEarlierIndexedPlayer) {
  const QList<MprisPlayer> players{
      makePlayer(QStringLiteral("A"), QStringLiteral("Playing"), 100),
      makePlayer(QStringLiteral("B"), QStringLiteral("Playing"), 100),
  };

  EXPECT_EQ(MprisPlayerSelector::selectActiveIndex(players), 0);
}

// --- MprisActivityTimestamp::update (REQ-F-005, REQ-F-006) ---

class MprisActivityTimestampTest : public ::testing::Test {
 protected:
  static constexpr qint64 kNowMs = 1'000'000;
};

// "shall update" branches

TEST_F(MprisActivityTimestampTest, PausedToPlayingUpdatesTimestamp) {
  MprisPlayer player;
  player.playback_status = QStringLiteral("Playing");
  player.track_id = QStringLiteral("track-1");

  MprisActivityTimestamp::update(player, /*previous_status=*/QStringLiteral("Paused"),
                                 /*previous_track_id=*/QStringLiteral("track-1"), /*is_discovery=*/false, kNowMs);

  EXPECT_EQ(player.last_activity_timestamp_ms, kNowMs);
}

TEST_F(MprisActivityTimestampTest, StoppedToPlayingUpdatesTimestamp) {
  MprisPlayer player;
  player.playback_status = QStringLiteral("Playing");
  player.track_id = QStringLiteral("track-1");

  MprisActivityTimestamp::update(player, /*previous_status=*/QStringLiteral("Stopped"),
                                 /*previous_track_id=*/QString(), /*is_discovery=*/false, kNowMs);

  EXPECT_EQ(player.last_activity_timestamp_ms, kNowMs);
}

TEST_F(MprisActivityTimestampTest, TrackChangeWhilePlayingUpdatesTimestamp) {
  MprisPlayer player;
  player.playback_status = QStringLiteral("Playing");
  player.track_id = QStringLiteral("track-2");

  MprisActivityTimestamp::update(player, /*previous_status=*/QStringLiteral("Playing"),
                                 /*previous_track_id=*/QStringLiteral("track-1"), /*is_discovery=*/false, kNowMs);

  EXPECT_EQ(player.last_activity_timestamp_ms, kNowMs);
}

TEST_F(MprisActivityTimestampTest, DiscoveryTimeInitAlreadyPlayingUpdatesTimestamp) {
  MprisPlayer player;
  player.playback_status = QStringLiteral("Playing");

  MprisActivityTimestamp::update(player, /*previous_status=*/QString(), /*previous_track_id=*/QString(),
                                 /*is_discovery=*/true, kNowMs);

  EXPECT_EQ(player.last_activity_timestamp_ms, kNowMs);
}

// "shall NOT update" branches

TEST_F(MprisActivityTimestampTest, RepeatedSameTrackDoesNotUpdate) {
  MprisPlayer player;
  player.playback_status = QStringLiteral("Playing");
  player.track_id = QStringLiteral("track-1");
  player.last_activity_timestamp_ms = 42;

  MprisActivityTimestamp::update(player, /*previous_status=*/QStringLiteral("Playing"),
                                 /*previous_track_id=*/QStringLiteral("track-1"), /*is_discovery=*/false, kNowMs);

  EXPECT_EQ(player.last_activity_timestamp_ms, 42);
}

TEST_F(MprisActivityTimestampTest, PlayingToPausedDoesNotUpdate) {
  MprisPlayer player;
  player.playback_status = QStringLiteral("Paused");
  player.track_id = QStringLiteral("track-1");
  player.last_activity_timestamp_ms = 42;

  MprisActivityTimestamp::update(player, /*previous_status=*/QStringLiteral("Playing"),
                                 /*previous_track_id=*/QStringLiteral("track-1"), /*is_discovery=*/false, kNowMs);

  EXPECT_EQ(player.last_activity_timestamp_ms, 42);
}

TEST_F(MprisActivityTimestampTest, CapabilityOnlyChangeDoesNotUpdate) {
  MprisPlayer player;
  player.playback_status = QStringLiteral("Playing");
  player.track_id = QStringLiteral("track-1");
  player.last_activity_timestamp_ms = 42;
  player.can_play = true;

  MprisActivityTimestamp::update(player, /*previous_status=*/QStringLiteral("Playing"),
                                 /*previous_track_id=*/QStringLiteral("track-1"), /*is_discovery=*/false, kNowMs);

  EXPECT_EQ(player.last_activity_timestamp_ms, 42);
}

TEST_F(MprisActivityTimestampTest, DiscoveryTimeInitNotPlayingDoesNotUpdate) {
  MprisPlayer player;
  player.playback_status = QStringLiteral("Paused");

  MprisActivityTimestamp::update(player, /*previous_status=*/QString(), /*previous_track_id=*/QString(),
                                 /*is_discovery=*/true, kNowMs);

  EXPECT_EQ(player.last_activity_timestamp_ms, -1);
}
