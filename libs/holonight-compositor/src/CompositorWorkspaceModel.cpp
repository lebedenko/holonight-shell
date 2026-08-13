#include "CompositorWorkspaceModel.h"

#include <QVariant>

CompositorWorkspaceModel::CompositorWorkspaceModel(QObject* parent) : QAbstractListModel(parent) {}

int CompositorWorkspaceModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : entries_.size();
}

QVariant CompositorWorkspaceModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) {
    return {};
  }
  const CompositorWorkspace& workspace = entries_.at(index.row());
  switch (role) {
    case WorkspaceIdRole:
      return workspace.id;
    case NumericSlotRole:
      return workspace.numeric_slot ? QVariant(*workspace.numeric_slot) : QVariant{};
    case DisplayNameRole:
      return workspace.display_name;
    case StableOrderRole:
      return workspace.stable_order;
    case WorkspaceKindRole:
      return workspace.kind;
    case OutputsRole:
      return workspace.outputs;
    case ActiveRole:
      return workspace.active;
    case FocusedRole:
      return workspace.focused;
    case UrgentRole:
      return workspace.urgent;
    case OccupiedRole:
      return workspace.occupied ? QVariant(*workspace.occupied) : QVariant{};
    case VisualStateRole:
      if (workspace.focused) return QStringLiteral("focused");
      if (workspace.urgent) return QStringLiteral("urgent");
      if (workspace.active) return QStringLiteral("active");
      if (workspace.occupied.value_or(false)) return QStringLiteral("occupied");
      return QStringLiteral("empty");
    default:
      return {};
  }
}

QHash<int, QByteArray> CompositorWorkspaceModel::roleNames() const {
  return {{WorkspaceIdRole, "workspaceId"},
          {NumericSlotRole, "numericSlot"},
          {DisplayNameRole, "displayName"},
          {StableOrderRole, "stableOrder"},
          {WorkspaceKindRole, "workspaceKind"},
          {OutputsRole, "outputs"},
          {ActiveRole, "active"},
          {FocusedRole, "focused"},
          {UrgentRole, "urgent"},
          {OccupiedRole, "occupied"},
          {VisualStateRole, "visualState"}};
}

void CompositorWorkspaceModel::replace(QList<CompositorWorkspace> workspaces) {
  beginResetModel();
  entries_ = std::move(workspaces);
  endResetModel();
}
