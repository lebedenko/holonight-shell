#include "MprisMetadata.h"

#include <QDBusArgument>
#include <QDBusObjectPath>

namespace MprisMetadata {

QVariantMap unwrapDict(const QVariant& raw) {
  if (raw.userType() == qMetaTypeId<QDBusArgument>()) {
    QVariantMap result;
    raw.value<QDBusArgument>() >> result;
    return result;
  }
  if (raw.userType() == QMetaType::QVariantMap) {
    return raw.toMap();
  }
  return {};
}

QStringList unwrapStringList(const QVariant& raw) {
  if (raw.userType() == qMetaTypeId<QDBusArgument>()) {
    QStringList result;
    raw.value<QDBusArgument>() >> result;
    return result;
  }
  // QMetaType::QVariantList covers a JS array crossing a Q_INVOKABLE boundary from QML (used by
  // the MprisTestSeed test seam) — a third representation distinct from both QDBusArgument (real
  // D-Bus) and QStringList (hand-built C++ test data). QVariant::toStringList() converts either
  // container type generically.
  if (raw.userType() == QMetaType::QStringList || raw.userType() == QMetaType::QVariantList) {
    return raw.toStringList();
  }
  return {};
}

QString unwrapTrackId(const QVariant& raw) {
  if (raw.userType() == qMetaTypeId<QDBusObjectPath>()) {
    return raw.value<QDBusObjectPath>().path();
  }
  return raw.toString();
}

Fields extractFields(const QVariantMap& metadataDict) {
  const qint64 raw_length = metadataDict.value(QStringLiteral("mpris:length")).toLongLong();
  return Fields{
      .title = metadataDict.value(QStringLiteral("xesam:title")).toString(),
      .artists = unwrapStringList(metadataDict.value(QStringLiteral("xesam:artist"))),
      .track_id = unwrapTrackId(metadataDict.value(QStringLiteral("mpris:trackid"))),
      .album = metadataDict.value(QStringLiteral("xesam:album")).toString(),
      .art_url = metadataDict.value(QStringLiteral("mpris:artUrl")).toString(),
      .length = raw_length > 0 ? raw_length : 0,
  };
}

}  // namespace MprisMetadata
