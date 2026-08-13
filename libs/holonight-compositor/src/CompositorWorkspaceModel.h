#pragma once

#include "CompositorSnapshot.h"

#include <QAbstractListModel>
#include <QtQml/qqml.h>

class CompositorWorkspaceModel final : public QAbstractListModel {
  Q_OBJECT

 public:
  enum Role {
    WorkspaceIdRole = Qt::UserRole + 1,
    NumericSlotRole,
    DisplayNameRole,
    StableOrderRole,
    WorkspaceKindRole,
    OutputsRole,
    ActiveRole,
    FocusedRole,
    UrgentRole,
    OccupiedRole,
    VisualStateRole,
  };
  Q_ENUM(Role)

  explicit CompositorWorkspaceModel(QObject* parent = nullptr);
  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  void replace(QList<CompositorWorkspace> workspaces);
  [[nodiscard]] const QList<CompositorWorkspace>& entries() const { return entries_; }

 private:
  QList<CompositorWorkspace> entries_;
};
