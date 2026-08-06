# Launcher Desktop Cache — EARS Specification

## Executive Summary

The launcher-desktop-cache feature adds SQLite-backed persistent caching of `.desktop` files with `QFileSystemWatcher`-based live updates. On startup, the launcher loads the cache immediately for instant UI responsiveness, then validates entries asynchronously against the filesystem. File system changes trigger debounced full rescans. Cache misses and corruption fall back to full scan and database rebuild.

---

## Functional Requirements

### Database & Schema

**REQ-F-001: SQLite database initialization**

The system shall initialize a SQLite database at `$XDG_CACHE_HOME/holonight-shell/launcher.db` on first run (creating parent directories as needed).

**AC:** Running the launcher for the first time creates the database file and a table named `desktop_entries` with columns `path TEXT PRIMARY KEY`, `mtime INTEGER`, `size INTEGER`, `name TEXT`, `categories TEXT`, `data TEXT`.

---

**REQ-F-002: Desktop entry table schema**

The system shall store desktop entries in a table named `desktop_entries` with the following columns:
- `path TEXT PRIMARY KEY` — absolute file system path to the `.desktop` file
- `mtime INTEGER` — file modification timestamp (seconds since epoch)
- `size INTEGER` — file size in bytes
- `name TEXT` — display name of the entry
- `categories TEXT` — application categories as comma-separated string
- `data TEXT` — complete JSON encoding of parsed `DesktopEntry` struct fields (`name`, `generic_name`, `comment`, `exec`, `icon`, `categories`, `path`, `desktop_file`, `terminal`, `actions`)

**AC:** A serialized `DesktopEntry` with `name="Firefox"`, `icon="firefox"`, `terminal=false`, and `actions=[{name:"Profile Manager", exec:"firefox -ProfileManager"}]` is stored in `data` column as valid JSON parseable back to the original struct.

---

### Startup Cache Load

**REQ-F-003: Load cache on startup before UI frame**

When the launcher service starts, the system shall load all rows from the SQLite cache and immediately call `LauncherModel::setEntries()` with cached entries before the first UI frame is drawn.

**AC:** Launching the shell shows cached application list (populated from prior run) in the launcher within 16ms of app startup; validator has not yet run.

---

**REQ-F-004: Fall back to full scan on cache miss or corruption**

If the database file does not exist, is corrupted, or the schema is missing, the system shall perform a full synchronous filesystem scan and rebuild the database from scratch.

**AC:** Deleting `launcher.db` and restarting the launcher triggers a full scan; the database is recreated with all discovered entries.

---

### Async Validator

**REQ-F-005: Start async validator after cache load**

After `LauncherModel::setEntries()` completes with cached entries, the system shall start an asynchronous validator (via `QtConcurrent::run`) that performs a full filesystem scan of all configured directories.

**AC:** Cache load completes in <5ms; validator starts within the same event loop cycle and the main thread remains unblocked during scan.

---

**REQ-F-006: Validator compares file metadata**

The async validator shall compare each discovered `.desktop` file against its cached row using file modification time (`mtime`) and size (`size`). Entries are categorized as: (1) new (path not in cache), (2) changed (mtime or size differs), (3) unchanged (both match), (4) missing (cached but not on disk).

**AC:** A `.desktop` file on disk with mtime=1000 and size=512 stored in cache with the same mtime and size is marked unchanged; the same file with mtime=1001 is marked changed.

---

**REQ-F-007: Validator updates database**

The async validator shall update the database: insert new entries, update changed entries, and delete missing entries; then call `LauncherModel::setEntries()` with the corrected entry list.

**AC:** After validator completes, the model is reset with final entry list; cached entries that no longer exist on disk are removed from the model and database.

---

**REQ-F-008: Validator output triggers full model reset**

The system shall call `LauncherModel::setEntries()` with the complete corrected entry list after the validator completes, accepting a full model reset.

**AC:** Running the validator after a `.desktop` file is deleted from disk updates the model; the deleted entry is no longer visible in the launcher.

---

### Directory Watching

**REQ-F-009: Initialize watcher with startup directories**

At startup, the system shall initialize a `QFileSystemWatcher` with all directories that exist at that moment from the following list:
- `$XDG_DATA_HOME/applications` (default `~/.local/share/applications`)
- All directories from `$XDG_DATA_DIRS` with `/applications` suffix appended
- `/usr/local/share/applications`
- `/usr/share/applications`
- `/var/lib/flatpak/exports/share/applications`
- `~/.local/share/flatpak/exports/share/applications`
- `/var/lib/snapd/desktop/applications`

**AC:** If `~/.local/share/applications` exists at startup, it is added to the watcher; if `/opt/custom/applications` does not exist, it is not added (even if it is in `XDG_DATA_DIRS`).

---

**REQ-F-010: Watch subdirectories discovered during initial scan**

During the initial filesystem scan, the system shall discover and add all subdirectories encountered (via `QDirIterator::Subdirectories`) to the `QFileSystemWatcher` to detect `.desktop` files added to nested paths.

**AC:** If a scan discovers `.desktop` files in `~/.local/share/applications/custom-subdir/`, that subdirectory is added to the watcher and subsequent file changes there trigger the watcher.

---

**REQ-F-011: Debounce watcher signals**

When `directoryChanged` signals fire from the watcher, the system shall debounce for 500ms before triggering a new async validator run, coalescing rapid file changes into a single rescan.

**AC:** Creating three `.desktop` files in rapid succession (within 100ms) in a watched directory triggers only one validator run, not three.

---

**REQ-F-012: Rescan on watcher event**

When a `directoryChanged` event fires (after 500ms debounce), the system shall run the async validator again with a full filesystem scan and update the model with the result.

**AC:** Adding a new `.desktop` file to `~/.local/share/applications/` and saving it causes the launcher to detect and display the new entry within 600ms of file write.

---

**REQ-F-013: Non-existent directories are not watched**

Directories that do not exist at startup (e.g., Snap not installed, custom application folder not created) shall not be added to the watcher and are not re-checked in later startup cycles.

**AC:** `/var/lib/snapd/desktop/applications` is only watched if it exists at launcher startup; creating it later does not trigger the watcher to add it.

---

### Cache Persistence

**REQ-F-014: Write entries to database after validator completes**

The system shall write all validated entries to the `desktop_entries` table in the database: insert rows for new entries, update rows for changed entries (setting new `mtime`, `size`, and `data`), and delete rows for missing entries.

**AC:** After a `.desktop` file's modification time changes on disk and the validator runs, the cached `mtime` value for that entry in the database is updated to match the new file mtime.

---

**REQ-F-015: Store JSON-serialized entry data**

For each `DesktopEntry`, the system shall serialize all parsed fields to JSON format and store in the `data` column: `{name, generic_name, comment, exec, icon, categories, path, desktop_file, terminal, actions}`.

**AC:** A cached entry can be deserialized from its `data` JSON column and reconstructed into a `DesktopEntry` struct identical to the original parsed entry.

---

**REQ-F-016: Maintain database consistency on errors**

If a database write operation fails, the system shall leave the database unchanged and log a warning; the model shall still be updated in memory with the new entries.

**AC:** If `UPDATE desktop_entries` fails due to a locked database, the launcher logs a warning and continues; in-memory entry list is still updated.

---

---

## Non-Functional Requirements

### Performance & Latency

**REQ-NF-001: Cache load completes before first frame**

The system shall load and deserialize all cached entries from the SQLite database and call `setEntries()` in under 16ms, ensuring the cached entry list is visible before the first UI frame is rendered.

**AC:** Measuring time from `LauncherService::start()` to `setEntries()` completion shows cache load ≤ 16ms when the cache contains 500+ entries.

---

**REQ-NF-002: Validator does not block the UI thread**

The async validator shall run on a background thread via `QtConcurrent::run` and must not block the main thread during filesystem scan, JSON serialization, or database updates.

**AC:** While the validator is running, the launcher UI remains responsive to user input; window animations do not stutter.

---

**REQ-NF-003: Debounce window is 500ms**

The system shall wait exactly 500ms between the last `directoryChanged` signal and starting a new validator run.

**AC:** Two `directoryChanged` events 250ms apart trigger one validator run; the same events 600ms apart trigger two separate runs.

---

**REQ-NF-004: Database file location is user-writable**

The system shall store the SQLite database at `$XDG_CACHE_HOME/holonight-shell/launcher.db` (defaulting to `~/.cache/holonight-shell/launcher.db` if `XDG_CACHE_HOME` is unset) in a location always writable by the user.

**AC:** The database file is created and updated with write permissions in the user's cache directory.

---

### Reliability

**REQ-NF-005: Watcher resilience to directory mount changes**

If a watched directory is unmounted or becomes inaccessible, the system shall log a warning and continue; subsequent validator runs shall detect missing entries and update the model accordingly.

**AC:** Unmounting a watched application directory causes the watcher to emit an error signal; the validator's next run sees those entries as missing and removes them from the model.

---

**REQ-NF-006: Concurrent database access**

The system shall serialize database writes via the async validator's completion; no two validator runs shall write to the database simultaneously.

**AC:** Running multiple validator instances (e.g., via rapid watcher events) never produces a "database is locked" crash; writes are serialized by the concurrent task queue.

---

---

## Constraints

### Technology & Dependencies

**REQ-C-001: Qt6::Sql dependency**

The system shall depend on `Qt6::Sql` library for SQLite database operations and is built with CMake target linking to `Qt6::Sql`.

**AC:** `CMakeLists.txt` includes `find_package(Qt6 COMPONENTS Sql)` and the launcher target links `Qt6::Sql`.

---

**REQ-C-002: Use QtConcurrent for async validation**

The system shall use `QtConcurrent::run` to execute the async validator on a thread-pool thread and shall not use raw `QThread` or `std::thread`.

**AC:** The validator runs via `QtConcurrent::run` and the returned `QFuture` is waited on before model reset.

---

### File System Behavior

**REQ-C-003: In-place file edits are not detected by watcher**

The system does not detect in-place byte-level edits to `.desktop` files (e.g., editing the `Name` field in place); it only detects file addition and removal. Package managers that recreate files on update are compatible.

**AC:** Editing a `.desktop` file with `sed -i` or a text editor does not trigger the watcher; the validator will detect the edit only if the file's mtime differs.

---

**REQ-C-004: Only existing directories are watched at startup**

Directories that do not exist when the launcher starts are never added to the watcher, even if they are created later. This is a Stage 1 limitation.

**AC:** If the launcher starts before Snapd is installed, `/var/lib/snapd/desktop/applications` is not watched even after Snapd installation and directory creation.

---

### Model Update Strategy

**REQ-C-005: Accept full model reset on validator completion**

The system shall replace the entire entry list with the validator's result via `setEntries()`, not attempt incremental updates (add/remove/update individual entries).

**AC:** After validator completes, `LauncherModel::setEntries(newList)` is called once with the complete corrected list; no incremental slot calls are made.

---

### Metadata Comparison

**REQ-C-006: Use mtime and size for cache invalidation**

The system shall determine whether a `.desktop` file has changed by comparing its modification timestamp (`mtime`) and file size (`size`) to cached values; it shall not use file content hash or inode number.

**AC:** A `.desktop` file with mtime=1000, size=512 is considered unchanged if the cache entry has the same mtime and size, even if unobserved edits occurred.

---

---

## Design Notes

### Rationale for Full Model Reset

A full model reset is simpler and more reliable than incremental updates. The launcher is not performance-sensitive during updates (validation happens in the background), and a full reset ensures the model is always consistent with the database and filesystem.

### Rationale for Debounce

Package managers and bulk file operations often create or update multiple `.desktop` files in rapid succession. A 500ms debounce coalesces these into a single validator run, reducing CPU and I/O during package installation.

### Rationale for Startup Directory List

The directory list covers:
- User-installed applications (`~/.local/share/applications`, `~/.local/share/flatpak`)
- System applications (`/usr/share/applications`, `/usr/local/share/applications`)
- Flatpak (system and user)
- Snapd
- Custom `XDG_DATA_DIRS` locations

Non-existent directories are skipped to avoid watcher errors; directories created after startup are not added (Stage 1 limitation).

### Rationale for Mtime+Size Comparison

Using mtime and size is fast (no file read) and sufficient for detecting real edits. It avoids the overhead of cryptographic hashing and is compatible with all file systems. In-place byte edits are rare in practice (package managers recreate files).

---

## Verification Strategy

1. **Cache Load Latency**: Measure time from `start()` call to `setEntries()` completion; confirm ≤ 16ms.
2. **Validator Background Execution**: Monitor main thread during validator run; confirm no stutter in UI animations.
3. **Watcher Debounce**: Create/delete 3 `.desktop` files within 100ms; confirm only 1 validator run.
4. **Full Model Reset**: Delete a `.desktop` file; trigger watcher; confirm old entry is removed from model.
5. **Cache Persistence**: Restart launcher; confirm all previously cached entries are loaded on startup.
6. **Directory Watch Coverage**: Verify watcher includes all existing dirs from startup list and all discovered subdirs.
7. **Missing Directory Handling**: Confirm non-existent dirs are silently skipped (no errors, not watched).
8. **Database Corruption Recovery**: Delete database; restart; confirm full scan runs and rebuilds database.

---

## Acceptance Criteria Summary

| Requirement | AC |
|---|---|
| REQ-F-001 | Database file created with `desktop_entries` table on first run |
| REQ-F-002 | JSON-serialized entry in `data` column round-trips to original `DesktopEntry` |
| REQ-F-003 | Cached entries visible before first frame (<16ms) |
| REQ-F-004 | Deleted database triggers full scan and rebuild |
| REQ-F-005 | Validator runs on background thread; main thread unblocked |
| REQ-F-006 | Entries marked new/changed/unchanged/missing by mtime+size |
| REQ-F-007 | Database updated after validator (inserts, updates, deletes) |
| REQ-F-008 | Model reset with final entry list after validator |
| REQ-F-009 | Only existing dirs at startup are added to watcher |
| REQ-F-010 | Subdirs discovered during scan are added to watcher |
| REQ-F-011 | 3 rapid file changes coalesce into 1 validator run |
| REQ-F-012 | New `.desktop` file detected within 600ms of write |
| REQ-F-013 | Non-existent dirs not watched; not re-checked later |
| REQ-F-014 | Cached mtime updated after validator completes |
| REQ-F-015 | Cached entry deserialized from `data` JSON |
| REQ-F-016 | Database write error leaves DB unchanged; in-memory list updated |
| REQ-NF-001 | Cache load ≤ 16ms for 500+ entries |
| REQ-NF-002 | UI responsive during validator (no stutter) |
| REQ-NF-003 | 250ms apart triggers 1 run; 600ms apart triggers 2 runs |
| REQ-NF-004 | Database written to `$XDG_CACHE_HOME/holonight-shell/launcher.db` |
| REQ-NF-005 | Unmounted dir detected; entries marked missing; model updated |
| REQ-NF-006 | Multiple validator runs never deadlock on DB |
| REQ-C-001 | `CMakeLists.txt` links `Qt6::Sql` |
| REQ-C-002 | Validator uses `QtConcurrent::run` |
| REQ-C-003 | In-place edits not detected; file recreation detects changes |
| REQ-C-004 | Post-startup directories never added to watcher |
| REQ-C-005 | Single `setEntries()` call per validator result |
| REQ-C-006 | mtime+size comparison determines cache validity |

---

## Related Documentation

- [Weather Icon Compositor](../weather-icon-compositor/SPEC.md)
- [Right Sidebar Architecture](../../../CLAUDE.md#right-sidebar-architecture)
