# Launcher Redesign – Architecture Design

**Document Version:** 1.0
**Last Updated:** 2026-06-18
**Status:** Draft — ready for task breakdown
**Spec reference:** `docs/sdd/launcher-redesign/SPEC.md`

---

## 1. Component Overview

### New C++ files

| File | Responsibility |
|---|---|
| `src/services/launcher/RecentAppsTracker.h/.cpp` | QML singleton; persists and queries up to 20 recent launch records in `launch-history.json`. |
| `src/services/launcher/CategoryMapper.h` | Header-only static helper; maps a semicolon-separated XDG `Categories` string to a single curated display-name string. |

### Modified C++ files

| File | Change summary |
|---|---|
| `src/services/launcher/DesktopEntryScanner.h` | Add `DesktopAction` struct and `actions` field to `DesktopEntry`. |
| `src/services/launcher/DesktopEntryScanner.cpp` | Parse `[Desktop Action *]` sections; populate `entry.actions`. |
| `src/services/launcher/LauncherModel.h/.cpp` | Add Browse/Search mode, `activeCategory` filtering, `ActionsRole`, `IsActionRole`, `ActionParentRole`, alphabetical browse sort. |
| `src/services/launcher/LauncherService.h/.cpp` | Add `activeCategory` property, `availableCategories()`, `countForCategory()`, `setActiveCategory()`; call `RecentAppsTracker::recordLaunch()` after successful launch; expose selected-entry detail properties for the context panel. |

### New QML files

| File | Responsibility |
|---|---|
| `src/qml/Launcher/LauncherRightPanelBrowse.qml` | Right column in Browse mode: RECENT section + CATEGORIES section. |
| `src/qml/Launcher/LauncherRightPanelSearch.qml` | Right column in Search mode: FILTERS section + SELECTED ITEM context panel. |
| `src/qml/Launcher/LauncherActionRow.qml` | Single row for a desktop action in both the left list (ACTIONS section) and the context panel. |

### Modified QML files

| File | Change summary |
|---|---|
| `src/qml/Launcher/Launcher.qml` | Restructure body from single-column to `RowLayout` with left/right columns; add mode-switching for right column; wire category selection and action launch signals. |
| `src/qml/Launcher/LauncherResultRow.qml` | Add `appDesktopFile` required property so the context panel can call `RecentAppsTracker.lastUsedFor()`. |
| `src/qml/Launcher/LauncherSearchField.qml` | No functional changes needed; kept as-is. |
| `CMakeLists.txt` | Add three new QML sources to `HOLONIGHT_QML_FILES`; add `RecentAppsTracker.h/.cpp` and `CategoryMapper.h` to `holonight_services`. |

---

## 2. New C++ Class: `RecentAppsTracker`

**Location:** `src/services/launcher/RecentAppsTracker.h` and `RecentAppsTracker.cpp`

### Registration

```cpp
class RecentAppsTracker : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // ...
};
```

Because the class is defined entirely in `RecentAppsTracker.cpp` (no `Q_OBJECT` in an `.h`-only implementation — actually the `Q_OBJECT` macro must live in a header for MOC to pick it up, so the declaration goes in the header as usual). The `.cpp` does **not** need `#include "RecentAppsTracker.moc"` since the declaration is in the header.

### Public API

```cpp
// Record (or refresh) desktopFile in history. Emits recentChanged().
Q_INVOKABLE void recordLaunch(const QString& desktop_file);

// Return the most recent `limit` entries as QVariantList of QVariantMap
// with keys "desktopFile" (QString) and "lastUsed" (QDateTime).
Q_INVOKABLE QVariantList recentEntries(int limit = 5) const;

// Return the stored lastUsed timestamp for desktopFile, or invalid QDateTime
// if not present. O(n) scan — fine for ≤20 entries.
Q_INVOKABLE QDateTime lastUsedFor(const QString& desktop_file) const;

Q_SIGNALS:
    void recentChanged();
```

### Storage format

File path: `QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/launch-history.json"`.
`CacheLocation` resolves to `$XDG_CACHE_HOME/holonight-shell` when the application name is set to `holonight-shell`.

JSON schema:

```json
[
  { "desktopFile": "/usr/share/applications/firefox.desktop", "lastUsed": "2026-06-18T14:30:45Z" },
  ...
]
```

### Load / save strategy

- **Load on construction**: call `loadFromDisk()` in the constructor. If the file is missing, start with an empty list (no warning needed — first run is expected). If the file exists but `QJsonDocument::fromJson()` sets a parse error or returns a non-array, log `qCWarning` and start with empty list.
- **Save on each `recordLaunch` call**: write synchronously with `QFile::WriteOnly | QFile::Truncate`. Create intermediate directories with `QDir::mkpath()` before the first write.
- **No background I/O**: the JSON is at most ~2 KB; synchronous write is sub-millisecond.

### Max-20 eviction policy

After inserting or refreshing an entry, if `entries_.size() > 20`, remove the entry with the minimum `lastUsed` value using `std::ranges::min_element`.

### REQ-C-003 compliance

`RecentAppsTracker` is a QML singleton — it lives for the application lifetime. This does not violate REQ-C-003 (no persistent resources while closed) because it holds only an in-memory `QVector` and opens no file handles after construction. The constraint applies to threads and timers, not to a passive data cache.

---

## 3. `DesktopEntry` Struct Changes

**Location:** `src/services/launcher/DesktopEntryScanner.h` (alongside `DesktopEntry`)

### New type

```cpp
struct DesktopAction {
    QString name;          // From [Desktop Action X] → Name=...
    QString exec;          // From [Desktop Action X] → Exec=...
};
```

`DesktopAction` is defined in `DesktopEntryScanner.h` immediately before `DesktopEntry` so both are available to any translation unit that includes the scanner header.

### Change to `DesktopEntry`

```cpp
struct DesktopEntry {
    // ... existing fields unchanged ...
    QVector<DesktopAction> actions;   // NEW — may be empty
};
```

No default initializer is needed for `QVector` (it zero-initializes itself). The designated-initializer pattern used in tests (`ScoredEntry{.entry = entry, .score = score}`) is unaffected because `actions` is a new field in `DesktopEntry`, not in `ScoredEntry`.

---

## 4. `DesktopEntryScanner` Changes

### Parse state extension

Add to `DesktopEntryParseState`:

```cpp
struct CurrentActionState {
    QString key;   // e.g. "LaunchBrowser"
    QString name;
    QString exec;
};
std::optional<CurrentActionState> current_action;
```

### Section detection in `handleDesktopSection`

The current `handleDesktopSection` returns `true` (stop reading) when it encounters any non-`[Desktop Entry]` section while `in_desktop_entry` is true. This logic must change: instead of stopping, the parser should transition to collecting an action section.

Updated section-dispatch logic (in `parseDesktopEntryFile`):

```
if line starts with '[' and ends with ']':
    if line == "[Desktop Entry]":
        state.in_desktop_entry = true
        state.saw_desktop_entry = true
        flush any pending current_action into state.parsed.actions
        state.current_action = nullopt
    else if line starts with "[Desktop Action ":
        flush any pending current_action
        extract action key = line stripped of "[Desktop Action " prefix and "]" suffix
        state.in_desktop_entry = false
        state.current_action = CurrentActionState{ .key = extracted_key }
    else:
        flush any pending current_action
        state.in_desktop_entry = false
        state.current_action = nullopt
    continue to next line
```

The "flush pending action" helper checks if `current_action` has both `name` and `exec` non-empty, and if so appends a `DesktopAction` to `state.parsed.actions`.

### Key dispatch for action sections

When `state.current_action` has a value, key/value lines are dispatched to it:

```
if state.current_action.has_value():
    if key == "Name"  -> state.current_action->name = value
    if key == "Exec"  -> state.current_action->exec = value
    (other keys ignored for now)
```

### End-of-file flush

After the stream ends, call the same flush-pending-action helper once more to capture the last section.

### Validation

Actions with an empty `name` or empty `exec` after parsing are silently discarded (REQ-F-027 acceptance criterion: "Invalid or malformed sections are silently skipped").

---

## 5. `LauncherModel` Changes

### Mode definition

```cpp
enum class Mode { Browse, Search };
```

`rebuildResults()` branches on `query_.isEmpty()`:
- **Browse mode**: filter `entries_` by `active_category_` (skip if empty or `"All"`), then sort case-insensitively on a sortKey derived by stripping leading "The ", "A ", "An " from `entry.name`.
- **Search mode**: existing scoring/ranking logic, extended to also score `DesktopAction` entries.

### Action results in Search mode

In Search mode, `results_` must accommodate both application rows and action rows. Extend `ScoredEntry`:

```cpp
struct ScoredEntry {
    DesktopEntry entry;
    int score{0};
    bool is_action{false};
    int action_index{-1};   // index into entry.actions; -1 = the app itself
};
```

Scoring runs twice per entry in Search mode:
1. Score the application (`action_index = -1`). If score > 0, add to results.
2. For each action in `entry.actions`, score `action.name` against the query using `fieldScore` with the same weight as `entry.name`. If score > 0, add an `ScoredEntry` with `is_action = true`, `action_index = i`.

Action results are appended to `results_` and sorted in the same pass. The ordering guarantee from the spec (BEST MATCH first, then APPLICATIONS, then ACTIONS) is enforced in `data()` via virtual section headers — see Section 8.

### New roles

```cpp
enum class Role : std::uint16_t {
    // ... existing roles ...
    ActionsRole,        // QVariantList of QVariantMap{{"name", ...}, {"exec", ...}}
    IsActionRole,       // bool
    ActionParentRole,   // QString — parent app name when is_action == true
    ActionExecRole,     // QString — the specific action's exec when is_action == true
    MappedCategoryRole, // QString — result of CategoryMapper for this entry
};
```

`ActionsRole` iterates `entry.actions` and builds a `QVariantList` on demand. This is called only when the context panel is visible; no pre-caching needed.

### New property and method

```cpp
void setActiveCategory(const QString& category);   // triggers rebuildResults()
[[nodiscard]] const QString& activeCategory() const;
```

`activeCategory_` defaults to `""` (same behavior as `"All"`).

### Browse-mode sort key helper

```cpp
[[nodiscard]] static QString browseDisplayName(const DesktopEntry& entry);
// strips leading "The ", "A ", "An " (case-insensitive) for sort purposes only
```

Comparator used in browse sort:

```cpp
std::ranges::sort(results_, [](const ScoredEntry& a, const ScoredEntry& b) {
    return QString::localeAwareCompare(
        browseDisplayName(a.entry), browseDisplayName(b.entry)) < 0;
});
```

---

## 6. `LauncherService` Changes

### New properties

```cpp
Q_PROPERTY(QString activeCategory READ activeCategory NOTIFY activeCategoryChanged)
Q_PROPERTY(QString selectedEntryName READ selectedEntryName NOTIFY selectedIndexChanged)
Q_PROPERTY(QString selectedEntryDesktopFile READ selectedEntryDesktopFile NOTIFY selectedIndexChanged)
Q_PROPERTY(QString selectedEntryIcon READ selectedEntryIcon NOTIFY selectedIndexChanged)
```

`selectedEntry*` properties are derived from `model_.entryAt(selected_index_)` — no extra storage. They notify via `selectedIndexChanged` (already emitted when selection changes).

### New invokables

```cpp
Q_INVOKABLE void setActiveCategory(const QString& category);
// → forwards to model_.setActiveCategory(); emits activeCategoryChanged()

Q_INVOKABLE QStringList availableCategories() const;
// Returns ["All", "Development", "Education", "Games", "Graphics",
//           "Internet", "Multimedia", "Office", "Other", "Science",
//           "Settings", "System"]
// Always the full curated list in this fixed display order (REQ-F-034).

Q_INVOKABLE int countForCategory(const QString& category) const;
// O(n) scan over entries_ asking CategoryMapper for each entry.
// "All" returns entries_.size(). Cached lazily; invalidated on setEntries().

Q_INVOKABLE bool launchAction(int entry_index, int action_index);
// Launch entry.actions[action_index].exec (after field-code stripping).
// Calls RecentAppsTracker::recordLaunch(entry.desktop_file) on success.
// Emits launched() on success.
```

### `launch()` and `launchSelected()` changes

Both call `RecentAppsTracker::instance()->recordLaunch(entry->desktop_file)` after `backend_->launch(*entry)` succeeds.

### Category count caching

Add `mutable QMap<QString, int> category_counts_cache_` and `mutable bool category_counts_dirty_{true}`. Invalidate (`category_counts_dirty_ = true`) in `setEntries()`. Rebuild lazily in `countForCategory()` and `availableCategories()` when dirty.

### `activeCategory` reset on open

`Launcher.qml`'s `resetAndFocus()` should call `LauncherService.setActiveCategory("")` to satisfy REQ-F-007 (default to "All" on open). Add this to `resetAndFocus()`.

---

## 7. `CategoryMapper` Helper

**Location:** `src/services/launcher/CategoryMapper.h` (header-only, no `.cpp` needed)

```cpp
namespace CategoryMapper {

// Returns the curated display name for the first matching XDG token in
// the semicolon-separated `categories` string, or "Other" if none match.
[[nodiscard]] inline QString map(const QString& categories) {
    static const QHash<QString, QString> kTable = {
        {"AudioVideo", "Multimedia"}, {"Audio", "Multimedia"}, {"Video", "Multimedia"},
        {"Development", "Development"},
        {"Education", "Education"},
        {"Game", "Games"},
        {"Graphics", "Graphics"},
        {"Network", "Internet"}, {"Internet", "Internet"},
        {"Office", "Office"},
        {"Science", "Science"},
        {"Settings", "Settings"},
        {"System", "System"}, {"Utility", "System"}, {"Utilities", "System"},
    };

    for (const QString& token :
         categories.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const auto it = kTable.find(token.trimmed());
        if (it != kTable.end()) {
            return it.value();
        }
    }
    return QStringLiteral("Other");
}

// Fixed display order for the CATEGORIES section (REQ-F-033, REQ-F-034).
[[nodiscard]] inline QStringList curatedOrder() {
    return {
        QStringLiteral("All"),
        QStringLiteral("Development"),
        QStringLiteral("Education"),
        QStringLiteral("Games"),
        QStringLiteral("Graphics"),
        QStringLiteral("Internet"),
        QStringLiteral("Multimedia"),
        QStringLiteral("Office"),
        QStringLiteral("Other"),
        QStringLiteral("Science"),
        QStringLiteral("Settings"),
        QStringLiteral("System"),
    };
}

} // namespace CategoryMapper
```

The `static const QHash` inside `map()` is initialized once and shared across all calls (no per-call allocation). `map()` is called from `LauncherModel::rebuildResults()` for browse-mode filtering and from `LauncherService::countForCategory()` for category counts.

---

## 8. QML Layout Redesign

### Overall panel structure (`Launcher.qml`)

The existing `ColumnLayout` inside `panel` gains a `RowLayout` body beneath the search field, replacing the current single-column results area:

```
panel (Item, width=panelWidth, height=panelHeight)
  ColumnLayout (fill, margins=28, spacing=18)
    LauncherSearchField          ← unchanged, Layout.fillWidth, height 62
    RowLayout                    ← NEW body
      leftColumn (Item, Layout.fillWidth, Layout.preferredWidth: leftColumnWeight)
      rightColumn (Item, width: 240)
```

Proportional split: `leftColumn` carries `Layout.fillWidth: true`; `rightColumn` is fixed 240 px. The `RowLayout` itself is `Layout.fillHeight: true`.

Mode is driven by `LauncherService.query.length === 0` (a JS boolean, evaluated in QML bindings).

### Left column — Browse mode

When `isSearchMode` is false:

```qml
ListView {
    id: browseList
    model: LauncherService.results   // model is in Browse mode (alphabetical)
    delegate: LauncherResultRow {
        required property string name
        required property string subtitle
        required property string iconName
        required property string desktopFile
        required property int index

        isBestMatch: false
        selected: index === LauncherService.selectedIndex
        appName: name
        appSubtitle: subtitle
        appIconName: iconName
        appDesktopFile: desktopFile
        onHovered: LauncherService.setSelectedIndex(index)
        onActivated: LauncherService.launch(index)
    }
}
```

Empty-state text ("No applications in this category") is a `Text` item visible when `LauncherService.resultCount === 0`.

### Left column — Search mode

The search-mode left column uses a **single `ListView` with a combined model** (see Section 13 for the DelegateChooser alternative). The model is a `LauncherCombinedSearchModel` — a thin `QAbstractListModel` assembled in QML from `LauncherService.results` — but building a full proxy model only to insert section headers introduces complexity. The pragmatic approach is simpler:

**Approach: virtual header items via a ListModel proxy**

`Launcher.qml` maintains a JS-side assembly triggered by `LauncherService.resultCount` changes:

```qml
// In Launcher.qml, searchItems is a ListModel rebuilt when results change
ListModel { id: searchItems }

function rebuildSearchItems() {
    searchItems.clear()
    const count = LauncherService.resultCount
    if (count === 0) return

    // Best match (index 0 in LauncherService.results)
    searchItems.append({ itemType: "header", label: "BEST MATCH" })
    searchItems.append({ itemType: "app", sourceIndex: 0, isBestMatch: true })

    // Applications (remaining non-action rows)
    let appCount = 0
    for (let i = 1; i < count; i++) {
        if (!LauncherService.results.data(..., "isAction")) appCount++
    }
    if (appCount > 0) {
        searchItems.append({ itemType: "header", label: "APPLICATIONS" })
        for (let i = 1; i < count; i++) {
            if (!LauncherService.results.data(..., "isAction"))
                searchItems.append({ itemType: "app", sourceIndex: i, isBestMatch: false })
        }
    }

    // Actions
    let actionCount = 0
    for (let i = 0; i < count; i++) {
        if (LauncherService.results.data(..., "isAction")) actionCount++
    }
    if (actionCount > 0) {
        searchItems.append({ itemType: "header", label: "ACTIONS" })
        for (let i = 0; i < count; i++) {
            if (LauncherService.results.data(..., "isAction"))
                searchItems.append({ itemType: "action", sourceIndex: i })
        }
    }
}
```

**Revised approach — avoid `data()` calls from JS**: The `rebuildSearchItems` pattern above requires calling `LauncherService.results.data()` from JS, which is verbose and not idiomatic. A cleaner alternative: expose two separate counts from `LauncherService`:

```cpp
Q_PROPERTY(int appResultCount   READ appResultCount   NOTIFY resultCountChanged)
Q_PROPERTY(int actionResultCount READ actionResultCount NOTIFY resultCountChanged)
```

Then in QML the delegate uses `index < 1+appResultCount` vs `index >= 1+appResultCount` to distinguish apps from actions. The `ListModel` approach is abandoned in favour of a **`DelegateChooser`** on top of the flat `LauncherService.results` model, with section header delegates synthesized by offset arithmetic:

```qml
ListView {
    model: LauncherService.results
    delegate: DelegateChooser {
        role: "isAction"
        DelegateChoice { roleValue: false; delegate: LauncherResultRow { ... } }
        DelegateChoice { roleValue: true;  delegate: LauncherActionRow { ... } }
    }
    // Section property for automatic section headers:
    section.property: "isAction"
    section.delegate: LauncherSectionHeader { ... }
}
```

This uses Qt's built-in `ListView.section` mechanism: when `isAction` transitions from `false` to `true`, `ListView` automatically inserts the section header delegate. The BEST MATCH visual accent is handled by `isBestMatch: index === 0 && !model.isAction`.

**BEST MATCH section header** — `section` only fires on changes. Since the first app is not preceded by an `isAction: true` run, we cannot use `section` for the "BEST MATCH" header. Instead, prepend it as an in-model concept:

The cleanest solution: add `IsBestMatchRole` (already planned) and use a `header` component on the `ListView`:

```qml
ListView {
    model: LauncherService.results
    header: Column {
        // "BEST MATCH" label is always the first row when in search mode
        visible: LauncherService.query.length > 0 && LauncherService.resultCount > 0
        LauncherSectionHeader { label: "BEST MATCH" }
    }
    section.property: "isActionSection"   // "apps" or "actions" string role
    section.delegate: LauncherSectionHeader { label: section }
    delegate: DelegateChooser { ... }
}
```

where `isActionSection` role returns `"APPLICATIONS"` for app rows (index > 0) and `"ACTIONS"` for action rows. Index 0 (best match) still uses `LauncherResultRow` with `isBestMatch: true`; the BEST MATCH section header comes from the `header` component. This keeps the model flat and the QML declarative.

### Right column — Browse mode (`LauncherRightPanelBrowse.qml`)

```
ColumnLayout (fill)
  Text "RECENT"                         ← section label
  Repeater {
      model: RecentAppsTracker.recentEntries(5)
      delegate: Row {
          Image { source: "image://icon/" + modelData.iconName }
          Text  { text: modelData.name }
          MouseArea { onClicked: LauncherService.launchByDesktopFile(modelData.desktopFile) }
      }
  }
  Text "No recent apps" (visible if Repeater.count === 0)

  Item { height: 16 }   ← spacer

  Text "CATEGORIES"                     ← section label
  Repeater {
      model: LauncherService.availableCategories()
      delegate: Row {
          Text { text: modelData }
          Text { text: "(" + LauncherService.countForCategory(modelData) + ")" }
          MouseArea { onClicked: LauncherService.setActiveCategory(modelData) }
          // Highlight: background rect visible when modelData === LauncherService.activeCategory
      }
  }
```

`RecentAppsTracker.recentEntries(5)` returns a `QVariantList` of maps with keys `desktopFile` and `lastUsed`. The recent panel needs app name and icon, which are not in the history file. Two options:
- **Option A**: add `name` and `iconName` fields to history JSON (denormalized, may go stale if the app is updated).
- **Option B**: look up the entry in `LauncherService.results` by `desktopFile` at render time (requires a `findByDesktopFile(path)` invokable on `LauncherService`).

**Decision: Option B.** History is the source of truth for recency order; `LauncherService` is the source of truth for metadata. Add:

```cpp
Q_INVOKABLE QVariantMap entryInfoForDesktopFile(const QString& desktop_file) const;
// returns {"name": ..., "iconName": ..., "desktopFile": ...}
// or empty map if not found
```

This is an O(n) scan, called at most 5 times per render — acceptable.

### Right column — Search mode (`LauncherRightPanelSearch.qml`)

```
ColumnLayout (fill)
  // FILTERS section
  Text "FILTERS"
  Row {
      FilterChip {
          label: "Applications (" + LauncherService.appResultCount + ")"
          active: activeFilter === "apps" || activeFilter === ""
          onClicked: activeFilter = (activeFilter === "apps" ? "" : "apps")
      }
      FilterChip {
          label: "Actions (" + LauncherService.actionResultCount + ")"
          active: activeFilter === "actions" || activeFilter === ""
          onClicked: activeFilter = (activeFilter === "actions" ? "" : "actions")
      }
  }

  Item { height: 16 }

  // SELECTED ITEM context panel
  Text "SELECTED ITEM"
  Image { source: "image://icon/" + LauncherService.selectedEntryIcon }
  Text  { text: LauncherService.selectedEntryName }
  Text  { text: formatLastUsed(RecentAppsTracker.lastUsedFor(LauncherService.selectedEntryDesktopFile)) }
  // Desktop actions for selected entry:
  Repeater {
      model: selectedActions   // LauncherService.results.data(selectedIndex, ActionsRole)
      delegate: LauncherActionRow { ... }
  }
```

`activeFilter` is a `string` local property on `LauncherRightPanelSearch` (`"apps"`, `"actions"`, or `""` for none). This local state is reset in `Component.onCompleted` and when Search mode is exited (managed by `Launcher.qml`'s `onIsSearchModeChanged`).

The filter state in the right panel must also affect left-panel visibility. `LauncherRightPanelSearch` exposes a `activeFilter` property that `Launcher.qml` reads to conditionally hide action or app rows. Alternatively, `activeFilter` can be promoted to `LauncherService.activeSearchFilter` (a `QString` property, default `""`). This avoids cross-component binding gymnastics in `Launcher.qml`. **Decision: keep `activeFilter` as a local property on `LauncherRightPanelSearch` but expose it via a `property alias` in `Launcher.qml`**, since the left panel already has its own `isAction` awareness.

### `formatLastUsed` JS helper

Defined as a JS function in `LauncherRightPanelSearch.qml`:

```js
function formatLastUsed(dt) {
    if (!dt.valid) return ""       // null QDateTime → empty
    const now = new Date()
    const d   = new Date(dt)
    const msPerDay = 86400000
    const diff = (now - d) / msPerDay
    if (diff < 1)    return "Today, " + Qt.formatTime(d, Qt.locale().timeFormat(Locale.ShortFormat))
    if (diff < 2)    return "Yesterday, " + Qt.formatTime(d, Qt.locale().timeFormat(Locale.ShortFormat))
    return Qt.formatDate(d, Qt.locale().dateFormat(Locale.ShortFormat))
}
```

### Mode switching

In `Launcher.qml`:

```qml
readonly property bool isSearchMode: LauncherService.query.length > 0

// Right column
Item {
    id: rightColumn
    width: 240
    Layout.fillHeight: true

    LauncherRightPanelBrowse {
        anchors.fill: parent
        visible: !root.isSearchMode
    }
    LauncherRightPanelSearch {
        id: searchPanel
        anchors.fill: parent
        visible: root.isSearchMode
    }
}
```

Using `visible` toggle rather than a `Loader` to avoid component creation latency on mode switch. Both panels exist in the scene tree but only one is visible at a time. This is acceptable because neither panel is expensive to instantiate.

---

## 9. New QML Files

| File | Location | Key required properties |
|---|---|---|
| `LauncherRightPanelBrowse.qml` | `src/qml/Launcher/` | None (reads directly from `LauncherService` and `RecentAppsTracker` singletons) |
| `LauncherRightPanelSearch.qml` | `src/qml/Launcher/` | None; exposes `property string activeFilter: ""` |
| `LauncherActionRow.qml` | `src/qml/Launcher/` | `required property string actionName`, `required property string parentAppName`, `required property string actionExec`; signals `activated()` |

All three files must be added to `HOLONIGHT_QML_FILES` in `CMakeLists.txt` in alphabetical order within their group (the build fails with `FATAL_ERROR` if the sorted list does not match the discovered files).

---

## 10. Existing Files Modified

### `Launcher.qml`

- Replace the `ColumnLayout` body (everything below `LauncherSearchField`) with a `RowLayout` containing `leftColumn` and `rightColumn` items.
- Add `isSearchMode` readonly property.
- Connect `LauncherService.resultCountChanged` to reset `searchPanel.activeFilter` when returning to Browse mode.
- Add `LauncherService.setActiveCategory("")` to `resetAndFocus()`.
- The `Keys.onEscapePressed` and animation functions are unchanged.
- Panel dimensions: increase `panelWidth` from 900 to 1100 px (or keep at 900 and allow the right column to compress the left). Recommend increasing to ~1100 to give the right panel 240 px without crowding the left list.

### `LauncherResultRow.qml`

Add one required property:

```qml
required property string appDesktopFile
```

The property is wired in the delegate binding in `Launcher.qml` (Browse and Search list). It is read by `LauncherRightPanelSearch` to perform `RecentAppsTracker.lastUsedFor(appDesktopFile)` lookups. The value is sourced from the `DesktopFileRole` already present in `LauncherModel`.

### `LauncherSearchField.qml`

No changes required. The `Ctrl+Enter` terminal-launch requirement (REQ-F-035) already maps to the `Keys.onPressed` handler — add `Qt.ControlModifier` check for `Key_Return`:

```qml
} else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
           && (event.modifiers & Qt.ControlModifier)) {
    root.launchInTerminalRequested()
    event.accepted = true
}
```

Add `signal launchInTerminalRequested()` and wire to a new `LauncherService.launchSelectedInTerminal()` invokable. This is a minor addition; it does not change the file's structure.

### `CMakeLists.txt`

Changes required:

1. In `holonight_services` `add_library(...)` block, add after the existing launcher sources:
   ```cmake
   src/services/launcher/CategoryMapper.h
   src/services/launcher/RecentAppsTracker.h
   src/services/launcher/RecentAppsTracker.cpp
   ```

2. In `HOLONIGHT_QML_FILES`, add (maintaining alphabetical order within the Launcher group):
   ```cmake
   src/qml/Launcher/LauncherActionRow.qml
   src/qml/Launcher/LauncherRightPanelBrowse.qml
   src/qml/Launcher/LauncherRightPanelSearch.qml
   ```

   The sorted `HOLONIGHT_QML_FILES` for the Launcher group will become:
   ```
   src/qml/Launcher/Launcher.qml
   src/qml/Launcher/LauncherActionRow.qml
   src/qml/Launcher/LauncherRightPanelBrowse.qml
   src/qml/Launcher/LauncherRightPanelSearch.qml
   src/qml/Launcher/LauncherResultRow.qml
   src/qml/Launcher/LauncherSearchField.qml
   ```

---

## 11. Data Flow Diagram

### Flow A: User opens launcher in Browse mode

```
Disk (launch-history.json)
    │ [at app startup]
    ▼
RecentAppsTracker::loadFromDisk()
    → entries_ : QVector<RecentRecord>   [in memory, lifetime = app]

Disk (/usr/share/applications/, ~/.local/share/applications/)
    │ [LauncherService::start() → QtConcurrent::run → scanner_.scan()]
    ▼
DesktopEntryScanner::scan()
    → QVector<DesktopEntry>  [each entry now has .actions populated]
    │ [future resolved on main thread]
    ▼
LauncherModel::setEntries(entries)
    → mode = Browse, query = ""
    → rebuildResults(): sort all entries alphabetically, no category filter
    → beginResetModel() / endResetModel()
    │ [QAbstractListModel change notification]
    ▼
Launcher.qml ListView (browseList)
    → delegates render via LauncherResultRow

LauncherRightPanelBrowse (visible)
    → RECENT: RecentAppsTracker.recentEntries(5)
              → for each: LauncherService.entryInfoForDesktopFile(df) for name/icon
    → CATEGORIES: LauncherService.availableCategories()
                  → LauncherService.countForCategory(cat) for each
```

### Flow B: User types a query (Search mode transition)

```
User types in LauncherSearchField
    │ [TextInput.onTextChanged]
    ▼
LauncherService::setQuery(text)
    → query_ = text
    → LauncherModel::setQuery(text)
        → mode = Search
        → rebuildResults():
            for each DesktopEntry in entries_:
                score app entry → push if score > 0
                for each action in entry.actions:
                    score action name → push ScoredEntry{is_action=true} if score > 0
            std::ranges::sort by score desc, then name asc
        → beginResetModel() / endResetModel()
    → emit queryChanged()
    │
    ▼
Launcher.qml: isSearchMode becomes true
    → leftColumn: browseList hidden, searchList visible
    → rightColumn: LauncherRightPanelBrowse hidden, LauncherRightPanelSearch visible

searchList (ListView)
    → header shows "BEST MATCH" + first row (isBestMatch: true)
    → section.delegate shows "APPLICATIONS" when isActionSection transitions
    → section.delegate shows "ACTIONS" when isActionSection transitions again
    → delegates: LauncherResultRow (apps), LauncherActionRow (actions)

LauncherRightPanelSearch (visible)
    → FILTERS: appResultCount + actionResultCount from LauncherService
    → SELECTED ITEM:
        LauncherService.selectedEntryName/Icon/DesktopFile
        → RecentAppsTracker.lastUsedFor(selectedEntryDesktopFile)
        → LauncherService.results.data(selectedIndex, ActionsRole) for action list
```

---

## 12. Key Design Decisions with Rationale

### Single model handles both Browse and Search modes

**Decision:** `LauncherModel` switches mode based on `query_.isEmpty()` rather than using two separate models.

**Rationale:** `LauncherService.results` is already the singular `QAbstractItemModel*` property consumed by the QML `ListView`. Introducing a parallel `allAppsModel` would require `Launcher.qml` to rebind the `ListView.model` property on mode transitions, which in Qt causes a full delegate teardown and recreation — a visible flicker. A single model that changes its row set internally (via `beginResetModel` / `endResetModel`) is already what the current code does on every query change; Browse mode is just the empty-query case of the same mechanism. An additional benefit: `LauncherService.resultCount` always reflects the visible count regardless of mode, keeping the existing keyboard navigation (`moveSelection`, `setSelectedIndex`) unchanged.

### `RecentAppsTracker` as a QML singleton, not owned by `LauncherService`

**Decision:** `RecentAppsTracker` is a `QML_SINGLETON` with application lifetime, independent of `LauncherService`.

**Rationale:** The right panel context (in Search mode) calls `RecentAppsTracker.lastUsedFor()` directly from QML for any app, not just the currently selected one. If the tracker were owned by `LauncherService`, QML would have to access it as `LauncherService.recentTracker.lastUsedFor(...)` — exposing an internal object through two hops. A singleton is cleaner and matches the pattern used for `ThemeService`, `ActiveWindowService`, etc. in this codebase. The singleton holds no threads or file handles — only an in-memory vector — so it does not violate REQ-C-003.

### Browse mode uses the same `results` model, not a parallel `allAppsModel`

**Decision:** Browse mode populates `results_` with all entries (filtered by category), sorted alphabetically. No `allAppsModel` property is introduced.

**Rationale:** Beyond the flicker argument above, maintaining two parallel models in sync (same underlying `entries_`, different views) would require either duplicating `entries_` or maintaining two `results_` vectors. The single-model approach is simpler: `rebuildResults()` already owns the rebuild logic; Browse mode is just a different code path inside it. The only cost is that Browse mode calls `beginResetModel` on every category change — which it would have to do anyway.

### Context panel data via `LauncherService` properties, not a separate C++ object

**Decision:** The SELECTED ITEM context panel reads `LauncherService.selectedEntryName`, `LauncherService.selectedEntryDesktopFile`, and `LauncherService.selectedEntryIcon` (new properties on `LauncherService`) and calls `RecentAppsTracker.lastUsedFor()` directly. No separate `SelectedEntryDetails` C++ object is introduced.

**Rationale:** The context panel needs three scalar fields and one `QVariantList` (actions). These can be returned as derived properties from `selectedIndex` at negligible cost. The `ActionsRole` QML model role already provides the actions list. A dedicated C++ object would be justified only if the context panel needed asynchronous loading (e.g., fetching thumbnails from disk) — this is not the case here.

### Browse mode context panel behaviour — resolving the spec ambiguity

REQ-F-020 states the context panel lists desktop actions "regardless of whether the launcher is in Browse or Search mode." Yet REQ-F-006 through REQ-F-008 describe the Browse-mode right panel as containing only RECENT and CATEGORIES sections. These two requirements create a conflict: should clicking an app in Browse mode replace the CATEGORIES section with a SELECTED ITEM view?

**Decision:** In Browse mode, the right panel continues to show RECENT + CATEGORIES exclusively. Clicking an app in Browse mode selects it (visual highlight) and updates `LauncherService.selectedIndex`, but does **not** change the right panel content. Desktop actions for a selected app are accessible only in Search mode (via the SELECTED ITEM context panel).

**Rationale:**
1. REQ-F-020's acceptance criterion says "Clicking an application in Browse mode filters the view" — the word "filters" suggests the left-panel filtering remains the primary browse interaction, not a context switch.
2. Switching the right panel on every click would make category navigation awkward: clicking an app would hide the CATEGORIES section, requiring the user to click away to restore it.
3. The spec is inconsistent on this point; the Browse-right-panel requirements (§2) make no mention of a SELECTED ITEM context, while the Search-right-panel requirements (§4) are explicit. The simpler interpretation — Browse mode shows RECENT+CATEGORIES, Search mode shows FILTERS+SELECTED ITEM — preserves both sections' utility.
4. If a user wants to see desktop actions, typing in the search field instantly shows them in the SELECTED ITEM context panel.

---

## 13. Alternatives Considered

### A. Separate `allAppsModel` vs. mode-switching single model

**Alternative:** Introduce `Q_PROPERTY(QAbstractItemModel* allAppsModel ...)` on `LauncherService`, backed by a separate `LauncherBrowseModel` class (sorted, filterable). `Launcher.qml` binds `browseList.model` to `allAppsModel` and `searchList.model` to `results`.

**Rejected because:** Two model objects with separate rebuild cycles add complexity without benefit. The delegate flickering on model rebind (Qt destroys and recreates all delegates on `model` property change) is a visible UX defect. The single-model approach already has all necessary hooks (`setActiveCategory`, `setQuery`) to support both modes cleanly.

### B. `DelegateChooser` vs. combined `ListModel` for Search mode sections

**Alternative (combined `ListModel`):** Build a JS `ListModel` by iterating `LauncherService.results` and inserting header items between sections. Bind the `ListView` to this `ListModel`.

**Rejected because:** Iterating `LauncherService.results` from JS requires calling `model.data(model.index(i, 0), role)` for each row, which is syntactically awkward in QML and involves repeated JS-to-C++ boundary crossings. The `ListModel` also duplicates data already held in `LauncherModel`.

**Chosen approach (`DelegateChooser` + `ListView.section`):** Uses Qt's built-in section mechanism for APPLICATIONS/ACTIONS boundary; `ListView.header` for the BEST MATCH label. This keeps the `ListView` bound directly to `LauncherService.results` with no intermediate model, preserving `currentIndex` ↔ `LauncherService.selectedIndex` synchronization.

### C. `Loader` vs. `visible` toggle for right panel mode switching

**Alternative (`Loader`):** Use a `Loader` with `source` bound to a `.qml` path string that switches between `LauncherRightPanelBrowse.qml` and `LauncherRightPanelSearch.qml`.

**Rejected because:** `Loader` destroys and recreates the component on every mode switch. The `LauncherRightPanelSearch.qml` context panel reads `LauncherService.selectedEntryName` etc.; destroying and recreating it on every keystroke (since each character triggers mode check) would cause unnecessary construction overhead. More importantly, `activeFilter` local state in the search panel would reset on every word deletion — annoying if the user backspaces one character and re-types it.

**Chosen approach (`visible` toggle):** Both panels exist in the scene tree always; `visible: false` panels are not rendered. The search panel retains its `activeFilter` state as long as the launcher is open. This matches the pattern used elsewhere in the shell (e.g., `SidebarContent.qml` tab visibility).

---

## 14. Known Risks

### Risk 1: `LauncherModel::rebuildResults()` called on every category click in Browse mode causes full `ListView` re-render

**Impact:** With 200+ applications, `beginResetModel()/endResetModel()` causes Qt to destroy and recreate all visible delegates. On a fast machine this may be imperceptible (<16 ms), but on slower hardware it could show a flicker.

**Mitigation:** For Browse mode category switches, use `beginRemoveRows`/`beginInsertRows` instead of `beginResetModel` to enable smooth animated row removal/insertion. This requires tracking which rows were added/removed between the old and new filtered sets. Alternatively, accept the reset for MVP and profile before optimizing. Qt's `ListView` is efficient at reusing delegates via the delegate recycling pool, so the visual impact is likely minimal.

### Risk 2: Desktop action `Exec` field codes — `%F`/`%U` with no file/URL context

**Impact:** Many apps use `%U` (URI list) or `%F` (file list) in their action `Exec` fields. `stripDesktopExecFieldCodes` strips these, resulting in a command with no arguments. For actions like "Open New Window" (`Exec=firefox --new-window`) this is fine; for actions like "Open URL" (`Exec=firefox %U`) the URL argument is stripped, launching Firefox with no URL.

**Mitigation:** The spec (REQ-F-028) explicitly accepts this: "Field codes like `%F` and `%U` are expanded or safely removed." The existing `stripDesktopExecFieldCodes` function already handles this correctly. No change needed; document this limitation.

### Risk 3: `entryInfoForDesktopFile()` O(n) scan called 5 times per Browse mode render

**Impact:** Each time `LauncherRightPanelBrowse` is visible, it calls `LauncherService.entryInfoForDesktopFile()` for up to 5 recent apps. With 300 entries, this is 1500 comparisons — negligible for a one-time render, but if the panel re-renders frequently (e.g., on `recentChanged` signal) it could be wasteful.

**Mitigation:** Add a `QHash<QString, int> desktop_file_index_` cache inside `LauncherService` (populated in `setEntries()`) for O(1) lookup. The cache maps `desktop_file` → index in `entries_`. This also speeds up `countForCategory()` lookups.

### Risk 4: `RecentAppsTracker` singleton lifetime vs. REQ-C-003

**Impact:** REQ-C-003 states "No background processes or timers run after the launcher closes." The `RecentAppsTracker` singleton lives for the application lifetime (not just while the launcher is open). If a future developer adds a timer or file watcher to it, REQ-C-003 would be violated silently.

**Mitigation:** Add a comment in `RecentAppsTracker.h` explicitly prohibiting timers and file watchers. The tracker intentionally has no `Q_PROPERTY` bindings that would require timers. At shutdown, no cleanup is needed since the OS closes all file handles. The constraint is documented; enforcement is by code review.

### Risk 5: `HOLONIGHT_QML_FILES` sorted-list check breaks configure if new QML files are added out of order

**Impact:** CMakeLists.txt compares the hand-maintained `HOLONIGHT_QML_FILES` list against a `file(GLOB_RECURSE)` discovery. If the three new files are inserted in wrong alphabetical position, CMake configure fails with `FATAL_ERROR`. This is easy to get wrong when editing the list.

**Mitigation:** The correct sorted order is specified explicitly in Section 10 of this document. The task breakdown should call out the exact lines to insert. At worst the developer sees a clear CMake error and corrects the sort order before anything else breaks.
