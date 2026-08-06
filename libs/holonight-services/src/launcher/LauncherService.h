#pragma once

#include "DesktopEntryScanner.h"
#include "LauncherModel.h"

#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <optional>

class LauncherBackend {
 public:
  virtual ~LauncherBackend() = default;

  LauncherBackend(const LauncherBackend&) = delete;
  LauncherBackend& operator=(const LauncherBackend&) = delete;
  LauncherBackend(LauncherBackend&&) = delete;
  LauncherBackend& operator=(LauncherBackend&&) = delete;

  [[nodiscard]] virtual bool launch(const DesktopEntry& entry) = 0;
  [[nodiscard]] virtual bool launchExec(const QString& exec, const QString& working_dir) = 0;

 protected:
  LauncherBackend() = default;
};

class RecentAppsTracker;

class ProcessLauncherBackend : public LauncherBackend {
 public:
  [[nodiscard]] bool launch(const DesktopEntry& entry) override;
  [[nodiscard]] bool launchExec(const QString& exec, const QString& working_dir) override;
};

class LauncherService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
  Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
  Q_PROPERTY(int resultCount READ resultCount NOTIFY resultCountChanged)
  Q_PROPERTY(int appResultCount READ appResultCount NOTIFY resultCountChanged)
  Q_PROPERTY(int actionResultCount READ actionResultCount NOTIFY resultCountChanged)
  Q_PROPERTY(QString activeCategory READ activeCategory NOTIFY activeCategoryChanged)
  Q_PROPERTY(QString selectedEntryName READ selectedEntryName NOTIFY selectedIndexChanged)
  Q_PROPERTY(QString selectedEntryDesktopFile READ selectedEntryDesktopFile NOTIFY selectedIndexChanged)
  Q_PROPERTY(QString selectedEntryIcon READ selectedEntryIcon NOTIFY selectedIndexChanged)
  Q_PROPERTY(QVariantList selectedEntryActions READ selectedEntryActions NOTIFY selectedIndexChanged)
  Q_PROPERTY(QAbstractItemModel* results READ results CONSTANT)

 public:
  explicit LauncherService(QObject* parent = nullptr);
  LauncherService(DesktopEntryScanner scanner, std::unique_ptr<LauncherBackend> backend, QObject* parent = nullptr);
  LauncherService(DesktopEntryScanner scanner, std::unique_ptr<LauncherBackend> backend, QString cache_db_path,
                  QObject* parent = nullptr);
  LauncherService(DesktopEntryScanner scanner, std::unique_ptr<LauncherBackend> backend, QString cache_db_path,
                  RecentAppsTracker* recent_apps_tracker, QObject* parent = nullptr);
  ~LauncherService() override;

  LauncherService(const LauncherService&) = delete;
  LauncherService& operator=(const LauncherService&) = delete;
  LauncherService(LauncherService&&) = delete;
  LauncherService& operator=(LauncherService&&) = delete;

  void start();
  void runValidator();
  [[nodiscard]] QString query() const { return query_; }
  [[nodiscard]] int selectedIndex() const { return selected_index_; }
  [[nodiscard]] int resultCount() const { return model_.rowCount(); }
  [[nodiscard]] int appResultCount() const { return model_.appResultCount(); }
  [[nodiscard]] int actionResultCount() const { return model_.actionResultCount(); }
  [[nodiscard]] QString activeCategory() const { return model_.activeCategory(); }
  [[nodiscard]] QString selectedEntryName() const;
  [[nodiscard]] QString selectedEntryDesktopFile() const;
  [[nodiscard]] QString selectedEntryIcon() const;
  [[nodiscard]] QVariantList selectedEntryActions() const;
  [[nodiscard]] QAbstractItemModel* results() { return &model_; }

  Q_INVOKABLE void setQuery(const QString& query);
  Q_INVOKABLE void setSelectedIndex(int index);
  Q_INVOKABLE void moveSelection(int delta);
  Q_INVOKABLE bool launchSelected();
  Q_INVOKABLE bool launch(int index);
  Q_INVOKABLE bool launchDesktopFile(const QString& desktop_file);
  Q_INVOKABLE bool launchAction(int entry_index, int action_index);
  Q_INVOKABLE void setActiveCategory(const QString& category);
  Q_INVOKABLE static QStringList availableCategories();
  Q_INVOKABLE int countForCategory(const QString& category) const;
  Q_INVOKABLE QVariantMap entryInfoForDesktopFile(const QString& desktop_file) const;
  Q_INVOKABLE void reload();
  Q_INVOKABLE QVariantList entriesForMimeTypes(const QStringList& mime_types) const;
  Q_INVOKABLE QVariantList entriesForMimeTypesAndCategories(const QStringList& mime_types,
                                                            const QStringList& categories) const;
  Q_INVOKABLE QVariantList defaultAppEntriesForMimeTypes(const QStringList& mime_types) const;
  Q_INVOKABLE QVariantList defaultAppEntriesForMimeTypesAndCategories(const QStringList& mime_types,
                                                                      const QStringList& categories) const;
  Q_INVOKABLE QVariantList entriesForCategory(const QString& category) const;
  Q_INVOKABLE QVariantList defaultAppEntriesForCategory(const QString& category) const;

 Q_SIGNALS:
  void queryChanged();
  void selectedIndexChanged();
  void resultCountChanged();
  void activeCategoryChanged();
  void launched();
  void entriesUpdated();

 private:
  struct SelectionIdentity {
    QString desktop_file;
    QString action_exec;
    int previous_index{-1};
    bool is_action{false};
  };

  void captureSelectionBeforeModelReset();
  void refreshSelectionAfterModelReset();
  void setSelectedIndexInternal(int index);
  void onModelEntriesReset();
  void invalidateCategoryCache();
  void invalidateDefaultAppsCache();
  [[nodiscard]] const QVector<DesktopEntry>& cachedDefaultApps() const;
  void recordRecentLaunch(const QString& desktop_file);

  DesktopEntryScanner scanner_;
  LauncherModel model_;
  std::unique_ptr<LauncherBackend> backend_;
  RecentAppsTracker* recent_apps_tracker_{nullptr};
  QFutureWatcher<ScanResult>* watcher_{nullptr};
  QFileSystemWatcher* fs_watcher_{nullptr};
  QTimer* debounce_timer_{nullptr};
  QString cache_db_path_override_;
  QString db_path_;
  QString query_;
  int selected_index_{-1};
  std::optional<SelectionIdentity> pending_selection_identity_;
  bool preserve_selection_on_model_reset_{false};
  bool started_{false};
  bool validator_rerun_pending_{false};

  mutable QMap<QString, int> category_counts_cache_;
  mutable bool category_counts_dirty_{true};
  mutable QVector<DesktopEntry> default_apps_cache_;
  mutable bool default_apps_cache_dirty_{true};
};
