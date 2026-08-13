#include "GenericBackend.h"

#include <QGuiApplication>
#include <QScreen>

GenericWorkspaceHandle::GenericWorkspaceHandle(struct ::ext_workspace_handle_v1* handle, GenericBackend* backend,
                                               int order)
    : QtWayland::ext_workspace_handle_v1(handle), raw_(handle), backend_(backend) {
  workspace_.stable_order = order;
}

GenericWorkspaceHandle::~GenericWorkspaceHandle() {
  if (isInitialized()) destroy();
}

void GenericWorkspaceHandle::ext_workspace_handle_v1_name(const QString& name) {
  workspace_.id = name;
  workspace_.display_name = name;
}

void GenericWorkspaceHandle::ext_workspace_handle_v1_state(uint32_t state) {
  workspace_.active = (state & 0x1U) != 0U;
  workspace_.urgent = (state & 0x2U) != 0U;
}

void GenericWorkspaceHandle::ext_workspace_handle_v1_removed() {
  backend_->remove(this);
  delete this;
}

GenericWorkspaceGroup::GenericWorkspaceGroup(struct ::ext_workspace_group_handle_v1* group, GenericBackend* backend)
    : QtWayland::ext_workspace_group_handle_v1(group), backend_(backend) {}

GenericWorkspaceGroup::~GenericWorkspaceGroup() {
  if (isInitialized()) destroy();
}

QStringList GenericWorkspaceGroup::outputNames() const {
  QStringList names;
  for (struct ::wl_output* output : outputs_) {
    for (QScreen* screen : QGuiApplication::screens()) {
      auto* wayland_screen = screen->nativeInterface<QNativeInterface::QWaylandScreen>();
      if (wayland_screen != nullptr && wayland_screen->output() == output) names.append(screen->name());
    }
  }
  names.removeDuplicates();
  return names;
}

void GenericWorkspaceGroup::ext_workspace_group_handle_v1_output_enter(struct ::wl_output* output) {
  if (!outputs_.contains(output)) outputs_.append(output);
}
void GenericWorkspaceGroup::ext_workspace_group_handle_v1_output_leave(struct ::wl_output* output) {
  outputs_.removeOne(output);
}
void GenericWorkspaceGroup::ext_workspace_group_handle_v1_workspace_enter(struct ::ext_workspace_handle_v1* workspace) {
  workspaces_.insert(workspace);
}
void GenericWorkspaceGroup::ext_workspace_group_handle_v1_workspace_leave(struct ::ext_workspace_handle_v1* workspace) {
  workspaces_.remove(workspace);
}
void GenericWorkspaceGroup::ext_workspace_group_handle_v1_removed() {
  backend_->remove(this);
  delete this;
}

GenericProtocol::GenericProtocol(GenericBackend* backend) : QWaylandClientExtensionTemplate(1), backend_(backend) {}

void GenericProtocol::ext_workspace_manager_v1_workspace_group(struct ::ext_workspace_group_handle_v1* group) {
  backend_->groups_.append(new GenericWorkspaceGroup(group, backend_));
}
void GenericProtocol::ext_workspace_manager_v1_workspace(struct ::ext_workspace_handle_v1* workspace) {
  backend_->handles_.insert(workspace, new GenericWorkspaceHandle(workspace, backend_, backend_->next_order_++));
}
void GenericProtocol::ext_workspace_manager_v1_done() { backend_->publishSnapshotOnDone(); }

GenericBackend::GenericBackend(QObject* parent) : CompositorBackend(parent), protocol_(this) {
  connect(&protocol_, &QWaylandClientExtension::activeChanged, this, [this] {
    if (!protocol_.isActive()) emit snapshotReady({.diagnostic = QStringLiteral("ext-workspace-v1 is unavailable")});
  });
}

GenericBackend::~GenericBackend() {
  qDeleteAll(groups_);
  qDeleteAll(handles_);
}

void GenericBackend::start() {
  if (!protocol_.isActive()) emit snapshotReady({.diagnostic = QStringLiteral("waiting for ext-workspace-v1")});
}

void GenericBackend::publishSnapshotOnDone() {
  CompositorSnapshot snapshot{
      .connected = true,
      .capabilities = {.workspace_listing = true, .workspace_activation = true, .urgency = true},
  };
  for (GenericWorkspaceHandle* handle : std::as_const(handles_)) {
    CompositorWorkspace workspace = handle->workspace_;
    for (const GenericWorkspaceGroup* group : std::as_const(groups_)) {
      if (group->workspaces_.contains(handle->raw_)) workspace.outputs += group->outputNames();
    }
    workspace.outputs.removeDuplicates();
    if (!workspace.id.isEmpty()) snapshot.workspaces.append(std::move(workspace));
  }
  emit snapshotReady(std::move(snapshot));
}

void GenericBackend::activateWorkspace(const QString& workspace_id) {
  for (GenericWorkspaceHandle* handle : std::as_const(handles_)) {
    if (handle->workspace_.id == workspace_id) {
      handle->activate();
      protocol_.commit();
      return;
    }
  }
}

void GenericBackend::remove(GenericWorkspaceHandle* handle) {
  handles_.remove(handle->raw_);
  if (handle->isInitialized()) handle->destroy();
}
void GenericBackend::remove(GenericWorkspaceGroup* group) {
  groups_.removeOne(group);
  if (group->isInitialized()) group->destroy();
}
