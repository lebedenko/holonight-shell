#pragma once

#include "CompositorSelection.h"
#include "CompositorSnapshot.h"
#include "CompositorWorkspaceModel.h"

#include <QObject>
#include <QtQml/qqml.h>

class CompositorService final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(CompositorKind backendKind READ backendKind CONSTANT)
  Q_PROPERTY(QString backendName READ backendName CONSTANT)
  Q_PROPERTY(bool connected READ connected NOTIFY revisionChanged)
  Q_PROPERTY(QString diagnostic READ diagnostic NOTIFY revisionChanged)
  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
  Q_PROPERTY(QString focusedOutput READ focusedOutput NOTIFY revisionChanged)
  Q_PROPERTY(QAbstractItemModel* workspaces READ workspaces CONSTANT)
  Q_PROPERTY(bool canListWorkspaces READ canListWorkspaces NOTIFY revisionChanged)
  Q_PROPERTY(bool canActivateWorkspaces READ canActivateWorkspaces NOTIFY revisionChanged)
  Q_PROPERTY(bool canCreateNumericWorkspaces READ canCreateNumericWorkspaces NOTIFY revisionChanged)
  Q_PROPERTY(bool supportsSpecialWorkspaces READ supportsSpecialWorkspaces NOTIFY revisionChanged)
  Q_PROPERTY(bool hasActiveWindowData READ hasActiveWindowData NOTIFY revisionChanged)
  Q_PROPERTY(bool hasFocusedOutput READ hasFocusedOutput NOTIFY revisionChanged)
  Q_PROPERTY(bool hasUrgency READ hasUrgency NOTIFY revisionChanged)
  Q_PROPERTY(bool hasOccupancy READ hasOccupancy NOTIFY revisionChanged)

 public:
  explicit CompositorService(QObject* parent = nullptr);
  explicit CompositorService(CompositorKind kind, QObject* parent = nullptr);
  [[nodiscard]] CompositorKind backendKind() const { return kind_; }
  [[nodiscard]] QString backendName() const;
  [[nodiscard]] bool connected() const { return snapshot_.connected; }
  [[nodiscard]] QString diagnostic() const { return snapshot_.diagnostic; }
  [[nodiscard]] int revision() const { return revision_; }
  [[nodiscard]] QString focusedOutput() const { return snapshot_.focused_output; }
  [[nodiscard]] QAbstractItemModel* workspaces() { return &workspace_model_; }
  [[nodiscard]] bool canListWorkspaces() const { return snapshot_.capabilities.workspace_listing; }
  [[nodiscard]] bool canActivateWorkspaces() const { return snapshot_.capabilities.workspace_activation; }
  [[nodiscard]] bool canCreateNumericWorkspaces() const { return snapshot_.capabilities.numeric_workspace_creation; }
  [[nodiscard]] bool supportsSpecialWorkspaces() const { return snapshot_.capabilities.special_workspaces; }
  [[nodiscard]] bool hasActiveWindowData() const { return snapshot_.capabilities.active_window; }
  [[nodiscard]] bool hasFocusedOutput() const { return snapshot_.capabilities.focused_output; }
  [[nodiscard]] bool hasUrgency() const { return snapshot_.capabilities.urgency; }
  [[nodiscard]] bool hasOccupancy() const { return snapshot_.capabilities.occupancy; }
  Q_INVOKABLE QString activeWindowTitle(const QString& output) const;
  Q_INVOKABLE QString activeWindowAppId(const QString& output) const;
  Q_INVOKABLE QString activeWindowCategory(const QString& output) const;
  Q_INVOKABLE bool isOutputEmpty(const QString& output) const;
  Q_INVOKABLE void activateWorkspace(const QString& workspace_id);
  void publishSnapshot(CompositorSnapshot snapshot);

 Q_SIGNALS:
  void revisionChanged();
  void workspaceActivationRequested(const QString& workspace_id);

 private:
  CompositorKind kind_;
  int revision_{0};
  CompositorSnapshot snapshot_;
  CompositorWorkspaceModel workspace_model_;
};

Q_DECLARE_METATYPE(CompositorKind)
