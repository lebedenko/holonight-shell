#pragma once

#include "WindowCatalog.h"

#include <QAbstractListModel>

class ToplevelModel final : public QAbstractListModel {
  Q_OBJECT
 public:
  using QAbstractListModel::QAbstractListModel;
  // NOLINTNEXTLINE(cppcoreguidelines-use-enum-class,performance-enum-size): Qt role API.
  enum Role { Id = Qt::UserRole + 1, Title, AppId, Activated, Minimized, Maximized, Fullscreen, Outputs, Operations };
  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
  }
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override {
    return {
        {Id, "windowId"},           {Title, "title"},         {AppId, "appId"},
        {Activated, "activated"},   {Minimized, "minimized"}, {Maximized, "maximized"},
        {Fullscreen, "fullscreen"}, {Outputs, "outputs"},     {Operations, "operations"},
    };
  }
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
      return {};
    }
    const auto& window = rows_[index.row()];
    switch (role) {
      case Id:
        return window.id;
      case Title:
        return window.title;
      case AppId:
        return window.app_id;
      case Activated:
        return window.activated;
      case Minimized:
        return window.minimized;
      case Maximized:
        return window.maximized;
      case Fullscreen:
        return window.fullscreen;
      case Outputs:
        return window.outputs;
      case Operations: {
        QVariantList result;
        for (auto command : window.operations) {
          result.append(static_cast<int>(command));
        }
        return result;
      }
      default:
        return {};
    }
  }
  void replace(QList<CompositorWindow> rows) {
    if (rows_ == rows) {
      return;
    }
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
  }

 private:
  QList<CompositorWindow> rows_;
};
