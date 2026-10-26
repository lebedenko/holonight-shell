#include "AudioApplicationModel.h"

#include <algorithm>

using HoloNight::System::AudioStreamModel;

AudioApplicationModel::AudioApplicationModel(HoloNight::System::AudioController* controller)
    : QAbstractListModel(controller), controller_(controller) {
  auto* source = controller_->playbackStreams();
  connect(source, &QAbstractItemModel::rowsInserted, this, &AudioApplicationModel::synchronize);
  connect(source, &QAbstractItemModel::rowsRemoved, this, &AudioApplicationModel::synchronize);
  connect(source, &QAbstractItemModel::dataChanged, this, &AudioApplicationModel::synchronize);
  connect(source, &QAbstractItemModel::modelReset, this, &AudioApplicationModel::synchronize);
  synchronize();
}

int AudioApplicationModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(groups_.size());
}

QVariant AudioApplicationModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= groups_.size()) return {};
  const auto& group = groups_.at(index.row());
  switch (role) {
    case ApplicationKey:
      return group.key;
    case Application:
      return group.name;
    case IconName:
      return group.icon;
    case StreamCount:
      return group.members.size();
    case Subtitle:
      return group.subtitle;
    case Volume:
      return group.volume;
    case Muted:
      return group.muted;
    default:
      return {};
  }
}

QHash<int, QByteArray> AudioApplicationModel::roleNames() const {
  return {{ApplicationKey, "applicationKey"},
          {Application, "application"},
          {IconName, "iconName"},
          {StreamCount, "streamCount"},
          {Subtitle, "subtitle"},
          {Volume, "volume"},
          {Muted, "muted"}};
}

void AudioApplicationModel::setVolume(const QString& key, int percent) {
  for (auto& group : groups_) {
    if (group.key != key) continue;
    percent = std::clamp(percent, 0, 100);
    group.requested_volume = percent;
    const auto members = group.members;
    for (auto id : members) controller_->setStreamVolume(id, percent);
    return;
  }
}

void AudioApplicationModel::setMuted(const QString& key, bool muted) {
  for (auto& group : groups_) {
    if (group.key != key) continue;
    group.requested_mute = muted;
    const auto members = group.members;
    for (auto id : members) controller_->setStreamMuted(id, muted);
    return;
  }
}

void AudioApplicationModel::synchronize() {
  QList<Group> next;
  auto* source = controller_->playbackStreams();
  for (int row = 0; row < source->rowCount(); ++row) {
    auto value = [&](AudioStreamModel::Role role) { return source->data(source->index(row), static_cast<int>(role)); };
    const auto id = value(AudioStreamModel::Role::StreamId).toUInt();
    const auto app_id = value(AudioStreamModel::Role::ApplicationId).toString().trimmed();
    const auto binary = value(AudioStreamModel::Role::ProcessBinary).toString().trimmed();
    const auto name = value(AudioStreamModel::Role::Application).toString().trimmed();
    const auto key = !app_id.isEmpty()   ? "id:" + app_id
                     : !binary.isEmpty() ? "binary:" + binary
                     : !name.isEmpty()   ? "name:" + name
                                         : "stream:" + QString::number(id);
    auto it = std::find_if(next.begin(), next.end(), [&](const Group& g) { return g.key == key; });
    if (it == next.end()) {
      Group group;
      group.key = key;
      group.fallback_name = !app_id.isEmpty() ? app_id : tr("Unknown application");
      group.subtitle = value(AudioStreamModel::Role::Name).toString();
      next.append(group);
      it = next.end() - 1;
    }
    it->members.append(id);
    if (it->binary.isEmpty()) it->binary = binary;
    if (it->name.isEmpty()) it->name = name;
    if (it->icon.isEmpty()) it->icon = value(AudioStreamModel::Role::IconName).toString().trimmed();
    it->volume = std::max(it->volume, value(AudioStreamModel::Role::Volume).toInt());
    it->muted = it->muted && value(AudioStreamModel::Role::Muted).toBool();
  }

  // Remove only vanished groups. Existing rows keep their order and delegates.
  for (int row = static_cast<int>(groups_.size()) - 1; row >= 0; --row) {
    if (std::none_of(next.begin(), next.end(), [&](const Group& g) { return g.key == groups_[row].key; })) {
      beginRemoveRows({}, row, row);
      groups_.removeAt(row);
      endRemoveRows();
    }
  }
  struct Inheritance {
    uint32_t id;
    std::optional<int> volume;
    std::optional<bool> mute;
  };
  QList<Inheritance> inherited;
  for (auto& group : next) {
    if (group.name.isEmpty()) {
      group.name = group.binary.isEmpty() ? group.fallback_name : group.binary;
    }
    if (group.members.size() > 1)
      group.subtitle = tr("%1 streams").arg(group.members.size());
    else if (group.subtitle == group.name)
      group.subtitle.clear();
    auto old = std::find_if(groups_.begin(), groups_.end(), [&](const Group& g) { return g.key == group.key; });
    if (old == groups_.end()) {
      const int row = static_cast<int>(groups_.size());
      beginInsertRows({}, row, row);
      groups_.append(group);
      endInsertRows();
    } else {
      group.requested_volume = old->requested_volume;
      group.requested_mute = old->requested_mute;
      for (auto id : group.members) {
        if (!old->members.contains(id)) inherited.append({id, group.requested_volume, group.requested_mute});
      }
      const int row = static_cast<int>(old - groups_.begin());
      const bool changed = old->name != group.name || old->icon != group.icon || old->subtitle != group.subtitle ||
                           old->members.size() != group.members.size() || old->volume != group.volume ||
                           old->muted != group.muted;
      *old = group;
      if (changed) emit dataChanged(index(row), index(row));
    }
  }
  // Membership is committed before commands: synchronous backend notifications cannot reapply inheritance.
  for (const auto& command : inherited) {
    if (command.volume) controller_->setStreamVolume(command.id, *command.volume);
    if (command.mute) controller_->setStreamMuted(command.id, *command.mute);
  }
}
