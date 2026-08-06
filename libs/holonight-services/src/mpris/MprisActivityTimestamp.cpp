#include "MprisActivityTimestamp.h"

namespace MprisActivityTimestamp {

void update(MprisPlayer& player, const QString& previous_status, const QString& previous_track_id, bool is_discovery,
            qint64 now_ms) {
  const bool entering_playing = player.playback_status == QStringLiteral("Playing");
  if (is_discovery) {
    if (entering_playing) {
      player.last_activity_timestamp_ms = now_ms;  // REQ-F-006c
    }
    return;
  }
  const bool was_playing_or_stopped = previous_status != QStringLiteral("Playing");
  if (entering_playing && was_playing_or_stopped) {
    player.last_activity_timestamp_ms = now_ms;  // REQ-F-006a: Paused/Stopped -> Playing
    return;
  }
  if (entering_playing && !was_playing_or_stopped && player.track_id != previous_track_id) {
    player.last_activity_timestamp_ms = now_ms;  // REQ-F-006b: still Playing, trackid changed
  }
  // Playing -> Paused/Stopped, capability-only changes, and same-track repeats: no-op, matching
  // REQ-F-006's explicit "shall NOT update" list.
}

}  // namespace MprisActivityTimestamp
