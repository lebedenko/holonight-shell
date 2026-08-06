#include "WorkspaceModel.h"

#include <QByteArray>
#include <QVariantMap>

#include <algorithm>
#include <print>

WorkspaceModel::WorkspaceModel(QObject* parent) : QAbstractListModel(parent) {}

int WorkspaceModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(rows_.size());
}

QVariant WorkspaceModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() >= rows_.size()) {
    return {};
  }
  const auto& entry = rows_.at(index.row());
  switch (role) {
    case WorkspaceIdRole:
      return entry.id;
    case WorkspaceNameRole:
      return entry.name;
    case WorkspaceStateRole:
      return QVariant::fromValue(effectiveState(entry));
    case WorkspaceOnMonitorRole:
      return entry.on_monitor;
    default:
      return {};
  }
}

QHash<int, QByteArray> WorkspaceModel::roleNames() const {
  static const QHash<int, QByteArray> kRoles{
      {WorkspaceIdRole, "wsId"},
      {WorkspaceNameRole, "wsName"},
      {WorkspaceStateRole, "wsState"},
      {WorkspaceOnMonitorRole, "wsOnMonitor"},
  };
  return kRoles;
}

int WorkspaceModel::stateForId(int wsId) const {
  const WorkspaceEntry* entry = entryForId(wsId);
  if (entry != nullptr) {
    return static_cast<int>(effectiveState(*entry));
  }
  return static_cast<int>(WorkspaceState::Empty);
}

int WorkspaceModel::activeWorkspaceForMonitor(const QString& monitorName) const {
  if (monitorName.isEmpty()) {
    return 0;
  }
  for (const auto& entry : rows_) {
    if (entry.is_special) {
      continue;
    }
    if (entry.state == WorkspaceState::Active && entry.monitor_names.contains(monitorName)) {
      return entry.id;
    }
  }
  return 0;
}

int WorkspaceModel::maxWorkspaceId() const {
  int max_id = 0;
  for (const auto& entry : rows_) {
    max_id = std::max(max_id, entry.id);
  }
  return max_id;
}

bool WorkspaceModel::hasOccupiedOrUrgentBeyond(int minId) const {
  return std::ranges::any_of(rows_, [this, minId](const WorkspaceEntry& entry) {
    if (entry.id < minId) {
      return false;
    }
    const WorkspaceState state = effectiveState(entry);
    return state == WorkspaceState::Occupied || state == WorkspaceState::Urgent;
  });
}

bool WorkspaceModel::hasVisibleOrNavigableBeyond(int minId) const {
  return std::ranges::any_of(rows_, [this, minId](const WorkspaceEntry& entry) {
    if (entry.id < minId) {
      return false;
    }
    const WorkspaceState state = effectiveState(entry);
    return state == WorkspaceState::Occupied || state == WorkspaceState::Urgent ||
           state == WorkspaceState::FocusedActiveMonitor || state == WorkspaceState::FocusedInactiveMonitor;
  });
}

bool WorkspaceModel::hasUrgentBeyond(int minId) const {
  return std::ranges::any_of(rows_, [this, minId](const WorkspaceEntry& entry) {
    return entry.id >= minId && effectiveState(entry) == WorkspaceState::Urgent;
  });
}

int WorkspaceModel::firstUrgentIdBeyond(int minId) const {
  int lowest = 0;
  for (const auto& entry : rows_) {
    if (entry.id >= minId && effectiveState(entry) == WorkspaceState::Urgent) {
      if (lowest == 0 || entry.id < lowest) {
        lowest = entry.id;
      }
    }
  }
  return lowest;
}

bool WorkspaceModel::hasUrgentBefore(int maxIdExclusive) const {
  return std::ranges::any_of(rows_, [this, maxIdExclusive](const WorkspaceEntry& entry) {
    return entry.id < maxIdExclusive && effectiveState(entry) == WorkspaceState::Urgent;
  });
}

int WorkspaceModel::lastUrgentIdBefore(int maxIdExclusive) const {
  int highest = 0;
  for (const auto& entry : rows_) {
    if (entry.id < maxIdExclusive && effectiveState(entry) == WorkspaceState::Urgent) {
      highest = std::max(highest, entry.id);
    }
  }
  return highest;
}

QVariantList WorkspaceModel::specialWorkspaceList() const {
  QVariantList list;
  list.reserve(special_rows_.size());
  for (const auto& entry : special_rows_) {
    list.append(QVariantMap{
        {QStringLiteral("name"), entry.name},
        {QStringLiteral("active"), entry.active},
        {QStringLiteral("urgent"), entry.urgent},
        {QStringLiteral("occupied"), entry.occupied},
        {QStringLiteral("monitorNames"), entry.monitor_names},
    });
  }
  return list;
}

void WorkspaceModel::setDisplayCount(int count) {
  if (display_count_ == count) {
    return;
  }
  display_count_ = count;
  ++revision_;
  emit revisionChanged();
  emit displayCountChanged();
}

void WorkspaceModel::activateWorkspace(int wsId) {
  if (wsId <= 0) {
    return;
  }
  emit activateWorkspaceRequested(wsId);
}

void WorkspaceModel::activateSpecialWorkspace(const QString& name) {
  if (name.isEmpty()) {
    return;
  }
  emit activateSpecialWorkspaceRequested(name);
}

void WorkspaceModel::applyBatchUpdate(const QList<WorkspaceEntry>& entries) {
  const int new_count = static_cast<int>(entries.size());
  if (first_populate_) {
    first_populate_ = false;
    beginResetModel();
    rows_ = entries;
    endResetModel();
  } else if (rows_.size() != new_count) {
    beginResetModel();
    rows_ = entries;
    endResetModel();
  } else {
    rows_ = entries;
    emitRowsChanged({WorkspaceIdRole, WorkspaceNameRole, WorkspaceStateRole, WorkspaceOnMonitorRole});
  }

  for (const auto& entry : rows_) {
    if (entry.id != focused_workspace_id_ && entry.state != WorkspaceState::Urgent &&
        !urgent_workspace_ids_.contains(entry.id)) {
      acknowledged_urgent_workspace_ids_.remove(entry.id);
    }
  }

  ++revision_;
  emit revisionChanged();

  if (!qgetenv("HOLONIGHT_DEBUG").isEmpty()) {
    auto stateStr = [](WorkspaceState ws_state) -> const char* {
      switch (ws_state) {
        case WorkspaceState::Empty:
          return "Empty";
        case WorkspaceState::Occupied:
          return "Occupied";
        case WorkspaceState::Active:
          return "Active";
        case WorkspaceState::FocusedInactiveMonitor:
          return "FocusedInactiveMonitor";
        case WorkspaceState::Urgent:
          return "Urgent";
      }
      return "Unknown";
    };
    for (const auto& entry : rows_) {
      std::println("[ws rev={}] id={} state={} on_monitor={}", revision_, entry.id, stateStr(entry.state),
                   entry.on_monitor ? "true" : "false");
    }
  }
}

void WorkspaceModel::applySpecialWorkspaces(const QList<SpecialWorkspaceEntry>& entries) {
  if (special_rows_ == entries) {
    return;
  }
  special_rows_ = entries;
  ++revision_;
  emit revisionChanged();
}

void WorkspaceModel::setFocusedWorkspaceId(int wsId) {
  if (wsId > 0) {
    acknowledged_urgent_workspace_ids_.insert(wsId);
  }
  if (focused_workspace_id_ == wsId) {
    if (urgent_workspace_ids_.remove(wsId)) {
      emitRowsChanged({WorkspaceStateRole});
      ++revision_;
      emit revisionChanged();
    }
    return;
  }
  focused_workspace_id_ = wsId;
  urgent_workspace_ids_.remove(wsId);
  emitRowsChanged({WorkspaceStateRole});
  ++revision_;
  emit revisionChanged();
}

bool WorkspaceModel::isWorkspaceOccupied(int wsId) const { return occupied_workspace_ids_.contains(wsId); }

void WorkspaceModel::setOccupiedWorkspaceIds(const QSet<int>& workspace_ids) {
  if (occupied_workspace_ids_ == workspace_ids) {
    return;
  }
  occupied_workspace_ids_ = workspace_ids;
  emitRowsChanged({WorkspaceStateRole});
  ++revision_;
  emit revisionChanged();
}

void WorkspaceModel::addUrgentWorkspaceId(int wsId) {
  if (wsId <= 0 || wsId == focused_workspace_id_ || urgent_workspace_ids_.contains(wsId) ||
      acknowledged_urgent_workspace_ids_.contains(wsId)) {
    return;
  }
  urgent_workspace_ids_.insert(wsId);
  emitRowsChanged({WorkspaceStateRole});
  ++revision_;
  emit revisionChanged();
}

WorkspaceModel::WorkspaceState WorkspaceModel::effectiveState(const WorkspaceEntry& entry) const {
  const bool active = entry.state == WorkspaceState::Active || entry.state == WorkspaceState::FocusedActiveMonitor ||
                      entry.state == WorkspaceState::FocusedInactiveMonitor;
  const bool urgent = entry.state == WorkspaceState::Urgent;
  const bool acknowledged_urgent = acknowledged_urgent_workspace_ids_.contains(entry.id);

  if (active) {
    if (entry.id == focused_workspace_id_ || focused_workspace_id_ == 0) {
      return WorkspaceState::FocusedActiveMonitor;
    }
    return WorkspaceState::FocusedInactiveMonitor;
  }
  if (entry.id == focused_workspace_id_ && urgent && acknowledged_urgent) {
    return WorkspaceState::FocusedActiveMonitor;
  }
  if (urgent && acknowledged_urgent) {
    return WorkspaceState::Occupied;
  }
  if (urgent) {
    return WorkspaceState::Urgent;
  }
  if (urgent_workspace_ids_.contains(entry.id) && acknowledged_urgent) {
    return WorkspaceState::Occupied;
  }
  if (urgent_workspace_ids_.contains(entry.id)) {
    return WorkspaceState::Urgent;
  }
  if (occupied_workspace_ids_.contains(entry.id) || entry.state == WorkspaceState::Occupied) {
    return WorkspaceState::Occupied;
  }
  return WorkspaceState::Empty;
}

const WorkspaceModel::WorkspaceEntry* WorkspaceModel::entryForId(int wsId) const {
  for (const auto& entry : rows_) {
    if (entry.id == wsId) {
      return &entry;
    }
  }
  return nullptr;
}

void WorkspaceModel::emitRowsChanged(const QList<int>& roles) {
  if (rows_.isEmpty()) {
    return;
  }
  emit dataChanged(index(0), index(static_cast<int>(rows_.size()) - 1), roles);
}
