#include "CompositorService.h"

#include "CompositorBackend.h"
#include "GenericBackend.h"
#include "HyprlandBackend.h"
#include "SwayBackend.h"

#include <algorithm>
#include <limits>

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
int CompositorService::activeNumericWorkspaceForOutput(const QString& output) const {
  const auto found = std::ranges::find_if(snapshot_.workspaces, [&output](const CompositorWorkspace& workspace) {
    return workspace.numeric_slot.has_value() && workspace.active && workspace.outputs.contains(output);
  });
  return found == snapshot_.workspaces.end() ? 0 : *found->numeric_slot;
}
QString CompositorService::numericWorkspaceVisualState(int slot) const {
  const auto found =
      std::ranges::find(snapshot_.workspaces, std::optional<int>{slot}, &CompositorWorkspace::numeric_slot);
  if (found == snapshot_.workspaces.end()) {
    return QStringLiteral("empty");
  }
  if (found->focused) {
    return QStringLiteral("focused-active");
  }
  if (found->active) {
    return QStringLiteral("focused-inactive");
  }
  if (found->urgent) {
    return QStringLiteral("urgent");
  }
  return found->occupied.value_or(false) ? QStringLiteral("occupied") : QStringLiteral("empty");
}
bool CompositorService::hasNavigableNumericWorkspaceAtOrBeyond(int slot) const {
  return std::ranges::any_of(snapshot_.workspaces, [slot](const CompositorWorkspace& workspace) {
    return workspace.numeric_slot.value_or(0) >= slot &&
           (workspace.active || workspace.urgent || workspace.occupied.value_or(false));
  });
}
bool CompositorService::hasUrgentNumericWorkspaceAtOrBeyond(int slot) const {
  return std::ranges::any_of(snapshot_.workspaces, [slot](const CompositorWorkspace& workspace) {
    return workspace.numeric_slot.value_or(0) >= slot && workspace.urgent;
  });
}
int CompositorService::firstUrgentNumericWorkspaceAtOrBeyond(int slot) const {
  int first = std::numeric_limits<int>::max();
  for (const CompositorWorkspace& workspace : snapshot_.workspaces) {
    if (workspace.numeric_slot.value_or(0) >= slot && workspace.urgent) {
      first = std::min(first, *workspace.numeric_slot);
    }
  }
  return first == std::numeric_limits<int>::max() ? 0 : first;
}
bool CompositorService::hasUrgentNumericWorkspaceBefore(int slot) const {
  return std::ranges::any_of(snapshot_.workspaces, [slot](const CompositorWorkspace& workspace) {
    const int numeric_slot = workspace.numeric_slot.value_or(0);
    return numeric_slot > 0 && numeric_slot < slot && workspace.urgent;
  });
}
int CompositorService::lastUrgentNumericWorkspaceBefore(int slot) const {
  int last = 0;
  for (const CompositorWorkspace& workspace : snapshot_.workspaces) {
    const int numeric_slot = workspace.numeric_slot.value_or(0);
    if (numeric_slot > 0 && numeric_slot < slot && workspace.urgent) {
      last = std::max(last, numeric_slot);
    }
  }
  return last;
}
QVariantList CompositorService::specialWorkspaces() const {
  QVariantList result;
  for (const CompositorWorkspace& workspace : snapshot_.workspaces) {
    if (workspace.kind != QLatin1String("special")) {
      continue;
    }
    result.append(QVariantMap{{QStringLiteral("id"), workspace.id},
                              {QStringLiteral("name"), workspace.display_name},
                              {QStringLiteral("active"), workspace.active},
                              {QStringLiteral("urgent"), workspace.urgent},
                              {QStringLiteral("occupied"), workspace.occupied.value_or(false)},
                              {QStringLiteral("monitorNames"), workspace.outputs}});
  }
  return result;
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
