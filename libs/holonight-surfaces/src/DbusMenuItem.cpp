#include "DbusMenuItem.h"

DbusMenuModel::DbusMenuModel(QList<DbusMenuItem> items, QObject* parent)
    : QAbstractListModel(parent), items_(std::move(items)) {}

int DbusMenuModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(items_.size());
}

QVariant DbusMenuModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= items_.size()) {
    return {};
  }
  const DbusMenuItem& item = items_.at(index.row());
  switch (static_cast<Roles>(role)) {
    case IdRole:
      return item.id;
    case LabelRole: {
      // Strip leading underscore mnemonic prefix used by some apps.
      QString label = item.label;
      if (label.startsWith(QLatin1Char('_'))) {
        label = label.sliced(1);
      }
      return label;
    }
    case TypeRole:
      return item.type;
    case IconNameRole:
      return item.icon_name;
    case EnabledRole:
      return item.enabled;
    case VisibleRole:
      return item.visible;
    case ToggleTypeRole:
      return item.toggle_type;
    case ToggleStateRole:
      return item.toggle_state;
    case HasSubmenuRole:
      return !item.children.isEmpty();
  }
  return {};
}

QHash<int, QByteArray> DbusMenuModel::roleNames() const {
  static const QHash<int, QByteArray> kRoles{
      {IdRole, "itemId"},
      {LabelRole, "label"},
      {TypeRole, "type"},
      {IconNameRole, "iconName"},
      {EnabledRole, "itemEnabled"},
      {VisibleRole, "itemVisible"},
      {ToggleTypeRole, "toggleType"},
      {ToggleStateRole, "toggleState"},
      {HasSubmenuRole, "hasSubmenu"},
  };
  return kRoles;
}

DbusMenuModel* DbusMenuModel::submenuAt(int row) {
  if (row < 0 || row >= items_.size()) {
    return nullptr;
  }
  if (items_.at(row).children.isEmpty()) {
    return nullptr;
  }
  auto pos = submenu_cache_.find(row);
  if (pos != submenu_cache_.end()) {
    return pos.value();
  }
  auto* child = new DbusMenuModel(items_.at(row).children, this);
  submenu_cache_.insert(row, child);
  return child;
}
