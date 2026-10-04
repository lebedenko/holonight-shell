#include "WorkspacePresentation.h"

#include <algorithm>
#include <limits>

WorkspacePresentation::WorkspacePresentation(CompositorService* service, NumberedWorkspaceProvider* provider,
                                             QObject* parent)
    : QObject(parent), service_(service), provider_(provider) {
  connect(service_, &CompositorService::revisionChanged, this, [this] {
    const auto state = provider_ && service_->connected() ? provider_->numberedWorkspaces() : NumberedWorkspaceState{};
    eligible_ = state.eligible;
    slots_ = state.assignments;
    emit revisionChanged();
  });
}
void WorkspacePresentation::setWorkspaceDisplayCount(int count) {
  count = std::clamp(count, 1, 20);
  if (display_count_ == count) {
    return;
  }
  display_count_ = count;
  emit workspaceDisplayCountChanged();
  emit revisionChanged();
}
int WorkspacePresentation::firstVisibleWorkspaceRow() const {
  return qobject_cast<CompositorWorkspaceModel*>(service_->workspaces())->firstVisibleRow(display_count_);
}
void WorkspacePresentation::activateNumberedSlot(int slot) {
  if (eligible_ && slot > 0 && (provider_ != nullptr)) {
    provider_->activateNumberedSlot(slot);
  }
}
int WorkspacePresentation::activeNumericWorkspaceForOutput(const QString& output) const {
  const auto found =
      std::ranges::find_if(service_->snapshot().workspaces, [this, &output](const CompositorWorkspace& workspace) {
        return slots_.contains(workspace.id) && workspace.active && workspace.outputs.contains(output);
      });
  return found == service_->snapshot().workspaces.end() ? 0 : slots_.value(found->id);
}
QString WorkspacePresentation::numericWorkspaceVisualState(int slot) const {
  const auto found = std::ranges::find_if(service_->snapshot().workspaces, [this, slot](const auto& workspace) {
    return slots_.value(workspace.id) == slot;
  });
  if (found == service_->snapshot().workspaces.end()) {
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
bool WorkspacePresentation::hasNavigableNumericWorkspaceAtOrBeyond(int slot) const {
  return std::ranges::any_of(service_->snapshot().workspaces, [this, slot](const CompositorWorkspace& workspace) {
    return slots_.value(workspace.id) >= slot &&
           (workspace.active || workspace.urgent || workspace.occupied.value_or(false));
  });
}
bool WorkspacePresentation::hasUrgentNumericWorkspaceAtOrBeyond(int slot) const {
  return std::ranges::any_of(service_->snapshot().workspaces, [this, slot](const CompositorWorkspace& workspace) {
    return slots_.value(workspace.id) >= slot && workspace.urgent;
  });
}
int WorkspacePresentation::firstUrgentNumericWorkspaceAtOrBeyond(int slot) const {
  int first = std::numeric_limits<int>::max();
  for (const CompositorWorkspace& workspace : service_->snapshot().workspaces) {
    if (slots_.value(workspace.id) >= slot && workspace.urgent) {
      first = std::min(first, slots_.value(workspace.id));
    }
  }
  return first == std::numeric_limits<int>::max() ? 0 : first;
}
bool WorkspacePresentation::hasUrgentNumericWorkspaceBefore(int slot) const {
  return std::ranges::any_of(service_->snapshot().workspaces, [this, slot](const CompositorWorkspace& workspace) {
    const int numeric_slot = slots_.value(workspace.id);
    return numeric_slot > 0 && numeric_slot < slot && workspace.urgent;
  });
}
int WorkspacePresentation::lastUrgentNumericWorkspaceBefore(int slot) const {
  int last = 0;
  for (const CompositorWorkspace& workspace : service_->snapshot().workspaces) {
    const int numeric_slot = slots_.value(workspace.id);
    if (numeric_slot > 0 && numeric_slot < slot && workspace.urgent) {
      last = std::max(last, numeric_slot);
    }
  }
  return last;
}

int WorkspacePresentation::viewportStart(const QString& output, int pan) const {
  const qint64 center = std::max(1, activeNumericWorkspaceForOutput(output) - ((display_count_ - 1) / 2));
  return static_cast<int>(std::clamp<qint64>(center + pan, 1, std::numeric_limits<int>::max() - display_count_ - 2));
}
QVariantList WorkspacePresentation::numberedSlots(int start, int count) const {
  QVariantList entries;
  if (!eligible_) {
    return entries;
  }
  count = std::clamp(count, 0, 24);
  for (int index = 0; index < count; ++index) {
    const qint64 slot = static_cast<qint64>(start) + index;
    if (slot <= 0 || slot > std::numeric_limits<int>::max()) {
      continue;
    }
    const auto number = static_cast<int>(slot);
    entries.append(QVariantMap{
        {QStringLiteral("slot"), number},
        {QStringLiteral("workspaceId"), slots_.key(number)},
        {QStringLiteral("visualState"), numericWorkspaceVisualState(number)},
    });
  }
  return entries;
}
