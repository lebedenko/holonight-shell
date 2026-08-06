# SDD Tasks — launcher-desktop-cache

- [x] T-001: Add Qt6::Sql to CMakeLists.txt
  - REQs: REQ-C-001
  - Check: `find_package(Qt6 ... Sql)` and `target_link_libraries(holonight_services ... Qt6::Sql)` are present, and `DesktopEntryCache.cpp` and `DesktopEntrySerializer.h` are listed in source files.

- [x] T-002: Create DesktopEntrySerializer header
  - REQs: REQ-F-002, REQ-F-015
  - Check: Header-only `toJson(DesktopEntry) -> QJsonObject` and `fromJson(QJsonObject) -> std::optional<DesktopEntry>` are defined; `fromJson` returns nullopt if `name`, `exec`, or `desktop_file` are missing; all fields including `actions` array are serialized and deserialized correctly.

- [x] T-003: Create DesktopEntryCache class
  - REQs: REQ-F-001, REQ-F-002, REQ-F-014
  - Check: `open(db_path)`, `close()`, `loadAll()`, `metadata(path)`, `upsert(entry, mtime, size)`, and `remove(path)` methods are implemented; schema `CREATE TABLE IF NOT EXISTS desktop_entries (path TEXT PK, mtime INTEGER, size INTEGER, name TEXT, categories TEXT, data TEXT)` is created; each DB operation opens a named connection with unique name and logs `qCWarning` on failure.

- [x] T-004: Add ScanResult struct and scanWithDirs() to DesktopEntryScanner
  - REQs: REQ-F-010
  - Check: `ScanResult` struct contains `QVector<DesktopEntry> entries` and `QStringList watched_dirs`; `scanWithDirs()` mirrors `scan()` and records every directory path seen during `QDirIterator` traversal; existing `scan()` is unchanged.

- [x] T-005: Modify LauncherService::start() to load cache synchronously
  - REQs: REQ-F-003, REQ-F-004, REQ-NF-001, REQ-NF-004
  - Check: `start()` resolves `db_path_` via `QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/holonight-shell/launcher.db"`, calls `cache_.open()`, and on success synchronously calls `cache_.loadAll()` then `model_.setEntries()`; on failure (missing/corrupt) starts empty and the validator rebuilds the cache; `db_path_` is resolved before `runValidator()` is called.

- [x] T-006: Create LauncherService::runValidator() async path
  - REQs: REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-NF-002
  - Check: `runValidator()` calls `QtConcurrent::run()` with lambda; lambda calls `scanner_.scanWithDirs()`; validates entries against cache using mtime+size comparison; writes new/changed entries to DB via `cache_.upsert()`, removes missing entries via `cache_.remove()`; `finished` handler calls `model_.setEntries()` with corrected list.

- [x] T-007: Wire QFileSystemWatcher and debounce timer in LauncherService::start()
  - REQs: REQ-F-009, REQ-F-011, REQ-F-012
  - Check: `fs_watcher_` and `debounce_timer_` are created in `start()` and initialized with hardcoded directories that exist at startup; `debounce_timer_` is single-shot with 500ms interval; `directoryChanged` signal is connected to `debounce_timer_->start()`; `timeout` signal is connected to `runValidator()`.

- [x] T-008: Update LauncherService::runValidator() finished handler to add discovered subdirectories to watcher
  - REQs: REQ-F-010
  - Check: After validator completes, `finished` handler calls `fs_watcher_->addPaths(scan_result.watched_dirs)`, deduplicating against already-watched directories; new subdirectories discovered during scan are added to watcher.

- [x] T-009: Add Flatpak and Snap paths to DesktopEntryScanner::defaultApplicationDirs()
  - REQs: REQ-F-009
  - Check: `defaultApplicationDirs()` includes `/var/lib/flatpak/exports/share/applications`, `~/.local/share/flatpak/exports/share/applications`, and `/var/lib/snapd/desktop/applications`; these directories are scanned and their subdirectories are watched.

- [ ] T-010: Verify cache load latency performance
  - REQs: REQ-NF-001
  - Check: Cache load from `start()` to `model_.setEntries()` completion is ≤ 16ms when cache contains 500+ entries; measure with cache hit on startup. Not covered by current automated tests.

- [ ] T-011: Verify validator does not block UI during filesystem scan
  - REQs: REQ-NF-002
  - Check: While validator is running on background thread, launcher UI remains responsive; window animations do not stutter; user input is processed immediately. Not covered by current automated tests.

- [ ] T-012: Verify debounce window coalesces rapid changes
  - REQs: REQ-NF-003
  - Check: Creating three `.desktop` files within 100ms in a watched directory triggers exactly one validator run; two `directoryChanged` events 250ms apart trigger one run; events 600ms apart trigger two runs. Not covered by current automated tests.

- [ ] T-013: Verify watcher resilience and error handling
  - REQs: REQ-NF-005, REQ-NF-006
  - Check: Unmounting a watched directory logs warning and continues; validator's next run detects entries as missing and removes from model; running multiple validator instances never produces database lock errors; concurrent writes are serialized. Not covered by current automated tests.

- [x] T-014: Verify database schema migration and corruption recovery
  - REQs: REQ-F-004
  - Check: Deleting `launcher.db` and restarting triggers full scan; database is recreated with schema; corrupted database falls back to full scan without error. Covered by `LauncherService.RebuildsCorruptedLauncherCacheDatabase`.

- [x] T-015: Verify cache persistence across restarts
  - REQs: REQ-F-014, REQ-F-015
  - Check: After first run with entries cached, restarting shows cached entries before validator runs; cached entries persist in database with updated mtime after modification; deleted entries are removed from database. JSON round-trip and transaction behavior are covered by `DesktopEntrySerializer.RoundTripsEntryWithActions` and `DesktopEntryCache.PersistsRowsAndRollsBackTransactions`; full restart timing remains manual.

- [x] T-016: Verify model reset on validator completion
  - REQs: REQ-C-005, REQ-F-008
  - Check: After validator completes, `model_.setEntries()` is called once with complete corrected list; no incremental add/remove/update slot calls are made; deleted entries no longer appear in launcher UI.
