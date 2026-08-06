#include "WifiNetworkModel.h"

#include <algorithm>
#include <ranges>

namespace {
[[nodiscard]] bool sameNetworkIdentity(const WifiNetwork& left, const WifiNetwork& right) {
  return left.ssid == right.ssid && left.secured == right.secured &&
         left.access_point_path == right.access_point_path && left.device_path == right.device_path &&
         left.connection_path == right.connection_path;
}

void sortNetworks(QList<WifiNetwork>& networks) {
  std::ranges::sort(networks, [](const WifiNetwork& left, const WifiNetwork& right) {
    if (left.connected != right.connected) {
      return left.connected;
    }
    if (left.strength != right.strength) {
      return left.strength > right.strength;
    }
    return QString::localeAwareCompare(left.ssid, right.ssid) < 0;
  });
}
}  // namespace

bool operator==(const WifiNetwork& left, const WifiNetwork& right) {
  return left.ssid == right.ssid && left.strength == right.strength && left.secured == right.secured &&
         left.known == right.known && left.connected == right.connected && left.active == right.active &&
         left.access_point_path == right.access_point_path && left.device_path == right.device_path &&
         left.connection_path == right.connection_path && left.frequency == right.frequency &&
         left.status_text == right.status_text;
}

WifiNetworkModel::WifiNetworkModel(QObject* parent) : QAbstractListModel(parent) {}

int WifiNetworkModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(networks_.size());
}

QVariant WifiNetworkModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(networks_.size())) {
    return {};
  }

  const WifiNetwork& network = networks_.at(index.row());
  switch (static_cast<Role>(role)) {
    case Role::Ssid:
      return network.ssid;
    case Role::Strength:
      return network.strength;
    case Role::Secured:
      return network.secured;
    case Role::Known:
      return network.known;
    case Role::Connected:
      return network.connected;
    case Role::Active:
      return network.active;
    case Role::AccessPointPath:
      return network.access_point_path;
    case Role::DevicePath:
      return network.device_path;
    case Role::ConnectionPath:
      return network.connection_path;
    case Role::Frequency:
      return network.frequency;
    case Role::StatusText:
      return network.status_text;
    default:
      return {};
  }
}

QHash<int, QByteArray> WifiNetworkModel::roleNames() const {
  static const QHash<int, QByteArray> kRoles{
      {static_cast<int>(Role::Ssid), "ssid"},
      {static_cast<int>(Role::Strength), "strength"},
      {static_cast<int>(Role::Secured), "secured"},
      {static_cast<int>(Role::Known), "known"},
      {static_cast<int>(Role::Connected), "connected"},
      {static_cast<int>(Role::Active), "active"},
      {static_cast<int>(Role::AccessPointPath), "accessPointPath"},
      {static_cast<int>(Role::DevicePath), "devicePath"},
      {static_cast<int>(Role::ConnectionPath), "connectionPath"},
      {static_cast<int>(Role::Frequency), "frequency"},
      {static_cast<int>(Role::StatusText), "statusText"},
  };
  return kRoles;
}

std::optional<WifiNetwork> WifiNetworkModel::networkAt(int row) const {
  if (row < 0 || row >= static_cast<int>(networks_.size())) {
    return std::nullopt;
  }
  return networks_.at(row);
}

void WifiNetworkModel::setNetworks(QList<WifiNetwork> networks) {
  sortNetworks(networks);

  if (networks == networks_) {
    return;
  }

  const bool can_update_in_place =
      networks.size() == networks_.size() && std::ranges::equal(networks, networks_, sameNetworkIdentity);
  if (!can_update_in_place) {
    beginResetModel();
    networks_ = std::move(networks);
    endResetModel();
    return;
  }

  for (int row = 0; row < static_cast<int>(networks.size()); ++row) {
    if (networks.at(row) == networks_.at(row)) {
      continue;
    }
    networks_.replace(row, networks.at(row));
    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed);
  }
}

void WifiNetworkModel::clear() {
  beginResetModel();
  networks_.clear();
  endResetModel();
}
