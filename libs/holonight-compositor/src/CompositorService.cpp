#include "CompositorService.h"

#include "CompositorBackend.h"
#include "GenericBackend.h"
#include "HyprlandBackend.h"
#include "SwayBackend.h"

#include <algorithm>

namespace {
std::unique_ptr<CompositorBackend> makeBackend(CompositorKind kind) {
  if (kind == CompositorKind::Hyprland) {
    return std::make_unique<HyprlandBackend>();
  }
  if (kind == CompositorKind::Sway) {
    return std::make_unique<SwayBackend>();
  }
  return std::make_unique<GenericBackend>();
}

void synthesizeNumericSlots(CompositorSnapshot* snapshot, int count) {
  if (!snapshot->capabilities.numeric_workspace_creation) {
    return;
  }
  for (int slot = 1; slot <= count; ++slot) {
    const auto found =
        std::ranges::find(snapshot->workspaces, std::optional<int>{slot}, &CompositorWorkspace::numeric_slot);
    if (found == snapshot->workspaces.end()) {
      snapshot->workspaces.append({.id = QString::number(slot),
                                   .numeric_slot = slot,
                                   .display_name = QString::number(slot),
                                   .stable_order = slot});
    }
  }
}
}  // namespace

CompositorService::CompositorService(QObject* parent)
    : CompositorService(selectCompositor(systemCompositorEnvironment()), parent) {}
CompositorService::CompositorService(CompositorKind kind, QObject* parent)
    : CompositorService(kind, makeBackend(kind), parent) {}
CompositorService::CompositorService(CompositorKind kind, std::unique_ptr<CompositorBackend> backend, QObject* parent)
    : QObject(parent), kind_(kind), workspace_model_(this), backend_(std::move(backend)) {
  if (backend_ != nullptr) {
    backend_->setParent(this);
    connect(backend_.get(), &CompositorBackend::snapshotReady, this, &CompositorService::publishSnapshot);
  }
}
CompositorService::~CompositorService() = default;
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
  if (!snapshot_.capabilities.occupancy) {
    return false;
  }
  return std::ranges::none_of(snapshot_.workspaces, [&output](const CompositorWorkspace& workspace) {
    return workspace.outputs.contains(output) && workspace.active && workspace.occupied.value_or(true);
  });
}
void CompositorService::activateWorkspace(const QString& workspace_id) {
  if (!snapshot_.connected || !snapshot_.capabilities.workspace_activation || workspace_id.isEmpty()) {
    return;
  }
  emit workspaceActivationRequested(workspace_id);
  if (backend_ != nullptr) {
    backend_->activateWorkspace(workspace_id);
  }
}
void CompositorService::start() {
  if (backend_ != nullptr) {
    backend_->start();
  }
}
void CompositorService::setWorkspaceDisplayCount(int count) {
  count = std::clamp(count, 1, 20);
  if (workspace_display_count_ == count) {
    return;
  }
  workspace_display_count_ = count;
  emit workspaceDisplayCountChanged();
}
void CompositorService::publishSnapshot(CompositorSnapshot snapshot) {
  if (!snapshot.connected) {
    snapshot.capabilities = {};
    snapshot.focused_output.clear();
    snapshot.workspaces.clear();
    snapshot.active_windows.clear();
  }
  synthesizeNumericSlots(&snapshot, workspace_display_count_);
  std::ranges::stable_sort(snapshot.workspaces, {}, &CompositorWorkspace::stable_order);
  QList<CompositorWorkspace> rows = snapshot.workspaces;
  workspace_model_.replaceTransactional(
      std::move(rows), [this, snapshot = std::move(snapshot)]() mutable { snapshot_ = std::move(snapshot); });
  ++revision_;
  emit revisionChanged();
}
