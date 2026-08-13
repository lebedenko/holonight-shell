#include "CompositorService.h"

#include <algorithm>

CompositorService::CompositorService(QObject* parent)
    : CompositorService(selectCompositor(systemCompositorEnvironment()), parent) {}
CompositorService::CompositorService(CompositorKind kind, QObject* parent)
    : QObject(parent), kind_(kind), workspace_model_(this) {}
QString CompositorService::backendName() const { return QString::fromLatin1(compositorName(kind_)); }
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
  if (!snapshot_.capabilities.occupancy) return false;
  for (const auto& workspace : snapshot_.workspaces) {
    if (workspace.outputs.contains(output) && workspace.active && workspace.occupied.value_or(true)) return false;
  }
  return true;
}
void CompositorService::activateWorkspace(const QString& workspace_id) {
  if (!snapshot_.capabilities.workspace_activation || workspace_id.isEmpty()) return;
  emit workspaceActivationRequested(workspace_id);
}
void CompositorService::publishSnapshot(CompositorSnapshot snapshot) {
  std::ranges::stable_sort(snapshot.workspaces, {}, &CompositorWorkspace::stable_order);
  workspace_model_.replace(snapshot.workspaces);
  snapshot_ = std::move(snapshot);
  ++revision_;
  emit revisionChanged();
}
