#include "InhibitorModel.h"

InhibitorModel::InhibitorModel(QObject* parent) : QAbstractListModel(parent) {}

int InhibitorModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(entries_.size());
}

QVariant InhibitorModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(entries_.size())) {
    return {};
  }
  const InhibitorEntry& entry = entries_.at(index.row());
  switch (static_cast<Roles>(role)) {
    case WhoRole:
      return entry.who;
    case WhyRole:
      return entry.why;
    default:
      return {};
  }
}

QHash<int, QByteArray> InhibitorModel::roleNames() const {
  return {
      {WhoRole, "who"},
      {WhyRole, "why"},
  };
}

void InhibitorModel::setEntries(const QList<InhibitorEntry>& entries) {
  if (entries == entries_) {
    return;
  }
  const int old_count = count();
  beginResetModel();
  entries_ = entries;
  endResetModel();
  if (count() != old_count) {
    emit countChanged();
  }
}
