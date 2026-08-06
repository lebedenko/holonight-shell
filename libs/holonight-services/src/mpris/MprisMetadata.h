#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

// Pure functions, no QObject, no D-Bus connection — QVariant/QVariantMap in, typed data out.
// Isolates the QDBusArgument extraction traps documented in CLAUDE.md's D-Bus gotchas section so
// MprisService never touches raw D-Bus marshalling directly (REQ-F-003, REQ-F-011, REQ-F-012).
namespace MprisMetadata {

struct Fields {
  QString title;
  QStringList artists;
  QString track_id;
  QString album;     // xesam:album, REQ-F-011
  QString art_url;   // mpris:artUrl, verbatim, REQ-F-012
  qint64 length{0};  // mpris:length, microseconds, clamped >= 0, REQ-F-014
};

// Unwraps a QVariant that may hold a raw QDBusArgument encoding a nested a{sv} dict (the
// "Metadata" entry inside a GetAll/PropertiesChanged a{sv} reply) into a flat QVariantMap.
// Passes through unchanged if `raw` already holds a QVariantMap (lets FakeMprisDBus build
// Metadata dicts directly in tests without touching QDBusArgument at all).
[[nodiscard]] QVariantMap unwrapDict(const QVariant& raw);

// Unwraps a QVariant that may hold a raw QDBusArgument encoding an `as` array (xesam:artist)
// into a QStringList. Passes through if already a QStringList.
[[nodiscard]] QStringList unwrapStringList(const QVariant& raw);

// Normalizes mpris:trackid: QDBusObjectPath (spec-compliant `o`) or a plain string (`s`, seen on
// some non-compliant players) -> QString path/value, for stable trackid-changed comparisons.
[[nodiscard]] QString unwrapTrackId(const QVariant& raw);

// Runs unwrapDict/unwrapStringList/unwrapTrackId over the three fields REQ-F-003 cares about.
[[nodiscard]] Fields extractFields(const QVariantMap& metadataDict);

}  // namespace MprisMetadata
