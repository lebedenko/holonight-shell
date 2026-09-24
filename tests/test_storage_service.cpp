#include "StorageService.h"

#include <QAbstractItemModelTester>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

#include <StorageBackend.h>
#include <gtest/gtest.h>
#include <utility>
using namespace HoloNight::System;
namespace {
class FakeStorage : public StorageBackend {
 public:
  QList<StorageDrive> drives;
  QList<StorageVolume> volumes;
  QList<StorageResult> calls;
  void start() override {}
  void stop() override {}
  void execute(const QString& targetId, StorageOperation operation, const QString& target) override {
    calls.append({.requestId = targetId,
                  .targetId = target,
                  .operation = operation,
                  .mountPath = {},
                  .errorName = {},
                  .errorMessage = {}});
  }
  void publish() {
    emit snapshotChanged(drives, volumes, true);
    QCoreApplication::processEvents();
  }
};
StorageDrive device(QString targetId = "drive", bool removable = true, bool media = true) {
  StorageDrive driveRecord;
  driveRecord.id = std::move(targetId);
  driveRecord.model = "USB SSD";
  driveRecord.removable = removable;
  driveRecord.mediaPresent = media;
  driveRecord.canPowerOff = true;
  return driveRecord;
}
StorageVolume volume(const QString& targetId = "volume", QString drive = "drive") {
  StorageVolume record;
  record.id = targetId;
  record.driveId = std::move(drive);
  record.label = targetId;
  record.usage = "filesystem";
  record.canMount = true;
  return record;
}
}  // namespace
TEST(ShellStorage, FiltersInfrastructureWithoutHidingHintSystemData) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  backend.drives = {device()};
  auto data = volume();
  data.hintSystem = true;
  data.mountPoints = {"/mnt/backup"};
  auto root = volume("root");
  root.mountPoints = {"/"};
  auto home = volume("home");
  home.mountPoints = {"/home"};
  auto efi = volume("efi");
  efi.partitionType = "c12a7328-f81f-11d2-ba4b-00a0c93ec93b";
  auto swap = volume("swap");
  swap.usage = "other";
  swap.filesystemType = "swap";
  auto ignored = volume("ignored");
  ignored.hintIgnore = true;
  auto loop = volume("loop");
  loop.loop = true;
  auto container = volume("container");
  container.partitionContainer = true;
  backend.volumes = {data, root, home, efi, swap, ignored, loop, container};
  backend.publish();
  ASSERT_EQ(model.count(), 1);
  EXPECT_EQ(model.data(model.index(0), static_cast<int>(StorageService::Role::TargetId)).toString(), "volume");
}
TEST(ShellStorage, PreservesMultipleVolumesAndDoesNotDuplicateUnlockedBacking) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  backend.drives = {device()};
  auto locked = volume("locked");
  locked.usage = "crypto";
  locked.locked = true;
  locked.canMount = false;
  backend.volumes = {volume(), locked};
  backend.publish();
  EXPECT_EQ(model.count(), 2);
  locked.locked = false;
  auto clear = volume("clear");
  clear.cryptoBackingId = locked.id;
  backend.volumes = {volume(), locked, clear};
  backend.publish();
  EXPECT_EQ(model.count(), 2);
}
TEST(ShellStorage, PowerOffPresentsHiddenSiblingScopeAndRejectsChangedConfirmation) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  auto first = device("first");
  first.siblingId = "physical";
  auto second = device("second");
  second.siblingId = "physical";
  auto visible = volume("visible", "first");
  auto hidden = volume("hidden", "second");
  hidden.hintIgnore = true;
  backend.drives = {first, second};
  backend.volumes = {visible, hidden};
  backend.publish();
  model.requestPowerOff("first");
  EXPECT_TRUE(model.confirmationText().contains("hidden"));
  backend.volumes.append(volume("new", "first"));
  backend.publish();
  model.confirmPowerOff();
  ASSERT_TRUE(QTest::qWaitFor([&] { return !model.errorMessage().isEmpty(); }));
  EXPECT_TRUE(backend.calls.isEmpty());
}

TEST(ShellStorage, FiltersFixedDisksAndEmptyReadersAccordingToConsumerPolicy) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  backend.drives = {device("empty", true, false), device("fixed", false), device("external")};
  auto mounted = volume("mounted", "fixed");
  mounted.mountPoints = {"/mnt/backup"};
  backend.volumes = {mounted, volume("unmounted", "fixed"), volume("usb", "external")};
  backend.publish();
  EXPECT_EQ(model.count(), 1);
}
