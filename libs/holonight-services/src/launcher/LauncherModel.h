#pragma once

#include "DesktopEntryScanner.h"

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

#include <cstdint>

class LauncherModel : public QAbstractListModel {
  Q_OBJECT

 public:
  enum class Role : std::uint16_t {
    NameRole = Qt::UserRole + 1,
    SubtitleRole,
    IconNameRole,
    ExecRole,
    DesktopFileRole,
    CategoriesRole,
    TerminalRole,
    ActionsRole,
    IsActionRole,
    ActionParentRole,
    ActionExecRole,
    ActionIndexRole,
    MappedCategoryRole,
    IsActionSectionRole,
    MimeTypesRole,
  };
  Q_ENUM(Role)

  explicit LauncherModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void setEntries(QVector<DesktopEntry> entries);
  void setQuery(const QString& query);
  void setActiveCategory(const QString& category);
  [[nodiscard]] const QString& query() const { return query_; }
  [[nodiscard]] const QString& activeCategory() const { return active_category_; }
  [[nodiscard]] const DesktopEntry* entryAt(int row) const;
  [[nodiscard]] const DesktopEntry* findEntryByDesktopFile(const QString& desktop_file) const;
  [[nodiscard]] int allEntriesCount() const { return static_cast<int>(entries_.size()); }
  [[nodiscard]] const DesktopEntry* allEntryAt(int idx) const;
  [[nodiscard]] int appResultCount() const;
  [[nodiscard]] int actionResultCount() const;
  // True when `row` is an action-entry row (as opposed to an app-launch row) in the flat results
  // list. Out-of-range rows are treated as non-action (false), matching entryAt()'s bounds style.
  [[nodiscard]] bool isActionRow(int row) const;
  // The action_index for an action row; -1 if `row` is out of range or not an action row.
  [[nodiscard]] int actionIndexAt(int row) const;

 private:
  struct ScoredEntry {
    int entry_index{-1};
    int score{0};
    bool is_action{false};
    int action_index{-1};
  };

  void rebuildResults();
  void rebuildBrowseResults();
  void rebuildSearchResults();
  void rebuildDesktopFileIndex();
  [[nodiscard]] static int matchScore(const DesktopEntry& entry, const QString& query);
  [[nodiscard]] static QString browseDisplayName(const DesktopEntry& entry);

  QVector<DesktopEntry> entries_;
  QVector<ScoredEntry> results_;
  QHash<QString, int> desktop_file_index_;
  QString query_;
  QString active_category_;
};
