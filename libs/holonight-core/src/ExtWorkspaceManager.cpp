#include "ExtWorkspaceManager.h"

#include "ConfigService.h"

#include <QGuiApplication>
#include <QScreen>

#include <algorithm>

// --- ExtWorkspaceHandle ---

ExtWorkspaceHandle::ExtWorkspaceHandle(struct ::ext_workspace_handle_v1* proto, ExtWorkspaceManager* mgr)
    : QtWayland::ext_workspace_handle_v1(proto), raw_(proto), manager_(mgr) {}

ExtWorkspaceHandle::~ExtWorkspaceHandle() {
  if (isInitialized()) {
    destroy();
  }
}

void ExtWorkspaceHandle::ext_workspace_handle_v1_name(const QString& name) {
  entry_.name = name;
  bool parsed_ok = false;
  const int parsed_id = name.toInt(&parsed_ok);
  entry_.is_special = !parsed_ok;
  if (parsed_ok) {
    entry_.id = parsed_id;
  }
}

void ExtWorkspaceHandle::ext_workspace_handle_v1_state(uint32_t state) {
  constexpr uint32_t kActive = 0x1U;
  constexpr uint32_t kUrgent = 0x2U;
  if ((state & kUrgent) != 0U) {
    entry_.state = WorkspaceModel::WorkspaceState::Urgent;
  } else if ((state & kActive) != 0U) {
    entry_.state = WorkspaceModel::WorkspaceState::Active;
  } else {
    entry_.state = WorkspaceModel::WorkspaceState::Empty;
  }
}

void ExtWorkspaceHandle::ext_workspace_handle_v1_removed() {
  manager_->handle_map_.remove(raw_);
  destroy();
  delete this;
}

// --- ExtWorkspaceGroup ---

ExtWorkspaceGroup::ExtWorkspaceGroup(struct ::ext_workspace_group_handle_v1* proto, ExtWorkspaceManager* mgr)
    : QtWayland::ext_workspace_group_handle_v1(proto), manager_(mgr) {}

ExtWorkspaceGroup::~ExtWorkspaceGroup() {
  if (isInitialized()) {
    destroy();
  }
}

void ExtWorkspaceGroup::ext_workspace_group_handle_v1_output_enter(struct ::wl_output* output) {
  if (!outputs_.contains(output)) {
    outputs_.append(output);
  }
}

void ExtWorkspaceGroup::ext_workspace_group_handle_v1_output_leave(struct ::wl_output* output) {
  outputs_.removeOne(output);
}

QStringList ExtWorkspaceGroup::monitorNames() const {
  QStringList names;
  for (auto* output : outputs_) {
    for (QScreen* screen : QGuiApplication::screens()) {
      auto* wayland_screen = screen->nativeInterface<QNativeInterface::QWaylandScreen>();
      if (wayland_screen != nullptr && wayland_screen->output() == output) {
        names.append(screen->name());
        break;
      }
    }
  }
  return names;
}

void ExtWorkspaceGroup::ext_workspace_group_handle_v1_workspace_enter(struct ::ext_workspace_handle_v1* workspace) {
  workspaces_.insert(workspace);
}

void ExtWorkspaceGroup::ext_workspace_group_handle_v1_workspace_leave(struct ::ext_workspace_handle_v1* workspace) {
  workspaces_.remove(workspace);
}

void ExtWorkspaceGroup::ext_workspace_group_handle_v1_removed() {
  manager_->groups_.removeOne(this);
  destroy();
  delete this;
}

// --- ExtWorkspaceManager ---

ExtWorkspaceManager::ExtWorkspaceManager(WorkspaceModel* model, ConfigService* config, QObject* parent)
    : QWaylandClientExtensionTemplate(1), model_(model) {
  if (config == nullptr) {
    qCritical("ExtWorkspaceManager: constructed with null ConfigService; workspace manager will remain inert");
    return;
  }
  model_->setDisplayCount(config->barWorkspaces().count);
  connect(config, &ConfigService::barWorkspacesChanged, this,
          [this, config]() { model_->setDisplayCount(config->barWorkspaces().count); });
  connect(model_, &WorkspaceModel::activateWorkspaceRequested, this, &ExtWorkspaceManager::activateWorkspace);
  connect(model_, &WorkspaceModel::activateSpecialWorkspaceRequested, this,
          &ExtWorkspaceManager::activateSpecialWorkspace);
  connect(this, &QWaylandClientExtension::activeChanged, this, [this]() {
    if (!isActive()) {
      qWarning("ExtWorkspaceManager: ext-workspace-v1 not supported by compositor");
    }
  });
}

ExtWorkspaceManager::~ExtWorkspaceManager() {
  qDeleteAll(groups_);
  qDeleteAll(handle_map_);
}

void ExtWorkspaceManager::ext_workspace_manager_v1_workspace_group(
    struct ::ext_workspace_group_handle_v1* workspace_group) {
  groups_.append(new ExtWorkspaceGroup(workspace_group, this));
}

void ExtWorkspaceManager::ext_workspace_manager_v1_workspace(struct ::ext_workspace_handle_v1* workspace) {
  handle_map_.insert(workspace, new ExtWorkspaceHandle(workspace, this));
}

void ExtWorkspaceManager::ext_workspace_manager_v1_done() {
  QList<WorkspaceModel::WorkspaceEntry> entries;
  QList<WorkspaceModel::SpecialWorkspaceEntry> specials;
  entries.reserve(handle_map_.size());

  for (auto* handle : std::as_const(handle_map_)) {
    WorkspaceModel::WorkspaceEntry entry = handle->entry_;
    for (const auto* group : std::as_const(groups_)) {
      if (group->hasOutput() && group->hasWorkspace(handle->raw_)) {
        entry.on_monitor = true;
        entry.monitor_names += group->monitorNames();
      }
    }
    if (entry.is_special) {
      specials.append(WorkspaceModel::SpecialWorkspaceEntry{
          .name = entry.name,
          .active = entry.state == WorkspaceModel::WorkspaceState::Active,
          .urgent = entry.state == WorkspaceModel::WorkspaceState::Urgent,
          .occupied = entry.state == WorkspaceModel::WorkspaceState::Occupied,
          .monitor_names = entry.monitor_names,
      });
    } else {
      entries.append(entry);
    }
  }

  std::ranges::sort(entries, {}, &WorkspaceModel::WorkspaceEntry::id);
  std::ranges::sort(specials, {}, &WorkspaceModel::SpecialWorkspaceEntry::name);
  model_->applyBatchUpdate(entries);
  model_->applySpecialWorkspaces(specials);
}

void ExtWorkspaceManager::activateWorkspace(int wsId) {
  for (auto* handle : std::as_const(handle_map_)) {
    if (handle->entry_.id == wsId) {
      handle->activate();
      commit();
      return;
    }
  }
}

void ExtWorkspaceManager::activateSpecialWorkspace(const QString& name) {
  for (auto* handle : std::as_const(handle_map_)) {
    if (handle->entry_.is_special && handle->entry_.name == name) {
      handle->activate();
      commit();
      return;
    }
  }
}
