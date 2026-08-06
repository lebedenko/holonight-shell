#include "OsdController.h"

#include <QLoggingCategory>

#include <algorithm>
#include <holonight_config/config_structs.h>
#include <type_traits>
#include <utility>
#include <variant>

Q_LOGGING_CATEGORY(lcOsd, "holonight.osd")

namespace {

// Both event kinds carry `channel`, but they are unrelated types with no common base, so reading
// the field generically needs a visit.
QString channelOf(const OsdEvent& event) {
  return std::visit([](const auto& value) { return value.channel; }, event);
}

// Deliberately narrower than the structs' own operator==, which compares every field. For a
// selection event only short_label counts: two layout names can share one code ("English (US)" and
// "English (UK)" both derive EN), and swapping between them is not a change worth interrupting the
// screen for (REQ-F-011). Level events compare value and muted, and ignore channel because the
// caller has already keyed the cache by it.
bool diffEquivalent(const OsdEvent& lhs, const OsdEvent& rhs) {
  return std::visit(
      []<typename LhsT, typename RhsT>(const LhsT& lhs_event, const RhsT& rhs_event) -> bool {
        if constexpr (!std::is_same_v<LhsT, RhsT>) {
          // A channel's kind never changes mid-run, so this is unreachable in practice; treating it
          // as "changed" rather than asserting keeps a future mixed-kind channel merely noisy.
          return false;
        } else if constexpr (std::is_same_v<LhsT, OsdLevelEvent>) {
          return lhs_event.value == rhs_event.value && lhs_event.muted == rhs_event.muted;
        } else {
          return lhs_event.short_label == rhs_event.short_label;
        }
      },
      lhs, rhs);
}

}  // namespace

OsdController::OsdController(std::vector<OsdChannelSource*> sources, NowFn now_fn, QObject* parent)
    : QObject(parent), sources_(std::move(sources)), now_fn_(std::move(now_fn)) {
  if (!now_fn_) {
    now_fn_ = [] { return std::chrono::steady_clock::now(); };
  }

  // Q_DECLARE_METATYPE alone only makes the types nameable; registering them here -- the one place
  // that owns the signals carrying them -- is what lets them cross a queued connection (REQ-NF-008).
  qRegisterMetaType<OsdLevelEvent>();
  qRegisterMetaType<OsdSelectionEvent>();
  qRegisterMetaType<OsdEvent>();

  grace_start_ = now_fn_();

  hide_timer_.setSingleShot(true);
  connect(&hide_timer_, &QTimer::timeout, this, &OsdController::onHideTimerTimeout);

  for (OsdChannelSource* source : sources_) {
    if (source == nullptr) {
      continue;
    }

    // Default AutoConnection on purpose (REQ-NF-008): every production source lives on the GUI
    // thread today, and if one ever does not, Qt promotes that connection to queued on its own
    // without this line changing.
    connect(source, &OsdChannelSource::eventObserved, this, &OsdController::onEventObserved);

    // availableChanged(bool) does not carry the channel, so the lambda supplies it.
    const QString channel = source->channel();
    connect(source, &OsdChannelSource::availableChanged, this,
            [this, channel](bool available) { onSourceAvailableChanged(channel, available); });

    // The read-once snapshot the base class documents. Today it is inert -- the only thing an
    // availability transition does is drop the cache entry, and there is none yet at construction
    // -- but it is what makes isAvailable() reachable for a source like BrightnessChannelSource,
    // whose backing property is CONSTANT and can never announce itself through the signal above.
    onSourceAvailableChanged(channel, source->isAvailable());
  }
}

void OsdController::onEventObserved(const OsdEvent& event) {
  const QString channel = channelOf(event);
  const auto cached = cache_.constFind(channel);
  const bool is_first = cached == cache_.constEnd();
  const bool changed = is_first || !diffEquivalent(*cached, event);

  // Unconditional, and ahead of every gate. A value the grace period or suppression discards must
  // still be what the next event diffs against -- otherwise un-suppressing would replay a change
  // the user already lived through. This is what makes setSuppressed(false) a pure gate flip
  // rather than a deferred queue (REQ-F-004, REQ-F-005).
  cache_.insert(channel, event);

  if (is_first) {
    return;  // silent prime: the first value is what the shell learns, not what changed
  }
  if (!changed) {
    return;
  }
  // Gates run coarse to fine. master_enabled_ and channel_enabled_ move only on a config reload,
  // inGracePeriod() flips exactly once per process, and isSuppressed() toggles on every popup
  // open/close -- so the most volatile check sits closest to the emission.
  if (!master_enabled_) {
    return;
  }
  if (!isChannelEnabled(channel)) {
    return;
  }
  if (inGracePeriod()) {
    return;
  }
  if (isSuppressed(channel)) {
    return;
  }

  current_channel_ = channel;
  restartHideTimer();

  std::visit(
      [this](const auto& value) {
        using EventT = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<EventT, OsdLevelEvent>) {
          emit displayLevelEvent(value);
        } else {
          emit displaySelectionEvent(value);
        }
      },
      event);
}

void OsdController::onHideTimerTimeout() {
  current_channel_.clear();
  emit hideRequested();
}

void OsdController::onSourceAvailableChanged(const QString& channel, bool available) {
  if (available) {
    return;
  }
  // Dropping the entry makes the next value re-prime silently. Without this, an audio backend
  // restart or a backlight reappearing after a monitor hotplug would diff the restored value
  // against whatever was current before the outage and flash an OSD nobody asked for.
  cache_.remove(channel);
}

void OsdController::setSuppressed(const QString& channel, bool suppressed) { suppressed_.insert(channel, suppressed); }

void OsdController::setChannelEnabled(const QString& channel, bool enabled) {
  channel_enabled_.insert(channel, enabled);
}

void OsdController::setEnabled(bool enabled) {
  // No forced hide when this goes false mid-display: the hide timer is already running and will
  // clear the surface on schedule. Adding a hide here would be the only path that emits
  // hideRequested() without a preceding display.
  master_enabled_ = enabled;
}

void OsdController::setTimeoutMs(int timeout_ms) {
  const int clamped = std::clamp(timeout_ms, OsdConfig::kMinTimeoutMs, OsdConfig::kMaxTimeoutMs);
  if (clamped != timeout_ms) {
    qCWarning(lcOsd) << "osd.timeout" << timeout_ms << "ms is out of range; using" << clamped << "ms";
  }
  // A running timer keeps the duration it was started with; the new value applies from the next
  // display. Restarting here would extend an OSD the user is already watching.
  timeout_ms_ = clamped;
}

bool OsdController::inGracePeriod() const { return (now_fn_() - grace_start_) < kGracePeriod; }

bool OsdController::isChannelEnabled(const QString& channel) const { return channel_enabled_.value(channel, true); }

bool OsdController::isSuppressed(const QString& channel) const { return suppressed_.value(channel, false); }

void OsdController::restartHideTimer() {
  // start() on a running QTimer restarts it rather than stacking a second shot, which is exactly
  // REQ-F-017's "the OSD persists for a full timeout after the last change".
  hide_timer_.start(timeout_ms_);
}
