#include "NotificationRuleModel.h"
#include "NotificationServer.h"
#include "NotificationService.h"
#include "StorageService.h"

#include <QAbstractItemModelTester>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingReply>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <StorageBackend.h>
#include <gtest/gtest.h>
#include <memory>
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
    calls.append({
        .requestId = targetId,
        .targetId = target,
        .operation = operation,
        .mountPath = {},
        .errorName = {},
        .errorMessage = {},
    });
  }
  void publish() {
    emit snapshotChanged(drives, volumes, true);
    QCoreApplication::processEvents();
  }
  // Completes the most recently dispatched request as the real UDisks2Backend eventually
  // would, correlating by the controller-assigned stepId recorded in `calls`.
  void completeLast(bool success, const QString& mountPath = {}) {
    const auto request = calls.last();
    emit operationFinished({
        .requestId = request.requestId,
        .targetId = request.targetId,
        .operation = request.operation,
        .mountPath = mountPath,
        .errorName = success ? QString() : QStringLiteral("org.holonight.Storage.Busy"),
        .errorMessage = success ? QString() : QStringLiteral("busy"),
    });
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

class StorageNotifications : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(state_dir_.isValid());
    previous_state_home_ = qgetenv("XDG_STATE_HOME");
    qputenv("XDG_STATE_HOME", state_dir_.path().toUtf8());
    notifications_ = std::make_unique<NotificationService>(nullptr, nullptr, &rules_);
    server_ = std::make_unique<NotificationServer>(notifications_.get());
    server_->start();
    ASSERT_FALSE(server_->conflictDetected());
    ASSERT_TRUE(QDBusConnection::sessionBus().isConnected());
  }
  void TearDown() override {
    auto bus = QDBusConnection::sessionBus();
    if (server_ && !server_->conflictDetected()) {
      bus.unregisterObject(QStringLiteral("/org/freedesktop/Notifications"));
      bus.unregisterService(QStringLiteral("org.freedesktop.Notifications"));
    }
    server_.reset();
    notifications_.reset();
    if (previous_state_home_.isNull()) {
      qunsetenv("XDG_STATE_HOME");
    } else {
      qputenv("XDG_STATE_HOME", previous_state_home_);
    }
  }
  static void flushNotifications() {
    // A bus round trip fences previously queued storage Notify calls without a timed sleep.
    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("GetCapabilities"));
    QDBusPendingReply<QStringList> reply = QDBusConnection::sessionBus().asyncCall(message);
    ASSERT_TRUE(QTest::qWaitFor([&] { return reply.isFinished(); }));
    ASSERT_FALSE(reply.isError());
  }
  [[nodiscard]] QString summary() const {
    return notifications_->index(0).data(NotificationService::SummaryRole).toString();
  }
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes): TEST_F state.
  QTemporaryDir state_dir_;
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes): TEST_F state.
  QByteArray previous_state_home_;
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes): TEST_F state.
  NotificationRuleModel rules_;
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes): TEST_F state.
  std::unique_ptr<NotificationService> notifications_;
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes): TEST_F state.
  std::unique_ptr<NotificationServer> server_;
};
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
TEST(ShellStorage, PowerOffActsImmediatelyAndScopeIncludesHiddenSiblingVolumes) {
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
  EXPECT_TRUE(controller.removalScope("first", true).contains("hidden"));
  model.powerOff("first");
  ASSERT_TRUE(QTest::qWaitFor([&] { return !backend.calls.isEmpty(); }));
  EXPECT_EQ(backend.calls.last().targetId, "first");
  EXPECT_EQ(backend.calls.last().operation, StorageOperation::PowerOff);
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

TEST(ShellStorage, DeviceCountAndHasMountedTrackDistinctDrivesAndMountState) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  auto stick = device("stick");
  auto reader = device("reader");
  backend.drives = {stick, reader};
  auto first = volume("first", "stick");
  auto second = volume("second", "stick");
  backend.volumes = {first, second, volume("third", "reader")};
  backend.publish();
  EXPECT_EQ(model.deviceCount(), 2);
  EXPECT_FALSE(model.hasMounted());
  first.mountPoints = {"/run/media/first"};
  backend.volumes = {first, second, volume("third", "reader")};
  backend.publish();
  EXPECT_EQ(model.deviceCount(), 2);
  EXPECT_TRUE(model.hasMounted());
}

TEST(ShellStorage, OperationTextTracksInFlightMountAndClearsOnCompletion) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  backend.drives = {device()};
  backend.volumes = {volume()};
  backend.publish();
  model.mount("volume");
  EXPECT_EQ(model.data(model.index(0), static_cast<int>(StorageService::Role::OperationText)).toString(), "Mounting…");
  // begin() defers to backend_->execute() via QTimer::singleShot(0, ...); let it fire before
  // the fake backend can report the request as completed.
  ASSERT_TRUE(QTest::qWaitFor([&] { return !backend.calls.isEmpty(); }));
  backend.completeLast(true, "/run/media/volume");
  EXPECT_TRUE(model.data(model.index(0), static_cast<int>(StorageService::Role::OperationText)).toString().isEmpty());
}

TEST(ShellStorage, FailedOperationStoresErrorAndRetryReplaysSameOperation) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  backend.drives = {device()};
  auto data = volume();
  data.canUnmount = true;
  data.mountPoints = {"/run/media/volume"};
  backend.volumes = {data};
  backend.publish();
  model.unmount("volume");
  ASSERT_TRUE(QTest::qWaitFor([&] { return !backend.calls.isEmpty(); }));
  ASSERT_EQ(backend.calls.size(), 1);
  backend.completeLast(false);
  const auto errorText = model.data(model.index(0), static_cast<int>(StorageService::Role::ErrorText)).toString();
  EXPECT_FALSE(errorText.isEmpty());
  model.retry("volume");
  ASSERT_TRUE(QTest::qWaitFor([&] { return backend.calls.size() == 2; }));
  EXPECT_EQ(backend.calls.last().operation, StorageOperation::Unmount);
  EXPECT_TRUE(model.data(model.index(0), static_cast<int>(StorageService::Role::ErrorText)).toString().isEmpty());
}

TEST(ShellStorage, DriveIconNameClassifiesByDeviceType) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  auto optical = device("optical");
  optical.optical = true;
  auto usbSsd = device("usb-ssd");
  usbSsd.connectionBus = "usb";
  usbSsd.rotationRate = 0;
  usbSsd.model = "Samsung T7 SSD";
  auto usbHdd = device("usb-hdd");
  usbHdd.connectionBus = "usb";
  usbHdd.rotationRate = 5400;
  auto flashReader = device("flash-reader");
  flashReader.connectionBus = "usb";
  flashReader.mediaCompatibility = {"flash_sd", "flash_sdhc"};
  auto unknown = device("unknown");
  backend.drives = {optical, usbSsd, usbHdd, flashReader, unknown};
  backend.publish();
  EXPECT_EQ(model.driveIconName("optical"), "media-optical-symbolic");
  EXPECT_EQ(model.driveIconName("usb-ssd"), "drive-harddisk-solidstate-symbolic");
  EXPECT_EQ(model.driveIconName("usb-hdd"), "drive-harddisk-symbolic");
  EXPECT_EQ(model.driveIconName("flash-reader"), "media-flash-symbolic");
  EXPECT_EQ(model.driveIconName("unknown"), "drive-removable-media-symbolic");
}

TEST(ShellStorage, HotplugAfterStartupDoesNotCrashAndUpdatesDeviceCount) {
  // The first publish() is the startup snapshot (must not attempt to notify); a drive that
  // appears on a later publish() is a genuine hotplug and exercises the newly-connected path.
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  backend.drives = {device("first")};
  backend.volumes = {volume("a", "first")};
  backend.publish();
  EXPECT_EQ(model.deviceCount(), 1);
  backend.drives = {device("first"), device("second")};
  backend.volumes = {volume("a", "first"), volume("b", "second")};
  backend.publish();
  EXPECT_EQ(model.deviceCount(), 2);
}

TEST(ShellStorage, DriveCapacityCountsPhysicalDiskOnce) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  backend.drives = {device()};
  auto disk = volume("disk");
  disk.partitionContainer = true;
  disk.capacity = 1000000000000ULL;
  auto first = volume("first");
  first.capacity = 500000000000ULL;
  auto second = volume("second");
  second.capacity = 500000000000ULL;
  backend.volumes = {disk, first, second};
  backend.publish();
  EXPECT_EQ(model.driveCapacityText("drive"), "1 TB total");
}

TEST(ShellStorage, DriveCapacityDoesNotCountUnlockedBackingTwice) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  backend.drives = {device()};
  auto backing = volume("backing");
  backing.usage = "crypto";
  backing.capacity = 1000000000000ULL;
  auto clear = volume("clear");
  clear.cryptoBackingId = backing.id;
  clear.capacity = 999000000000ULL;
  backend.volumes = {backing, clear};
  backend.publish();
  EXPECT_EQ(model.driveCapacityText("drive"), "1 TB total");
}

TEST_F(StorageNotifications, InitialDiscoveryIsSilentAndHotplugNotifiesOnce) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  backend.drives = {device("first")};
  backend.volumes = {volume("a", "first")};
  backend.publish();
  flushNotifications();
  EXPECT_EQ(notifications_->rowCount(), 0);
  backend.drives.append(device("second"));
  backend.volumes.append(volume("b", "second"));
  backend.publish();
  flushNotifications();
  ASSERT_EQ(notifications_->rowCount(), 1);
  EXPECT_EQ(summary(), "USB SSD connected");
  backend.publish();
  flushNotifications();
  EXPECT_EQ(notifications_->rowCount(), 1);
}

TEST_F(StorageNotifications, HotplugBypassesDndAndAppRulesWithoutEnteringHistory) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  backend.publish();
  rules_.ensureApp(QStringLiteral("holonight-shell"));
  rules_.setEnabled(0, false);
  notifications_->setDndEnabled(true);
  backend.drives = {device()};
  backend.volumes = {volume()};
  backend.publish();
  flushNotifications();
  ASSERT_EQ(notifications_->rowCount(), 1);
  const auto identifier = notifications_->index(0).data(NotificationService::NotifIdRole).toUInt();
  notifications_->requestClose(identifier);
  EXPECT_TRUE(notifications_->recentHistoryGrouped(10).isEmpty());
  EXPECT_EQ(notifications_->unreadCount(), 0);
}

TEST_F(StorageNotifications, TransientExpirationSkipsHistoryWhileNormalExpirationIsArchived) {
  NotificationData transient;
  transient.app_name = QStringLiteral("holonight-shell");
  transient.monitor_name = QStringLiteral("DP-1");
  transient.summary = QStringLiteral("transient");
  transient.hints.insert(QStringLiteral("transient"), true);
  transient.expire_timeout_ms = 30;
  notifications_->addOrReplace(transient);
  auto normal = transient;
  normal.summary = QStringLiteral("normal");
  normal.hints.clear();
  notifications_->addOrReplace(normal);
  ASSERT_TRUE(QTest::qWaitFor([&] { return notifications_->rowCount() == 0; }));
  const auto history = notifications_->recentHistoryGrouped(10);
  ASSERT_EQ(history.size(), 1);
  EXPECT_EQ(history.first().toMap().value(QStringLiteral("totalCount")).toInt(), 1);
  EXPECT_EQ(history.first().toMap().value(QStringLiteral("latestSummary")).toString(), QStringLiteral("normal"));
}

TEST_F(StorageNotifications, MountAndUnmountRemainSilent) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  backend.drives = {device()};
  backend.volumes = {volume()};
  backend.publish();
  model.mount("volume");
  ASSERT_TRUE(QTest::qWaitFor([&] { return backend.calls.size() == 1; }));
  backend.volumes.first().mountPoints = {QStringLiteral("/run/media/volume")};
  backend.volumes.first().canMount = false;
  backend.volumes.first().canUnmount = true;
  backend.publish();
  backend.completeLast(true, QStringLiteral("/run/media/volume"));
  flushNotifications();
  EXPECT_EQ(notifications_->rowCount(), 0);
  model.unmount("volume");
  ASSERT_TRUE(QTest::qWaitFor([&] { return backend.calls.size() == 2; }));
  backend.volumes.first().mountPoints.clear();
  backend.publish();
  backend.completeLast(true);
  flushNotifications();
  EXPECT_EQ(notifications_->rowCount(), 0);
}

TEST_F(StorageNotifications, PowerOffRetainsNameAfterDriveDisappears) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService model(&controller);
  backend.drives = {device()};
  backend.volumes = {volume()};
  backend.publish();
  flushNotifications();
  model.powerOff("drive");
  ASSERT_TRUE(QTest::qWaitFor([&] { return !backend.calls.isEmpty(); }));
  backend.drives.clear();
  backend.volumes.clear();
  backend.publish();
  backend.completeLast(true);
  flushNotifications();
  ASSERT_EQ(notifications_->rowCount(), 1);
  EXPECT_EQ(summary(), "USB SSD can be safely removed");
}

TEST_F(StorageNotifications, OtherBusClientsCannotBypassDndByImpersonatingStorage) {
  notifications_->setDndEnabled(true);
  const auto peer = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("storage-test-peer"));
  ASSERT_TRUE(peer.isConnected());
  auto message = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
      QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
  message << QStringLiteral("holonight-shell") << 0U << QString() << QStringLiteral("spoofed") << QString()
          << QStringList{} << QVariantMap{{"transient", true}, {"category", "x-holonight.storage"}} << 5000;
  QDBusPendingReply<uint> reply = peer.asyncCall(message);
  ASSERT_TRUE(QTest::qWaitFor([&] { return reply.isFinished(); }));
  EXPECT_FALSE(reply.isError());
  EXPECT_EQ(reply.value(), 0U);
  EXPECT_EQ(notifications_->rowCount(), 0);
  QDBusConnection::disconnectFromBus(QStringLiteral("storage-test-peer"));
}

TEST(ShellStorage, MetadataClassifierKeepsIconsAndSubtitlesConsistent) {
  FakeStorage backend;
  StorageController controller(&backend);
  StorageService service(&controller);
  struct Case {
    QString media;
    QStringList compatibility;
    std::optional<int> rotation;
    bool optical;
    QString icon;
    QString label;
  };
  const QList<Case> cases = {
      {
          .media = "thumb",
          .compatibility = {},
          .rotation = {},
          .optical = false,
          .icon = "qrc:/HolonightShell/common/usb-stick.svg",
          .label = "USB flash drive",
      },
      {
          .media = "",
          .compatibility = {},
          .rotation = 0,
          .optical = false,
          .icon = "drive-harddisk-solidstate-symbolic",
          .label = "Solid state",
      },
      {
          .media = "",
          .compatibility = {},
          .rotation = 5400,
          .optical = false,
          .icon = "drive-harddisk-symbolic",
          .label = "Hard disk",
      },
      {
          .media = "",
          .compatibility = {},
          .rotation = {},
          .optical = false,
          .icon = "drive-removable-media-symbolic",
          .label = "Removable",
      },
      {
          .media = "",
          .compatibility = {"thumb", "flash_sd"},
          .rotation = 7200,
          .optical = false,
          .icon = "qrc:/HolonightShell/common/usb-stick.svg",
          .label = "USB flash drive",
      },
      {
          .media = "flash",
          .compatibility = {},
          .rotation = 0,
          .optical = false,
          .icon = "media-flash-symbolic",
          .label = "Flash media",
      },
      {
          .media = "",
          .compatibility = {"flash"},
          .rotation = 7200,
          .optical = false,
          .icon = "media-flash-symbolic",
          .label = "Flash media",
      },
      {
          .media = "flash_sd",
          .compatibility = {},
          .rotation = 7200,
          .optical = false,
          .icon = "media-flash-symbolic",
          .label = "Flash media",
      },
      {
          .media = "",
          .compatibility = {"flash_mmc"},
          .rotation = {},
          .optical = false,
          .icon = "media-flash-symbolic",
          .label = "Flash media",
      },
      {
          .media = "thumb",
          .compatibility = {"optical_cd"},
          .rotation = 0,
          .optical = false,
          .icon = "media-optical-symbolic",
          .label = "Optical",
      },
      {
          .media = "optical",
          .compatibility = {"thumb"},
          .rotation = 7200,
          .optical = false,
          .icon = "media-optical-symbolic",
          .label = "Optical",
      },
      {
          .media = "optical_dvd",
          .compatibility = {"flash_sd"},
          .rotation = {},
          .optical = false,
          .icon = "media-optical-symbolic",
          .label = "Optical",
      },
      {
          .media = "thumb",
          .compatibility = {},
          .rotation = {},
          .optical = true,
          .icon = "media-optical-symbolic",
          .label = "Optical",
      },
      {
          .media = "",
          .compatibility = {},
          .rotation = -1,
          .optical = false,
          .icon = "drive-removable-media-symbolic",
          .label = "Removable",
      },
      {
          .media = "optical_unknown",
          .compatibility = {},
          .rotation = {},
          .optical = false,
          .icon = "drive-removable-media-symbolic",
          .label = "Removable",
      },
      {
          .media = "thumb_ssd",
          .compatibility = {"nvme", "not_flash_sd"},
          .rotation = {},
          .optical = false,
          .icon = "drive-removable-media-symbolic",
          .label = "Removable",
      },
  };
  auto drive = device();
  drive.connectionBus = "usb";
  backend.volumes = {volume()};
  QSignalSpy changes(&service, &StorageService::changed);
  for (const auto& item : cases) {
    SCOPED_TRACE(item.label.toStdString());
    drive.media = item.media;
    drive.mediaCompatibility = item.compatibility;
    drive.rotationRate = item.rotation;
    drive.optical = item.optical;
    backend.drives = {drive};
    changes.clear();
    backend.publish();
    EXPECT_EQ(service.driveIconName(drive.id), item.icon);
    EXPECT_EQ(service.driveSubtitle(drive.id), "USB · " + item.label);
    EXPECT_EQ(service.count(), 1);
    EXPECT_FALSE(changes.isEmpty());
  }
  drive.media.clear();
  drive.mediaCompatibility.clear();
  drive.rotationRate = 7200;
  drive.connectionBus = "sata";
  drive.removable = false;
  drive.optical = false;
  backend.drives = {drive};
  backend.publish();
  EXPECT_EQ(service.driveIconName(drive.id), "drive-harddisk-symbolic");
  EXPECT_EQ(service.driveSubtitle(drive.id), "SATA · Hard disk");
  drive.rotationRate.reset();
  backend.drives = {drive};
  backend.publish();
  EXPECT_EQ(service.driveSubtitle(drive.id), "SATA");
}
