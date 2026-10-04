#include "WindowPresentation.h"

#include <algorithm>

namespace {
QVariantMap record(const CompositorWindow& window) {
  QVariantList operations;
  for (auto operation : window.operations) {
    operations.append(static_cast<int>(operation));
  }
  return {
      {"windowId", window.id},           {"title", window.title},         {"appId", window.app_id},
      {"activated", window.activated},   {"minimized", window.minimized}, {"maximized", window.maximized},
      {"fullscreen", window.fullscreen}, {"operations", operations},
  };
}
}  // namespace
QVariant GroupedApplicationModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size()) {
    return {};
  }
  const auto& row = rows_[index.row()];
  switch (role) {
    case Key:
      return row.key;
    case AppId:
      return row.app;
    case Title:
      return row.title;
    case Active:
      return std::ranges::any_of(row.windows, [](const auto& window) { return window.activated; });
    case Minimized:
      return std::ranges::all_of(row.windows, [](const auto& window) { return window.minimized; });
    case Windows: {
      QVariantList result;
      for (const auto& window : row.windows) {
        result.append(record(window));
      }
      return result;
    }
    default:
      return {};
  }
}
void GroupedApplicationModel::replace(const QList<CompositorWindow>& windows, bool grouped) {
  GroupedApplicationModel next;
  for (const auto& window : windows) {
    const auto key = WindowCatalog::groupKey(window, grouped);
    auto found = std::ranges::find(next.rows_, key, &Group::key);
    if (found == next.rows_.end()) {
      next.rows_.append({.key = key, .app = window.app_id, .title = window.title, .windows = {window}});
    } else {
      found->windows.append(window);
    }
  }
  const bool same_structure =
      rows_.size() == next.rows_.size() && std::ranges::equal(rows_, next.rows_, {}, &Group::key, &Group::key);
  if (!same_structure) {
    beginResetModel();
    rows_ = std::move(next.rows_);
    endResetModel();
    return;
  }

  QList<QList<int>> changed_roles;
  for (int row = 0; row < rowCount(); ++row) {
    QList<int> roles;
    for (int role = Key; role <= Minimized; ++role) {
      if (data(index(row, 0), role) != next.data(next.index(row, 0), role)) {
        roles.append(role);
      }
    }
    changed_roles.append(roles);
  }
  rows_ = std::move(next.rows_);
  for (int row = 0; row < changed_roles.size(); ++row) {
    if (!changed_roles[row].isEmpty()) {
      emit dataChanged(index(row, 0), index(row, 0), changed_roles[row]);
    }
  }
}
WindowPresentation::WindowPresentation(CompositorService* service, bool task_management, QObject* parent)
    : QObject(parent),
      service_(service),
      task_management_(task_management),
      catalog_(service->windowCatalog()),
      groups_(this) {
  connect(service_, &CompositorService::revisionChanged, this, &WindowPresentation::refresh);
  refresh();
}
void WindowPresentation::refresh() {
  groups_.replace(service_->snapshot().windows, grouped_);
  if (!frozen_) {
    auto windows = catalog_.search({});
    std::ranges::stable_sort(windows, [this](const auto& left, const auto& right) {
      return catalog_.activationOrder(left.id) > catalog_.activationOrder(right.id);
    });
    order_.clear();
    for (const auto& window : windows) {
      order_.append(window.id);
    }
  } else {
    // New windows append; closures disappear without reordering surviving cards.
    for (const auto& window : service_->snapshot().windows) {
      if (!order_.contains(window.id)) {
        order_.append(window.id);
      }
    }
    order_.removeIf([this](const QString& identifier) { return !catalog_.find(identifier); });
  }
  emit changed();
}
void WindowPresentation::setGrouped(bool value) {
  if (grouped_ == value) {
    return;
  }
  grouped_ = value;
  refresh();
}
QVariantMap WindowPresentation::window(const QString& identifier) const {
  const auto* value = catalog_.find(identifier);
  return (value != nullptr) ? record(*value) : QVariantMap{};
}
QVariantList WindowPresentation::overview() const {
  QVariantList result;
  const auto matches = catalog_.search(query_);
  for (const auto& identifier : order_) {
    const auto found = std::ranges::find(matches, identifier, &CompositorWindow::id);
    if (found != matches.end()) {
      result.append(record(*found));
    }
  }
  return result;
}
void WindowPresentation::click(const QString& identifier) {
  const auto* target = catalog_.find(identifier);
  if ((target == nullptr) || !enabled()) {
    return;
  }
  if (target->activated) {
    command(identifier, static_cast<int>(WindowCommand::Minimize));
  } else {
    if (target->minimized) {
      command(identifier, static_cast<int>(WindowCommand::Restore));
    }
    command(identifier, static_cast<int>(WindowCommand::Activate));
  }
}
void WindowPresentation::beginOverview() {
  query_.clear();
  frozen_ = true;
  emit changed();
}
void WindowPresentation::endOverview() {
  frozen_ = false;
  refresh();
}
void WindowPresentation::setSearch(const QString& query) {
  query_ = query;
  emit changed();
}
