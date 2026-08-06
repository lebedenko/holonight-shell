#include "LauncherModel.h"

#include "CategoryMapper.h"

#include <QVariantList>
#include <QVariantMap>

#include <algorithm>

namespace {
bool containsWordPrefix(const QString& value, const QString& query) {
  if (query.isEmpty() || value.size() < query.size()) {
    return false;
  }

  auto isWordChar = [](const QChar& code_char) {
    return code_char.isLetterOrNumber() || code_char == QLatin1Char('_');
  };

  qsizetype pos = 0;
  while ((pos = value.indexOf(query, pos, Qt::CaseInsensitive)) != -1) {
    if (pos == 0 || !isWordChar(value.at(pos - 1))) {
      return true;
    }
    pos += 1;
  }
  return false;
}

int fieldScore(const QString& value, const QString& query, int exact, int prefix, int word_prefix, int substring) {
  if (value.isEmpty()) {
    return 0;
  }
  if (value.compare(query, Qt::CaseInsensitive) == 0) {
    return exact;
  }
  if (value.startsWith(query, Qt::CaseInsensitive)) {
    return prefix;
  }
  if (containsWordPrefix(value, query)) {
    return word_prefix;
  }
  if (value.contains(query, Qt::CaseInsensitive)) {
    return substring;
  }
  return 0;
}
}  // namespace

LauncherModel::LauncherModel(QObject* parent) : QAbstractListModel(parent) {}

int LauncherModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(results_.size());
}

QVariant LauncherModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(results_.size())) {
    return {};
  }

  const ScoredEntry& scored = results_.at(index.row());
  const DesktopEntry* entry = allEntryAt(scored.entry_index);
  if (entry == nullptr) {
    return {};
  }

  switch (static_cast<Role>(role)) {
    case Role::NameRole:
      return scored.is_action ? entry->actions.at(scored.action_index).name : entry->name;
    case Role::SubtitleRole: {
      if (scored.is_action) {
        return entry->name;
      }
      return entry->generic_name.isEmpty() ? entry->comment : entry->generic_name;
    }
    case Role::IconNameRole:
      return entry->icon;
    case Role::ExecRole:
      return scored.is_action ? entry->actions.at(scored.action_index).exec : entry->exec;
    case Role::DesktopFileRole:
      return entry->desktop_file;
    case Role::CategoriesRole:
      return entry->categories;
    case Role::TerminalRole:
      return entry->terminal;
    case Role::ActionsRole: {
      QVariantList list;
      for (const auto& action : entry->actions) {
        QVariantMap map;
        map[QStringLiteral("name")] = action.name;
        map[QStringLiteral("exec")] = action.exec;
        list.append(map);
      }
      return list;
    }
    case Role::IsActionRole:
      return scored.is_action;
    case Role::ActionParentRole:
      return scored.is_action ? entry->name : QString{};
    case Role::ActionExecRole:
      return scored.is_action ? entry->actions.at(scored.action_index).exec : QString{};
    case Role::ActionIndexRole:
      return scored.action_index;
    case Role::MappedCategoryRole:
      return CategoryMapper::map(entry->categories);
    case Role::IsActionSectionRole: {
      if (scored.is_action) {
        return QStringLiteral("ACTIONS");
      }
      return index.row() == 0 ? QStringLiteral("") : QStringLiteral("APPLICATIONS");
    }
    case Role::MimeTypesRole: {
      QVariantList list;
      list.reserve(entry->mime_types.size());
      for (const QString& mime : entry->mime_types) {
        list.append(mime);
      }
      return list;
    }
  }
  return {};
}

QHash<int, QByteArray> LauncherModel::roleNames() const {
  static const QHash<int, QByteArray> kRoles{
      {static_cast<int>(Role::NameRole), "name"},
      {static_cast<int>(Role::SubtitleRole), "subtitle"},
      {static_cast<int>(Role::IconNameRole), "iconName"},
      {static_cast<int>(Role::ExecRole), "exec"},
      {static_cast<int>(Role::DesktopFileRole), "desktopFile"},
      {static_cast<int>(Role::CategoriesRole), "categories"},
      {static_cast<int>(Role::TerminalRole), "terminal"},
      {static_cast<int>(Role::ActionsRole), "actions"},
      {static_cast<int>(Role::IsActionRole), "isAction"},
      {static_cast<int>(Role::ActionParentRole), "actionParent"},
      {static_cast<int>(Role::ActionExecRole), "actionExec"},
      {static_cast<int>(Role::ActionIndexRole), "actionIndex"},
      {static_cast<int>(Role::MappedCategoryRole), "mappedCategory"},
      {static_cast<int>(Role::IsActionSectionRole), "isActionSection"},
      {static_cast<int>(Role::MimeTypesRole), "mimeTypes"},
  };
  return kRoles;
}

void LauncherModel::setEntries(QVector<DesktopEntry> entries) {
  entries_ = std::move(entries);
  rebuildDesktopFileIndex();
  rebuildResults();
}

void LauncherModel::setQuery(const QString& query) {
  const QString trimmed = query.trimmed();
  if (query_ == trimmed) {
    return;
  }
  query_ = trimmed;
  rebuildResults();
}

void LauncherModel::setActiveCategory(const QString& category) {
  if (active_category_ == category) {
    return;
  }
  active_category_ = category;
  if (query_.isEmpty()) {
    rebuildResults();
  }
}

const DesktopEntry* LauncherModel::entryAt(int row) const {
  if (row < 0 || row >= static_cast<int>(results_.size())) {
    return nullptr;
  }
  return allEntryAt(results_.at(row).entry_index);
}

const DesktopEntry* LauncherModel::findEntryByDesktopFile(const QString& desktop_file) const {
  const auto index_it = desktop_file_index_.constFind(desktop_file);
  if (index_it == desktop_file_index_.constEnd()) {
    return nullptr;
  }
  return allEntryAt(index_it.value());
}

const DesktopEntry* LauncherModel::allEntryAt(int idx) const {
  if (idx < 0 || idx >= static_cast<int>(entries_.size())) {
    return nullptr;
  }
  return &entries_.at(idx);
}

int LauncherModel::appResultCount() const {
  int count = 0;
  for (const auto& scored : results_) {
    if (!scored.is_action) {
      ++count;
    }
  }
  return count;
}

int LauncherModel::actionResultCount() const {
  int count = 0;
  for (const auto& scored : results_) {
    if (scored.is_action) {
      ++count;
    }
  }
  return count;
}

bool LauncherModel::isActionRow(int row) const {
  if (row < 0 || row >= static_cast<int>(results_.size())) {
    return false;
  }
  return results_.at(row).is_action;
}

int LauncherModel::actionIndexAt(int row) const {
  if (row < 0 || row >= static_cast<int>(results_.size())) {
    return -1;
  }
  const ScoredEntry& scored = results_.at(row);
  return scored.is_action ? scored.action_index : -1;
}

QString LauncherModel::browseDisplayName(const DesktopEntry& entry) {
  static const QStringList kArticles = {
      QStringLiteral("the "),
      QStringLiteral("a "),
      QStringLiteral("an "),
  };
  const QString lower = entry.name.toLower();
  for (const QString& article : kArticles) {
    if (lower.startsWith(article)) {
      return entry.name.mid(article.size());
    }
  }
  return entry.name;
}

void LauncherModel::rebuildBrowseResults() {
  const bool filter_active = !active_category_.isEmpty() && active_category_ != QStringLiteral("All");
  for (int entry_index = 0; entry_index < entries_.size(); ++entry_index) {
    const DesktopEntry& entry = entries_.at(entry_index);
    if (filter_active && CategoryMapper::map(entry.categories) != active_category_) {
      continue;
    }
    results_.append(ScoredEntry{.entry_index = entry_index, .score = 0, .is_action = false, .action_index = -1});
  }
  std::ranges::sort(results_, [this](const ScoredEntry& left, const ScoredEntry& right) {
    return QString::localeAwareCompare(browseDisplayName(entries_.at(left.entry_index)),
                                       browseDisplayName(entries_.at(right.entry_index))) < 0;
  });
}

void LauncherModel::rebuildSearchResults() {
  for (int entry_index = 0; entry_index < entries_.size(); ++entry_index) {
    const DesktopEntry& entry = entries_.at(entry_index);
    const int app_score = matchScore(entry, query_);
    if (app_score > 0) {
      results_.append(
          ScoredEntry{.entry_index = entry_index, .score = app_score, .is_action = false, .action_index = -1});
      // Include ALL actions of matched apps in the ACTIONS section.
      for (int action_idx = 0; action_idx < entry.actions.size(); ++action_idx) {
        results_.append(
            ScoredEntry{.entry_index = entry_index, .score = app_score, .is_action = true, .action_index = action_idx});
      }
    } else {
      // App didn't match — include only actions whose names match the query.
      for (int action_idx = 0; action_idx < entry.actions.size(); ++action_idx) {
        const DesktopAction& action = entry.actions.at(action_idx);
        const int action_score = fieldScore(action.name, query_, 900, 800, 750, 600);
        if (action_score > 0) {
          results_.append(ScoredEntry{
              .entry_index = entry_index, .score = action_score, .is_action = true, .action_index = action_idx});
        }
      }
    }
  }
  std::ranges::stable_sort(results_, [this](const ScoredEntry& left, const ScoredEntry& right) {
    if (left.is_action != right.is_action) {
      return !left.is_action;
    }
    if (left.score != right.score) {
      return left.score > right.score;
    }
    return QString::localeAwareCompare(entries_.at(left.entry_index).name, entries_.at(right.entry_index).name) < 0;
  });
}

void LauncherModel::rebuildResults() {
  beginResetModel();
  results_.clear();
  if (query_.isEmpty()) {
    rebuildBrowseResults();
  } else {
    rebuildSearchResults();
  }
  endResetModel();
}

void LauncherModel::rebuildDesktopFileIndex() {
  desktop_file_index_.clear();
  const int total = static_cast<int>(entries_.size());
  desktop_file_index_.reserve(total);
  for (int idx = 0; idx < total; ++idx) {
    const QString& desktop_file = entries_.at(idx).desktop_file;
    if (!desktop_file_index_.contains(desktop_file)) {
      desktop_file_index_.insert(desktop_file, idx);
    }
  }
}

int LauncherModel::matchScore(const DesktopEntry& entry, const QString& query) {
  const int name_score = fieldScore(entry.name, query, 1000, 900, 850, 700);
  if (name_score > 0) {
    return name_score;
  }
  const int generic_score = fieldScore(entry.generic_name, query, 650, 620, 600, 520);
  if (generic_score > 0) {
    return generic_score;
  }
  const int comment_score = fieldScore(entry.comment, query, 460, 430, 410, 340);
  if (comment_score > 0) {
    return comment_score;
  }
  const int exec_score = fieldScore(entry.exec, query, 300, 280, 260, 220);
  if (exec_score > 0) {
    return exec_score;
  }
  return fieldScore(entry.categories, query, 180, 160, 140, 100);
}
