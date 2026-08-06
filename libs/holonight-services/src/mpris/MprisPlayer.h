#pragma once

#include <QString>
#include <QStringList>

// Plain data — deliberately not a QObject, so MprisPlayerSelector and the activity-timestamp
// logic can construct, copy, and compare instances in unit tests with no D-Bus/Qt-meta-object
// machinery (REQ-NF-001, REQ-NF-002).
struct MprisPlayer {
  QString service_name;                                // "org.mpris.MediaPlayer2.vlc"
  QString identity;                                    // REQ-F-003
  QString desktop_entry;                               // REQ-F-003, REQ-F-018
  QString playback_status{QStringLiteral("Stopped")};  // REQ-F-003
  QString title;                                       // xesam:title, REQ-F-003/011
  QStringList artists;                                 // xesam:artist, REQ-F-003/012
  QString track_id;                                    // mpris:trackid, REQ-F-003/006b
  QString album;                                       // xesam:album, REQ-F-011
  QString art_url;                                     // mpris:artUrl, verbatim, REQ-F-012
  qint64 position{0};                                  // microseconds, REQ-F-013
  double rate{1.0};                                    // sanitized Player.Rate
  bool position_known{false};                          // successful Position snapshot/live update
  qint64 length{0};                                    // mpris:length, microseconds, REQ-F-014
  bool can_go_next{false};
  bool can_go_previous{false};
  bool can_play{false};
  bool can_pause{false};
  bool can_control{false};                // REQ-F-003, REQ-F-015
  bool can_seek{false};                   // CanSeek, REQ-F-015 (mpris-desktop-widget spec)
  qint64 last_activity_timestamp_ms{-1};  // REQ-F-005; -1 = unset
};
