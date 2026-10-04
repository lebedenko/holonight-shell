#include "CompositorWorkspaceModel.h"

#include <QVariant>

#include <algorithm>

CompositorWorkspaceModel::CompositorWorkspaceModel(QObject* parent) : QAbstractListModel(parent) {}

int CompositorWorkspaceModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

QVariant CompositorWorkspaceModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) {
    return {};
  }
  const CompositorWorkspace& workspace = entries_.at(index.row());
  switch (role) {
    case WorkspaceIdRole:
      return workspace.id;
    case DisplayNameRole:
      return workspace.display_name;
    case StableOrderRole:
      return workspace.stable_order;
    case CanActivateRole:
      return workspace.can_activate;
    case GroupsRole:
      return workspace.groups;
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
      if (workspace.focused) {
        return QStringLiteral("focused");
      }
      if (workspace.urgent) {
        return QStringLiteral("urgent");
      }
      if (workspace.active) {
        return QStringLiteral("active");
      }
      if (workspace.occupied.value_or(false)) {
        return QStringLiteral("occupied");
      }
      return workspace.occupied.has_value() ? QStringLiteral("empty") : QStringLiteral("inactive");
    default:
      return {};
  }
}

QHash<int, QByteArray> CompositorWorkspaceModel::roleNames() const {
  return {{WorkspaceIdRole, "workspaceId"}, {DisplayNameRole, "displayName"}, {StableOrderRole, "stableOrder"},
          {CanActivateRole, "canActivate"}, {GroupsRole, "groups"},           {OutputsRole, "outputs"},
          {ActiveRole, "active"},           {FocusedRole, "focused"},         {UrgentRole, "urgent"},
          {OccupiedRole, "occupied"},       {VisualStateRole, "visualState"}};
}

void CompositorWorkspaceModel::replace(QList<CompositorWorkspace> workspaces) {
  replaceTransactional(std::move(workspaces), [] {});
}

void CompositorWorkspaceModel::replaceTransactional(QList<CompositorWorkspace> workspaces,
                                                    const std::function<void()>& commit) {
  if (entries_ == workspaces) {
    commit();
    return;
  }
  const bool same_structure =
      entries_.size() == workspaces.size() &&
      std::ranges::equal(entries_, workspaces, {}, &CompositorWorkspace::id, &CompositorWorkspace::id);
  if (!same_structure) {
    beginResetModel();
    entries_ = std::move(workspaces);
    commit();
    endResetModel();
    return;
  }

  CompositorWorkspaceModel next;
  next.entries_ = std::move(workspaces);
  QList<QList<int>> changed_roles;
  for (int row = 0; row < rowCount(); ++row) {
    QList<int> roles;
    for (int role = WorkspaceIdRole; role <= VisualStateRole; ++role) {
      if (data(index(row, 0), role) != next.data(next.index(row, 0), role)) roles.append(role);
    }
    changed_roles.append(roles);
  }
  entries_ = std::move(next.entries_);
  commit();
  for (int row = 0; row < changed_roles.size(); ++row) {
    if (!changed_roles[row].isEmpty()) emit dataChanged(index(row, 0), index(row, 0), changed_roles[row]);
  }
}

int CompositorWorkspaceModel::focusedRow() const {
  for (int row = 0; row < static_cast<int>(entries_.size()); ++row) {
    if (entries_.at(row).focused) {
      return row;
    }
  }
  return -1;
}

int CompositorWorkspaceModel::firstVisibleRow(int display_count) const {
  if (display_count <= 0 || entries_.size() <= display_count) {
    return 0;
  }
  int focus = focusedRow();
  if (focus < 0) {
    const auto active = std::ranges::find(entries_, true, &CompositorWorkspace::active);
    focus = active == entries_.end() ? 0 : static_cast<int>(active - entries_.begin());
  }
  return std::clamp(focus - ((display_count - 1) / 2), 0, static_cast<int>(entries_.size()) - display_count);
}
