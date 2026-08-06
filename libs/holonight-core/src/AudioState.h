#pragma once

#include <cstdint>

struct AudioSinkState {
  int channel_count;
  int volume_percent;
  bool muted;
};

[[nodiscard]] int clampAudioVolumePercent(int percent);
[[nodiscard]] int audioVolumePercentFromPulse(uint32_t volume, uint32_t normal_volume);
[[nodiscard]] uint32_t pulseVolumeFromAudioPercent(int percent, uint32_t normal_volume);
[[nodiscard]] AudioSinkState audioSinkStateFromPulse(uint32_t average_volume, unsigned channel_count, int mute,
                                                     uint32_t normal_volume);
