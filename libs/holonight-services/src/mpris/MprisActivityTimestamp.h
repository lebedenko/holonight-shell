#pragma once

#include "MprisPlayer.h"

#include <QString>

// Pure, D-Bus-free activity-timestamp update logic (REQ-F-005, REQ-F-006). Takes the *previous*
// playback_status/track_id explicitly as parameters (not re-derived from anything stateful) so it
// is unit-testable the same way as MprisPlayerSelector::selectActiveIndex.
namespace MprisActivityTimestamp {

// Mutates `player.last_activity_timestamp_ms` to `now_ms` if and only if REQ-F-006's three
// "shall update" conditions are met. Must be called after `player.playback_status`/`track_id`
// have already been overwritten with their new values.
void update(MprisPlayer& player, const QString& previous_status, const QString& previous_track_id, bool is_discovery,
            qint64 now_ms);

}  // namespace MprisActivityTimestamp
