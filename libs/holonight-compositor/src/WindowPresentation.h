#pragma once

#include "CompositorService.h"

class GroupedApplicationModel final : public QAbstractListModel {
  Q_OBJECT
 public:
  using QAbstractListModel::QAbstractListModel;
  // NOLINTNEXTLINE(cppcoreguidelines-use-enum-class,performance-enum-size): Qt role API.
  enum Role { Key = Qt::UserRole + 1, AppId, Title, Windows, Active, Minimized };
  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
  }
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override {
    return {
        {Key, "groupKey"},    {AppId, "appId"},   {Title, "title"},
        {Windows, "windows"}, {Active, "active"}, {Minimized, "minimized"},
    };
  }
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  void replace(const QList<CompositorWindow>& windows, bool grouped);

 private:
  struct Group {
    QString key;
    QString app;
    QString title;
    QList<CompositorWindow> windows;
  };
  QList<Group> rows_;
};

class WindowPresentation final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int revision READ revision NOTIFY changed)
  Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
  Q_PROPERTY(bool overviewAccess READ overviewAccess NOTIFY changed)
  Q_PROPERTY(bool desktopMenu READ desktopMenu NOTIFY changed)
  Q_PROPERTY(bool taskbarEnabled READ taskbarEnabled NOTIFY changed)
  Q_PROPERTY(bool grouped READ grouped WRITE setGrouped NOTIFY changed)
  Q_PROPERTY(QAbstractItemModel* applications READ applications CONSTANT)
  Q_PROPERTY(QStringList windowIds READ windowIds NOTIFY changed)
  Q_PROPERTY(QVariantList overview READ overview NOTIFY changed)
 public:
  WindowPresentation(CompositorService* service, bool task_management, QObject* parent = nullptr);
  [[nodiscard]] int revision() const { return service_->revision(); }
  [[nodiscard]] bool enabled() const { return task_management_ && service_->canListWindows(); }
  [[nodiscard]] bool taskbarEnabled() const { return enabled() && taskbar_enabled_; }
  [[nodiscard]] bool overviewAccess() const { return enabled() && overview_access_; }
  [[nodiscard]] bool desktopMenu() const { return task_management_ && desktop_menu_; }
  void configure(bool enabled, bool grouped, bool overview_access, bool desktop_menu) {
    taskbar_enabled_ = enabled;
    grouped_ = grouped;
    overview_access_ = overview_access;
    desktop_menu_ = desktop_menu;
    refresh();
  }
  [[nodiscard]] bool grouped() const { return grouped_; }
  void setGrouped(bool value);
  QAbstractItemModel* applications() { return &groups_; }
  [[nodiscard]] QStringList windowIds() const {
    QStringList ids;
    for (const auto& window : service_->snapshot().windows) {
      ids.append(window.id);
    }
    return ids;
  }
  [[nodiscard]] QVariantList overview() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap window(const QString& identifier) const;
  Q_INVOKABLE int command(const QString& identifier, int operation) {
    return service_->commandWindow(identifier, operation);
  }
  Q_INVOKABLE void click(const QString& identifier);
  Q_INVOKABLE void beginOverview();
  Q_INVOKABLE void endOverview();
  Q_INVOKABLE void setSearch(const QString& query);
 Q_SIGNALS:
  void changed();

 private:
  void refresh();
  CompositorService* service_;
  bool task_management_;
  bool taskbar_enabled_{true};
  bool overview_access_{true};
  bool desktop_menu_{false};
  bool grouped_{true};
  bool frozen_{false};
  QString query_;
  QStringList order_;
  const WindowCatalog& catalog_;
  GroupedApplicationModel groups_;
};
