#include "AudioService.h"

#include <QSignalSpy>

#include <gtest/gtest.h>

namespace {

AudioDevice makeDevice(uint32_t dev_id, AudioDeviceType type, uint8_t volume = 50, bool muted = false,
                       bool is_default = false) {
  AudioDevice dev;
  dev.id = dev_id;
  dev.name = QStringLiteral("device-%1").arg(dev_id);
  dev.description = QStringLiteral("Test device");
  dev.volume = volume;
  dev.muted = muted;
  dev.is_default = is_default;
  dev.type = type;
  return dev;
}

}  // namespace

TEST(AudioService, ApplyVolumeMutedAndAvailableEmitSignalsForChanges) {
  AudioService service(AudioService::SkipInit);
  QSignalSpy volume_changed(&service, &AudioService::volumeChanged);
  QSignalSpy muted_changed(&service, &AudioService::mutedChanged);
  QSignalSpy available_changed(&service, &AudioService::availableChanged);

  service.applyVolume(42);
  service.applyMuted(true);
  service.setAvailable(true);

  EXPECT_EQ(service.volume(), 42);
  EXPECT_TRUE(service.muted());
  EXPECT_TRUE(service.available());
  EXPECT_EQ(volume_changed.count(), 1);
  EXPECT_EQ(muted_changed.count(), 1);
  EXPECT_EQ(available_changed.count(), 1);
}

TEST(AudioService, ReapplyingSameStateDoesNotEmitSignals) {
  AudioService service(AudioService::SkipInit);
  service.applyVolume(42);
  service.applyMuted(true);
  service.setAvailable(true);

  QSignalSpy volume_changed(&service, &AudioService::volumeChanged);
  QSignalSpy muted_changed(&service, &AudioService::mutedChanged);
  QSignalSpy available_changed(&service, &AudioService::availableChanged);

  service.applyVolume(42);
  service.applyMuted(true);
  service.setAvailable(true);

  EXPECT_EQ(volume_changed.count(), 0);
  EXPECT_EQ(muted_changed.count(), 0);
  EXPECT_EQ(available_changed.count(), 0);
}

TEST(AudioService, SetVolumeIsNoOpWithoutBackend) {
  AudioService service(AudioService::SkipInit);
  QSignalSpy volume_changed(&service, &AudioService::volumeChanged);

  service.setVolume(75);

  EXPECT_EQ(service.volume(), 0);
  EXPECT_EQ(volume_changed.count(), 0);
}

TEST(AudioService, SourceControlMethodsAreNoOpWithoutBackend) {
  AudioService service(AudioService::SkipInit);

  service.setInputDeviceVolume(1, 65);
  service.setInputDeviceMuted(1, true);

  EXPECT_FALSE(service.available());
}

TEST(AudioService, DefaultOutputStateTracksDefaultSink) {
  AudioService service(AudioService::SkipInit);
  QSignalSpy default_output_changed(&service, &AudioService::defaultOutputIdChanged);
  QSignalSpy volume_changed(&service, &AudioService::volumeChanged);
  QSignalSpy muted_changed(&service, &AudioService::mutedChanged);

  service.onDeviceAdded(makeDevice(4, AudioDeviceType::Sink, 72, true, true));

  EXPECT_EQ(service.defaultOutputId(), 4U);
  EXPECT_EQ(service.volume(), 72);
  EXPECT_TRUE(service.muted());
  EXPECT_EQ(default_output_changed.count(), 1);
  EXPECT_EQ(volume_changed.count(), 1);
  EXPECT_EQ(muted_changed.count(), 1);
}

TEST(AudioService, DefaultOutputIgnoresDefaultSource) {
  AudioService service(AudioService::SkipInit);
  QSignalSpy default_output_changed(&service, &AudioService::defaultOutputIdChanged);

  service.onDeviceAdded(makeDevice(9, AudioDeviceType::Source, 63, false, true));

  EXPECT_NE(service.defaultOutputId(), 9U);
  EXPECT_EQ(default_output_changed.count(), 0);
}

TEST(AudioService, StartIsIdempotent) {
  AudioService service(AudioService::SkipInit);

  service.start();
  service.start();

  EXPECT_FALSE(service.available());
}

TEST(AudioService, ModelsAreNonNull) {
  AudioService service(AudioService::SkipInit);

  EXPECT_NE(service.outputs(), nullptr);
  EXPECT_NE(service.inputs(), nullptr);
  EXPECT_NE(service.playbackStreams(), nullptr);
  EXPECT_NE(service.recordingStreams(), nullptr);
}

TEST(AudioService, SinkRemovalOnlyAffectsOutputsModel) {
  AudioService service(AudioService::SkipInit);

  service.onDeviceAdded(makeDevice(1, AudioDeviceType::Sink));
  service.onDeviceAdded(makeDevice(2, AudioDeviceType::Sink));
  service.onDeviceAdded(makeDevice(1, AudioDeviceType::Source));
  service.onDeviceAdded(makeDevice(2, AudioDeviceType::Source));
  ASSERT_EQ(service.outputs()->rowCount(), 2);
  ASSERT_EQ(service.inputs()->rowCount(), 2);

  // A sink and a source can legitimately share the same PulseAudio index — removing sink id 1
  // must not touch a source that happens to also have id 1 (this is the exact bug REQ-F-005/006
  // exist to fix).
  service.onSinkRemoved(1);

  EXPECT_EQ(service.outputs()->rowCount(), 1);
  EXPECT_EQ(service.inputs()->rowCount(), 2);
}

TEST(AudioService, SourceRemovalOnlyAffectsInputsModel) {
  AudioService service(AudioService::SkipInit);

  service.onDeviceAdded(makeDevice(1, AudioDeviceType::Sink));
  service.onDeviceAdded(makeDevice(2, AudioDeviceType::Sink));
  service.onDeviceAdded(makeDevice(1, AudioDeviceType::Source));
  service.onDeviceAdded(makeDevice(2, AudioDeviceType::Source));
  ASSERT_EQ(service.outputs()->rowCount(), 2);
  ASSERT_EQ(service.inputs()->rowCount(), 2);

  service.onSourceRemoved(1);

  EXPECT_EQ(service.outputs()->rowCount(), 2);
  EXPECT_EQ(service.inputs()->rowCount(), 1);
}

TEST(AudioService, SinkRemovalOfDefaultOutputClearsDefaultOutputId) {
  AudioService service(AudioService::SkipInit);
  service.onDeviceAdded(makeDevice(4, AudioDeviceType::Sink, 72, true, true));
  ASSERT_EQ(service.defaultOutputId(), 4U);

  QSignalSpy default_output_changed(&service, &AudioService::defaultOutputIdChanged);
  service.onSinkRemoved(4);

  EXPECT_NE(service.defaultOutputId(), 4U);
  EXPECT_EQ(default_output_changed.count(), 1);
}
