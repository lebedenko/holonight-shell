#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class ActiveWindowService;
class WorkspaceModel;

// Answers one question per monitor: "is the workspace currently visible on this monitor empty?"
// (i.e. holds zero windows). It composes two existing data sources rather than opening its own
// Hyprland IPC socket:
//   - ActiveWindowService::visibleWorkspaceIdForMonitor() — which workspace is shown on a monitor;
//     ActiveWindowService::monitorWindowChanged() fires when that mapping changes.
//   - WorkspaceModel::isWorkspaceOccupied() — whether a workspace holds windows;
//     WorkspaceModel::revisionChanged() fires whenever the occupied set changes.
// It emits occupancyChanged(monitorName, isEmpty) only on a real transition for that monitor, which
// the desktop-widget WidgetManagers use to map/unmap their surfaces and freeze/resume timers.
class MonitorOccupancyService : public QObject {
  Q_OBJECT

 public:
  MonitorOccupancyService(WorkspaceModel* model, ActiveWindowService* aws, QObject* parent = nullptr);

  MonitorOccupancyService(const MonitorOccupancyService&) = delete;
  MonitorOccupancyService& operator=(const MonitorOccupancyService&) = delete;
  MonitorOccupancyService(MonitorOccupancyService&&) = delete;
  MonitorOccupancyService& operator=(MonitorOccupancyService&&) = delete;
  ~MonitorOccupancyService() override = default;

  // True iff the workspace currently visible on monitor_name has no windows. Unknown monitors are
  // treated as empty (conservative: a freshly created widget surface starts visible).
  [[nodiscard]] bool isMonitorEmpty(const QString& monitor_name) const;

 Q_SIGNALS:
  void occupancyChanged(const QString& monitor_name, bool is_empty);

 private Q_SLOTS:
  void onMonitorWindowChanged(const QString& monitor_name);
  void onWorkspaceOccupancyChanged();

 private:
  // Recompute one monitor's emptiness; emit occupancyChanged and update the cache iff it changed.
  void evaluateMonitor(const QString& monitor_name);

  WorkspaceModel* model_;
  ActiveWindowService* aws_;
  QHash<QString, bool> last_empty_;  // monitorName -> last emitted is_empty value
};
