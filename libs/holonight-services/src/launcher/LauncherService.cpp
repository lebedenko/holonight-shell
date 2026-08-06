#include "LauncherService.h"

#include "CategoryMapper.h"
#include "DesktopEntryCache.h"
#include "LauncherCommand.h"
#include "RecentAppsTracker.h"

#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <utility>

namespace {
Q_LOGGING_CATEGORY(lcLauncher, "holonight.launcher")

constexpr int kFileWatcherDebounceMs = 500;

QString launcherCacheDbPath() {
  const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
  if (base.isEmpty()) {
    return {};
  }
  return base + QStringLiteral("/holonight-shell/launcher.db");
}

QSet<QString> desktopFilePaths(const QVector<DesktopEntry>& entries) {
  QSet<QString> paths;
  paths.reserve(entries.size());
  for (const DesktopEntry& entry : entries) {
    paths.insert(entry.desktop_file);
  }
  return paths;
}

QStringList desktopEntryCategories(const DesktopEntry& entry) {
  QStringList categories;
  const QStringList parts = entry.categories.split(QLatin1Char(';'), Qt::SkipEmptyParts);
  categories.reserve(parts.size());
  for (const QString& part : parts) {
    const QString trimmed = part.trimmed();
    if (!trimmed.isEmpty()) {
      categories.append(trimmed);
    }
  }
  return categories;
}

QVariantMap desktopEntryComboMap(const DesktopEntry& entry) {
  QVariantList mime_types;
  mime_types.reserve(entry.mime_types.size());
  for (const QString& mime : entry.mime_types) {
    mime_types.append(mime);
  }

  return QVariantMap{
      {QStringLiteral("name"), entry.name},
      {QStringLiteral("icon"), entry.icon},
      // xdg-mime returns basenames ("firefox.desktop"), so store basename here
      // to allow direct comparison without path stripping at call sites.
      {QStringLiteral("desktopFile"), QFileInfo(entry.desktop_file).fileName()},
      {QStringLiteral("mimeTypes"), mime_types},
  };
}

bool entryMatchesMimeTypes(const DesktopEntry& entry, const QSet<QString>& target_mimes) {
  return std::ranges::any_of(entry.mime_types,
                             [&target_mimes](const QString& mime) { return target_mimes.contains(mime); });
}

bool entryMatchesCategories(const DesktopEntry& entry, const QSet<QString>& target_categories) {
  const QStringList entry_categories = desktopEntryCategories(entry);
  return std::ranges::any_of(
      entry_categories, [&target_categories](const QString& category) { return target_categories.contains(category); });
}

bool persistValidationResult(DesktopEntryCache* cache, const ScanResult& scan, const QSet<QString>& scanned_paths) {
  const QVector<DesktopEntry> cached_entries = cache->loadAll();
  if (!cache->beginTransaction()) {
    return false;
  }

  bool parsed_ok = true;
  for (const DesktopEntry& entry : scan.entries) {
    const QFileInfo info(entry.desktop_file);
    const qint64 disk_mtime = info.lastModified().toSecsSinceEpoch();
    const qint64 disk_size = info.size();
    const auto meta = cache->metadata(entry.desktop_file);
    if (!meta.has_value() || meta->mtime != disk_mtime || meta->size != disk_size) {
      parsed_ok = cache->upsert(entry, disk_mtime, disk_size) && parsed_ok;
    }
  }

  for (const DesktopEntry& cached : cached_entries) {
    if (!scanned_paths.contains(cached.desktop_file)) {
      parsed_ok = cache->remove(cached.desktop_file) && parsed_ok;
    }
  }

  if (parsed_ok && cache->commitTransaction()) {
    return true;
  }
  cache->rollbackTransaction();
  return false;
}

ScanResult validateAgainstCache(const DesktopEntryScanner& scanner, const QString& db_path) {
  ScanResult scan = scanner.scanWithDirs();
  const QSet<QString> scanned_paths = desktopFilePaths(scan.entries);

  DesktopEntryCache cache;
  if (!cache.open(db_path)) {
    if (QFileInfo::exists(db_path)) {
      qCWarning(lcLauncher) << "Launcher cache could not be opened; rebuilding:" << db_path;
      QFile::remove(db_path);
    }
    if (!cache.open(db_path)) {
      return scan;
    }
    if (!persistValidationResult(&cache, scan, scanned_paths)) {
      qCWarning(lcLauncher) << "Failed to persist rebuilt launcher cache";
    }
    cache.close();
    return scan;
  }

  if (!persistValidationResult(&cache, scan, scanned_paths)) {
    qCWarning(lcLauncher) << "Failed to persist launcher cache validation result";
  }
  cache.close();
  return scan;
}

QString findTerminalEmulator() {
  QString env_terminal = QString::fromLocal8Bit(qgetenv("TERMINAL")).trimmed();
  if (!env_terminal.isEmpty() && !QStandardPaths::findExecutable(env_terminal).isEmpty()) {
    return env_terminal;
  }

  static const QStringList candidates = {QStringLiteral("foot"),           QStringLiteral("kitty"),
                                         QStringLiteral("alacritty"),      QStringLiteral("wezterm"),
                                         QStringLiteral("konsole"),        QStringLiteral("gnome-terminal"),
                                         QStringLiteral("xfce4-terminal"), QStringLiteral("xterm")};
  for (const QString& term : candidates) {
    if (!QStandardPaths::findExecutable(term).isEmpty()) {
      return term;
    }
  }
  return {};
}
}  // namespace

bool ProcessLauncherBackend::launch(const DesktopEntry& entry) {
  const LauncherCommand command = commandForDesktopEntry(entry, findTerminalEmulator());
  if (!command.isValid()) {
    return false;
  }
  return QProcess::startDetached(command.program, command.arguments, command.working_dir);
}

bool ProcessLauncherBackend::launchExec(const QString& exec, const QString& working_dir) {
  const LauncherCommand command = commandForExec(exec, working_dir);
  if (!command.isValid()) {
    return false;
  }
  return QProcess::startDetached(command.program, command.arguments, command.working_dir);
}

LauncherService::LauncherService(QObject* parent)
    : LauncherService(DesktopEntryScanner(), std::make_unique<ProcessLauncherBackend>(), parent) {}

LauncherService::LauncherService(DesktopEntryScanner scanner, std::unique_ptr<LauncherBackend> backend, QObject* parent)
    : LauncherService(std::move(scanner), std::move(backend), {}, RecentAppsTracker::instance(), parent) {}

LauncherService::LauncherService(DesktopEntryScanner scanner, std::unique_ptr<LauncherBackend> backend,
                                 QString cache_db_path, QObject* parent)
    : LauncherService(std::move(scanner), std::move(backend), std::move(cache_db_path), RecentAppsTracker::instance(),
                      parent) {}

LauncherService::LauncherService(DesktopEntryScanner scanner, std::unique_ptr<LauncherBackend> backend,
                                 QString cache_db_path, RecentAppsTracker* recent_apps_tracker, QObject* parent)
    : QObject(parent),
      scanner_(std::move(scanner)),
      backend_(std::move(backend)),
      recent_apps_tracker_(recent_apps_tracker),
      cache_db_path_override_(std::move(cache_db_path)) {
  connect(&model_, &QAbstractItemModel::modelAboutToBeReset, this, &LauncherService::captureSelectionBeforeModelReset);
  connect(&model_, &QAbstractItemModel::modelReset, this, [this] {
    onModelEntriesReset();
    refreshSelectionAfterModelReset();
    emit resultCountChanged();
  });
}

LauncherService::~LauncherService() {
  if (watcher_ != nullptr) {
    watcher_->cancel();
    watcher_->waitForFinished();
  }
}

void LauncherService::start() {
  if (started_) {
    return;
  }
  started_ = true;

  db_path_ = cache_db_path_override_.isEmpty() ? launcherCacheDbPath() : cache_db_path_override_;

  const bool cache_existed = QFileInfo::exists(db_path_);
  bool rebuild_cache = !cache_existed;
  bool remove_failed_cache = false;
  {
    // Synchronous cache load: populate the model before the first frame.
    DesktopEntryCache load_cache;
    if (cache_existed && load_cache.open(db_path_)) {
      const QVector<DesktopEntry> cached_entries = load_cache.loadAll();
      if (cached_entries.isEmpty()) {
        rebuild_cache = true;
      } else {
        preserve_selection_on_model_reset_ = true;
        model_.setEntries(cached_entries);
      }
      load_cache.close();
    } else {
      remove_failed_cache = QFileInfo::exists(db_path_);
      rebuild_cache = true;
      qCInfo(lcLauncher) << "No launcher cache found, starting with empty model";
    }
  }
  if (remove_failed_cache) {
    QFile::remove(db_path_);
  }
  if (rebuild_cache) {
    const ScanResult fallback_scan = scanner_.scanWithDirs();
    preserve_selection_on_model_reset_ = true;
    model_.setEntries(fallback_scan.entries);

    DesktopEntryCache rebuild_cache_writer;
    if (rebuild_cache_writer.open(db_path_)) {
      if (!persistValidationResult(&rebuild_cache_writer, fallback_scan, desktopFilePaths(fallback_scan.entries))) {
        qCWarning(lcLauncher) << "Failed to persist launcher cache rebuild";
      }
      rebuild_cache_writer.close();
    }
  }

  // Set up filesystem watcher + debounce.
  fs_watcher_ = new QFileSystemWatcher(this);
  debounce_timer_ = new QTimer(this);
  debounce_timer_->setSingleShot(true);
  debounce_timer_->setInterval(kFileWatcherDebounceMs);
  connect(fs_watcher_, &QFileSystemWatcher::directoryChanged, debounce_timer_, qOverload<>(&QTimer::start));
  connect(debounce_timer_, &QTimer::timeout, this, &LauncherService::runValidator);

  // Add existing application directories to the watcher.
  fs_watcher_->addPaths(scanner_.applicationDirs());

  runValidator();
}

QString LauncherService::selectedEntryName() const {
  const DesktopEntry* entry = model_.entryAt(selected_index_);
  return entry != nullptr ? entry->name : QString{};
}

QString LauncherService::selectedEntryDesktopFile() const {
  const DesktopEntry* entry = model_.entryAt(selected_index_);
  return entry != nullptr ? entry->desktop_file : QString{};
}

QString LauncherService::selectedEntryIcon() const {
  const DesktopEntry* entry = model_.entryAt(selected_index_);
  return entry != nullptr ? entry->icon : QString{};
}

QVariantList LauncherService::selectedEntryActions() const {
  const DesktopEntry* entry = model_.entryAt(selected_index_);
  if (entry == nullptr) {
    return {};
  }
  QVariantList list;
  for (const auto& action : entry->actions) {
    QVariantMap map;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    map[QStringLiteral("name")] = action.name;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    map[QStringLiteral("exec")] = action.exec;
    list.append(map);
  }
  return list;
}

void LauncherService::setQuery(const QString& query) {
  const QString trimmed = query.trimmed();
  if (query_ == trimmed) {
    return;
  }
  query_ = trimmed;
  model_.setQuery(query_);
  emit queryChanged();
}

void LauncherService::setActiveCategory(const QString& category) {
  model_.setActiveCategory(category);
  emit activeCategoryChanged();
}

QStringList LauncherService::availableCategories() { return CategoryMapper::curatedOrder(); }

int LauncherService::countForCategory(const QString& category) const {
  if (category_counts_dirty_) {
    category_counts_cache_.clear();
    for (const QString& cat : CategoryMapper::curatedOrder()) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      category_counts_cache_[cat] = 0;
    }
    const int total = model_.allEntriesCount();
    for (int idx = 0; idx < total; ++idx) {
      const DesktopEntry* entry = model_.allEntryAt(idx);
      if (entry != nullptr) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        ++category_counts_cache_[CategoryMapper::map(entry->categories)];
      }
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    category_counts_cache_[QStringLiteral("All")] = total;
    category_counts_dirty_ = false;
  }
  return category_counts_cache_.value(category, 0);
}

QVariantMap LauncherService::entryInfoForDesktopFile(const QString& desktop_file) const {
  const DesktopEntry* entry = model_.findEntryByDesktopFile(desktop_file);
  if (entry == nullptr) {
    return {};
  }
  QVariantMap map;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  map[QStringLiteral("name")] = entry->name;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  map[QStringLiteral("iconName")] = entry->icon;
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  map[QStringLiteral("desktopFile")] = entry->desktop_file;
  return map;
}

void LauncherService::onModelEntriesReset() {
  invalidateCategoryCache();
  invalidateDefaultAppsCache();
  QSet<QString> available_desktop_files;
  const int total = model_.allEntriesCount();
  available_desktop_files.reserve(total);
  for (int idx = 0; idx < total; ++idx) {
    const DesktopEntry* entry = model_.allEntryAt(idx);
    if (entry != nullptr) {
      available_desktop_files.insert(entry->desktop_file);
    }
  }
  if (recent_apps_tracker_ != nullptr) {
    recent_apps_tracker_->removeUnavailableDesktopFiles(available_desktop_files);
  }
}

void LauncherService::invalidateCategoryCache() { category_counts_dirty_ = true; }

void LauncherService::invalidateDefaultAppsCache() { default_apps_cache_dirty_ = true; }

const QVector<DesktopEntry>& LauncherService::cachedDefaultApps() const {
  if (default_apps_cache_dirty_) {
    default_apps_cache_ = scanner_.scanForVisibleDefaultApps();
    default_apps_cache_dirty_ = false;
  }
  return default_apps_cache_;
}

void LauncherService::recordRecentLaunch(const QString& desktop_file) {
  if (recent_apps_tracker_ != nullptr) {
    recent_apps_tracker_->recordLaunch(desktop_file);
  }
}

void LauncherService::setSelectedIndex(int index) { setSelectedIndexInternal(index); }

void LauncherService::moveSelection(int delta) {
  const int count = resultCount();
  if (count <= 0) {
    setSelectedIndexInternal(-1);
    return;
  }
  if (selected_index_ < 0) {
    setSelectedIndexInternal(delta > 0 ? 0 : count - 1);
    return;
  }
  int new_index = (selected_index_ + delta) % count;
  if (new_index < 0) {
    new_index += count;
  }
  setSelectedIndexInternal(new_index);
}

bool LauncherService::launchSelected() {
  if (model_.isActionRow(selected_index_)) {
    return launchAction(selected_index_, model_.actionIndexAt(selected_index_));
  }
  return launch(selected_index_);
}

bool LauncherService::launch(int index) {
  const DesktopEntry* entry = model_.entryAt(index);
  if (entry == nullptr || backend_ == nullptr) {
    return false;
  }
  const bool launched_ok = backend_->launch(*entry);
  if (launched_ok) {
    recordRecentLaunch(entry->desktop_file);
    emit launched();
  }
  return launched_ok;
}

bool LauncherService::launchDesktopFile(const QString& desktop_file) {
  const DesktopEntry* entry = model_.findEntryByDesktopFile(desktop_file);
  if (entry == nullptr || backend_ == nullptr) {
    return false;
  }
  const bool launched_ok = backend_->launch(*entry);
  if (launched_ok) {
    recordRecentLaunch(entry->desktop_file);
    emit launched();
  }
  return launched_ok;
}

bool LauncherService::launchAction(int entry_index, int action_index) {
  const DesktopEntry* entry = model_.entryAt(entry_index);
  if (entry == nullptr || backend_ == nullptr) {
    return false;
  }
  if (action_index < 0 || action_index >= static_cast<int>(entry->actions.size())) {
    return false;
  }
  const DesktopAction& action = entry->actions.at(action_index);
  const bool launched_ok = backend_->launchExec(action.exec, entry->path);
  if (launched_ok) {
    recordRecentLaunch(entry->desktop_file);
    emit launched();
  }
  return launched_ok;
}

void LauncherService::reload() { runValidator(); }

QVariantList LauncherService::entriesForMimeTypes(const QStringList& mime_types) const {
  const QSet<QString> target(mime_types.begin(), mime_types.end());
  QVariantList result;
  const int count = model_.allEntriesCount();
  for (int idx = 0; idx < count; ++idx) {
    const DesktopEntry* entry = model_.allEntryAt(idx);
    if (entry == nullptr) {
      continue;
    }
    if (entryMatchesMimeTypes(*entry, target)) {
      result.append(desktopEntryComboMap(*entry));
    }
  }
  return result;
}

QVariantList LauncherService::entriesForMimeTypesAndCategories(const QStringList& mime_types,
                                                               const QStringList& categories) const {
  const QSet<QString> target_mimes(mime_types.begin(), mime_types.end());
  const QSet<QString> target_categories(categories.begin(), categories.end());
  QVariantList result;
  const int count = model_.allEntriesCount();
  for (int idx = 0; idx < count; ++idx) {
    const DesktopEntry* entry = model_.allEntryAt(idx);
    if (entry == nullptr) {
      continue;
    }
    if (!entryMatchesMimeTypes(*entry, target_mimes)) {
      continue;
    }
    if (!entryMatchesCategories(*entry, target_categories)) {
      continue;
    }
    result.append(desktopEntryComboMap(*entry));
  }
  return result;
}

QVariantList LauncherService::defaultAppEntriesForMimeTypes(const QStringList& mime_types) const {
  const QSet<QString> target(mime_types.begin(), mime_types.end());
  QVariantList result;
  for (const DesktopEntry& entry : cachedDefaultApps()) {
    if (entryMatchesMimeTypes(entry, target)) {
      result.append(desktopEntryComboMap(entry));
    }
  }
  return result;
}

QVariantList LauncherService::defaultAppEntriesForMimeTypesAndCategories(const QStringList& mime_types,
                                                                         const QStringList& categories) const {
  const QSet<QString> target_mimes(mime_types.begin(), mime_types.end());
  const QSet<QString> target_categories(categories.begin(), categories.end());
  QVariantList result;
  for (const DesktopEntry& entry : cachedDefaultApps()) {
    if (entryMatchesMimeTypes(entry, target_mimes) && entryMatchesCategories(entry, target_categories)) {
      result.append(desktopEntryComboMap(entry));
    }
  }
  return result;
}

QVariantList LauncherService::entriesForCategory(const QString& category) const {
  QVariantList result;
  const int count = model_.allEntriesCount();
  for (int idx = 0; idx < count; ++idx) {
    const DesktopEntry* entry = model_.allEntryAt(idx);
    if (entry == nullptr) {
      continue;
    }
    const QStringList entry_categories = desktopEntryCategories(*entry);
    if (std::ranges::any_of(entry_categories, [&category](const QString& cat) { return cat == category; })) {
      result.append(desktopEntryComboMap(*entry));
    }
  }
  return result;
}

QVariantList LauncherService::defaultAppEntriesForCategory(const QString& category) const {
  QVariantList result;
  for (const DesktopEntry& entry : cachedDefaultApps()) {
    if (entryMatchesCategories(entry, QSet<QString>{category})) {
      result.append(desktopEntryComboMap(entry));
    }
  }
  return result;
}

void LauncherService::runValidator() {
  if (watcher_ != nullptr && watcher_->isRunning()) {
    validator_rerun_pending_ = true;
    return;
  }
  if (watcher_ == nullptr) {
    watcher_ = new QFutureWatcher<ScanResult>(this);
    connect(watcher_, &QFutureWatcher<ScanResult>::finished, this, [this]() {
      const ScanResult& result = watcher_->result();
      preserve_selection_on_model_reset_ = true;
      model_.setEntries(result.entries);
      emit entriesUpdated();
      if (fs_watcher_ != nullptr && !result.watched_dirs.isEmpty()) {
        const QStringList already = fs_watcher_->directories();
        QStringList to_add;
        for (const QString& dir : result.watched_dirs) {
          if (!already.contains(dir)) {
            to_add.append(dir);
          }
        }
        if (!to_add.isEmpty()) {
          fs_watcher_->addPaths(to_add);
        }
      }
      if (validator_rerun_pending_) {
        validator_rerun_pending_ = false;
        QTimer::singleShot(0, this, &LauncherService::runValidator);
      }
    });
  }

  const QString db_path = db_path_;
  watcher_->setFuture(QtConcurrent::run([this, db_path]() { return validateAgainstCache(scanner_, db_path); }));
}

void LauncherService::captureSelectionBeforeModelReset() {
  pending_selection_identity_.reset();
  if (!preserve_selection_on_model_reset_ || selected_index_ < 0) {
    return;
  }

  const DesktopEntry* entry = model_.entryAt(selected_index_);
  if (entry == nullptr) {
    return;
  }

  SelectionIdentity identity{.desktop_file = entry->desktop_file,
                             .previous_index = selected_index_,
                             .is_action = model_.isActionRow(selected_index_)};
  if (identity.is_action) {
    const int action_index = model_.actionIndexAt(selected_index_);
    if (action_index < 0 || action_index >= entry->actions.size()) {
      return;
    }
    identity.action_exec = entry->actions.at(action_index).exec;
  }
  pending_selection_identity_ = std::move(identity);
}

void LauncherService::refreshSelectionAfterModelReset() {
  int restored_index = resultCount() > 0 ? 0 : -1;
  if (pending_selection_identity_.has_value()) {
    const SelectionIdentity& identity = *pending_selection_identity_;
    bool found_identity = false;
    for (int row = 0; row < resultCount(); ++row) {
      const DesktopEntry* entry = model_.entryAt(row);
      if (entry == nullptr || entry->desktop_file != identity.desktop_file ||
          model_.isActionRow(row) != identity.is_action) {
        continue;
      }
      if (!identity.is_action || entry->actions.at(model_.actionIndexAt(row)).exec == identity.action_exec) {
        restored_index = row;
        found_identity = true;
        break;
      }
    }
    if (!found_identity && resultCount() > 0) {
      restored_index = qMin(identity.previous_index, resultCount() - 1);
    }
  }
  pending_selection_identity_.reset();
  preserve_selection_on_model_reset_ = false;
  selected_index_ = restored_index;
  emit selectedIndexChanged();
}

void LauncherService::setSelectedIndexInternal(int index) {
  const int clamped = resultCount() <= 0 ? -1 : std::clamp(index, 0, resultCount() - 1);
  if (selected_index_ == clamped) {
    return;
  }
  selected_index_ = clamped;
  emit selectedIndexChanged();
}
