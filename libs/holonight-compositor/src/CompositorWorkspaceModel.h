#pragma once

#include "CompositorSnapshot.h"

#include <QAbstractListModel>
#include <QtQml/qqml.h>

#include <cstdint>
#include <functional>

class CompositorWorkspaceModel final : public QAbstractListModel {
  Q_OBJECT

 public:
  enum Role : std::uint16_t {  // NOLINT(cppcoreguidelines-use-enum-class): Qt model roles are int-compatible.
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
  void replaceTransactional(QList<CompositorWorkspace> workspaces, const std::function<void()>& commit);
  [[nodiscard]] int focusedRow() const;
  [[nodiscard]] int firstVisibleRow(int display_count) const;
  [[nodiscard]] const QList<CompositorWorkspace>& entries() const { return entries_; }

 private:
  QList<CompositorWorkspace> entries_;
};
