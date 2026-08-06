# Launcher Desktop Cache — Architecture Design

## Overview

This document describes the implementation architecture for the `launcher-desktop-cache` feature.
The feature adds SQLite-backed persistence and `QFileSystemWatcher`-based live updates to the
launcher's `.desktop` file index. On startup the launcher loads the cache synchronously for instant
display, then validates and corrects it on a background thread. File system changes are debounced
and trigger the same validation path.

---

## 1. New Components

### 1.1 `DesktopEntryCache`

**Files:** `src/services/launcher/DesktopEntryCache.h`, `src/services/launcher/DesktopEntryCache.cpp`

Owns a single `QSqlDatabase` connection to the SQLite file. All public methods are synchronous and
must only be called from a background thread (the `QtConcurrent` task), except `open()` and
`close()` which are called from the main thread during startup and shutdown.

```cpp
class DesktopEntryCache {
 public:
  // Opens (or creates) the DB at the given absolute path. Creates the parent
  // directory if missing. Returns false and logs qCWarning on failure.
  [[nodiscard]] bool open(const QString& db_path);

  // Closes the database connection. Safe to call even if open() failed.
  void close();

  // Reads all rows and deserializes each data JSON blob via DesktopEntrySerializer.
  // Returns an empty vector (not a throw) on DB error.
  [[nodiscard]] QVector<DesktopEntry> loadAll() const;

  struct FileMeta {
    qint64 mtime{};
    qint64 size{};
  };

  // Returns the cached mtime+size for path, or nullopt if not present.
  [[nodiscard]] std::optional<FileMeta> metadata(const QString& path) const;

  // INSERT OR REPLACE — serializes entry to JSON for the data column.
  bool upsert(const DesktopEntry& entry, qint64 mtime, qint64 size);

  // DELETE FROM desktop_entries WHERE path = :path.
  bool remove(const QString& path);

 private:
  QSqlDatabase db_;
};
```

Key implementation notes:

- The `QSqlDatabase` connection name is `"holonight_launcher"`. Because `LauncherService` is a QML
  singleton (one instance per process), there is no risk of connection-name collision.
- All write methods (`upsert`, `remove`) log a `qCWarning` and return `false` on SQL error; the
  caller still propagates the corrected in-memory vector to the model.
- `loadAll()` skips individual rows that fail JSON deserialization (logs `qCWarning` per row) and
  continues rather than aborting the entire load.

### 1.2 `DesktopEntrySerializer`

**Files:** `src/services/launcher/DesktopEntrySerializer.h` (header-only)

Converts between `DesktopEntry` and `QJsonObject`. Placed in a header so it can be inlined by both
the cache and any future test code without a separate translation unit.

```cpp
namespace DesktopEntrySerializer {

QJsonObject toJson(const DesktopEntry& entry);
std::optional<DesktopEntry> fromJson(const QJsonObject& obj);

}  // namespace DesktopEntrySerializer
```

Covers all fields of `DesktopEntry` (`name`, `generic_name`, `comment`, `exec`, `icon`,
`categories`, `path`, `desktop_file`, `terminal`) and the nested `actions` array (`name`, `exec`
per `DesktopAction`). `fromJson` returns `std::nullopt` when a required field is missing or has the
wrong type.

### 1.3 Changes to `DesktopEntryScanner`

**Modified file:** `src/services/launcher/DesktopEntryScanner.h/.cpp`

Add a method that performs the same directory traversal as `scan()` but also collects every
directory path visited (including recursively discovered subdirectories). This is the set of paths
to register with `QFileSystemWatcher`.

```cpp
struct ScanResult {
  QVector<DesktopEntry> entries;
  QStringList watched_dirs;   // all dirs seen during QDirIterator traversal
};

[[nodiscard]] ScanResult scanWithDirs() const;
```

The existing `scan()` method is preserved unchanged so callers that do not need the watcher set
continue to compile without modification. `scanWithDirs()` is the method used by the validator path
introduced in this feature.

Implementation: `scanWithDirs()` mirrors `scan()` but after each `iterator.next()` call, records
`QFileInfo(path).absolutePath()` into a `QSet<QString>` to deduplicate, then converts to
`QStringList` at the end. The root `dir_path` entries are also included.

### 1.4 Changes to `LauncherService`

**Modified file:** `src/services/launcher/LauncherService.h/.cpp`

New private members:

```cpp
DesktopEntryCache cache_;
QFileSystemWatcher* fs_watcher_{nullptr};
QTimer* debounce_timer_{nullptr};
QString db_path_;           // resolved once in start()
```

**`start()` — modified:**

Currently calls `reload()` unconditionally. With the cache:

1. Resolve `db_path_` (`$XDG_CACHE_HOME/holonight-shell/launcher.db`, defaulting via
   `QStandardPaths::GenericCacheLocation + "/holonight-shell/launcher.db"`).
2. Call `cache_.open(db_path_)`.
3. If `open` succeeds: call `cache_.loadAll()` → `model_.setEntries(entries)` synchronously.
   If `open` fails (missing/corrupt): `model_.setEntries({})` (empty, will be filled by validator).
4. Set up `fs_watcher_` and `debounce_timer_` (see Section 6 for directory list).
5. Call `runValidator()` (the renamed/new shared async path).

**`reload()` — renamed to `runValidator()`** (public slot kept as `reload()` for QML
compatibility — it simply calls `runValidator()`):

The existing body of `reload()` becomes `runValidator()`. The only change to the async lambda is
that it now calls `scanner_.scanWithDirs()` instead of `scanner_.scan()`, uses
`DesktopEntryCache` for cache operations, and returns a `QVector<DesktopEntry>` built via the
validation algorithm (Section 7). The `QFutureWatcher::finished` slot receives the corrected
vector and calls `model_.setEntries()` as before, then updates the watcher's directory set (see
Section 6).

**Watcher and debounce setup in `start()`:**

```cpp
fs_watcher_ = new QFileSystemWatcher(this);
debounce_timer_ = new QTimer(this);
debounce_timer_->setSingleShot(true);
debounce_timer_->setInterval(500);
connect(fs_watcher_, &QFileSystemWatcher::directoryChanged, debounce_timer_,
        qOverload<>(&QTimer::start));
connect(debounce_timer_, &QTimer::timeout, this, &LauncherService::runValidator);
```

**`~LauncherService()` — unchanged** (existing cancel/waitForFinished on `watcher_` is sufficient).

---

## 2. Data Flow Diagrams

### 2.1 Startup Flow

```
Main thread
───────────────────────────────────────────────────────────────────────
LauncherService::start()
  │
  ├─ resolve db_path_
  ├─ cache_.open(db_path_)          ← synchronous; creates file if absent
  │     └─ schema migration (CREATE TABLE IF NOT EXISTS)
  │
  ├─ [if open succeeded]
  │     entries = cache_.loadAll()  ← synchronous; deserializes JSON rows
  │     model_.setEntries(entries)  ← UI can render before first frame
  │
  ├─ setup QFileSystemWatcher
  │     addPaths(existing dirs from hardcoded list)
  │
  ├─ setup debounce_timer_ (500ms single-shot)
  │
  └─ runValidator()
        │
        └─ QtConcurrent::run(lambda) ──────────────────────────────────►
                                                     Background thread
                                                     ─────────────────
                                                     result = scanWithDirs()
                                                     // entries + watched_dirs
                                                     for each scanned file:
                                                       compare mtime+size
                                                       vs cache_.metadata()
                                                       → parse if new/changed
                                                       → use cached if same
                                                     for each cached path
                                                       not in scan:
                                                       cache_.remove(path)
                                                     cache_.upsert(new/changed)
                                                     return corrected entries
                                                     ◄─────────────────────────
        QFutureWatcher::finished (main thread)
          model_.setEntries(correctedEntries)
          fs_watcher_->addPaths(newSubdirs)
```

### 2.2 Watcher Event Flow

```
[inotify / kqueue event]
  │
  QFileSystemWatcher::directoryChanged
  │
  debounce_timer_->start()     ← resets the 500ms window on each rapid event
  │
  (500ms of silence)
  │
  QTimer::timeout
  │
  runValidator()               ← same path as startup validator
  │
  QtConcurrent::run(lambda)
  │
  ... (same background scan as startup, see above) ...
  │
  QFutureWatcher::finished
    model_.setEntries(correctedEntries)
    fs_watcher_->addPaths(newSubdirs)
```

---

## 3. SQLite Schema

```sql
CREATE TABLE IF NOT EXISTS desktop_entries (
    path       TEXT    PRIMARY KEY,   -- absolute path to .desktop file
    mtime      INTEGER NOT NULL,      -- seconds since Unix epoch (QFileInfo::lastModified)
    size       INTEGER NOT NULL,      -- file size in bytes (QFileInfo::size)
    name       TEXT    NOT NULL,      -- DesktopEntry::name (display name)
    categories TEXT    NOT NULL,      -- DesktopEntry::categories (raw semicolon string)
    data       TEXT    NOT NULL       -- JSON blob: all DesktopEntry fields
);
```

There are no additional indices. The cache is always read in full (`loadAll`), and lookups by path
during validation use `metadata()` which executes `SELECT mtime, size WHERE path = ?` — a primary
key lookup, so no extra index is needed.

Schema migration strategy: `open()` runs `CREATE TABLE IF NOT EXISTS` on every startup. If the
statement fails (e.g., corrupt file), `open()` returns `false` and the startup path falls back to
a full scan without using the cache.

---

## 4. JSON Blob Format

The `data` column stores all `DesktopEntry` fields as a JSON object. Example for Firefox with one
action:

```json
{
  "name": "Firefox Web Browser",
  "generic_name": "Web Browser",
  "comment": "Browse the World Wide Web",
  "exec": "/usr/lib/firefox/firefox %u",
  "icon": "firefox",
  "categories": "Network;WebBrowser;",
  "path": "",
  "desktop_file": "/usr/share/applications/firefox.desktop",
  "terminal": false,
  "actions": [
    {
      "name": "Open a New Window",
      "exec": "/usr/lib/firefox/firefox --new-window %u"
    },
    {
      "name": "Profile Manager",
      "exec": "/usr/lib/firefox/firefox -ProfileManager"
    }
  ]
}
```

Field notes:

- `path` is `DesktopEntry::path` (the `Path=` key for working directory), which may be empty.
- `desktop_file` is the absolute filesystem path to the `.desktop` file itself.
- `terminal` is stored as a JSON boolean, not a string.
- `actions` is an array of objects, each with `name` and `exec` only (the fields parsed by
  `DesktopEntryScanner`).
- `fromJson` returns `std::nullopt` if `name`, `exec`, or `desktop_file` are missing or not
  strings; other missing fields are defaulted to empty string / `false` / empty array.

---

## 5. Thread Safety

| Operation | Thread | Notes |
|---|---|---|
| `cache_.open()` | Main | Called once in `start()` before any background work |
| `cache_.loadAll()` | Main | Synchronous; completes before `runValidator()` is called |
| `fs_watcher_` setup | Main | Created in `start()`, signals delivered to main thread |
| `debounce_timer_` | Main | Qt timer; `start()` called from `directoryChanged` slot on main thread |
| `runValidator()` | Main | Schedules work; returns immediately |
| `scanner_.scanWithDirs()` | Background (`QtConcurrent`) | No shared mutable state |
| `cache_.metadata()` | Background | Reads from DB on background thread |
| `cache_.upsert()` | Background | Writes to DB on background thread |
| `cache_.remove()` | Background | Writes to DB on background thread |
| `model_.setEntries()` | Main | Called from `QFutureWatcher::finished`, which is delivered to the main thread |
| `fs_watcher_->addPaths()` | Main | Called from `QFutureWatcher::finished` |

**Why no mutex is needed:** All DB writes occur exclusively inside the single `QtConcurrent` task.
`LauncherService::reload()` (and `runValidator()`) guards against overlapping runs with the
existing check:

```cpp
if (watcher_ != nullptr && watcher_->isRunning()) {
    return;
}
```

This means only one background task can be active at a time, so no two tasks can write to the DB
concurrently. The debounce timer ensures rapid `directoryChanged` events do not launch a second run
while one is already in progress.

**`QSqlDatabase` and threads:** Qt's SQL module associates a database connection with the thread
that called `QSqlDatabase::addDatabase`. Because `open()` is called on the main thread but all
read/write methods are later called from the background thread, the connection must either be
opened on the background thread or the main-thread connection must not be used from the background
thread.

Chosen approach: `open()` does NOT add the connection to Qt's global registry under a thread-bound
name. Instead, the `DesktopEntryCache` stores the raw `QSqlDatabase` handle and the background
task receives it (or a wrapper) via capture. A clean implementation alternative is to open the
connection inside the `QtConcurrent` lambda (on the background thread) each time the validator
runs, passing only `db_path_` as a captured `QString`. This avoids the cross-thread connection
sharing issue entirely and is the recommended implementation path. `loadAll()` on the main thread
uses a separate, short-lived connection opened and closed within that call.

---

## 6. Watcher Directory Collection

### Hardcoded Directory List

The following directories are evaluated at startup. Each is checked for existence with
`QDir::exists()` before being passed to `QFileSystemWatcher::addPaths()`. Non-existent paths are
silently skipped — `QFileSystemWatcher::addPaths()` returns a list of paths it failed to add, and
those paths simply remain unwatched.

```
$XDG_DATA_HOME/applications          (~/.local/share/applications)
$XDG_DATA_DIRS[*]/applications       (each colon-separated entry)
/usr/local/share/applications        (appended if not already in XDG_DATA_DIRS)
/usr/share/applications              (appended if not already in XDG_DATA_DIRS)
/var/lib/flatpak/exports/share/applications
~/.local/share/flatpak/exports/share/applications
/var/lib/snapd/desktop/applications
```

The Flatpak and Snap paths are hardcoded additions on top of what `DesktopEntryScanner::defaultApplicationDirs()` already provides. They are not in `applicationDirs_` and do not participate in
scanning — they are watcher-only, so a Flatpak install creates the `.desktop` file in one of the
XDG dirs covered by the scan anyway. Alternatively, `LauncherService` can add these extra paths to
the scanner's `application_dirs_` list so they are both scanned and watched, which is the
preferred approach for consistency.

### Subdirectory Collection

`DesktopEntryScanner::scanWithDirs()` uses `QDirIterator::Subdirectories`. Every directory path
encountered during iteration is recorded. After the validator completes, the `finished` handler
calls:

```cpp
const QStringList already = fs_watcher_->directories();
QStringList to_add;
for (const QString& dir : scan_result.watched_dirs) {
    if (!already.contains(dir)) {
        to_add.append(dir);
    }
}
if (!to_add.isEmpty()) {
    fs_watcher_->addPaths(to_add);
}
```

This is additive only — the watcher never removes directories automatically (a directory that
disappears stops emitting events; the validator handles the case where its entries go missing).

### `QFileSystemWatcher::addPaths()` Behavior

Per Qt docs, `addPaths()` silently ignores paths that do not exist or cannot be watched. The
return value (list of failed paths) is discarded. This means the Flatpak and Snap directories can
be unconditionally passed to `addPaths()` without a prior existence check — the behaviour is
identical to filtering first. For clarity, the implementation checks existence first using
`QDir::exists()` and only passes confirmed paths, matching the constraint in REQ-C-004.

---

## 7. Cache Validation Algorithm

The validator runs entirely on the background thread. It receives access to `DesktopEntryCache`
(opened on the background thread) and `DesktopEntryScanner`.

```
function runValidation(scanner, cache) -> ScanResult:

    result = scanner.scanWithDirs()
    // result.entries: all .desktop files found on disk right now
    // result.watched_dirs: all dirs traversed

    scanned_paths = set of result.entries[*].desktop_file

    corrected = []

    for each entry in result.entries:
        path = entry.desktop_file
        fi   = QFileInfo(path)
        disk_mtime = fi.lastModified().toSecsSinceEpoch()
        disk_size  = fi.size()

        cached_meta = cache.metadata(path)   // nullopt if not in DB

        if cached_meta is nullopt:
            // New file — parse already done by scanner; insert into DB
            cache.upsert(entry, disk_mtime, disk_size)
            corrected.append(entry)

        elif cached_meta.mtime != disk_mtime OR cached_meta.size != disk_size:
            // File changed — scanner already re-parsed it; update DB
            cache.upsert(entry, disk_mtime, disk_size)
            corrected.append(entry)

        else:
            // Unchanged — use the already-parsed entry from scanner
            // (scanner output and cache output are equivalent; scanner
            //  result is used to avoid a second DB round-trip per entry)
            corrected.append(entry)

    // Find cached entries that no longer exist on disk
    all_cached = cache.loadAll()
    for each cached_entry in all_cached:
        if cached_entry.desktop_file NOT IN scanned_paths:
            cache.remove(cached_entry.desktop_file)
            // do NOT append to corrected

    // Sort by locale-aware name (matches existing scanner sort)
    std::ranges::sort(corrected, ...)

    return { corrected, result.watched_dirs }
```

Notes:

- Because `DesktopEntryScanner::scan()` already parses every `.desktop` file it finds, the
  validator does not need to re-read files for the "unchanged" case beyond what the scanner
  already did. The DB provides the benefit of skipping parsing on the *next* startup, not during
  the current validator run. An optimisation for large installs (future work) would be to skip
  parsing unchanged files in the scanner itself, using the DB metadata as a pre-scan filter.
- `cache.loadAll()` called at the end for the deletion pass is a second full DB read per validator
  run. For the expected entry count (hundreds, not millions) this is fast and simpler than
  maintaining a separate set of all cached paths.

---

## 8. Key Decisions and Rationale

### Why one `QSqlDatabase` connection, not per-thread

Qt documents that `QSqlDatabase` connections are not shared across threads. The chosen approach
(opening the connection on the background thread for writes, and a separate short-lived connection
on the main thread for `loadAll()`) keeps each connection in the thread that created it. There is
no concurrent access: the main thread only reads during `start()` before any background task
starts, and the background task owns all subsequent reads and writes.

WAL mode (`PRAGMA journal_mode=WAL`) is set on `open()` to allow a reader and writer to overlap
without blocking. Since the main thread's read (`loadAll`) and the background task's writes do not
actually overlap in the current design, WAL is a belt-and-suspenders measure for future
extensibility.

### Why `QTimer` single-shot for debounce

A single-shot `QTimer` with `start()` called on each `directoryChanged` signal resets the timer
window on each event. This is idiomatic Qt, runs on the main thread's event loop, and requires no
extra state. Alternatives (`QEventLoop`, manual cooldown timestamps) are more complex with no
benefit here.

### Why subdirectories are watched

`DesktopEntryScanner::scan()` uses `QDirIterator::Subdirectories`. Applications can install
`.desktop` files in subdirectories (e.g., Flatpak user installs under
`~/.local/share/flatpak/exports/share/applications/`). Watching only the top-level directories
would miss changes in those subdirectories.

### Why `loadAll()` is synchronous on the main thread

REQ-F-003 and REQ-NF-001 require the cached entry list to be visible before the first UI frame
(≤ 16ms). Making `loadAll()` async would require the UI to render empty on the first frame and
update on a second frame, eliminating the latency benefit of the cache. The read is fast (hundreds
of rows, JSON deserialization) and does not block the compositor.

---

## 9. Alternatives Considered

### Incremental model updates

Rather than calling `model_.setEntries(fullList)` after the validator, the validator could emit
individual add/change/remove signals to update the model row-by-row. Rejected: the existing
`LauncherModel` has no incremental update API, the full reset path is already correct and tested,
and the validator is fast enough that a full reset is imperceptible.

### File content hashing (MD5/SHA-1)

Comparing file content hashes instead of mtime+size would detect in-place edits to `.desktop`
files (e.g., `sed -i`). Rejected: hashing requires reading every file on every validator run,
which is slower; mtime+size is sufficient for package manager use cases (files are recreated, not
edited in place); and REQ-C-006 explicitly constrains the implementation to mtime+size.

### Watching parent directories for new subdirectory creation

If a new subdirectory is created under a watched directory *after* startup, `QFileSystemWatcher`
emits `directoryChanged` for the parent. The validator would then run but would only add the
subdirectory to the watcher at the end of that run. Files already in the new subdirectory would be
picked up; files added to the subdirectory later (after the first run) would be picked up in
subsequent runs because the subdirectory is now watched. This "one event lag" is accepted as a
Stage 1 limitation (REQ-C-004). Supporting dynamically created directories would require watching
parent paths and checking for new subdirs on each `directoryChanged` event, adding complexity not
justified for the common case.

### `QThread` vs `QtConcurrent`

Using a dedicated `QThread` with a worker object would allow the cache connection to live
permanently on the worker thread (created in its `run()`, owned there). Rejected: the existing
`LauncherService` already uses `QtConcurrent::run` + `QFutureWatcher` and there is no need for a
persistent thread. `QtConcurrent` reuses the global thread pool, which is simpler and adequate for
the infrequent validator runs (once at startup, then on watcher events).

---

## 10. Known Risks

### `QSqlDatabase` cross-thread usage

If `open()` is called on the main thread and subsequent methods (`loadAll` in validator,
`upsert`/`remove`) are called from the background thread, Qt will emit a warning and the behaviour
is undefined. **Mitigation:** open a fresh, named connection inside the background lambda for all
validator DB work; open a separate short-lived connection in `loadAll()` when called from the main
thread. Each connection is opened, used, and closed within the thread that created it.

### `QFileSystemWatcher` spurious signals

On some file systems (NFS, CIFS, certain inotify configurations), `directoryChanged` can fire
without any actual change. The 500ms debounce absorbs these, and a spurious validator run is
harmless — it will find no changes and call `model_.setEntries` with an identical list.

### DB writes from the background thread

All `upsert` and `remove` calls happen inside the `QtConcurrent` task. The guard
`if (watcher_->isRunning()) return;` in `runValidator()` ensures at most one task is active.
Therefore no two tasks can write to the DB simultaneously and no mutex is needed around DB writes.

### Validator skipped while one is running

If `QFileSystemWatcher` fires while a validator is already running (the guard returns early), the
debounce timer is not restarted — the watcher event is dropped. This is acceptable: the next
watcher event (or a subsequent `reload()` call) will catch any remaining changes. A future
improvement could track a "needs-rerun" flag and schedule another run when the current one
finishes.

### `QSqlDatabase` connection name collision

The connection name `"holonight_launcher"` is a fixed string. Because `LauncherService` is a QML
singleton, exactly one instance exists per process. If a second connection with the same name were
opened on a different thread without closing the first, Qt would log an error. The implementation
must ensure each open/close pair uses a unique name or closes before re-opening.

---

## 11. CMakeLists.txt Changes

### `find_package`

Append `Sql` to the existing `find_package(Qt6 REQUIRED COMPONENTS ...)` line:

```cmake
# Before:
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml DBus Network Concurrent)

# After:
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml DBus Network Concurrent Sql)
```

### `target_link_libraries` for `holonight_services`

Add `Qt6::Sql` to the PUBLIC link list of `holonight_services`:

```cmake
target_link_libraries(holonight_services PUBLIC
    holonight_core
    holonight_platform
    Qt6::Core
    Qt6::Qml
    Qt6::DBus
    Qt6::Network
    Qt6::Concurrent
    Qt6::Sql          # <-- add this line
    ${LIBPULSE_LIBRARIES}
)
```

### New source files in `holonight_services`

Add to the `add_library(holonight_services STATIC ...)` source list:

```cmake
src/services/launcher/DesktopEntryCache.h
src/services/launcher/DesktopEntryCache.cpp
src/services/launcher/DesktopEntrySerializer.h
```

`DesktopEntrySerializer.h` is header-only and does not need a `.cpp` entry, but including the
header in the source list ensures it appears in IDE file trees and is picked up by
`file(GLOB_RECURSE ALL_SOURCE_FILES ...)` for `clang-format` and `clang-tidy` targets (those
globs already cover `src/*.h` recursively, so no additional change is required there).

No changes to `target_include_directories` are needed — `src/services/launcher` is already on the
include path for `holonight_services`.

---

## File Map Summary

| File | Status | Purpose |
|---|---|---|
| `src/services/launcher/DesktopEntryCache.h` | New | SQLite connection owner; synchronous CRUD |
| `src/services/launcher/DesktopEntryCache.cpp` | New | Implementation of `DesktopEntryCache` |
| `src/services/launcher/DesktopEntrySerializer.h` | New | Header-only JSON ↔ `DesktopEntry` conversion |
| `src/services/launcher/DesktopEntryScanner.h` | Modified | Add `ScanResult` struct and `scanWithDirs()` |
| `src/services/launcher/DesktopEntryScanner.cpp` | Modified | Implement `scanWithDirs()` |
| `src/services/launcher/LauncherService.h` | Modified | Add `cache_`, `fs_watcher_`, `debounce_timer_`, `db_path_` members |
| `src/services/launcher/LauncherService.cpp` | Modified | Implement cache load in `start()`, refactor `reload()` into `runValidator()`, wire watcher |
| `CMakeLists.txt` | Modified | Add `Qt6::Sql` to `find_package` and `target_link_libraries`; add two new source files |
