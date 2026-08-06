# mime-desktop-integration — Architecture Design

## 1. Overview

This feature adds MIME type awareness to the shell in three layers:

1. **Data layer** — `DesktopEntry` gains a `mime_types` field populated by the scanner from each
   `.desktop` file's `MimeType=` line. The field is serialized into the existing JSON blob in the
   launcher SQLite cache; no schema change is required.

2. **Service layer** — `MimeService` (new QML singleton) queries and sets system defaults via
   `xdg-mime` subprocesses, with the browser role using `xdg-settings default-web-browser` so it
   matches browser self-check behavior. `KdeCompatService` (new QML singleton) detects a common
   KDE misconfiguration (`kbuildsycoca6` present but `XDG_MENU_PREFIX` unset) and provides an
   on-demand cache rebuild action.

3. **Presentation layer** — `SidebarSystem.qml` (stub replacement) renders six role-based
   application selectors. Each selector filters the existing in-memory launcher model and binds to
   `MimeService` properties for its initial value and mutation.

Both new services live in `holonight_services`, following every prior singleton (portal, brightness,
idle). No new Wayland protocol, no new D-Bus service, no new SQLite table.

---

## 2. Components

### 2.1 DesktopEntry struct extension

**File**: `src/services/launcher/DesktopEntryScanner.h`

Add one field to the existing `DesktopEntry` struct:

```cpp
struct DesktopEntry {
  QString name;
  QString generic_name;
  QString comment;
  QString exec;
  QString icon;
  QString categories;
  QString path;
  QString desktop_file;
  bool terminal{false};
  QVector<DesktopAction> actions;
  QStringList mime_types;   // ← new field; empty when MimeType= absent
};
```

The field is populated only by `DesktopEntryScanner::parseDesktopEntryFile()` and
`DesktopEntrySerializer::fromJson()`. No other code path writes to it after construction
(satisfying REQ-C-004).

---

### 2.2 DesktopEntryScanner extension

**File**: `src/services/launcher/DesktopEntryScanner.cpp`

Extend the anonymous-namespace helper `applyDesktopEntryField()` with a new `else if` branch:

```cpp
} else if (key == QStringLiteral("MimeType")) {
    // value has already been passed through unescapeDesktopString() which trims the whole
    // string; individual semicolon-delimited parts may still carry leading/trailing whitespace
    // if someone mis-formats the line.
    const QStringList parts = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            state->parsed.mime_types.append(trimmed);
        }
    }
}
```

`Qt::SkipEmptyParts` discards the empty token produced by a trailing `;` (the common form in
`.desktop` files) and double semicolons, satisfying REQ-NF-003.

No changes to `parseDesktopEntryFile()` itself — only the field dispatcher needs updating.

---

### 2.3 DesktopEntrySerializer extension

**File**: `src/services/launcher/DesktopEntrySerializer.h`

Both `toJson()` and `fromJson()` are inline functions in this header. Add `mime_types` round-trip:

**`toJson()` addition** (after the `actions_array` block):

```cpp
QJsonArray mime_types_array;
mime_types_array.reserve(entry.mime_types.size());
for (const QString& mime : entry.mime_types) {
    mime_types_array.append(mime);
}
obj[QStringLiteral("mime_types")] = mime_types_array;
```

**`fromJson()` addition** (after the `actions` block):

```cpp
const QJsonArray mime_types_array = obj[QStringLiteral("mime_types")].toArray();
entry.mime_types.reserve(static_cast<int>(mime_types_array.size()));
for (const auto& val : mime_types_array) {
    const QString mime = val.toString().trimmed();
    if (!mime.isEmpty()) {
        entry.mime_types.append(mime);
    }
}
```

When the `"mime_types"` key is absent (rows cached before this upgrade), `.toArray()` returns an
empty `QJsonArray`, leaving `entry.mime_types` as an empty `QStringList`. The existing
`fromJson()` validation gates on `name`, `exec`, and `desktop_file` only — it does not reject
entries with no MIME types.

---

### 2.4 DesktopEntryCache extension

**File**: `src/services/launcher/DesktopEntryCache.cpp` (read-only concern)

No schema change is required. The existing `data TEXT NOT NULL` column stores a JSON blob via
`DesktopEntrySerializer::toJson()`. Adding `"mime_types"` to that blob is additive: `upsert()`
calls `DesktopEntrySerializer::toJson()` (which now includes the field), and `loadAll()` calls
`DesktopEntrySerializer::fromJson()` (which now reads it). Old rows missing the key produce an
empty list and self-heal on next mtime/size-triggered re-scan.

The `kSchema` constant and all SQL queries remain unchanged.

---

### 2.5 LauncherService extension

**File**: `src/services/launcher/LauncherService.h/.cpp`

Add one Q_INVOKABLE for role-selector population:

```cpp
// Returns a QVariantList of QVariantMap, each with keys "name", "icon", "desktopFile".
// Iterates all loaded entries (not the current search-filtered results) and returns those
// whose mime_types intersect with the given list. No I/O, no DB queries.
Q_INVOKABLE QVariantList entriesForMimeTypes(const QStringList& mime_types) const;
```

Implementation iterates `model_.allEntriesCount()` / `model_.allEntryAt()` and builds the result
with a `QSet<QString>` constructed from `mime_types` for O(1) intersection testing:

```cpp
QVariantList LauncherService::entriesForMimeTypes(const QStringList& mime_types) const {
    const QSet<QString> target(mime_types.begin(), mime_types.end());
    QVariantList result;
    const int count = model_.allEntriesCount();
    for (int idx = 0; idx < count; ++idx) {
        const DesktopEntry* entry = model_.allEntryAt(idx);
        if (entry == nullptr) { continue; }
        if (std::ranges::any_of(entry->mime_types,
                                [&target](const QString& m) { return target.contains(m); })) {
            result.append(QVariantMap{
                {QStringLiteral("name"),        entry->name},
                {QStringLiteral("icon"),        entry->icon},
                {QStringLiteral("desktopFile"), entry->desktop_file},
            });
        }
    }
    return result;
}
```

Additionally, add `MimeTypesRole` to `LauncherModel::Role` and implement it in `data()` and
`roleNames()` so the field is accessible via the model's existing delegate API, even if the
combobox population path does not use it directly.

---

### 2.6 MimeService

**Files**:
```
src/services/mime/
    MimeService.h      # Class declaration, Q_PROPERTYs, Q_INVOKABLEs
    MimeService.cpp    # All implementation; #include "MimeService.moc"
```

#### Class declaration summary

```cpp
class MimeService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(QString defaultBrowser     READ defaultBrowser     NOTIFY defaultBrowserChanged     FINAL)
  Q_PROPERTY(QString defaultTerminal    READ defaultTerminal    NOTIFY defaultTerminalChanged    FINAL)
  Q_PROPERTY(QString defaultFileManager READ defaultFileManager NOTIFY defaultFileManagerChanged FINAL)
  Q_PROPERTY(QString defaultImageViewer READ defaultImageViewer NOTIFY defaultImageViewerChanged FINAL)
  Q_PROPERTY(QString defaultTextEditor  READ defaultTextEditor  NOTIFY defaultTextEditorChanged  FINAL)
  Q_PROPERTY(QString defaultVideoPlayer READ defaultVideoPlayer NOTIFY defaultVideoPlayerChanged FINAL)

 public:
  explicit MimeService(QObject* parent = nullptr);
  ~MimeService() override;

  [[nodiscard]] QString defaultBrowser()     const;
  [[nodiscard]] QString defaultTerminal()    const;
  [[nodiscard]] QString defaultFileManager() const;
  [[nodiscard]] QString defaultImageViewer() const;
  [[nodiscard]] QString defaultTextEditor()  const;
  [[nodiscard]] QString defaultVideoPlayer() const;

  Q_INVOKABLE void setDefaultBrowser(const QString& desktop_file);
  Q_INVOKABLE void setDefaultTerminal(const QString& desktop_file);
  Q_INVOKABLE void setDefaultFileManager(const QString& desktop_file);
  Q_INVOKABLE void setDefaultImageViewer(const QString& desktop_file);
  Q_INVOKABLE void setDefaultTextEditor(const QString& desktop_file);
  Q_INVOKABLE void setDefaultVideoPlayer(const QString& desktop_file);

 Q_SIGNALS:
  void defaultBrowserChanged();
  void defaultTerminalChanged();
  void defaultFileManagerChanged();
  void defaultImageViewerChanged();
  void defaultTextEditorChanged();
  void defaultVideoPlayerChanged();
  ...
};
```

#### Hardcoded role MIME mappings

Defined as `static const QStringList` in `MimeService.cpp` (not exposed to QML or header
consumers, satisfying REQ-C-003):

| Role | MIME types |
|---|---|
| terminal | `application/x-terminal-emulator` |
| file-manager | `inode/directory` |
| image-viewer | `image/jpeg`, `image/png`, `image/gif`, `image/webp` |
| text-editor | `text/plain` |
| video-player | `video/mp4`, `video/x-matroska`, `video/webm` |

The browser row still filters candidates with `text/html`, `x-scheme-handler/http`, and
`x-scheme-handler/https`, but its default value is resolved through `xdg-settings
default-web-browser` rather than by taking the first non-empty browser MIME association.

#### In-memory cache and resolution

```cpp
QHash<QString, QString> mime_cache_;   // mimeType → desktopFile (empty = not yet known or unset)
QString                 browser_default_;
bool                    browser_pending_;
QSet<QString>           pending_;      // in-flight mime types (dedup guard)
```

`resolveRole(const QStringList& role_mimes)` iterates the role's MIME list in order, returning
the first `mime_cache_` entry with a non-empty value for non-browser roles (REQ-F-005). If all are
empty or absent, returns `QString{}`. Browser resolution uses `xdg-settings get
default-web-browser`, then only stores that desktop file in `browser_default_` when `xdg-settings
check default-web-browser <desktop-file>` returns `yes`.

#### Subprocess management

Each query spawns a `QProcess` (not `DetachedCommandRunner`, which is fire-and-forget with no
stdout capture). The process is heap-allocated and parented to `this` so it is cleaned up when
`MimeService` is destroyed.

```
spawn QProcess("xdg-mime", {"query", "default", mime_type})
  → start 5-second QTimer kill guard parented to the QProcess
  → QProcess::finished → onQueryFinished(mime_type, process)
  → QProcess::errorOccurred → log qCWarning, remove from pending_, deleteLater
```

`onQueryFinished()`:
1. Read `process->readAllStandardOutput().trimmed()` → `desktop_file`
2. Stop kill-guard timer
3. `process->deleteLater()`
4. Remove `mime_type` from `pending_`
5. Update `mime_cache_[mime_type]`
6. Re-evaluate all six role properties; for each role whose value changed, emit NOTIFY signal

**Set path** (`setDefaultAsync`): for each MIME in the role list, spawn
`QProcess("xdg-mime", {"default", desktop_file, mime_type})`. On success (exit code 0),
invalidate `mime_cache_[mime_type]` and re-query it asynchronously. On failure, log
`qCWarning` and leave the cache untouched (REQ-C-006).

**Browser set path**: `setDefaultBrowser()` spawns `QProcess("xdg-settings", {"set",
"default-web-browser", desktop_file})`. On success it re-runs the browser `get`/`check` sequence.
This intentionally delegates to xdg-utils because its `check default-web-browser` command is what
Chromium-based browser self-checks align with; `get default-web-browser` alone can report a desktop
file while the underlying MIME/scheme associations are only partially configured.

#### Subprocess timeout

Each `QProcess` gets a dedicated `QTimer` (interval 5000 ms, single-shot, parented to the
process). When the timer fires it calls `QProcess::kill()`. The `finished` handler that fires
after kill logs a warning, removes the MIME type from `pending_`, and leaves `mime_cache_` with
whatever value was there before (empty string on first query).

#### Test seam

Production constructor creates real subprocesses. A second constructor accepts an injected
backend abstraction:

```cpp
class IMimeResolver {
 public:
  virtual ~IMimeResolver() = default;
  virtual void queryDefault(const QString& mime_type,
                            std::function<void(QString)> callback) = 0;
  virtual void setDefault(const QString& desktop_file,
                          const QString& mime_type,
                          std::function<void(bool)> callback) = 0;
};

explicit MimeService(std::unique_ptr<IMimeResolver> resolver, QObject* parent = nullptr);
```

`NullMimeResolver` in `MimeService.cpp` holds a configurable
`QHash<QString, QString>` of canned answers. Unit tests inject it to exercise role resolution,
cache invalidation, and signal emission without spawning subprocesses.

---

### 2.7 KdeCompatService

**Files**:
```
src/services/kde-compat/
    KdeCompatService.h     # Class declaration
    KdeCompatService.cpp   # Implementation; #include "KdeCompatService.moc"
```

#### Class declaration summary

```cpp
class KdeCompatService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(bool kdeWarningActive READ kdeWarningActive NOTIFY kdeWarningActiveChanged FINAL)
  Q_PROPERTY(bool rebuildInProgress READ rebuildInProgress NOTIFY rebuildInProgressChanged FINAL)

 public:
  explicit KdeCompatService(QObject* parent = nullptr);
  ~KdeCompatService() override;

  [[nodiscard]] bool kdeWarningActive()  const { return kde_warning_active_; }
  [[nodiscard]] bool rebuildInProgress() const { return rebuild_in_progress_; }

  Q_INVOKABLE void recheckDiagnostics();
  Q_INVOKABLE void rebuildCaches();

 Q_SIGNALS:
  void warningEmitted();
  void kdeWarningActiveChanged();
  void rebuildInProgressChanged();
  void rebuildFinished(bool success);
  ...
};
```

#### Initialization

```
KdeCompatService ctor
    └─ kbuildsycoca_present_ = !QStandardPaths::findExecutable("kbuildsycoca6").isEmpty()
    └─ recheckDiagnostics()   // may emit warningEmitted() synchronously
```

`recheckDiagnostics()`:
```
if (!kbuildsycoca_present_): setKdeWarningActive(false); return
missing_prefix = qgetenv("XDG_MENU_PREFIX").trimmed().isEmpty()
if (missing_prefix): emit warningEmitted()
setKdeWarningActive(missing_prefix)
```

This is a read-only environment check. No subprocess is spawned and no env variable is mutated
(REQ-C-002).

#### Cache rebuild sequence

`rebuildCaches()` is a no-op when `!kbuildsycoca_present_` or `rebuild_in_progress_` is true.
Otherwise:

```
1. setRebuildInProgress(true)
2. Spawn QProcess("update-desktop-database")
3. On finished → log exit code
4. Spawn QProcess("kbuildsycoca6", {"--noincremental"})
5. On finished → log exit code → setRebuildInProgress(false)
            → emit rebuildFinished(exit_code == 0)
            → recheckDiagnostics()
```

Steps 2 and 4 are strictly sequential: `kbuildsycoca6` reads the database rebuilt by
`update-desktop-database`, so launching them in parallel would produce a race. The signal
`rebuildFinished(bool success)` lets QML display a brief status indicator.

---

### 2.8 SidebarSystem.qml

**File**: `src/qml/RightSidebar/SidebarSystem.qml` — replace the placeholder stub.

Top-level layout mirrors `SidebarQuickSettings.qml`: `Item` root with `preferredWidth`/
`preferredHeight` properties and a `ColumnLayout` anchored with 16 px margins.

Structure:

```
Item (root)
  ColumnLayout (contentColumn)
    Text "Default Applications"          // section header
    DefaultAppRow (browser)
    DefaultAppRow (terminal)
    DefaultAppRow (file-manager)
    DefaultAppRow (image-viewer)
    DefaultAppRow (text-editor)
    DefaultAppRow (video-player)
    ContentSeparator { visible: KdeCompatService.kdeWarningActive }
    KdeCompatRow { visible: KdeCompatService.kdeWarningActive }
```

`KdeCompatRow` is an inline `Row` (not a separate file) containing a warning icon, descriptive
`Text`, and a `Controls.Button` labeled "Rebuild caches". The button calls
`KdeCompatService.rebuildCaches()` and disables itself while `KdeCompatService.rebuildInProgress`
is true (REQ-F-018, REQ-F-019).

All colors use `HoloniightPalette` tokens (note double-i). No hardcoded hex values.

---

### 2.9 DefaultAppRow.qml

**File**: `src/qml/RightSidebar/DefaultAppRow.qml` (new)

A reusable row component for a single role selector.

```qml
Item {
    id: root

    required property string label             // human name: "Browser", "Terminal", etc.
    required property var    mimeTypesForFilter // QStringList of role MIMEs for candidate filter
    required property string currentDefault    // desktop filename from MimeService.<role>

    signal defaultChanged(string desktopFile)

    // Internal model populated once when the launcher model is ready.
    property var candidates: []

    Component.onCompleted: {
        candidates = LauncherService.entriesForMimeTypes(mimeTypesForFilter)
        // Locate the currentDefault in candidates to pre-select ComboBox index.
    }

    RowLayout {
        Text { text: root.label; color: HoloniightPalette.onSurface; ... }
        Controls.ComboBox {
            model: root.candidates
            textRole: "name"
            // currentIndex: index of entry whose "desktopFile" matches root.currentDefault
            onActivated: root.defaultChanged(root.candidates[currentIndex]["desktopFile"])
        }
    }
}
```

`candidates` is populated once in `Component.onCompleted` using
`LauncherService.entriesForMimeTypes()`. Because this call is a pure in-memory operation
(REQ-NF-005), it completes synchronously within the same event-loop tick and never triggers a
new filesystem scan or database query.

When `currentDefault` changes (because `MimeService.defaultBrowserChanged` propagates through
the binding in `SidebarSystem.qml`), the component updates the ComboBox `currentIndex` to match.

---

## 3. Data Flow

### 3.1 Startup flow

```
Shell startup
  └─ LauncherService ctor → QTimer::singleShot(0) → scan (background thread)
       └─ each .desktop file → parseDesktopEntryFile() → entry.mime_types populated
       └─ upsert(entry) → DesktopEntrySerializer::toJson() → includes "mime_types" key
       └─ model_.setEntries(entries) → in-memory entries ready

  └─ MimeService ctor → QTimer::singleShot(0) → refreshAllRoles()
       └─ browser:
            spawn QProcess("xdg-settings", {"get", "default-web-browser"})
            spawn QProcess("xdg-settings", {"check", "default-web-browser", desktopFile})
       └─ for each non-browser role MIME:
            spawn QProcess("xdg-mime", {"query", "default", mime})
       └─ onQueryFinished(mime, process):
            mime_cache_[mime] = desktopFile
            if role value changed → emit defaultTerminalChanged() (etc.)
       └─ onBrowserQueryFinished(desktopFile):
            browser_default_ = desktopFile when xdg-settings check returned yes, else empty
            if value changed → emit defaultBrowserChanged()
            QML properties bound to MimeService.defaultBrowser update automatically

  └─ KdeCompatService ctor → recheckDiagnostics()
       if warning: kdeWarningActive = true → KdeCompatRow becomes visible in sidebar
```

The scanner and `MimeService` run concurrently. `SidebarSystem.qml` is not shown until the user
opens the sidebar, so it is fine if `MimeService` properties are still empty when the view is
first instantiated — QML bindings update reactively when `defaultBrowserChanged()` fires.

### 3.2 User changes default application

```
User opens sidebar → SidebarSystem.qml instantiates DefaultAppRow items
  └─ DefaultAppRow.Component.onCompleted
       → LauncherService.entriesForMimeTypes(mimeTypesForFilter)
       → candidates list populated synchronously from in-memory launcher model
  └─ ComboBox currentIndex = indexOf(MimeService.defaultBrowser in candidates)

User selects new app in ComboBox
  └─ onActivated → root.defaultChanged("firefox.desktop")
  └─ SidebarSystem.qml handler → MimeService.setDefaultBrowser("firefox.desktop")
  └─ MimeService.setDefaultBrowser("firefox.desktop"):
       spawn QProcess("xdg-settings", {"set", "default-web-browser", "firefox.desktop"})
       on success:
           queryBrowserAsync()  // xdg-settings get + check
       on failure: qCWarning, no cache change
  └─ onBrowserQueryFinished:
       browser_default_ = "firefox.desktop"   // only if xdg-settings check says yes
       if defaultBrowser() changed → emit defaultBrowserChanged()
       QML binding on MimeService.defaultBrowser updates ComboBox selection
```

Cache invalidation is per-MIME (not per-role): only the MIMEs touched by the setter are
re-queried. Other roles that happen to share a MIME type (none do in the hardcoded list) would
also see their cache entry evicted and refreshed.

---

## 4. Key Decisions with Rationale

### 4.1 Wrap XDG utilities instead of parsing `mimeapps.list` directly

`xdg-mime` implements the XDG MIME Applications specification cascade: it merges per-user
(`$XDG_CONFIG_HOME/mimeapps.list`), per-user data (`$XDG_DATA_HOME/applications/mimeapps.list`),
system-wide (`$XDG_CONFIG_DIRS`), and distribution-provided defaults in the correct precedence
order, handling `[Added Associations]` vs `[Removed Associations]` vs `[Default Applications]`
sections. Replicating this logic correctly and keeping it in sync with future XDG spec revisions
is error-prone. `xdg-mime` is the canonical, always-correct tool.

The browser role is the exception: `xdg-settings check default-web-browser <desktop-file>` can
return `no` even when `xdg-settings get default-web-browser` returns that same desktop file, because
it validates the underlying browser-related associations. The sidebar should match that stricter
meaning so it does not show Chrome as configured while Chrome still warns that it is not default.

Tradeoff: subprocess overhead (~5–15 ms each × role query) at startup. Acceptable because queries
run concurrently in the background; the UI is not blocked.

### 4.2 Extend `DesktopEntry` / `LauncherService` rather than a new `DesktopEntryService`

`LauncherService` already maintains the authoritative, live, de-duplicated, sorted list of all
installed applications: it runs the background scan, validates the SQLite cache, and watches
directories for changes. Introducing a parallel `DesktopEntryService` would double the I/O
(two concurrent scans), produce two divergent inventories that must be kept in sync, and
complicate the CMake dependency graph. MIME type information is a property of each desktop entry,
not a separate domain. Extending the existing struct, serializer, and service with one field and
one Q_INVOKABLE is minimal and correct.

### 4.3 Store `mimeTypes` in the JSON blob, not a new SQLite column

The `DesktopEntryCache` schema stores all entry fields as a single JSON blob in the `data TEXT`
column. Alternatives:
- Adding a `mime_types TEXT` column would require an `ALTER TABLE` migration (or drop/recreate),
  NULL handling in every query, and a new bind value in `upsert()`.
- A separate `desktop_entry_mimes` junction table would complicate `loadAll()` to a JOIN.

Neither alternative provides a benefit here: the spec requires no SQL-level MIME filtering. All
MIME filtering happens in-memory after `loadAll()` finishes. Keeping `mime_types` in the blob is
additive (old rows missing the key deserialize to empty list) and requires changes only in
`DesktopEntrySerializer`, not in the cache SQL layer.

### 4.4 `KdeCompatService` is diagnostic-only, no env mutation

The shell may run as a systemd user service (UWSM). Calling `setenv("XDG_MENU_PREFIX", ...)` in
the shell process would not propagate to other user processes, to the systemd session environment,
or to the KDE daemon (`kded6`). The correct fix is a session-level environment change (e.g., in
`~/.config/environment.d/` or `plasma-workspace`). Automatic mutation would give a false sense of
correctness for the shell alone while leaving the rest of the session broken. REQ-C-002 codifies
this: diagnostics are read-only; the user's explicit "Rebuild caches" action is the only mutation.

### 4.5 Six hardcoded roles, not user-configurable

A user-configurable MIME role system would require: a UI to browse and select MIME types from the
full XDG MIME database; a persistence layer for role definitions; dynamic Q_INVOKABLE generation
or a map-based API. The six preset roles (browser, terminal, file manager, image viewer, text
editor, video player) cover the dominant use cases with near-zero UI complexity. Custom roles are
explicitly out of scope (REQ-C-003) and can be handled by dedicated tools (`xdg-settings`,
GNOME/KDE system settings). Hardcoding the roles as static data in `MimeService.cpp` keeps the
code reviewable in a single pass.

### 4.6 ComboBox filter without new scans

`LauncherService.entriesForMimeTypes()` iterates the already-loaded in-memory
`LauncherModel::entries_` vector. No file I/O, no database queries, no `QFutureWatcher` threads.
The scan completes before the user can open the sidebar (the sidebar requires the compositor to
be running, which starts after the shell's layer-shell surfaces appear, which happen after
`start()` — by which time the background scan is either complete or has loaded the cache). If the
scan is still in progress, `entriesForMimeTypes()` returns whatever entries are available; when
the scan completes and `model_.setEntries()` is called, `Component.onCompleted` has already run
but `SidebarSystem.qml` can re-query on a `Connections { target: LauncherService }` hook to
refresh candidates if the sidebar is open at that moment.

---

## 5. Alternatives Considered

### 5.1 Parse `mimeapps.list` directly

Parse `$XDG_CONFIG_HOME/mimeapps.list` and system counterparts in C++ using `QSettings` or a
custom parser. **Rejected** because: the XDG MIME Applications specification defines a five-step
cascade with additive/subtractive merge semantics that is non-trivial to implement correctly; the
file format has changed between XDG spec versions; different distributions apply patches to this
behavior (e.g., Debian's `update-alternatives` integration). Any deviation from the reference
implementation (`xdg-mime`) produces user-visible bugs. The subprocess approach is authoritative
by construction.

### 5.2 Use `QSortFilterProxyModel` for role comboboxes

Attach a `QSortFilterProxyModel` to `LauncherService.results` in QML, configured to filter by a
MIME type role. **Rejected** because: `LauncherService.results` is the search-filtered view of
entries (it respects the current `query` and `activeCategory`); wrapping it in a proxy would
return zero results when the search query does not match the default app's name. The combobox
needs the full entry list regardless of the launcher search state. A proxy on the filtered results
would be fundamentally wrong. The `entriesForMimeTypes()` Q_INVOKABLE explicitly traverses the
unfiltered backing store (`entries_`), which is the correct source.

### 5.3 Implement a new `DesktopEntryService` for MIME integration

A separate `DesktopEntryService` with its own scan, cache, and MIME-aware model, independent of
the launcher. **Rejected** because: this duplicates the most expensive infrastructure in the shell
(filesystem scan, SQLite cache, background thread management, `QFileSystemWatcher`). Double-scan
doubles startup time and doubled memory. Keeping two inventories consistent under concurrent file
changes introduces synchronization bugs. Reusing `LauncherService`'s data is simpler, faster, and
correct — the MIME field is a property of each desktop entry.

### 5.4 SQLite column for `mime_types` (junction table or delimited string)

Store MIME types either as a delimited `TEXT` column or in a `desktop_entry_mimes(path, mime)`
junction table. **Rejected** for the column variant: SQL-level filtering is not needed (all
filtering is in-memory), so the complexity of schema migration, NULL handling, and new bind
values is pure cost with no benefit. **Rejected** for the junction table variant: `loadAll()`
becomes a JOIN, `upsert()` becomes a multi-table transaction, and `remove()` must cascade.
The JSON blob is the right abstraction boundary for structured per-entry data that does not need
to be queried via SQL predicates.

---

## 6. Known Risks

### 6.1 Cache invalidation: existing rows lack `mime_types`

Rows cached before this upgrade do not have the `"mime_types"` key in the JSON blob.
`fromJson()` deserializes them with `mime_types = []`. These entries will not appear in any
role's combobox until their source `.desktop` file changes on disk (triggering a mtime/size
mismatch and re-scan). On a typical system where applications are not frequently updated, this
means the role comboboxes could show fewer candidates on first run after upgrade.

**Mitigation options (not implemented in this spec)**:
- Persist a schema-version integer in the JSON blob; `fromJson()` schedules a forced re-scan
  when the version is absent or lower than current.
- `LauncherService.reload()` (already Q_INVOKABLE) forces a full re-scan regardless of mtime;
  a one-time call at startup after the upgrade could be triggered by detecting missing
  `mime_types` on loaded entries.

For now, the behavior is documented as "first-run degraded; self-heals when apps are updated or
`reload()` is called."

### 6.2 No SQLite schema migration required

Confirmed: `mimeTypes` goes in the `data` JSON blob. The `kSchema` constant and all SQL
queries (`loadAll`, `metadata`, `upsert`, `remove`) remain unchanged. This risk is a non-issue.

### 6.3 Terminal role MIME convention

`application/x-terminal-emulator` is widely used (Debian/Ubuntu `update-alternatives`, GNOME's
`gio` mime database) but is **not** defined in the formal XDG MIME specification. Some terminals
(particularly those not packaged for Debian-derived distributions) may not declare it in their
`.desktop` file. The terminal role combobox may show fewer candidates or be empty on minimal
distributions (e.g., Arch Linux base without an XDG-aware terminal). No mitigation in this spec
— custom terminal role MIME selection is out of scope (REQ-C-003).

### 6.4 `xdg-mime` subprocess timeout

`xdg-mime` is typically a shell script that reads from the filesystem. On NFS-mounted or slow
encrypted home directories, it may take longer than expected. The 5-second per-process timeout
(implemented as a `QTimer` calling `QProcess::kill()`) prevents the shell from hanging, but
means the role property remains empty until the next `refreshAllRoles()` call (which currently
has no automatic retry — the user would need to reopen the sidebar or trigger a manual
`MimeService.refresh()`). A future improvement could add a single retry with exponential backoff,
but that is not in scope for this spec.

### 6.5 Six parallel subprocesses at startup

`refreshAllRoles()` spawns up to 13 `QProcess` instances concurrently (one per unique MIME type
across all roles). Each `xdg-mime` invocation is a lightweight shell script with minimal resource
usage (~2 MB RSS on Linux), so this is not expected to saturate process or file descriptor limits.
On constrained embedded systems this assumption should be revisited.

---

## 7. File Map

| Path | Status | Description |
|---|---|---|
| `src/services/launcher/DesktopEntryScanner.h` | modified | Add `QStringList mime_types{}` field to `DesktopEntry` |
| `src/services/launcher/DesktopEntryScanner.cpp` | modified | Parse `MimeType=` line in `applyDesktopEntryField()` |
| `src/services/launcher/DesktopEntrySerializer.h` | modified | Add `"mime_types"` key to `toJson()` and `fromJson()` |
| `src/services/launcher/LauncherModel.h` | modified | Add `MimeTypesRole` to `Role` enum |
| `src/services/launcher/LauncherModel.cpp` | modified | Implement `MimeTypesRole` in `data()` and `roleNames()` |
| `src/services/launcher/LauncherService.h` | modified | Declare `entriesForMimeTypes(QStringList)` Q_INVOKABLE |
| `src/services/launcher/LauncherService.cpp` | modified | Implement `entriesForMimeTypes()` |
| `src/services/mime/MimeService.h` | new | `MimeService` QML singleton: six role properties, six setters |
| `src/services/mime/MimeService.cpp` | new | `xdg-mime` subprocess logic plus browser-specific `xdg-settings` handling; includes `MimeService.moc` |
| `src/services/kde-compat/KdeCompatService.h` | new | `KdeCompatService` QML singleton: diagnostic + rebuild |
| `src/services/kde-compat/KdeCompatService.cpp` | new | Diagnostic check, `rebuildCaches()` subprocess chain; includes `KdeCompatService.moc` |
| `src/qml/RightSidebar/SidebarSystem.qml` | modified | Replace placeholder stub with real role selector layout |
| `src/qml/RightSidebar/DefaultAppRow.qml` | new | Reusable role selector row (label + ComboBox) |
| `CMakeLists.txt` | modified | Add `MimeService.h/.cpp`, `KdeCompatService.h/.cpp` to `holonight_services`; add include dirs for `src/services/mime` and `src/services/kde-compat`; add `DefaultAppRow.qml` to `HOLONIGHT_QML_FILES` |
