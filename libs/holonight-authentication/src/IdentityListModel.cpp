#include "IdentityListModel.h"

#include "ExternalText.h"

#include <algorithm>
namespace Holonight::Authentication {
int IdentityListModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(items_.size());
}
QVariant IdentityListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= items_.size()) {
    return {};
  }
  const auto& item = items_.at(index.row());
  if (role == StableIdRole) {
    return item.stable_id;
  }
  if (role == DisplayLabelRole) {
    return item.display_label;
  }
  if (role == UsernameRole) {
    return item.username;
  }
  if (role == FullNameRole) {
    return item.full_name;
  }
  if (role == AvatarUrlRole) {
    return item.avatar_url;
  }
  return {};
}
QHash<int, QByteArray> IdentityListModel::roleNames() const {
  return {{StableIdRole, "stableId"},
          {DisplayLabelRole, "displayLabel"},
          {UsernameRole, "username"},
          {FullNameRole, "fullName"},
          {AvatarUrlRole, "avatarUrl"}};
}
void IdentityListModel::setItems(QList<Identity> items) {
  beginResetModel();
  for (auto& item : items) {
    item.username = normalizeExternalText(item.username, kReferenceLimits);
    item.full_name = normalizeExternalText(item.full_name, kReferenceLimits);
    if (!item.avatar_url.isLocalFile() || !item.avatar_url.host().isEmpty()) {
      item.avatar_url = QUrl{};
    }
  }
  items_ = std::move(items);
  endResetModel();
}
bool IdentityListModel::updateProfile(const Identity& profile) {
  for (int row = 0; row < items_.size(); ++row) {
    auto& item = items_[row];
    if (item.stable_id != profile.stable_id) {
      continue;
    }
    item.username = normalizeExternalText(profile.username, kReferenceLimits);
    item.full_name = normalizeExternalText(profile.full_name, kReferenceLimits);
    item.avatar_url =
        profile.avatar_url.isLocalFile() && profile.avatar_url.host().isEmpty() ? profile.avatar_url : QUrl{};
    emit dataChanged(index(row), index(row), {UsernameRole, FullNameRole, AvatarUrlRole});
    return true;
  }
  return false;
}
QVariantMap IdentityListModel::profile(const QString& stable_id) const {
  for (const auto& item : items_) {
    if (item.stable_id == stable_id) {
      return {{QStringLiteral("displayLabel"), item.display_label},
              {QStringLiteral("username"), item.username},
              {QStringLiteral("fullName"), item.full_name},
              {QStringLiteral("avatarUrl"), item.avatar_url}};
    }
  }
  return {};
}
bool IdentityListModel::contains(const QString& stable_id) const {
  return std::ranges::any_of(items_, [&](const Identity& item) { return item.stable_id == stable_id; });
}
QString IdentityListModel::preferred(uint uid) const {
  for (const auto& item : items_) {
    if (item.has_uid && item.uid == uid) {
      return item.stable_id;
    }
  }
  return items_.isEmpty() ? QString{} : items_.front().stable_id;
}
}  // namespace Holonight::Authentication
