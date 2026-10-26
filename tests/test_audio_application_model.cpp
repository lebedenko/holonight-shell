#include "AudioApplicationModel.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>

#include <gtest/gtest.h>

using namespace HoloNight::System;

namespace {
class Backend : public AudioBackend {
 public:
  QList<QPair<uint32_t, int>> volumes;
  QList<QPair<uint32_t, bool>> mutes;
  void start() override {}
  void stop() override {}
  void startInputLevelMonitor() override {}
  void stopInputLevelMonitor() override {}
  void setDeviceVolume(uint32_t, int) override {}
  void setDeviceMuted(uint32_t, bool) override {}
  void setSourceVolume(uint32_t, int) override {}
  void setSourceMuted(uint32_t, bool) override {}
  void setDefaultOutput(uint32_t) override {}
  void setDefaultInput(uint32_t) override {}
  void setDefaultOutputByName(const QString&) override {}
  void setDefaultInputByName(const QString&) override {}
  void setStreamVolume(uint32_t id, int value) override { volumes.append({id, value}); }
  void setStreamMuted(uint32_t id, bool value) override { mutes.append({id, value}); }
  void moveStreamToDevice(uint32_t, uint32_t) override {}
};

AudioStream stream(uint32_t id, QString name = "Teams") {
  AudioStream result;
  result.id = id;
  result.application = name;
  result.name = "Media title";
  result.volume = 30;
  return result;
}

QVariant value(AudioApplicationModel& model, int row, int role) { return model.data(model.index(row), role); }

TEST(AudioApplicationModel, TeamsNotificationsKeepMpvDiscoverableAndRowsStable) {
  AudioController controller(AudioController::SkipInit);
  AudioApplicationModel model(&controller);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  for (uint32_t id = 1; id <= 4; ++id) controller.onStreamAdded(stream(id));
  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_EQ(value(model, 0, AudioApplicationModel::Subtitle), "4 streams");
  QPersistentModelIndex teams(model.index(0));
  controller.onStreamAdded(stream(5, "mpv"));
  QPersistentModelIndex mpv(model.index(1));
  QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
  for (uint32_t id = 1; id <= 4; ++id) controller.onStreamAdded(stream(id));
  EXPECT_EQ(model.rowCount(), 2);
  EXPECT_EQ(inserted.count(), 0);
  EXPECT_TRUE(teams.isValid());
  EXPECT_TRUE(mpv.isValid());
  controller.onStreamRemoved(5);
  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_TRUE(teams.isValid());
  EXPECT_FALSE(mpv.isValid());
}

TEST(AudioApplicationModel, IdentityPriorityTrimmingAndUnknownStreams) {
  AudioController controller(AudioController::SkipInit);
  AudioApplicationModel model(&controller);
  auto a = stream(1, "");
  a.application_id = " org.browser ";
  a.process_binary = " browser ";
  controller.onStreamAdded(a);
  a.id = 2;
  a.application_id = "org.browser";
  a.application = "Browser";
  a.icon_name = "browser-icon";
  controller.onStreamAdded(a);
  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_EQ(value(model, 0, AudioApplicationModel::ApplicationKey), "id:org.browser");
  EXPECT_EQ(value(model, 0, AudioApplicationModel::Application), "Browser");
  EXPECT_EQ(value(model, 0, AudioApplicationModel::IconName), "browser-icon");
  a.id = 3;
  a.application_id.clear();
  a.process_binary = "org.browser";
  controller.onStreamAdded(a);
  EXPECT_EQ(model.rowCount(), 2);
  EXPECT_EQ(value(model, 1, AudioApplicationModel::ApplicationKey), "binary:org.browser");
  controller.onStreamAdded(stream(4, ""));
  controller.onStreamAdded(stream(5, ""));
  EXPECT_EQ(model.rowCount(), 4);
  EXPECT_EQ(value(model, 2, AudioApplicationModel::Application), "Unknown application");
  a.id = 6;
  a.application.clear();
  a.process_binary = "different";
  controller.onStreamAdded(a);
  EXPECT_EQ(value(model, 4, AudioApplicationModel::Application), "different");
}

TEST(AudioApplicationModel, AggregatesFanOutAndInheritsOnlyExplicitSettingsOnce) {
  auto backend = std::make_unique<Backend>();
  auto* calls = backend.get();
  AudioController controller(std::move(backend));
  AudioApplicationModel model(&controller);
  auto a = stream(1);
  a.muted = true;
  controller.onStreamAdded(a);
  a.id = 2;
  a.volume = 80;
  a.muted = false;
  controller.onStreamAdded(a);
  const auto key = value(model, 0, AudioApplicationModel::ApplicationKey).toString();
  EXPECT_EQ(value(model, 0, AudioApplicationModel::Volume), 80);
  EXPECT_FALSE(value(model, 0, AudioApplicationModel::Muted).toBool());
  model.setVolume(key, 150);
  EXPECT_EQ(calls->volumes, (QList<QPair<uint32_t, int>>{{1, 100}, {2, 100}}));
  EXPECT_TRUE(calls->mutes.isEmpty());
  // Commands display server results rather than optimistic values.
  EXPECT_EQ(value(model, 0, AudioApplicationModel::Volume), 80);
  a.id = 3;
  controller.onStreamAdded(a);
  EXPECT_EQ(calls->volumes.size(), 3);
  EXPECT_TRUE(calls->mutes.isEmpty());
  model.setMuted(key, true);
  EXPECT_EQ(calls->mutes.size(), 3);
  a.id = 4;
  controller.onStreamAdded(a);
  EXPECT_EQ(calls->volumes.back(), qMakePair(4U, 100));
  EXPECT_EQ(calls->mutes.back(), qMakePair(4U, true));
  const auto volume_count = calls->volumes.size();
  const auto mute_count = calls->mutes.size();
  // External changes and duplicate upserts never reapply commands.
  a.volume = 12;
  a.muted = true;
  controller.onStreamChanged(a);
  controller.onStreamAdded(a);
  EXPECT_EQ(calls->volumes.size(), volume_count);
  EXPECT_EQ(calls->mutes.size(), mute_count);
  a.id = 5;
  controller.onStreamAdded(a);
  EXPECT_EQ(calls->volumes.back(), qMakePair(5U, 100));
  EXPECT_EQ(calls->mutes.back(), qMakePair(5U, true));
  // All-muted is an aggregate of server state, even after an explicit mute command.
  for (uint32_t id = 1; id <= 5; ++id) {
    a.id = id;
    controller.onStreamChanged(a);
  }
  EXPECT_TRUE(value(model, 0, AudioApplicationModel::Muted).toBool());
  EXPECT_EQ(value(model, 0, AudioApplicationModel::Volume), 12);
  model.setVolume(key, -10);
  EXPECT_EQ(calls->volumes.back().second, 0);
  model.setMuted(key, false);
  EXPECT_FALSE(calls->mutes.back().second);
  controller.setAvailable(false);
  EXPECT_EQ(model.rowCount(), 0);
  model.setVolume(key, 50);
  const auto cleared_count = calls->volumes.size();
  controller.onStreamAdded(a);
  EXPECT_EQ(calls->volumes.size(), cleared_count);
  EXPECT_EQ(value(model, 0, AudioApplicationModel::Subtitle), "Media title");
}

TEST(AudioApplicationModel, FinalRemovalForgetsSettingsAndIdentityChangesMoveMembership) {
  auto backend = std::make_unique<Backend>();
  auto* calls = backend.get();
  AudioController controller(std::move(backend));
  AudioApplicationModel model(&controller);
  controller.onStreamAdded(stream(1));
  model.setVolume("name:Teams", 55);
  auto a = stream(1, "mpv");
  controller.onStreamChanged(a);
  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_EQ(value(model, 0, AudioApplicationModel::Application), "mpv");
  controller.onStreamAdded(stream(2));
  EXPECT_EQ(calls->volumes.size(), 1);
  controller.onStreamRemoved(2);
  controller.onStreamAdded(stream(3));
  EXPECT_EQ(calls->volumes.size(), 1);
}
}  // namespace
