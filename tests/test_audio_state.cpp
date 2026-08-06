#include "AudioState.h"

#include <cstdint>
#include <gtest/gtest.h>

TEST(AudioState, ConvertsPulseVolumeToPercent) {
  EXPECT_EQ(audioVolumePercentFromPulse(0, 1000), 0);
  EXPECT_EQ(audioVolumePercentFromPulse(500, 1000), 50);
  EXPECT_EQ(audioVolumePercentFromPulse(666, 1000), 67);
}

TEST(AudioState, RoundsPulseVolumeAtPercentBoundaries) {
  EXPECT_EQ(audioVolumePercentFromPulse(4, 1000), 0);
  EXPECT_EQ(audioVolumePercentFromPulse(5, 1000), 1);
  EXPECT_EQ(audioVolumePercentFromPulse(994, 1000), 99);
  EXPECT_EQ(audioVolumePercentFromPulse(995, 1000), 100);
  EXPECT_EQ(audioVolumePercentFromPulse(1000, 1000), 100);
}

TEST(AudioState, ClampsPulseVolumeToWidgetRange) {
  EXPECT_EQ(audioVolumePercentFromPulse(1500, 1000), 100);
  EXPECT_EQ(audioVolumePercentFromPulse(500, 0), 0);
}

TEST(AudioState, ClampsUnusuallyLargePulseVolumes) {
  EXPECT_EQ(audioVolumePercentFromPulse(UINT32_MAX, 1), 100);
  EXPECT_EQ(audioVolumePercentFromPulse(UINT32_MAX, UINT32_MAX), 100);
}

TEST(AudioState, ConvertsPercentToPulseVolume) {
  EXPECT_EQ(pulseVolumeFromAudioPercent(0, 1000), 0U);
  EXPECT_EQ(pulseVolumeFromAudioPercent(50, 1000), 500U);
  EXPECT_EQ(pulseVolumeFromAudioPercent(67, 1000), 670U);
}

TEST(AudioState, ConvertsPercentAtExactBoundaries) {
  EXPECT_EQ(pulseVolumeFromAudioPercent(0, UINT32_MAX), 0U);
  EXPECT_EQ(pulseVolumeFromAudioPercent(100, 1000), 1000U);
}

TEST(AudioState, ClampsPercentBeforePulseConversion) {
  EXPECT_EQ(pulseVolumeFromAudioPercent(-10, 1000), 0U);
  EXPECT_EQ(pulseVolumeFromAudioPercent(140, 1000), 1000U);
}

TEST(AudioState, BuildsSinkStateFromPulseValues) {
  const AudioSinkState state = audioSinkStateFromPulse(750, 2, 1, 1000);

  EXPECT_EQ(state.channel_count, 2);
  EXPECT_EQ(state.volume_percent, 75);
  EXPECT_TRUE(state.muted);
}

TEST(AudioState, KeepsChannelCountUsable) {
  const AudioSinkState state = audioSinkStateFromPulse(250, 0, 0, 1000);

  EXPECT_EQ(state.channel_count, 1);
  EXPECT_FALSE(state.muted);
}

TEST(AudioState, PreservesOneAndMultipleChannels) {
  const AudioSinkState mono = audioSinkStateFromPulse(250, 1, 0, 1000);
  const AudioSinkState surround = audioSinkStateFromPulse(250, 8, 0, 1000);

  EXPECT_EQ(mono.channel_count, 1);
  EXPECT_EQ(surround.channel_count, 8);
}
