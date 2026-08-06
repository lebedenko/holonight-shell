#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqml.h>

class WorkspaceModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
  Q_PROPERTY(int displayCount READ displayCount NOTIFY displayCountChanged)

 public:
  enum class WorkspaceState : uint8_t {
    Empty = 0,
    Occupied = 1,
    FocusedActiveMonitor = 2,
    FocusedInactiveMonitor = 3,
    Urgent = 4,
    Active = FocusedActiveMonitor,
  };
  Q_ENUM(WorkspaceState)

  enum Roles : uint16_t {  // NOLINT(cppcoreguidelines-use-enum-class): Qt model roles are int-compatible.
    WorkspaceIdRole = Qt::UserRole + 1,
    WorkspaceNameRole,
    WorkspaceStateRole,
    WorkspaceOnMonitorRole,
  };
  Q_ENUM(Roles)

  struct WorkspaceEntry {
    int id{0};
    QString name;
    WorkspaceState state{WorkspaceState::Empty};
    bool on_monitor{false};
    bool is_special{false};
    QStringList monitor_names;
  };

  struct SpecialWorkspaceEntry {
    QString name;
    bool active{false};
    bool urgent{false};
    bool occupied{false};
    QStringList monitor_names;

    bool operator==(const SpecialWorkspaceEntry&) const = default;
  };

  explicit WorkspaceModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  [[nodiscard]] int revision() const { return revision_; }
  [[nodiscard]] int displayCount() const { return display_count_; }

  void setDisplayCount(int count);

  [[nodiscard]] Q_INVOKABLE int stateForId(int wsId) const;
  // True if the given workspace currently holds at least one window. Drives the desktop-widget
  // occupancy gate via MonitorOccupancyService.
  [[nodiscard]] bool isWorkspaceOccupied(int wsId) const;
  [[nodiscard]] Q_INVOKABLE int activeWorkspaceForMonitor(const QString& monitorName) const;
  [[nodiscard]] Q_INVOKABLE int maxWorkspaceId() const;
  [[nodiscard]] Q_INVOKABLE bool hasVisibleOrNavigableBeyond(int minId) const;
  [[nodiscard]] Q_INVOKABLE bool hasOccupiedOrUrgentBeyond(int minId) const;
  [[nodiscard]] Q_INVOKABLE bool hasUrgentBeyond(int minId) const;
  [[nodiscard]] Q_INVOKABLE int firstUrgentIdBeyond(int minId) const;
  [[nodiscard]] Q_INVOKABLE bool hasUrgentBefore(int maxIdExclusive) const;
  [[nodiscard]] Q_INVOKABLE int lastUrgentIdBefore(int maxIdExclusive) const;
  [[nodiscard]] Q_INVOKABLE QVariantList specialWorkspaceList() const;
  Q_INVOKABLE void activateWorkspace(int wsId);
  Q_INVOKABLE void activateSpecialWorkspace(const QString& name);

  void applyBatchUpdate(const QList<WorkspaceEntry>& entries);
  void applySpecialWorkspaces(const QList<SpecialWorkspaceEntry>& entries);
  void setFocusedWorkspaceId(int wsId);
  void setOccupiedWorkspaceIds(const QSet<int>& workspace_ids);
  void addUrgentWorkspaceId(int wsId);

 Q_SIGNALS:
  void revisionChanged();
  void displayCountChanged();
  void activateWorkspaceRequested(int wsId);
  void activateSpecialWorkspaceRequested(const QString& name);

 private:
  [[nodiscard]] WorkspaceState effectiveState(const WorkspaceEntry& entry) const;
  [[nodiscard]] const WorkspaceEntry* entryForId(int wsId) const;
  void emitRowsChanged(const QList<int>& roles);

  QList<WorkspaceEntry> rows_;
  QList<SpecialWorkspaceEntry> special_rows_;
  QSet<int> occupied_workspace_ids_;
  QSet<int> urgent_workspace_ids_;
  QSet<int> acknowledged_urgent_workspace_ids_;
  bool first_populate_{true};
  int revision_{0};
  int focused_workspace_id_{0};
  int display_count_{5};
};
