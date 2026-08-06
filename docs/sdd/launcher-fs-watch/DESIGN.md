# launcher-fs-watch DESIGN

## Overview

This feature adds two synchronized reactive pipelines so that the MIME defaults panel in the
right sidebar stays current without a shell restart. The first pipeline adds a new
`entriesUpdated` signal to `LauncherService` that fires after every background validator run
completes and the model is refreshed; `MimeService` connects to that signal and re-queries all
six role defaults so any newly-installed application appears in the dropdowns immediately. The
second pipeline gives `MimeService` its own `QFileSystemWatcher` that monitors the user's MIME
preference files (`mimeapps.list` at two XDG locations) and the `mimeinfo.cache` file in each
application directory; any write to those files triggers a 500 ms debounced re-query, keeping
the UI consistent with system-level default changes made outside the shell.

---

## Component Changes

### `src/services/launcher/LauncherService.h` and `.cpp`

**What changes:**

Add `entriesUpdated` to the `Q_SIGNALS` block:

```cpp
// existing signals …
void launched();
void entriesUpdated();   // NEW — emitted after model_ is refreshed
```

**Where it is emitted:**

Inside the `QFutureWatcher<ScanResult>::finished` lambda in `runValidator()`, immediately after
`model_.setEntries(result.entries)` and before the `validator_rerun_pending_` re-schedule check:

```cpp
model_.setEntries(result.entries);
emit entriesUpdated();   // NEW
// … existing: update fs_watcher_ paths, check validator_rerun_pending_ …
```

**Why here and not elsewhere:** `runValidator()` is the sole code path that mutates the live
model from an async scan. Emitting at this point guarantees the signal carries a consistent
model state — any consumer that calls back into `LauncherService` will see the new entries.
The synchronous `model_.setEntries` call in `start()` (cache-miss / cold-start path) does NOT
need to emit; `MimeService` issues its own `refreshAllRoles` call from `QTimer::singleShot(0)`
in its own constructor, so the initial state is already covered.

**No other changes** to `LauncherService`. The existing `QFileSystemWatcher` + 500 ms debounce
that drives directory watching and calls `runValidator()` is untouched.

---

### `src/services/mime/MimeService.h` and `.cpp`

**What changes (header):**

1. Add includes for `QFileSystemWatcher`, `QTimer`, `QStringList`.
2. Add a new constructor overload:

```cpp
// Existing:
explicit MimeService(QObject* parent = nullptr);
explicit MimeService(std::unique_ptr<IMimeResolver> resolver, QObject* parent = nullptr);

// New — accepts XDG application dirs to build mimeinfo.cache watch paths:
explicit MimeService(QStringList app_dirs, QObject* parent = nullptr);
MimeService(QStringList app_dirs, std::unique_ptr<IMimeResolver> resolver,
            QObject* parent = nullptr);  // full injection constructor for tests
```

3. Promote `refreshAllRoles()` from `private` to `public Q_SLOTS` so `ShellApplication` can
   connect `LauncherService::entriesUpdated` to it via `QObject::connect`.

4. Add two value members (not pointers — no heap allocation, owned inline):

```cpp
QFileSystemWatcher mime_file_watcher_;
QTimer             mime_debounce_timer_;
```

**What changes (implementation):**

The two new constructors delegate to the full-injection constructor. The minimal `(parent)`
constructor passes an empty `app_dirs` list (no `mimeinfo.cache` watching — acceptable for the
zero-arg/test path that currently exists). The `(app_dirs, parent)` production path creates a
`ProcessMimeResolver` and forwards to the full constructor.

In the full constructor body, after the existing `QTimer::singleShot(0, this, &MimeService::refreshAllRoles)`:

```cpp
// Build watch-file list.
QStringList watch_files;
watch_files << QDir::homePath() + QStringLiteral("/.config/mimeapps.list");
watch_files << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
               + QStringLiteral("/applications/mimeapps.list");
for (const QString& dir : app_dirs) {
    const QString cache = dir + QStringLiteral("/mimeinfo.cache");
    if (QFileInfo::exists(cache)) {
        watch_files << cache;
    }
}

// Only add files that currently exist (REQ-C-001).
for (const QString& path : watch_files) {
    if (QFileInfo::exists(path)) {
        mime_file_watcher_.addPath(path);
    }
}

mime_debounce_timer_.setSingleShot(true);
mime_debounce_timer_.setInterval(500);

connect(&mime_file_watcher_, &QFileSystemWatcher::fileChanged,
        &mime_debounce_timer_, qOverload<>(&QTimer::start));
connect(&mime_debounce_timer_, &QTimer::timeout,
        this, &MimeService::refreshAllRoles);
```

`QFileSystemWatcher::fileChanged` (not `directoryChanged`) is used because the watcher targets
specific files, not directories. On some inotify implementations a file-level watch is also
more reliable for atomic-write editors that rename a temp file over the target (the watch must
be re-added after `fileChanged` fires; see Known Risks).

---

### `src/app/ShellApplication.cpp`

**Constructor change:**

Replace:

```cpp
mime_service_(new MimeService(this)),
```

With:

```cpp
mime_service_(new MimeService(DesktopEntryScanner::defaultApplicationDirs(), this)),
```

`DesktopEntryScanner::defaultApplicationDirs()` is already `static` and returns the same set
of existing XDG application directories that `launcher_`'s internal `DesktopEntryScanner`
uses. Calling the static method avoids exposing `LauncherService::scanner_` and keeps the
construction order dependency-free (both services are constructed in the initializer list, so
one cannot call the other's methods yet).

**`startServices()` change:**

Add one `connect` after `launcher_->start()`:

```cpp
launcher_->start();
connect(launcher_, &LauncherService::entriesUpdated,
        mime_service_,  &MimeService::refreshAllRoles);
```

Placing this after `launcher_->start()` — not in the constructor — is correct because
`launcher_->start()` creates the `fs_watcher_` and fires the first `runValidator()` run; the
connection must be live before any subsequent `entriesUpdated` fires. The initial
`MimeService::refreshAllRoles` call is already scheduled from within the `MimeService`
constructor via `QTimer::singleShot(0)`, so no double-call occurs at startup.

---

## Data Flow

### Flow A: Application install / removal

```
[package manager writes to /usr/share/applications/]
        │
        ▼
LauncherService::fs_watcher_  (directoryChanged)
        │
        ▼  debounce_timer_ (500 ms, resets on each event)
        │
        ▼
LauncherService::runValidator()
        │  QtConcurrent::run → validateAgainstCache()
        ▼
QFutureWatcher<ScanResult>::finished
        │
        ├─► model_.setEntries(result.entries)   ← LauncherModel updated
        │
        └─► emit entriesUpdated()               ← NEW
                    │
                    ▼
            MimeService::refreshAllRoles()      ← slot (public)
                    │
                    ▼  queryMimeAsync() × 6 roles → xdg-mime subprocesses
                    │
                    ▼
            onQueryResult() → emitChangedSignals()
                    │
                    ▼
            QML bindings update (defaultBrowser, defaultTerminal, …)
```

### Flow B: MIME default change (`mimeapps.list` edited / xdg-mime default called)

```
[xdg-mime default firefox.desktop text/html]
        │  writes ~/.config/mimeapps.list
        ▼
MimeService::mime_file_watcher_  (fileChanged)
        │
        ▼  mime_debounce_timer_ (500 ms, resets on each event)
        │
        ▼
MimeService::refreshAllRoles()
        │  queryMimeAsync() × 6 roles → xdg-mime subprocesses
        ▼
onQueryResult() → emitChangedSignals()
        │
        ▼
QML bindings update
```

Note: Flow B is entirely self-contained inside `MimeService`. `LauncherService` is not
involved and its model is unaffected.

---

## Interface Changes

### New signal — `LauncherService`

```cpp
// LauncherService.h — Q_SIGNALS block
void entriesUpdated();
```

Emitted once per completed `runValidator()` cycle, after `model_.setEntries`. No arguments —
consumers that need entry data call into `LauncherService` directly.

### Constructor signature change — `MimeService`

```cpp
// New overloads (existing two are unchanged):
explicit MimeService(QStringList app_dirs, QObject* parent = nullptr);
MimeService(QStringList app_dirs, std::unique_ptr<IMimeResolver> resolver,
            QObject* parent = nullptr);
```

The zero-arg and resolver-only constructors remain for backward compatibility. Tests that
construct `MimeService` with a `NullMimeResolver` continue to compile without modification;
they simply get no file watching (empty `app_dirs`).

### Visibility change — `MimeService::refreshAllRoles`

Moved from `private:` to `public Q_SLOTS:`. This is a minimal promotion: the method's
semantics are unchanged; it becomes visible to `QObject::connect` in `ShellApplication` and
to any future unit test that wants to trigger a refresh programmatically.

### No new public methods or properties

The QML API surface of both services is unchanged. No new `Q_PROPERTY` declarations. The six
role properties and their `NOTIFY` signals are identical before and after this feature.

---

## Key Decisions with Rationale

### 1. `entriesUpdated` emitted in `runValidator()::finished` lambda, not in `model_.setEntries`

`LauncherModel::setEntries` is a model method with no knowledge of the validation pipeline; it
could be called from multiple code paths (startup cache load, cold-start fallback scan, async
validator). Emitting a service-level signal from a model method would violate the
unidirectional dependency (`LauncherService` owns `LauncherModel`, not the reverse). Placing
the emit in the `QFutureWatcher::finished` lambda is the only site where the caller knows that
a full async cycle has completed and the model is stable. The cold-start path in `start()` does
not emit because `MimeService` schedules its own initial refresh via `QTimer::singleShot(0)`.

### 2. Constructor injection of `QStringList app_dirs` rather than `const DesktopEntryScanner&`

Option A (`QStringList`) was chosen over Option B (`const DesktopEntryScanner&`) for three
reasons:

- **No header dependency**: `MimeService.h` does not need to include `DesktopEntryScanner.h`,
  keeping the services in separate header dependency trees.
- **No lifetime coupling**: passing a `const&` to a scanner owned by `LauncherService` would
  require that `LauncherService` outlive `MimeService`, an implicit constraint invisible at the
  call site. A copied `QStringList` is unconditionally safe.
- **REQ-NF-004 compliance**: the watch list must be populated at construction. Passing the dirs
  as a constructor argument makes this explicit and testable; a post-construction signal
  approach (Option C) cannot satisfy the requirement without extra machinery.

Option C (connect via signal after construction) was explicitly rejected by REQ-NF-004.

### 3. `QFileSystemWatcher::fileChanged` (not `directoryChanged`) for the MIME watcher

`LauncherService` already uses `directoryChanged` on the application directories because it
cares about any file appearing or disappearing inside them. `MimeService` cares about the
content of exactly three to five specific files. Watching directories for MIME changes would
require path filtering on every event to identify which file changed. `fileChanged` delivers
events only for the watched files, which is both more precise and avoids spurious re-queries
when unrelated files change in the same directory. The cost is that on atomic-write editors
(those that rename a temp file over the target), the kernel removes the inotify watch on the
replaced inode; this is handled via the re-watch strategy described in Known Risks.

### 4. Value members `QFileSystemWatcher mime_file_watcher_` and `QTimer mime_debounce_timer_`

`LauncherService` uses pointer members for its watcher and timer because they are created in
`start()`, not the constructor. `MimeService`'s watcher and timer are created in the
constructor (REQ-NF-004), so heap allocation via `new` is unnecessary. Value members express
this ownership unambiguously and eliminate one allocation per `MimeService` instance. The
trade-off is a slightly larger sizeof `MimeService`, which is acceptable for a singleton.

### 5. Signal-slot connection placed in `ShellApplication::startServices`, not in `MimeService`

Connecting `LauncherService::entriesUpdated` to `MimeService::refreshAllRoles` inside
`MimeService` would create a compile-time dependency from `MimeService` on `LauncherService`.
The services are peers: `MimeService` provides MIME resolution, `LauncherService` provides
desktop entries; neither is a logical owner of the other. Wiring them in `ShellApplication`
keeps both services free of mutual includes, preserves testability (each can be tested without
the other), and follows the existing pattern in `ShellApplication` where cross-service
connections are expressed at the application layer (e.g. `IdleService` → `WeatherService`,
`SidebarManager` → `NotificationService`).

---

## Alternatives Considered

### A. Poll `mimeapps.list` on a fixed interval instead of inotify

A `QTimer` polling approach (e.g. every 5 s) would require no `QFileSystemWatcher` and would
not have the inotify atomic-write edge case. It was rejected because: (a) REQ-NF-001 mandates
≤500 ms latency, which a 5 s poll cannot meet; (b) a sub-second poll wastes CPU on every
running shell instance even when no change has occurred; (c) inotify is already used by
`LauncherService`, so it is a well-understood pattern in this codebase.

### B. Have `MimeService` subscribe to `LauncherService` internally (include `LauncherService.h`)

This would avoid the `ShellApplication` wiring line, at the cost of a direct dependency from
`MimeService` to `LauncherService`. Beyond the coupling problem, it would prevent unit-testing
`MimeService` in isolation (the constructor would require a `LauncherService` instance). The
cross-service connect in `ShellApplication` is three lines and is consistent with every other
cross-service wiring in the file.

### C. Emit `entriesUpdated` from `LauncherModel::setEntries` (model-layer signal)

`LauncherModel` is a view-layer model; adding a service-semantics signal to it would blur
the model/service boundary. `LauncherService` owns the model and is the correct level of
abstraction for "the app list has changed."

### D. Re-use LauncherService's `QFileSystemWatcher` for MIME files

`LauncherService::fs_watcher_` watches directories (via `directoryChanged`). Adding file paths
to it would mix two concerns and cause every directory event — including unrelated `.desktop`
file changes — to re-trigger `MimeService::refreshAllRoles`. Separate watchers keep the
debounce timers independent and the signal semantics clean.

### E. Watch `~/.local/share/applications/` directory instead of `mimeapps.list` file

Some desktop environments write `mimeapps.list` atomically (rename-over). Watching the
containing directory for `directoryChanged` would catch the rename. However it would also
fire on every `.desktop` file install into `~/.local/share/applications/`, which is already
handled by the LauncherService flow (Flow A). Watching the specific file avoids double-
triggering `refreshAllRoles` on app installs, even though re-watch after atomic rename is
needed (see Known Risks).

---

## Known Risks / Edge Cases

### 1. inotify watch lost after atomic file replacement

Some editors (and `xdg-mime default` itself on certain distro patches) write MIME config files
by writing a `.tmp` file then calling `rename(2)`, which replaces the inode. `QFileSystemWatcher`
drops the watch when the original inode is unlinked. Mitigation: in the `fileChanged` slot,
re-add the path unconditionally before restarting the debounce timer:

```cpp
connect(&mime_file_watcher_, &QFileSystemWatcher::fileChanged,
        this, [this](const QString& path) {
    // Re-add in case of atomic rename replacing the inode.
    if (QFileInfo::exists(path) && !mime_file_watcher_.files().contains(path)) {
        mime_file_watcher_.addPath(path);
    }
    mime_debounce_timer_.start();
});
```

This is a known Qt / inotify limitation documented in `QFileSystemWatcher::fileChanged`.

### 2. `mimeapps.list` does not exist at startup

REQ-C-001 says not to watch non-existent paths. If the user has never configured a default
application, `~/.config/mimeapps.list` may not exist. The constructor skips it; the MIME watch
for that file is simply absent. If the file is created later (e.g. the user sets a default
from another application), the watcher will not auto-add it. Mitigation: the LauncherService
flow (Flow A) covers this indirectly — a new app installation updates the launcher model and
emits `entriesUpdated`, which calls `refreshAllRoles` once. For a pure preference change in a
freshly-created `mimeapps.list`, the user must wait for the next app-install event or restart
the shell. Adding directory watching for `~/.config/` to detect file creation was evaluated but
rejected (REQ-C-004 / noise); this edge case is acceptable for the initial implementation.

### 3. `refreshAllRoles` is not re-entrant

`MimeService` already guards against concurrent queries via the `pending_` set in
`queryMimeAsync()`. If both `LauncherService::entriesUpdated` and `mime_debounce_timer_::timeout`
fire within the same event loop turn (unlikely but possible on a slow system where an app
install also rewrites `mimeapps.list`), the second `refreshAllRoles()` call will find all MIME
types already in `pending_` and no-op. This is the correct behavior and requires no additional
guard.

### 4. `DesktopEntryScanner::defaultApplicationDirs()` vs. runtime-added dirs

`LauncherService` adds newly-discovered directories to its `fs_watcher_` as scans run
(`result.watched_dirs`). `MimeService` is initialized with `defaultApplicationDirs()` at
construction and does not observe these later additions. New `mimeinfo.cache` files in
late-discovered dirs will not be watched. In practice `defaultApplicationDirs()` covers all
standard XDG data directories; runtime-added paths arise only when a custom XDG_DATA_DIRS
entry is added after shell startup, which is rare. If this becomes a problem, `MimeService`
can expose a `addApplicationDir(const QString&)` slot for `ShellApplication` to call from a
future `LauncherService::watchedDirsChanged` signal.

### 5. Test constructor backward compatibility

The zero-arg `MimeService(QObject*)` and single-resolver `MimeService(resolver, QObject*)`
constructors pass an empty `app_dirs` list to the full constructor. They therefore set up
`mime_file_watcher_` and `mime_debounce_timer_` with no paths watched. Existing tests that
use `NullMimeResolver` continue to compile and run without modification; the file-watch codepath
is simply inert in the test environment, which is the desired behavior (no real filesystem
access in unit tests).
