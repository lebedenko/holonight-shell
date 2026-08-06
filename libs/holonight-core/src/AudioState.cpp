#include "AudioState.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

int clampAudioVolumePercent(int percent) { return std::clamp(percent, 0, 100); }

int audioVolumePercentFromPulse(uint32_t volume, uint32_t normal_volume) {
  if (normal_volume == 0) {
    return 0;
  }
  if (volume >= normal_volume) {
    return 100;
  }
  return clampAudioVolumePercent(qRound(static_cast<double>(volume) / static_cast<double>(normal_volume) * 100.0));
}

uint32_t pulseVolumeFromAudioPercent(int percent, uint32_t normal_volume) {
  const int clamped_percent = clampAudioVolumePercent(percent);
  if (clamped_percent == 0) {
    return 0;
  }
  if (clamped_percent == 100) {
    return normal_volume;
  }

  return static_cast<uint32_t>(
      std::llround(static_cast<double>(clamped_percent) / 100.0 * static_cast<double>(normal_volume)));
}

AudioSinkState audioSinkStateFromPulse(uint32_t average_volume, unsigned channel_count, int mute,
                                       uint32_t normal_volume) {
  return {
      .channel_count = std::max(1, static_cast<int>(channel_count)),
      .volume_percent = audioVolumePercentFromPulse(average_volume, normal_volume),
      .muted = mute != 0,
  };
}
