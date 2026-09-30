#include "CompositorService.h"

#include "CompositorBackend.h"

#include <algorithm>

CompositorService::CompositorService(QObject* parent) : CompositorService({}, parent) {}
CompositorService::CompositorService(std::unique_ptr<CompositorBackend> backend, QObject* parent)
    : QObject(parent), workspace_model_(this), backend_(std::move(backend)) {
  if (backend_) connect(backend_.get(), &CompositorBackend::snapshotReady, this, &CompositorService::publishSnapshot);
}
CompositorService::~CompositorService() = default;
QString CompositorService::activeWindowTitle(const QString& output) const {
  return snapshot_.active_windows.value(output).title;
}
QString CompositorService::activeWindowAppId(const QString& output) const {
  return snapshot_.active_windows.value(output).app_id;
}
QString CompositorService::activeWindowCategory(const QString& output) const {
  return snapshot_.active_windows.value(output).category;
}
bool CompositorService::isOutputEmpty(const QString& output) const {
  if (!snapshot_.capabilities.occupancy) {
    return false;
  }
  if (snapshot_.occupied_outputs.contains(output)) return !snapshot_.occupied_outputs.value(output);
  return std::ranges::none_of(snapshot_.workspaces, [&output](const CompositorWorkspace& workspace) {
    return workspace.outputs.contains(output) && workspace.active && workspace.occupied.value_or(true);
  });
}
void CompositorService::activateWorkspace(const QString& workspace_id) {
  if (!snapshot_.connected || !snapshot_.capabilities.workspace_activation || workspace_id.isEmpty()) {
    return;
  }
  const auto found = std::ranges::find(snapshot_.workspaces, workspace_id, &CompositorWorkspace::id);
  if (found == snapshot_.workspaces.end() || !found->can_activate) return;
  emit workspaceActivationRequested(workspace_id);
  if (backend_ != nullptr) {
    backend_->activateWorkspace(workspace_id);
  }
}
WindowActivationResult CompositorService::requestWindowActivation(const WindowActivationRequest& request) {
  if (!isValidWindowActivationRequest(request)) {
    return WindowActivationResult::InvalidRequest;
  }
  if (!snapshot_.connected) {
    return WindowActivationResult::Disconnected;
  }
  if (!snapshot_.capabilities.window_activation || backend_ == nullptr) {
    return WindowActivationResult::Unsupported;
  }
  return backend_->requestWindowActivation(request);
}
void CompositorService::start() {
  if (backend_ != nullptr) {
    backend_->start();
  } else {
    publishSnapshot({.diagnostic = QStringLiteral("No compositor integration available")});
  }
}
void CompositorService::publishSnapshot(CompositorSnapshot snapshot) {
  if (!snapshot.connected) {
    snapshot.capabilities = {};
    snapshot.focused_output.clear();
    snapshot.workspaces.clear();
    snapshot.active_windows.clear();
    snapshot.occupied_outputs.clear();
  }
  std::ranges::stable_sort(snapshot.workspaces, {}, &CompositorWorkspace::stable_order);
  QList<CompositorWorkspace> rows = snapshot.workspaces;
  workspace_model_.replaceTransactional(
      std::move(rows), [this, snapshot = std::move(snapshot)]() mutable { snapshot_ = std::move(snapshot); });
  ++revision_;
  emit revisionChanged();
}
