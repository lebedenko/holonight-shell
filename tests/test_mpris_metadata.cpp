#include "MprisMetadata.h"

#include <QDBusObjectPath>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <gtest/gtest.h>

// Note: the QDBusArgument-encoded branches of unwrapDict/unwrapStringList are intentionally not
// exercised here — a real QDBusArgument holding a nested a{sv}/as payload is not reliably
// constructible outside an active D-Bus call (see DESIGN.md's "Metadata extraction design" /
// "Testing" note). That branch is covered by SystemMprisDBus's live path, verified manually in a
// live Wayland session with a real MPRIS player (T-021).

// --- unwrapDict ---

TEST(MprisMetadataUnwrapDict, PassesThroughQVariantMap) {
  const QVariantMap dict{{QStringLiteral("xesam:title"), QStringLiteral("Song")}};
  EXPECT_EQ(MprisMetadata::unwrapDict(QVariant(dict)), dict);
}

TEST(MprisMetadataUnwrapDict, ReturnsEmptyMapForUnrelatedType) {
  EXPECT_TRUE(MprisMetadata::unwrapDict(QVariant(42)).isEmpty());
}

TEST(MprisMetadataUnwrapDict, ReturnsEmptyMapForAbsentValue) {
  EXPECT_TRUE(MprisMetadata::unwrapDict(QVariant()).isEmpty());
}

// --- unwrapStringList ---

TEST(MprisMetadataUnwrapStringList, PassesThroughQStringList) {
  const QStringList artists{QStringLiteral("Artist A"), QStringLiteral("Artist B")};
  EXPECT_EQ(MprisMetadata::unwrapStringList(QVariant(artists)), artists);
}

TEST(MprisMetadataUnwrapStringList, DecodesQVariantListFromJsArrayBoundary) {
  // A JS array crossing a Q_INVOKABLE boundary from QML (MprisTestSeed.seedPlayer in tst_Mpris.qml)
  // marshals as QVariantList, not QStringList — a third representation distinct from both
  // QDBusArgument (real D-Bus) and QStringList (hand-built C++ test data).
  const QVariantList raw{QVariant(QStringLiteral("Artist A")), QVariant(QStringLiteral("Artist B"))};
  const QStringList expected{QStringLiteral("Artist A"), QStringLiteral("Artist B")};
  EXPECT_EQ(MprisMetadata::unwrapStringList(QVariant(raw)), expected);
}

TEST(MprisMetadataUnwrapStringList, ReturnsEmptyListForAbsentValue) {
  EXPECT_TRUE(MprisMetadata::unwrapStringList(QVariant()).isEmpty());
}

TEST(MprisMetadataUnwrapStringList, ReturnsEmptyListForEmptyStringList) {
  EXPECT_TRUE(MprisMetadata::unwrapStringList(QVariant(QStringList{})).isEmpty());
}

// --- unwrapTrackId ---

TEST(MprisMetadataUnwrapTrackId, DecodesQDBusObjectPath) {
  const QDBusObjectPath path(QStringLiteral("/org/mpris/MediaPlayer2/Track/1"));
  EXPECT_EQ(MprisMetadata::unwrapTrackId(QVariant::fromValue(path)), path.path());
}

TEST(MprisMetadataUnwrapTrackId, FallsBackToPlainString) {
  EXPECT_EQ(MprisMetadata::unwrapTrackId(QVariant(QStringLiteral("non-compliant-id"))),
            QStringLiteral("non-compliant-id"));
}

TEST(MprisMetadataUnwrapTrackId, ReturnsEmptyStringForAbsentValue) {
  EXPECT_EQ(MprisMetadata::unwrapTrackId(QVariant()), QString());
}

// --- extractFields ---

TEST(MprisMetadataExtractFields, PopulatedMetadataExtractsAllThreeFields) {
  const QVariantMap metadata{
      {QStringLiteral("xesam:title"), QStringLiteral("Song Title")},
      {QStringLiteral("xesam:artist"), QVariant(QStringList{QStringLiteral("Artist A")})},
      {QStringLiteral("mpris:trackid"), QVariant::fromValue(QDBusObjectPath(QStringLiteral("/track/1")))},
  };

  const MprisMetadata::Fields fields = MprisMetadata::extractFields(metadata);

  EXPECT_EQ(fields.title, QStringLiteral("Song Title"));
  EXPECT_EQ(fields.artists, QStringList{QStringLiteral("Artist A")});
  EXPECT_EQ(fields.track_id, QStringLiteral("/track/1"));
}

TEST(MprisMetadataExtractFields, AbsentMetadataExtractsEmptyFields) {
  const MprisMetadata::Fields fields = MprisMetadata::extractFields({});

  EXPECT_TRUE(fields.title.isEmpty());
  EXPECT_TRUE(fields.artists.isEmpty());
  EXPECT_TRUE(fields.track_id.isEmpty());
  EXPECT_TRUE(fields.album.isEmpty());
  EXPECT_TRUE(fields.art_url.isEmpty());
  EXPECT_EQ(fields.length, 0);
}

TEST(MprisMetadataExtractFields, PopulatedMetadataExtractsAlbumArtUrlAndLength) {
  const QVariantMap metadata{
      {QStringLiteral("xesam:album"), QStringLiteral("Album Title")},
      {QStringLiteral("mpris:artUrl"), QStringLiteral("https://example.com/art.jpg")},
      {QStringLiteral("mpris:length"), QVariant::fromValue<qint64>(180'000'000)},
  };

  const MprisMetadata::Fields fields = MprisMetadata::extractFields(metadata);

  EXPECT_EQ(fields.album, QStringLiteral("Album Title"));
  EXPECT_EQ(fields.art_url, QStringLiteral("https://example.com/art.jpg"));
  EXPECT_EQ(fields.length, 180'000'000);
}

TEST(MprisMetadataExtractFields, FileArtUrlIsStoredVerbatim) {
  const QVariantMap metadata{
      {QStringLiteral("mpris:artUrl"), QStringLiteral("file:///home/user/.cache/art.png")},
  };

  const MprisMetadata::Fields fields = MprisMetadata::extractFields(metadata);

  EXPECT_EQ(fields.art_url, QStringLiteral("file:///home/user/.cache/art.png"));
}

TEST(MprisMetadataExtractFields, NonPositiveLengthClampsToZero) {
  const QVariantMap zero_metadata{{QStringLiteral("mpris:length"), QVariant::fromValue<qint64>(0)}};
  const QVariantMap negative_metadata{{QStringLiteral("mpris:length"), QVariant::fromValue<qint64>(-1)}};

  EXPECT_EQ(MprisMetadata::extractFields(zero_metadata).length, 0);
  EXPECT_EQ(MprisMetadata::extractFields(negative_metadata).length, 0);
}

TEST(MprisMetadataExtractFields, EmptyArtistArrayYieldsEmptyList) {
  const QVariantMap metadata{
      {QStringLiteral("xesam:title"), QStringLiteral("Song Title")},
      {QStringLiteral("xesam:artist"), QVariant(QStringList{})},
  };

  const MprisMetadata::Fields fields = MprisMetadata::extractFields(metadata);

  EXPECT_EQ(fields.title, QStringLiteral("Song Title"));
  EXPECT_TRUE(fields.artists.isEmpty());
}
