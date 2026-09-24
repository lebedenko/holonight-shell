#include "StorageService.h"

#include "StorageFilter.h"

#include <algorithm>
using namespace HoloNight::System;
namespace {
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
}  // namespace
StorageService::StorageService(QObject* parent) : StorageService(new StorageController, parent) {
  controller_->setParent(this);
}
StorageService::StorageService(StorageController* controller, QObject* parent)
    : QAbstractListModel(parent), controller_(controller) {
  for (auto* model : {static_cast<QAbstractItemModel*>(controller_->drives()),
                      static_cast<QAbstractItemModel*>(controller_->volumes())}) {
    connect(model, &QAbstractItemModel::rowsInserted, this, &StorageService::refresh);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &StorageService::refresh);
    connect(model, &QAbstractItemModel::dataChanged, this, &StorageService::refresh);
  }
  connect(controller_, &StorageController::operationStateChanged, this, &StorageService::refresh);
  connect(controller_, &StorageController::availableChanged, this, &StorageService::refresh);
  connect(controller_, &StorageController::operationFinished, this, [this](const StorageResult& result) {
    error_message_ = operationError(result);
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
  return {{static_cast<int>(Role::TargetId), "targetId"},
          {static_cast<int>(Role::DriveId), "driveId"},
          {static_cast<int>(Role::Name), "name"},
          {static_cast<int>(Role::DriveName), "driveName"},
          {static_cast<int>(Role::State), "stateText"},
          {static_cast<int>(Role::Mounted), "mounted"},
          {static_cast<int>(Role::Busy), "busy"},
          {static_cast<int>(Role::CanMount), "canMount"},
          {static_cast<int>(Role::CanUnmount), "canUnmount"},
          {static_cast<int>(Role::CanEject), "canEject"},
          {static_cast<int>(Role::CanPowerOff), "canPowerOff"}};
}
void StorageService::refresh() {
  QList<QVariantMap> rows;
  for (const auto& volume : controller_->volumes()->items()) {
    const auto drive = controller_->drives()->find(volume.driveId);
    if (!drive || !drive->removable || !drive->mediaPresent || !StorageFilter::eligible(volume)) {
      continue;
    }
    const bool busy = controller_->busy(volume.id) || controller_->busy(drive->id);
    rows.append({{"targetId", volume.id},
                 {"driveId", drive->id},
                 {"name", volumeName(volume)},
                 {"driveName", driveName(*drive)},
                 {"mounted", !volume.mountPoints.isEmpty()},
                 {"busy", busy},
                 {"stateText", volumeState(volume, busy)},
                 {"canMount", volume.canMount && !volume.locked && !busy},
                 {"canUnmount", volume.canUnmount && !busy},
                 {"canEject", drive->canEject && !busy},
                 {"canPowerOff", drive->canPowerOff && !busy}});
  }
  appendOpticalDrives(rows);
  std::ranges::sort(rows, [](const auto& first, const auto& second) {
    return first.value("driveId").toString() + first.value("targetId").toString() <
           second.value("driveId").toString() + second.value("targetId").toString();
  });
  if (rows != rows_) {
    beginResetModel();
    rows_ = rows;
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

void StorageService::mount(const QString& targetId) {
  if (visibleTarget(targetId, "canMount")) {
    controller_->mount(targetId);
  }
}
void StorageService::unmount(const QString& targetId) {
  if (visibleTarget(targetId, "canUnmount")) {
    controller_->unmount(targetId);
  }
}
void StorageService::eject(const QString& targetId) {
  if (visibleTarget(targetId, "canEject")) {
    controller_->eject(targetId);
  }
}
void StorageService::requestPowerOff(const QString& targetId) {
  if (!visibleTarget(targetId, "canPowerOff")) {
    return;
  }
  confirmation_drive_ = targetId;
  confirmation_scope_ = controller_->removalScope(targetId, true);
  QStringList names;
  for (const auto& target : confirmation_scope_) {
    if (const auto drive = controller_->drives()->find(target)) {
      names.append(driveName(*drive));
    } else if (const auto volume = controller_->volumes()->find(target)) {
      names.append(volumeName(*volume));
    }
  }
  confirmation_text_ = tr("Power off these devices and volumes?\n%1").arg(names.join("\n"));
  emit changed();
}
void StorageService::confirmPowerOff() {
  if (confirmation_drive_.isEmpty()) {
    return;
  }
  const auto targetId = confirmation_drive_;
  const auto scope = confirmation_scope_;
  cancelPowerOff();
  controller_->powerOff(targetId, scope);
}
void StorageService::cancelPowerOff() {
  confirmation_drive_.clear();
  confirmation_scope_.clear();
  confirmation_text_.clear();
  emit changed();
}

QString StorageService::driveLabel(const QString& targetId) const {
  const auto drive = controller_->drives()->find(targetId);
  return drive ? driveName(*drive) : tr("Storage device");
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
    rows.append({{"targetId", drive.id},
                 {"driveId", drive.id},
                 {"name", driveName(drive)},
                 {"driveName", driveName(drive)},
                 {"stateText", tr("No usable filesystem")},
                 {"mounted", false},
                 {"busy", busy},
                 {"canMount", false},
                 {"canUnmount", false},
                 {"canEject", drive.canEject && !busy},
                 {"canPowerOff", drive.canPowerOff && !busy}});
  }
}
