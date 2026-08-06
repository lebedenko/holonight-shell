#include "AudioDeviceModel.h"

#include <QSignalSpy>

#include <gtest/gtest.h>

namespace {

AudioDevice makeSink(uint32_t dev_id, const char* name, uint8_t volume = 50, bool muted = false,
                     bool is_default = false) {
  AudioDevice dev;
  dev.id = dev_id;
  dev.name = QString::fromUtf8(name);
  dev.description = QStringLiteral("Test description");
  dev.volume = volume;
  dev.muted = muted;
  dev.is_default = is_default;
  dev.type = AudioDeviceType::Sink;
  return dev;
}

AudioDevice makeSource(uint32_t dev_id, const char* name, uint8_t volume = 50, bool muted = false,
                       bool is_default = false) {
  AudioDevice dev = makeSink(dev_id, name, volume, muted, is_default);
  dev.type = AudioDeviceType::Source;
  return dev;
}

}  // namespace

TEST(AudioDeviceModel, RowCountIsZeroInitially) {
  AudioDeviceModel model;
  EXPECT_EQ(model.rowCount(), 0);
}

TEST(AudioDeviceModel, ApplyAddIncreasesRowCount) {
  AudioDeviceModel model;
  QSignalSpy rows_inserted(&model, &AudioDeviceModel::rowsInserted);

  model.applyAdd(makeSink(1, "sink1"));

  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_EQ(rows_inserted.count(), 1);
}

TEST(AudioDeviceModel, ApplyAddUpdatesExistingRowByDeviceId) {
  AudioDeviceModel model;
  model.applyAdd(makeSource(1, "source1", 40));
  QSignalSpy rows_inserted(&model, &AudioDeviceModel::rowsInserted);
  QSignalSpy data_changed(&model, &AudioDeviceModel::dataChanged);

  model.applyAdd(makeSource(1, "source1", 70, true, true));

  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_EQ(rows_inserted.count(), 0);
  EXPECT_EQ(data_changed.count(), 1);
  const QModelIndex item = model.index(0);
  EXPECT_EQ(model.data(item, static_cast<int>(AudioDeviceModel::Role::Volume)).toUInt(), 70U);
  EXPECT_TRUE(model.data(item, static_cast<int>(AudioDeviceModel::Role::Muted)).toBool());
  EXPECT_TRUE(model.data(item, static_cast<int>(AudioDeviceModel::Role::IsDefault)).toBool());
}

TEST(AudioDeviceModel, DataReturnsAllRoles) {
  AudioDeviceModel model;
  model.applyAdd(makeSink(42, "alsa_output", 75, true, true));

  const QModelIndex item = model.index(0);
  EXPECT_EQ(model.data(item, static_cast<int>(AudioDeviceModel::Role::DeviceId)).toUInt(), 42U);
  EXPECT_EQ(model.data(item, static_cast<int>(AudioDeviceModel::Role::Name)).toString(), QStringLiteral("alsa_output"));
  EXPECT_EQ(model.data(item, static_cast<int>(AudioDeviceModel::Role::Volume)).toUInt(), 75U);
  EXPECT_TRUE(model.data(item, static_cast<int>(AudioDeviceModel::Role::Muted)).toBool());
  EXPECT_TRUE(model.data(item, static_cast<int>(AudioDeviceModel::Role::IsDefault)).toBool());
}

TEST(AudioDeviceModel, InvalidIndexReturnsNullVariant) {
  AudioDeviceModel model;
  EXPECT_FALSE(model.data(model.index(0)).isValid());
  EXPECT_FALSE(model.data(QModelIndex()).isValid());
}

TEST(AudioDeviceModel, ApplyChangeEmitsDataChanged) {
  AudioDeviceModel model;
  model.applyAdd(makeSink(1, "sink1", 50));
  QSignalSpy data_changed(&model, &AudioDeviceModel::dataChanged);

  model.applyChange(makeSink(1, "sink1", 80));

  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_EQ(data_changed.count(), 1);
  const QModelIndex item = model.index(0);
  EXPECT_EQ(model.data(item, static_cast<int>(AudioDeviceModel::Role::Volume)).toUInt(), 80U);
}

TEST(AudioDeviceModel, ApplyChangeUpsertIfNotFound) {
  AudioDeviceModel model;
  QSignalSpy rows_inserted(&model, &AudioDeviceModel::rowsInserted);

  model.applyChange(makeSink(99, "new_sink"));

  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_EQ(rows_inserted.count(), 1);
}

TEST(AudioDeviceModel, ApplyRemoveByDeviceId) {
  AudioDeviceModel model;
  model.applyAdd(makeSink(1, "s1"));
  model.applyAdd(makeSink(2, "s2"));
  QSignalSpy rows_removed(&model, &AudioDeviceModel::rowsRemoved);

  model.applyRemove(1);

  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_EQ(rows_removed.count(), 1);
  const QModelIndex item = model.index(0);
  EXPECT_EQ(model.data(item, static_cast<int>(AudioDeviceModel::Role::DeviceId)).toUInt(), 2U);
}

TEST(AudioDeviceModel, ApplyRemoveIgnoresMissingId) {
  AudioDeviceModel model;
  model.applyAdd(makeSink(1, "s1"));
  QSignalSpy rows_removed(&model, &AudioDeviceModel::rowsRemoved);

  model.applyRemove(999);

  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_EQ(rows_removed.count(), 0);
}

// T-014: locks AudioDeviceModel::applyRemove() to the same shape as
// AudioStreamModel::applyRemove() — linear scan from the front, remove the first id match,
// silent no-op when absent. See the mirrored test in test_audio_stream_model.cpp.
TEST(AudioDeviceModel, ApplyRemoveMiddleElementPreservesOrderOfRemaining) {
  AudioDeviceModel model;
  model.applyAdd(makeSink(1, "s1"));
  model.applyAdd(makeSink(2, "s2"));
  model.applyAdd(makeSink(3, "s3"));

  model.applyRemove(2);

  ASSERT_EQ(model.rowCount(), 2);
  EXPECT_EQ(model.data(model.index(0), static_cast<int>(AudioDeviceModel::Role::DeviceId)).toUInt(), 1U);
  EXPECT_EQ(model.data(model.index(1), static_cast<int>(AudioDeviceModel::Role::DeviceId)).toUInt(), 3U);
}

TEST(AudioDeviceModel, ClearEmitsModelReset) {
  AudioDeviceModel model;
  model.applyAdd(makeSink(1, "s1"));
  model.applyAdd(makeSink(2, "s2"));
  QSignalSpy model_reset(&model, &AudioDeviceModel::modelReset);

  model.clear();

  EXPECT_EQ(model.rowCount(), 0);
  EXPECT_EQ(model_reset.count(), 1);
}

TEST(AudioDeviceModel, RoleNamesAreCorrect) {
  AudioDeviceModel model;
  const QHash<int, QByteArray> roles = model.roleNames();

  EXPECT_EQ(roles.value(static_cast<int>(AudioDeviceModel::Role::DeviceId)), "deviceId");
  EXPECT_EQ(roles.value(static_cast<int>(AudioDeviceModel::Role::Name)), "name");
  EXPECT_EQ(roles.value(static_cast<int>(AudioDeviceModel::Role::Description)), "description");
  EXPECT_EQ(roles.value(static_cast<int>(AudioDeviceModel::Role::Volume)), "volume");
  EXPECT_EQ(roles.value(static_cast<int>(AudioDeviceModel::Role::Muted)), "muted");
  EXPECT_EQ(roles.value(static_cast<int>(AudioDeviceModel::Role::IsDefault)), "isDefault");
}
