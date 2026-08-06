#include "MprisPlayerSelector.h"

#include <limits>

namespace MprisPlayerSelector {

int selectActiveIndex(const QList<MprisPlayer>& players) {
  // Sentinel strictly below every legal timestamp, including the -1 "unset" value a player that
  // never transitioned to Playing carries (REQ-F-005) — using -1 itself here would make a lone
  // unset Paused/Playing player lose to nothing via the strict `>` comparison below and never get
  // selected at all.
  constexpr qint64 kBelowAnyTimestamp = std::numeric_limits<qint64>::min();

  int best_playing_index = -1;
  qint64 best_playing_ts = kBelowAnyTimestamp;
  int best_paused_index = -1;
  qint64 best_paused_ts = kBelowAnyTimestamp;

  for (int i = 0; i < players.size(); ++i) {
    const MprisPlayer& player = players.at(i);
    if (player.playback_status == QStringLiteral("Playing")) {
      if (player.last_activity_timestamp_ms > best_playing_ts) {
        best_playing_ts = player.last_activity_timestamp_ms;
        best_playing_index = i;
      }
    } else if (player.playback_status == QStringLiteral("Paused")) {
      if (player.last_activity_timestamp_ms > best_paused_ts) {
        best_paused_ts = player.last_activity_timestamp_ms;
        best_paused_index = i;
      }
    }
    // "Stopped" (or any other status string) is never a candidate — REQ-F-007's explicit
    // "shall NOT select a Stopped player or use Stopped as a fallback".
  }

  return best_playing_index >= 0 ? best_playing_index : best_paused_index;
}

}  // namespace MprisPlayerSelector
