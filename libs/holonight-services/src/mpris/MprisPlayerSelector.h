#pragma once

#include "MprisPlayer.h"

#include <QList>

// Pure, D-Bus-free active-player-selection algorithm (REQ-F-007, REQ-NF-002): no D-Bus handle,
// no QObject parent, no timers — a free function operating only on a snapshot of known players.
namespace MprisPlayerSelector {

// Returns the index into `players` selected by REQ-F-007's algorithm, or -1 if none qualifies.
[[nodiscard]] int selectActiveIndex(const QList<MprisPlayer>& players);

}  // namespace MprisPlayerSelector
