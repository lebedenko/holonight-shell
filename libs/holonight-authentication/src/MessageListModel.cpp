#include "MessageListModel.h"
namespace Holonight::Authentication {
int MessageListModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(items_.size());
}
QVariant MessageListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= items_.size()) {
    return {};
  }
  const auto& item = items_.at(index.row());
  if (role == SeverityRole) {
    return static_cast<int>(item.severity);
  }
  if (role == TextRole) {
    return item.text;
  }
  return {};
}
QHash<int, QByteArray> MessageListModel::roleNames() const { return {{SeverityRole, "severity"}, {TextRole, "text"}}; }
void MessageListModel::setItems(QList<AuthenticationMessage> items) {
  beginResetModel();
  items_ = std::move(items);
  endResetModel();
}
void MessageListModel::append(AuthenticationMessage item) {
  const int row = static_cast<int>(items_.size());
  beginInsertRows({}, row, row);
  items_.append(std::move(item));
  endInsertRows();
}
}  // namespace Holonight::Authentication
