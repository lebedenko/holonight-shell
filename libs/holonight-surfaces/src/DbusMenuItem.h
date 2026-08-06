#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QtQml/qqml.h>

struct DbusMenuItem {
  int id{0};
  QString label;
  QString type;  // "standard" | "separator"
  QString icon_name;
  bool enabled{true};
  bool visible{true};
  QString toggle_type;   // "checkmark" | "radio" | ""
  int toggle_state{-1};  // -1=indeterminate, 0=off, 1=on
  QList<DbusMenuItem> children;
};

class DbusMenuModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT

 public:
  enum Roles : uint16_t {  // NOLINT(cppcoreguidelines-use-enum-class): Qt model roles are int-compatible.
    IdRole = Qt::UserRole + 1,
    LabelRole,
    TypeRole,
    IconNameRole,
    EnabledRole,
    VisibleRole,
    ToggleTypeRole,
    ToggleStateRole,
    HasSubmenuRole,
  };
  Q_ENUM(Roles)

  explicit DbusMenuModel(QList<DbusMenuItem> items, QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE DbusMenuModel* submenuAt(int row);

 private:
  QList<DbusMenuItem> items_;
  QHash<int, DbusMenuModel*> submenu_cache_;
};
