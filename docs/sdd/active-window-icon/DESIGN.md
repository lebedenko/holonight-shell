# DESIGN — active-window-icon

## 1. Component Overview

| File | Change | Description |
|---|---|---|
| `src/ActiveWindowService.h` | Modified | Add `category` Q_PROPERTY, `categoryChanged` signal, private cache and scanner methods |
| `src/ActiveWindowService.cpp` | Modified | Implement `resolveCategory`, `scanDesktopFiles`, `onAppClassChanged` — cache lookup and QtConcurrent dispatch |
| `src/qml/Topbar/AppWindowIcon.qml` | New | 16×16 Canvas 2D component; dispatches `category` to one of 9 draw functions |
| `src/qml/Topbar/ActiveWindowSection.qml` | Modified | Title row becomes a `Row` containing `AppWindowIcon` then the existing `Label` |
| `CMakeLists.txt` | Modified | Register `AppWindowIcon.qml` with `set_source_files_properties` and add it to `qt6_add_qml_module` |

---

## 2. C++ Changes to ActiveWindowService

### New Q_PROPERTY

```cpp
Q_PROPERTY(QString category READ category NOTIFY categoryChanged)
```

Added to the class declaration in `ActiveWindowService.h` alongside `title` and `appClass`. Accessor is `[[nodiscard]] QString category() const { return category_; }`.

### New Private Members

```cpp
// .h
QString category_;
QHash<QString, QString> category_cache_;
```

`category_cache_` maps an `appClass` string to its resolved category string (e.g., `"firefox"` → `"browser"`). An empty string value means "resolved and found no matching category", distinguishing a cached miss from an uncached entry. A sentinel value `"\x01"` (or an `std::optional` wrapper via a separate `QHash<QString, bool> cache_resolved_`) is used to distinguish the two cases — the simpler approach is a separate `QSet<QString> resolved_classes_` that records which keys have been looked up, allowing the main cache to store empty string for a real miss.

Concrete members:

```cpp
QString category_;
QHash<QString, QString> category_cache_;   // appClass -> resolved category ("" = miss)
QSet<QString> resolved_classes_;           // appClass keys already scanned
```

### New Signal

```cpp
void categoryChanged();
```

Declared in the `Q_SIGNALS` block alongside `titleChanged` and `appClassChanged`.

### New Private Methods

```cpp
void setCategory(const QString& value);
void scheduleResolveCategory(const QString& app_class);
static QString scanDesktopFiles(const QString& app_class);
static QString mapCategoriesToIcon(const QString& categories_field);
```

`setCategory` follows the same guard-emit pattern as `setTitle` / `setAppClass`.

`scheduleResolveCategory` is called from `setAppClass` (and therefore from `setActiveWindow`). It:
1. Clears `category_` to `""` immediately if `appClass` is empty (REQ-F-006), calling `setCategory("")`.
2. If the new `app_class` is in `resolved_classes_`, reads `category_cache_[app_class]` and calls `setCategory` directly — no I/O (REQ-NF-003).
3. Otherwise, dispatches `scanDesktopFiles(app_class)` on a background thread via `QtConcurrent::run` and connects the returned `QFuture<QString>` to a `QFutureWatcher<QString>` whose `finished` signal posts the result back to the main thread.

`scanDesktopFiles` is a `static` function (no `this` capture) so QtConcurrent can run it safely. It returns the resolved category string (empty on miss).

`mapCategoriesToIcon` is a `static` helper that takes the raw `Categories=` field value, splits on `';'`, and walks the priority table, returning the first match or `""`.

### QtConcurrent Usage Pattern

```cpp
void ActiveWindowService::scheduleResolveCategory(const QString& app_class) {
    if (app_class.isEmpty()) {
        setCategory({});
        return;
    }
    if (resolved_classes_.contains(app_class)) {
        setCategory(category_cache_.value(app_class));
        return;
    }

    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, app_class] {
        const QString resolved = watcher->result();
        resolved_classes_.insert(app_class);
        category_cache_.insert(app_class, resolved);
        // Only apply if appClass hasn't changed while we were scanning.
        if (app_class_ == app_class) {
            setCategory(resolved);
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(&ActiveWindowService::scanDesktopFiles, app_class));
}
```

The `QFutureWatcher` is parented to `this` so it is cleaned up if the service is destroyed before the future completes. The `app_class_ == app_class` guard prevents stale results from a slow scan applying to a different focused window.

`QtConcurrent` header: `#include <QtConcurrent>`. No new `find_package` entry is needed — `QtConcurrent` is included by `Qt6::Core` via the concurrent module which is part of qtbase; however, to link correctly add `Qt6::Concurrent` to `find_package` and `target_link_libraries`.

### Desktop File Scan (scanDesktopFiles)

```cpp
static QString scanDesktopFiles(const QString& app_class);
```

Algorithm (executed on background thread):

1. Build the two search paths:
   - `QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/applications/"` (resolves to `~/.local/share/applications/`)
   - `/usr/share/applications/`

2. **Pass 1 — exact filename match**: for each directory, try `{app_class}.desktop` then `{app_class.toLower()}.desktop`. If the file exists and is readable, read it and call `mapCategoriesToIcon` on the `Categories=` field. Return immediately on first hit.

3. **Pass 2 — name/exec scan**: iterate all `*.desktop` entries in both directories using `QDir::entryInfoList({"*.desktop"}, QDir::Files)`. For each file, read lines until `Name=`, `Exec=`, or `[Desktop Entry]` end is reached (stop at `[` on a new section). If `Name=` or `Exec=` contains `app_class` (case-insensitive), read `Categories=` from the same file and return `mapCategoriesToIcon` of that field.

4. Return `""` if neither pass finds a match (REQ-F-004).

**Malformed file handling** (REQ-C-005): all file reads use `QFile` + `QTextStream`. If `QFile::open` fails, the file is skipped. Lines that contain no `=` character are skipped silently. Non-UTF-8 bytes produce replacement characters, which will simply fail to match any known token — no crash.

### mapCategoriesToIcon

Priority table, evaluated in order with `QStringList::contains` after splitting `Categories=` on `';'`:

| Token | Returned string |
|---|---|
| `WebBrowser` | `browser` |
| `TextEditor` | `editor` |
| `Development` | `editor` |
| `TerminalEmulator` | `terminal` |
| `FileManager` | `files` |
| `InstantMessaging` | `chat` |
| `Chat` | `chat` |
| `Audio` | `music` |
| `Music` | `music` |
| `Video` | `video` |
| `Settings` | `settings` |
| `System` | `settings` |

Returns `""` if no token matches (REQ-F-003, REQ-F-004).

### Integration Point

`setActiveWindow` calls `setAppClass` then `setTitle`. After `setAppClass(app_class)` begins updating `app_class_`, `scheduleResolveCategory` is called from within `setAppClass` — specifically, `setAppClass` calls `scheduleResolveCategory(value)` before emitting `appClassChanged`. This ensures `category` is always in sync with the current `appClass`.

---

## 3. AppWindowIcon.qml Structure

**File**: `src/qml/Topbar/AppWindowIcon.qml`

### Property Interface

```qml
Item {
    id: root

    property string category: ""

    readonly property color _stroke: HoloniightPalette.onSurface

    width: 16
    height: 16
    ...
}
```

Only one public property: `category`. The stroke color is bound to a `readonly property color _stroke` so palette changes trigger repaint via `on_StrokeChanged`.

### Canvas Structure

```qml
Canvas {
    id: iconCanvas
    anchors.fill: parent
    opacity: 0.9

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.strokeStyle = root._stroke
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        if (root.category === "browser")       root.drawBrowser(ctx, width, height)
        else if (root.category === "editor")   root.drawEditor(ctx, width, height)
        else if (root.category === "terminal") root.drawTerminal(ctx, width, height)
        else if (root.category === "files")    root.drawFiles(ctx, width, height)
        else if (root.category === "chat")     root.drawChat(ctx, width, height)
        else if (root.category === "music")    root.drawMusic(ctx, width, height)
        else if (root.category === "video")    root.drawVideo(ctx, width, height)
        else if (root.category === "settings") root.drawSettings(ctx, width, height)
        else                                   root.drawWindow(ctx, width, height)
    }
}
```

`opacity: 0.9` is set on the `Canvas` element itself (REQ-F-008). No `MultiEffect` child is present (REQ-C-003).

### Repaint Triggers

```qml
onCategoryChanged:  iconCanvas.requestPaint()
on_StrokeChanged:   iconCanvas.requestPaint()
```

`on_StrokeChanged` fires when `HoloniightPalette.onSurface` changes (e.g., theme switch), satisfying REQ-F-020 for palette changes.

### Color and Opacity Application

`ctx.strokeStyle = root._stroke` is set once at the top of `onPaint`, before the draw-function dispatch. All nine draw functions inherit this stroke style; none set their own `strokeStyle`. No fill colors other than transparent are used (REQ-F-008). No `createLinearGradient` calls. Canvas `opacity` provides the 0.9 alpha at the compositing level rather than per-path alpha.

### Draw Functions (signatures)

All draw functions scale from a logical 16×16 coordinate space. Each function calls `ctx.scale(width / 16, height / 16)` at the start and then uses coordinates in the `[0, 16]` range.

| Function | Shape description |
|---|---|
| `drawBrowser(ctx, w, h)` | Circle outline + vertical center line + two horizontal ellipse arcs (globe) |
| `drawEditor(ctx, w, h)` | `<` and `>` angle brackets |
| `drawTerminal(ctx, w, h)` | `>` chevron at left + short `_` horizontal line (prompt `>_`) |
| `drawFiles(ctx, w, h)` | Folder outline: rectangular body with a small tab on top-left |
| `drawChat(ctx, w, h)` | Rounded rectangle + small triangular tail at bottom-left |
| `drawMusic(ctx, w, h)` | Three vertical bars of varying height (waveform) |
| `drawVideo(ctx, w, h)` | Rounded rectangle frame + filled right-pointing triangle inside |
| `drawSettings(ctx, w, h)` | Small circle + 6 evenly-spaced rectangular teeth (cog) |
| `drawWindow(ctx, w, h)` | Rounded rectangle + horizontal divider line near the top (same geometry as `BarIcon`'s `"window"` case, REQ-F-017) |

`drawWindow` replicates the geometry from `BarIcon.qml`'s `"window"` branch (lines 341–349), adapted to a 16×16 canvas.

### qmllint Compliance

All property accesses inside `onPaint` are qualified with `root.` (e.g., `root._stroke`, `root.category`, `root.drawBrowser`) to satisfy the qmllint "unqualified access" rule documented in CLAUDE.md.

---

## 4. ActiveWindowSection.qml Changes

### Before

The title row is a bare `Controls.Label` (id `activeTitleLabel`) inside a `Column`. The `Column` carries the `visible: ActiveWindowService.title !== ""` guard.

```
Column (visible guard)
  └── Controls.Label  "// ACTIVE WINDOW"
  └── Controls.Label  activeTitleLabel  [title text + fade rectangle child]
```

### After

The `Controls.Label` for the title is wrapped in a `Row` alongside `AppWindowIcon`. The `Row` replaces the previous bare `Label` as the second child of `Column`.

```
Column (visible guard)
  └── Controls.Label  "// ACTIVE WINDOW"
  └── Row  (spacing: 6)
        ├── AppWindowIcon  (category: ActiveWindowService.category)
        └── Controls.Label  activeTitleLabel  [title text + fade rectangle child]
```

Key details:

- `Row { spacing: 6 }` (REQ-F-018).
- `AppWindowIcon` is the first child of the `Row` (REQ-F-018 source order).
- `AppWindowIcon` does not need its own `visible` binding — it is already inside the `Column` whose `visible` is gated on `ActiveWindowService.title !== ""` (REQ-F-019).
- `activeTitleLabel` retains all existing properties (`elide: Text.ElideRight`, `width`, `color`) and its `titleFade` child `Rectangle` is unchanged.
- The fade `Rectangle` width calculation (`Math.min(40, parent.width)`) remains relative to `activeTitleLabel`, not the `Row`, so elide behavior is unaffected.
- The `BarTooltipArea` at the bottom of the component is unchanged.

No `implicitWidth` change is needed on `BarSection` — the `Column` already constrains layout.

---

## 5. CMakeLists.txt Changes

Two additions are required, both immediately adjacent to the existing `ActiveWindowSection.qml` entries to keep the file ordered by component.

**After line 90** (the `ActiveWindowSection.qml` alias):

```cmake
set_source_files_properties(src/qml/Topbar/AppWindowIcon.qml     PROPERTIES QT_RESOURCE_ALIAS "Topbar/AppWindowIcon.qml")
```

**In the `qt6_add_qml_module` `QML_FILES` block**, after `ActiveWindowSection.qml`:

```cmake
src/qml/Topbar/AppWindowIcon.qml
```

No other CMake changes are needed unless `QtConcurrent` is not already linked. If `Qt6::Concurrent` is absent from `target_link_libraries`, add it:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick WaylandClient GuiPrivate DBus Network Concurrent)
```

and:

```cmake
target_link_libraries(holonight-shell PRIVATE
    ...
    Qt6::Concurrent
)
```

---

## 6. Key Decisions with Rationale

### QtConcurrent::run over QThread

A dedicated `QThread` subclass would require a full thread lifecycle (start/stop/wait) and inter-thread signalling boilerplate. `QtConcurrent::run` dispatches the scan to the global thread pool, keeps the call site at a single lambda, and returns a `QFuture` that integrates with `QFutureWatcher` for clean main-thread callback delivery. The scan is a one-shot operation with no persistent state on the background side, making the thread-pool model a natural fit (REQ-NF-001).

### QHash over QMap for the cache

`QHash` provides O(1) average-case lookup versus O(log n) for `QMap`. The cache key is a plain `QString` (appClass), with no ordering requirement. For typical shell usage (a handful of distinct app classes), the difference is negligible in absolute terms, but `QHash` is idiomatic for string-keyed caches throughout the codebase.

### Separate QSet<QString> resolved_classes_ alongside QHash

A single `QHash<QString, QString>` cannot distinguish "not yet looked up" from "looked up and found no category (empty string)". Using `QSet<QString> resolved_classes_` as a membership tracker avoids storing a sentinel value in the cache map and keeps the lookup logic readable.

### Separate AppWindowIcon component rather than extending BarIcon

`BarIcon` is sized 24×24, uses multi-color gradient strokes and a `MultiEffect` glow layer, and is designed around named icon strings (`"window"`, `"clock"`, etc.). `AppWindowIcon` is 16×16, uses a single flat stroke color at 0.9 opacity, has no glow, and is keyed on category strings. Extending `BarIcon` would require overriding or disabling the glow `MultiEffect`, the gradient stroke helpers, and the 24×24 geometry — all dead weight. A standalone component has zero coupling to `BarIcon`'s internals and satisfies REQ-C-003 (no `MultiEffect`) without workarounds.

---

## 7. Alternatives Considered

### QML/JS desktop file reading

XDG desktop file scanning could be implemented as a JS function reading files via `XMLHttpRequest` with `file://` URLs. Rejected: file I/O in QML is synchronous and blocks the render thread for the duration of the scan; it also violates REQ-C-004 and would require granting the QML engine filesystem access. The C++ background-thread approach is strictly cleaner.

### Extending BarIcon with a category mode

`BarIcon` could grow a `category` property and a new dispatch branch in `onPaint`. Rejected: it couples two unrelated icon families in one component, forces `AppWindowIcon` to inherit the glow `MultiEffect` (violating REQ-C-003), and requires disabling size assumptions (24×24 vs 16×16). The dead-weight concern is compounded by the fact that `BarIcon` is used on every status widget — adding unused branches increases maintenance surface.

### QFileSystemWatcher for live cache invalidation

A `QFileSystemWatcher` could watch `~/.local/share/applications/` and `/usr/share/applications/` and clear `category_cache_` when files change (e.g., on app install). Rejected as out of scope for this feature: the shell process lifetime is typically shorter than the interval at which desktop files change, and adding filesystem watches introduces an additional source of events and potential false positives. Cache-on-first-use is sufficient.

---

## 8. Known Risks

### appClass does not match any desktop filename (Pass 1 miss rate)

Many compositors report `appClass` as the WM_CLASS window property, which is set by the application and may differ from the `.desktop` filename. Examples: `appClass = "Alacritty"` vs. `alacritty.desktop`; `appClass = "code-oss"` vs. `code.desktop`. Pass 1's case-insensitive fallback mitigates the common case. Pass 2's name/exec scan is a broader net. Residual misses result in `category = ""` and the generic window icon — visually acceptable, not a crash.

### Pass 2 scan cost on large application directories

`/usr/share/applications/` on a typical Arch Linux install contains 100–300 `.desktop` files. Pass 2 reads each file line-by-line until a name/exec match or section boundary. Worst case (no match): reads the `[Desktop Entry]` section of every file. At ~1–5 KB per file, this is well within the capacity of a background thread and completes in single-digit milliseconds. The cache ensures it runs at most once per distinct `appClass`.

### Cache not invalidated on desktop file install/removal

If a user installs an application while the shell is running, the new `.desktop` file will not be seen until the shell restarts, because the resolved `appClass` is marked in `resolved_classes_` on first scan. Consequence: the icon stays at the window fallback for that app until restart. This is explicitly accepted as out of scope (see Section 7 — QFileSystemWatcher).

### Malformed or binary files in the XDG application directories

Files with no `=` separator, zero-byte files, or files containing non-UTF-8 byte sequences are handled by skipping lines without `=` and relying on `QTextStream`'s replacement-character behavior for non-UTF-8 content. The `QFile::open` check ensures unreadable files (permission denied, broken symlinks) are skipped. No exceptions are thrown (REQ-C-005).

### Race between fast appClass changes and slow background scans

If the user switches focus rapidly between two apps whose categories are not yet cached, two concurrent `QtConcurrent::run` invocations may be in flight. The `QFutureWatcher::finished` lambda guards against stale application with `if (app_class_ == app_class)` before calling `setCategory`. Both watchers will still update `category_cache_` and `resolved_classes_`, which is safe because both operations are executed on the main thread in the `finished` slot (Qt signal delivery is on the receiving object's thread). No mutex is needed.
