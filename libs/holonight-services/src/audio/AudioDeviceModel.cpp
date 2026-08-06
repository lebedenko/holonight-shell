#include "AudioDeviceModel.h"

AudioDeviceModel::AudioDeviceModel(QObject* parent) : QAbstractListModel(parent) {}

int AudioDeviceModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(devices_.size());
}

QVariant AudioDeviceModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(devices_.size())) {
    return {};
  }
  const AudioDevice& dev = devices_.at(index.row());
  switch (static_cast<Role>(role)) {
    case Role::DeviceId:
      return dev.id;
    case Role::Name:
      return dev.name;
    case Role::Description:
      return dev.description;
    case Role::Volume:
      return dev.volume;
    case Role::Muted:
      return dev.muted;
    case Role::IsDefault:
      return dev.is_default;
    default:
      return {};
  }
}

QHash<int, QByteArray> AudioDeviceModel::roleNames() const {
  static const QHash<int, QByteArray> kRoles{
      {static_cast<int>(Role::DeviceId), "deviceId"},
      {static_cast<int>(Role::Name), "name"},
      {static_cast<int>(Role::Description), "description"},
      {static_cast<int>(Role::Volume), "volume"},
      {static_cast<int>(Role::Muted), "muted"},
      {static_cast<int>(Role::IsDefault), "isDefault"},
  };
  return kRoles;
}

void AudioDeviceModel::applyAdd(const AudioDevice& device) {
  for (int row = 0; row < static_cast<int>(devices_.size()); ++row) {
    if (devices_.at(row).id == device.id) {
      devices_.replace(row, device);
      const QModelIndex idx = index(row);
      emit dataChanged(idx, idx);
      return;
    }
  }

  const int row = static_cast<int>(devices_.size());
  beginInsertRows(QModelIndex(), row, row);
  devices_.append(device);
  endInsertRows();
}

void AudioDeviceModel::applyChange(const AudioDevice& device) {
  for (int row = 0; row < static_cast<int>(devices_.size()); ++row) {
    if (devices_.at(row).id == device.id) {
      devices_.replace(row, device);
      const QModelIndex idx = index(row);
      emit dataChanged(idx, idx);
      return;
    }
  }
  applyAdd(device);
}

void AudioDeviceModel::applyRemove(uint32_t device_id) {
  for (int row = 0; row < static_cast<int>(devices_.size()); ++row) {
    if (devices_.at(row).id == device_id) {
      beginRemoveRows(QModelIndex(), row, row);
      devices_.removeAt(row);
      endRemoveRows();
      return;
    }
  }
}

void AudioDeviceModel::clear() {
  beginResetModel();
  devices_.clear();
  endResetModel();
}
