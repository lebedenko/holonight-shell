#include "StorageService.h"

#include "ApplicationLaunchService.h"
#include "NotificationTypes.h"
#include "StorageFilter.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QProcess>
#include <QStorageInfo>
#include <QTimer>

#include <algorithm>
using namespace HoloNight::System;
namespace {
constexpr int kErrorExpiryMs = 30000;
QString operationError(const StorageResult& result) {
  if (result.succeeded()) {
    return {};
  }
  if (result.errorName.endsWith("NotAuthorizedDismissed")) {
    return StorageService::tr("Authorization was canceled.");
  }
  if (result.errorName.endsWith("NotAuthorized")) {
    return StorageService::tr("Permission to access this storage was denied.");
  }
  if (result.errorName.endsWith("Busy")) {
    return StorageService::tr("The device is busy. Close files using it and try again.");
  }
  if (result.errorName.endsWith("ScopeChanged")) {
    return StorageService::tr("The affected devices changed. Review them before trying again.");
  }
  if (result.errorName.endsWith("Unavailable") || result.errorName.endsWith("Disappeared")) {
    return StorageService::tr("The storage device or service is no longer available.");
  }
  return StorageService::tr("Storage operation failed: %1")
      .arg(result.errorMessage.isEmpty() ? result.errorName : result.errorMessage);
}
QString driveName(const StorageDrive& drive) {
  const auto name = (drive.vendor + ' ' + drive.model).trimmed();
  return name.isEmpty() ? StorageService::tr("Storage device") : name;
}
QString volumeName(const StorageVolume& volume) {
  if (!volume.label.isEmpty()) {
    return volume.label;
  }
  return !volume.device.isEmpty() ? volume.device : StorageService::tr("Volume");
}

QString volumeState(const StorageVolume& volume, bool busy) {
  if (volume.locked) {
    return StorageService::tr("Locked — unlocking is not available");
  }
  if (busy) {
    return StorageService::tr("Working…");
  }
  return volume.mountPoints.isEmpty() ? StorageService::tr("Not mounted") : volume.mountPoints.join(", ");
}
QString formatBytes(qint64 bytes) {
  static const QStringList units = {"B", "KB", "MB", "GB", "TB", "PB"};
  auto value = static_cast<double>(bytes);
  int unit_index = 0;
  while (value >= 1000.0 && unit_index < units.size() - 1) {
    value /= 1000.0;
    ++unit_index;
  }
  QString number_text = QString::number(value, 'f', value < 10.0 ? 1 : 0);
  if (number_text.endsWith(".0")) {
    number_text.chop(2);
  }
  return QStringLiteral("%1 %2").arg(number_text, units[unit_index]);
}
}  // namespace
StorageService::DeviceKind StorageService::classifyDevice(const StorageDrive& drive) {
  auto media = drive.mediaCompatibility;
  media.append(drive.media);
  const auto has = [&media](auto predicate) { return std::ranges::any_of(media, predicate); };
  static const QStringList opticalMedia = {
      "optical",
      "optical_cd",
      "optical_cd_r",
      "optical_cd_rw",
      "optical_dvd",
      "optical_dvd_r",
      "optical_dvd_rw",
      "optical_dvd_ram",
      "optical_dvd_plus_r",
      "optical_dvd_plus_rw",
      "optical_dvd_plus_r_dl",
      "optical_dvd_plus_rw_dl",
      "optical_bd",
      "optical_bd_r",
      "optical_bd_re",
      "optical_hddvd",
      "optical_hddvd_r",
      "optical_hddvd_rw",
      "optical_mo",
      "optical_mrw",
      "optical_mrw_w",
  };
  if (drive.optical || has([](const QString& token) { return opticalMedia.contains(token); })) {
    return DeviceKind::Optical;
  }
  if (media.contains(QStringLiteral("thumb"))) {
    return DeviceKind::Thumb;
  }
  if (has([](const QString& token) { return token == "flash" || token.startsWith("flash_"); })) {
    return DeviceKind::Flash;
  }
  if (drive.rotationRate && *drive.rotationRate > 0) {
    return DeviceKind::HardDisk;
  }
  if (drive.rotationRate && *drive.rotationRate == 0) {
    return DeviceKind::SolidState;
  }
  return DeviceKind::Unknown;
}
StorageService::StorageService(QObject* parent) : StorageService(new StorageController, parent) {
  controller_->setParent(this);
}
StorageService::StorageService(StorageController* controller, QObject* parent)
    : QAbstractListModel(parent), controller_(controller) {
  for (auto* model : {
           static_cast<QAbstractItemModel*>(controller_->drives()),
           static_cast<QAbstractItemModel*>(controller_->volumes()),
       }) {
    connect(model, &QAbstractItemModel::rowsInserted, this, &StorageService::refresh);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &StorageService::refresh);
    connect(model, &QAbstractItemModel::dataChanged, this, &StorageService::refresh);
  }
  connect(controller_, &StorageController::operationStateChanged, this, &StorageService::refresh);
  connect(controller_, &StorageController::availableChanged, this, &StorageService::refresh);
  connect(controller_, &StorageController::operationFinished, this, [this](const StorageResult& result) {
    in_flight_ops_.remove(result.targetId);
    const auto removal_label = removal_labels_.take(result.targetId);
    error_message_ = operationError(result);
    if (result.succeeded()) {
      last_errors_.remove(result.targetId);
      if (result.operation == StorageOperation::Eject || result.operation == StorageOperation::PowerOff) {
        sendSafeToRemoveNotification(removal_label.isEmpty() ? driveLabel(result.targetId) : removal_label);
      }
      if (result.operation == StorageOperation::Mount && pending_open_after_mount_.remove(result.targetId)) {
        launchFiles({result.mountPath}, result.targetId);
      }
    } else {
      pending_open_after_mount_.remove(result.targetId);
      const auto generation = ++next_error_generation_;
      last_errors_.insert(result.targetId,
                          {.operation = result.operation, .message = operationError(result), .generation = generation});
      const auto targetId = result.targetId;
      QTimer::singleShot(kErrorExpiryMs, this, [this, targetId, generation] {
        const auto iterator = last_errors_.constFind(targetId);
        if (iterator != last_errors_.constEnd() && iterator->generation == generation) {
          last_errors_.remove(targetId);
          refresh();
        }
      });
    }
    refresh();
  });
  refresh();
}
int StorageService::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : count(); }
QVariant StorageService::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= count()) {
    return {};
  }
  return rows_[index.row()].value(QString::fromLatin1(roleNames().value(role)));
}
QHash<int, QByteArray> StorageService::roleNames() const {
  return {
      {static_cast<int>(Role::TargetId), "targetId"},
      {static_cast<int>(Role::DriveId), "driveId"},
      {static_cast<int>(Role::Name), "name"},
      {static_cast<int>(Role::DriveName), "driveName"},
      {static_cast<int>(Role::State), "stateText"},
      {static_cast<int>(Role::Mounted), "mounted"},
      {static_cast<int>(Role::Busy), "busy"},
      {static_cast<int>(Role::CanMount), "canMount"},
      {static_cast<int>(Role::CanUnmount), "canUnmount"},
      {static_cast<int>(Role::CanEject), "canEject"},
      {static_cast<int>(Role::CanPowerOff), "canPowerOff"},
      {static_cast<int>(Role::UsedBytes), "usedBytes"},
      {static_cast<int>(Role::TotalBytes), "totalBytes"},
      {static_cast<int>(Role::FreeBytes), "freeBytes"},
      {static_cast<int>(Role::OperationText), "operationText"},
      {static_cast<int>(Role::ErrorText), "errorText"},
  };
}
int StorageService::deviceCount() const {
  QSet<QString> ids;
  for (const auto& row : rows_) {
    ids.insert(row.value("driveId").toString());
  }
  return static_cast<int>(ids.size());
}
bool StorageService::hasMounted() const {
  return std::ranges::any_of(rows_, [](const auto& row) { return row.value("mounted").toBool(); });
}
void StorageService::refresh() {
  QList<QVariantMap> rows;
  for (const auto& volume : controller_->volumes()->items()) {
    const auto drive = controller_->drives()->find(volume.driveId);
    if (!drive || !drive->removable || !drive->mediaPresent || !StorageFilter::eligible(volume)) {
      continue;
    }
    const bool busy = controller_->busy(volume.id) || controller_->busy(drive->id);
    qint64 used_bytes = 0;
    qint64 total_bytes = 0;
    qint64 free_bytes = 0;
    if (!volume.mountPoints.isEmpty()) {
      const QStorageInfo info(volume.mountPoints.first());
      total_bytes = info.bytesTotal();
      free_bytes = info.bytesAvailable();
      used_bytes = total_bytes - free_bytes;
    }
    rows.append({
        {"targetId", volume.id},
        {"driveId", drive->id},
        {"name", volumeName(volume)},
        {"driveName", driveName(*drive)},
        {"mounted", !volume.mountPoints.isEmpty()},
        {"busy", busy},
        {"stateText", volumeState(volume, busy)},
        {"canMount", volume.canMount && !volume.locked && !busy},
        {"canUnmount", volume.canUnmount && !busy},
        {"canEject", drive->canEject && !busy},
        {"canPowerOff", drive->canPowerOff && !busy},
        {"usedBytes", used_bytes},
        {"totalBytes", total_bytes},
        {"freeBytes", free_bytes},
        {"operationText", operationTextFor(volume.id)},
        {"errorText", errorTextFor(volume.id)},
    });
  }
  appendOpticalDrives(rows);
  std::ranges::sort(rows, [](const auto& first, const auto& second) {
    return first.value("driveId").toString() + first.value("targetId").toString() <
           second.value("driveId").toString() + second.value("targetId").toString();
  });
  QSet<QString> current_drive_ids;
  for (const auto& row : rows) {
    current_drive_ids.insert(row.value("driveId").toString());
  }
  notifyNewlyConnectedDrives(current_drive_ids);
  if (rows != rows_) {
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
  }
  emit changed();
}
bool StorageService::visibleTarget(const QString& targetId, const char* capability) const {
  return std::ranges::any_of(rows_, [&](const auto& row) {
    return (row.value("targetId").toString() == targetId || row.value("driveId").toString() == targetId) &&
           row.value(QLatin1String(capability)).toBool();
  });
}
QString StorageService::operationTextFor(const QString& targetId) const {
  const auto iterator = in_flight_ops_.constFind(targetId);
  if (iterator == in_flight_ops_.constEnd()) {
    return {};
  }
  switch (iterator.value()) {
    case StorageOperation::Mount:
      return tr("Mounting…");
    case StorageOperation::Unmount:
      return tr("Unmounting…");
    case StorageOperation::Eject:
      return tr("Ejecting…");
    case StorageOperation::PowerOff:
      return tr("Powering off…");
  }
  return {};
}
QString StorageService::errorTextFor(const QString& targetId) const {
  const auto iterator = last_errors_.constFind(targetId);
  return iterator == last_errors_.constEnd() ? QString() : iterator->message;
}
void StorageService::beginOperation(const QString& targetId, StorageOperation operation) {
  last_errors_.remove(targetId);
  in_flight_ops_.insert(targetId, operation);
  if (operation == StorageOperation::Eject || operation == StorageOperation::PowerOff) {
    removal_labels_.insert(targetId, driveLabel(targetId));
  }
  refresh();
}
void StorageService::notifyNewlyConnectedDrives(const QSet<QString>& current_drive_ids) {
  // The initial snapshot populates both models before availableChanged fires. Constructor
  // refreshes and intermediate model signals must not seed an incomplete startup snapshot.
  if (!controller_->available()) {
    return;
  }
  if (known_drive_ids_seeded_) {
    for (const auto& driveId : current_drive_ids) {
      if (!known_drive_ids_.contains(driveId)) {
        sendDriveConnectedNotification(driveId);
      }
    }
  }
  known_drive_ids_ = current_drive_ids;
  known_drive_ids_seeded_ = true;
}
void StorageService::sendStorageNotification(const QString& summary) {
  auto message = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
      QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
  message << QStringLiteral("holonight-shell") << 0U << QString() << summary << QString() << QStringList{}
          << QVariantMap{{QStringLiteral("transient"), true},
                         {QStringLiteral("category"), QString::fromLatin1(kStorageNotificationCategory)},}
          << 5000;
  QDBusConnection::sessionBus().asyncCall(message);
}
void StorageService::sendSafeToRemoveNotification(const QString& label) {
  sendStorageNotification(tr("%1 can be safely removed").arg(label));
}
void StorageService::sendDriveConnectedNotification(const QString& driveId) const {
  sendStorageNotification(tr("%1 connected").arg(driveLabel(driveId)));
}

void StorageService::mount(const QString& targetId) {
  if (!visibleTarget(targetId, "canMount") || in_flight_ops_.contains(targetId)) {
    return;
  }
  beginOperation(targetId, StorageOperation::Mount);
  controller_->mount(targetId);
}
void StorageService::unmount(const QString& targetId) {
  if (!visibleTarget(targetId, "canUnmount") || in_flight_ops_.contains(targetId)) {
    return;
  }
  beginOperation(targetId, StorageOperation::Unmount);
  controller_->unmount(targetId);
}
void StorageService::eject(const QString& targetId) {
  if (!visibleTarget(targetId, "canEject") || in_flight_ops_.contains(targetId)) {
    return;
  }
  beginOperation(targetId, StorageOperation::Eject);
  controller_->eject(targetId);
}
void StorageService::powerOff(const QString& targetId) {
  if (!visibleTarget(targetId, "canPowerOff") || in_flight_ops_.contains(targetId)) {
    return;
  }
  beginOperation(targetId, StorageOperation::PowerOff);
  controller_->powerOff(targetId, controller_->removalScope(targetId, true));
}
void StorageService::retry(const QString& targetId) {
  const auto iterator = last_errors_.constFind(targetId);
  if (iterator == last_errors_.constEnd()) {
    return;
  }
  if (iterator->files_launch) {
    const auto arguments = iterator->launch_arguments;
    last_errors_.remove(targetId);
    launchFiles(arguments, targetId);
    return;
  }
  switch (iterator->operation) {
    case StorageOperation::Mount:
      mount(targetId);
      return;
    case StorageOperation::Unmount:
      unmount(targetId);
      return;
    case StorageOperation::Eject:
      eject(targetId);
      return;
    case StorageOperation::PowerOff:
      powerOff(targetId);
      return;
  }
}
void StorageService::openVolume(const QString& targetId) {
  if (const auto volumeRecord = controller_->volumes()->find(targetId);
      volumeRecord && !volumeRecord->mountPoints.isEmpty()) {
    launchFiles({volumeRecord->mountPoints.first()}, targetId);
    return;
  }
  if (!visibleTarget(targetId, "canMount") || in_flight_ops_.contains(targetId)) {
    return;
  }
  pending_open_after_mount_.insert(targetId);
  mount(targetId);
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static): QML method.
void StorageService::openInFiles() { launchFiles(); }
void StorageService::showAllDevices() {
  for (const auto& row : rows_) {
    if (!row.value("mounted").toBool()) {
      continue;
    }
    const auto targetId = row.value("targetId").toString();
    if (const auto volumeRecord = controller_->volumes()->find(targetId);
        volumeRecord && !volumeRecord->mountPoints.isEmpty()) {
      launchFiles({volumeRecord->mountPoints.first()}, targetId);
      return;
    }
  }
  launchFiles();
}

QString StorageService::driveLabel(const QString& targetId) const {
  const auto drive = controller_->drives()->find(targetId);
  return drive ? driveName(*drive) : tr("Storage device");
}
QString StorageService::driveIconName(const QString& driveId) const {
  const auto drive = controller_->drives()->find(driveId);
  switch (drive ? classifyDevice(*drive) : DeviceKind::Unknown) {
    case DeviceKind::Optical:
      return QStringLiteral("media-optical-symbolic");
    case DeviceKind::Thumb:
      return QStringLiteral("qrc:/HolonightShell/common/usb-stick.svg");
    case DeviceKind::Flash:
      return QStringLiteral("media-flash-symbolic");
    case DeviceKind::HardDisk:
      return QStringLiteral("drive-harddisk-symbolic");
    case DeviceKind::SolidState:
      return QStringLiteral("drive-harddisk-solidstate-symbolic");
    case DeviceKind::Unknown:
      return QStringLiteral("drive-removable-media-symbolic");
  }
  return {};
}
QString StorageService::driveSubtitle(const QString& driveId) const {
  const auto drive = controller_->drives()->find(driveId);
  if (!drive) {
    return {};
  }
  QStringList parts;
  if (!drive->connectionBus.isEmpty()) {
    parts.append(drive->connectionBus.toUpper());
  }
  switch (classifyDevice(*drive)) {
    case DeviceKind::Optical:
      parts.append(tr("Optical"));
      break;
    case DeviceKind::Thumb:
      parts.append(tr("USB flash drive"));
      break;
    case DeviceKind::Flash:
      parts.append(tr("Flash media"));
      break;
    case DeviceKind::HardDisk:
      parts.append(tr("Hard disk"));
      break;
    case DeviceKind::SolidState:
      parts.append(tr("Solid state"));
      break;
    case DeviceKind::Unknown:
      if (drive->removable || drive->mediaRemovable) {
        parts.append(tr("Removable"));
      }
      break;
  }
  return parts.join(QStringLiteral(" · "));
}
QString StorageService::driveCapacityText(const QString& driveId) const {
  // A disk and its partitions describe overlapping bytes. Prefer the whole-disk record.
  for (const auto& volumeRecord : controller_->volumes()->items()) {
    if (volumeRecord.driveId == driveId && volumeRecord.partitionContainer && volumeRecord.partitionNumber == 0 &&
        volumeRecord.cryptoBackingId.isEmpty() && volumeRecord.capacity > 0) {
      return tr("%1 total").arg(formatBytes(static_cast<qint64>(volumeRecord.capacity)));
    }
  }
  qint64 total = 0;
  bool any = false;
  for (const auto& volumeRecord : controller_->volumes()->items()) {
    if (volumeRecord.driveId == driveId && !volumeRecord.partitionContainer && volumeRecord.cryptoBackingId.isEmpty() &&
        volumeRecord.capacity > 0) {
      total += static_cast<qint64>(volumeRecord.capacity);
      any = true;
    }
  }
  return any ? tr("%1 total").arg(formatBytes(total)) : QString();
}
QString StorageService::driveOperationText(const QString& driveId) const { return operationTextFor(driveId); }
QString StorageService::driveErrorText(const QString& driveId) const { return errorTextFor(driveId); }
bool StorageService::driveCanEject(const QString& driveId) const {
  const auto drive = controller_->drives()->find(driveId);
  return drive && drive->canEject && !controller_->busy(driveId) && !in_flight_ops_.contains(driveId);
}
bool StorageService::driveCanPowerOff(const QString& driveId) const {
  const auto drive = controller_->drives()->find(driveId);
  return drive && drive->canPowerOff && !controller_->busy(driveId) && !in_flight_ops_.contains(driveId);
}

void StorageService::appendOpticalDrives(QList<QVariantMap>& rows) const {
  // Optical media may have no usable filesystem (for example an audio CD).
  for (const auto& drive : controller_->drives()->items()) {
    if (!drive.removable || !drive.mediaPresent || !drive.optical) {
      continue;
    }
    const bool hasRow =
        std::ranges::any_of(rows, [&](const auto& row) { return row.value("driveId").toString() == drive.id; });
    const bool suppressed =
        std::any_of(controller_->volumes()->items().cbegin(), controller_->volumes()->items().cend(),
                    [&](const auto& volumeRecord) {
                      return volumeRecord.driveId == drive.id &&
                             (volumeRecord.hintIgnore || StorageFilter::infrastructure(volumeRecord));
                    });
    if (hasRow || suppressed) {
      continue;
    }
    const bool busy = controller_->busy(drive.id);
    rows.append({
        {"targetId", drive.id},
        {"driveId", drive.id},
        {"name", driveName(drive)},
        {"driveName", driveName(drive)},
        {"stateText", tr("No usable filesystem")},
        {"mounted", false},
        {"busy", busy},
        {"canMount", false},
        {"canUnmount", false},
        {"canEject", drive.canEject && !busy},
        {"canPowerOff", drive.canPowerOff && !busy},
        {"usedBytes", static_cast<qint64>(0)},
        {"totalBytes", static_cast<qint64>(0)},
        {"freeBytes", static_cast<qint64>(0)},
        {"operationText", operationTextFor(drive.id)},
        {"errorText", errorTextFor(drive.id)},
    });
  }
}

void StorageService::launchFiles(const QStringList& arguments, const QString& target) {
  ApplicationLaunchService service;
  auto* launcher = application_launch_service_ ? application_launch_service_ : &service;
  launcher->launch({.program = QStringLiteral("holonight-files"), .arguments = arguments}, this,
                   [this, target, arguments](const QString&, const QString& error) {
                     error_message_ = error;
                     if (!target.isEmpty() && error.isEmpty()) last_errors_.remove(target);
                     if (!target.isEmpty() && !error.isEmpty())
                       last_errors_.insert(target, {.message = error,
                                                    .generation = ++next_error_generation_,
                                                    .files_launch = true,
                                                    .launch_arguments = arguments});
                     refresh();
                   });
}
