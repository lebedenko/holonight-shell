#pragma once

#include "WindowCatalog.h"

#include <QAbstractListModel>

class ToplevelModel final : public QAbstractListModel {
  Q_OBJECT
 public:
  using QAbstractListModel::QAbstractListModel;
  enum Role { Id = Qt::UserRole + 1, Title, AppId, Activated, Minimized, Maximized, Fullscreen, Outputs, Operations };
  int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : rows_.size(); }
  QHash<int, QByteArray> roleNames() const override {
    return {{Id, "windowId"},           {Title, "title"},         {AppId, "appId"},
            {Activated, "activated"},   {Minimized, "minimized"}, {Maximized, "maximized"},
            {Fullscreen, "fullscreen"}, {Outputs, "outputs"},     {Operations, "operations"}};
  }
  QVariant data(const QModelIndex& index, int role) const override {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) return {};
    const auto& w = rows_[index.row()];
    switch (role) {
      case Id:
        return w.id;
      case Title:
        return w.title;
      case AppId:
        return w.app_id;
      case Activated:
        return w.activated;
      case Minimized:
        return w.minimized;
      case Maximized:
        return w.maximized;
      case Fullscreen:
        return w.fullscreen;
      case Outputs:
        return w.outputs;
      case Operations: {
        QVariantList result;
        for (auto command : w.operations) result.append(static_cast<int>(command));
        return result;
      }
      default:
        return {};
    }
  }
  void replace(QList<CompositorWindow> rows) {
    if (rows_ == rows) return;
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
  }

 private:
  QList<CompositorWindow> rows_;
};
