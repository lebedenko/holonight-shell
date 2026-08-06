#include "MonitorOccupancyService.h"

#include "ActiveWindowService.h"
#include "WorkspaceModel.h"

#include <QGuiApplication>
#include <QScreen>

MonitorOccupancyService::MonitorOccupancyService(WorkspaceModel* model, ActiveWindowService* aws, QObject* parent)
    : QObject(parent), model_(model), aws_(aws) {
  connect(aws_, &ActiveWindowService::monitorWindowChanged, this, &MonitorOccupancyService::onMonitorWindowChanged);
  // The occupied-workspace set changed (a window opened/closed somewhere); any monitor whose visible
  // workspace flipped occupancy must be re-evaluated, so sweep all connected monitors.
  connect(model_, &WorkspaceModel::revisionChanged, this, &MonitorOccupancyService::onWorkspaceOccupancyChanged);
}

bool MonitorOccupancyService::isMonitorEmpty(const QString& monitor_name) const {
  const int workspace_id = aws_->visibleWorkspaceIdForMonitor(monitor_name);
  if (workspace_id == -1) {
    return true;  // unknown monitor: default to empty so a new widget surface starts visible
  }
  return !model_->isWorkspaceOccupied(workspace_id);
}

void MonitorOccupancyService::onMonitorWindowChanged(const QString& monitor_name) { evaluateMonitor(monitor_name); }

void MonitorOccupancyService::onWorkspaceOccupancyChanged() {
  for (const QScreen* screen : QGuiApplication::screens()) {
    evaluateMonitor(screen->name());
  }
}

void MonitorOccupancyService::evaluateMonitor(const QString& monitor_name) {
  const bool is_empty = isMonitorEmpty(monitor_name);
  const auto cached = last_empty_.constFind(monitor_name);
  if (cached != last_empty_.constEnd() && *cached == is_empty) {
    return;  // no transition for this monitor
  }
  last_empty_.insert(monitor_name, is_empty);
  emit occupancyChanged(monitor_name, is_empty);
}
