# SDD Tasks — launcher-redesign

## C++ Data Layer

- [x] T-001: Add `DesktopAction` struct and `actions` field to `DesktopEntry`
  - REQs: REQ-F-027, REQ-NF-008
  - Check: `DesktopEntry` has a `QVector<DesktopAction> actions` field and `DesktopAction` struct with `name` and `exec` fields.

- [x] T-002: Extend `DesktopEntryScanner.cpp` to parse `[Desktop Action *]` sections
  - REQs: REQ-F-027, REQ-F-028
  - Check: Running the scanner on a `.desktop` file with `[Desktop Action Open]` sections produces entries with non-empty `actions` vector and both Name/Exec fields populated.

- [x] T-003: Create header-only `CategoryMapper.h` with `map()` and `curatedOrder()` functions
  - REQs: REQ-F-031, REQ-F-032, REQ-F-033
  - Check: `CategoryMapper::map("Game;Graphics")` returns "Games" and `CategoryMapper::curatedOrder()` returns a 12-element list with "All" first.

- [x] T-004: Create `RecentAppsTracker.h` + `RecentAppsTracker.cpp` as QML singleton
  - REQs: REQ-F-021, REQ-F-022, REQ-F-023, REQ-F-024, REQ-F-025, REQ-NF-008
  - Check: `RecentAppsTracker` loads `launch-history.json` on construction, has `recordLaunch()` and `lastUsedFor()` invokables, and maintains max 20 entries with JSON persistence.

- [x] T-005: Add `ActionsRole`, `IsActionRole`, `ActionParentRole`, `MappedCategoryRole`, `IsActionSectionRole` roles to `LauncherModel`
  - REQs: REQ-F-010, REQ-F-012, REQ-F-013, REQ-F-020, REQ-F-031
  - Check: `LauncherModel::data()` returns action name/exec in `ActionsRole` and boolean flags for `IsActionRole`/`IsActionSectionRole` without error.

- [x] T-006: Add Browse mode to `LauncherModel` with alphabetical sorting and article-stripping
  - REQs: REQ-F-001, REQ-F-003
  - Check: When query is empty, `LauncherModel` returns results sorted case-insensitively by display name with "The", "A", "An" stripped from the sort key.

- [x] T-007: Extend `LauncherModel` Search mode to score and include desktop actions
  - REQs: REQ-F-010, REQ-F-012, REQ-F-013
  - Check: Searching "file" in a model with Firefox and its "Open File" action produces both the app and the action as separate results with `IsActionRole` differentiating them.

- [x] T-008: Add `activeCategory` property and `setActiveCategory()` invokable to `LauncherService`
  - REQs: REQ-F-002, REQ-F-003, REQ-F-007
  - Check: Calling `LauncherService::setActiveCategory("Games")` filters `results` to games only and clicking "All" shows all apps.

- [x] T-009: Add `availableCategories()`, `countForCategory()`, and `desktop_file_index_` cache to `LauncherService`
  - REQs: REQ-F-006, REQ-F-034
  - Check: `LauncherService::availableCategories()` returns 12 categories in curated order and `countForCategory()` matches the number of visible apps when filtered.

- [x] T-010: Add `selectedEntryName`, `selectedEntryDesktopFile`, `selectedEntryIcon` derived properties to `LauncherService`
  - REQs: REQ-F-017, REQ-F-020
  - Check: These properties reflect the currently selected app/action's details and update when `selectedIndex` changes.

- [x] T-011: Add `appResultCount` and `actionResultCount` properties to `LauncherService`
  - REQs: REQ-F-015, REQ-F-010
  - Check: In Search mode with query "fire", both properties return the correct count of matching apps and actions.

- [x] T-012: Add `launchAction(int entryIndex, int actionIndex)` invokable to `LauncherService`
  - REQs: REQ-F-029, REQ-F-028
  - Check: Calling `launchAction()` executes the action's Exec field with field codes stripped and returns true on success.

- [x] T-013: Add `entryInfoForDesktopFile(desktopFile)` invokable to `LauncherService`
  - REQs: REQ-F-005, REQ-F-026
  - Check: Calling with a known desktop file path returns a map with `name`, `iconName`, and `desktopFile` keys.

- [x] T-014: Wire `RecentAppsTracker::recordLaunch()` into `LauncherService::launch()` and `launchAction()`
  - REQs: REQ-F-021, REQ-F-024
  - Check: After launching an app via either method, `RecentAppsTracker.recentEntries(5)` includes it as the most recent entry.

- [x] T-015: Register `RecentAppsTracker` in `CMakeLists.txt` and application startup
  - REQs: REQ-NF-008
  - Check: `RecentAppsTracker` is callable from QML without instantiation boilerplate and survives singleton registration in the application startup code.

## QML Components

- [x] T-016: Add `appDesktopFile` required property to `LauncherResultRow.qml`
  - REQs: REQ-F-005, REQ-F-026
  - Check: `LauncherResultRow` accepts `appDesktopFile` as a required property and the binding in `Launcher.qml` passes the desktop file path without error.

- [x] T-017: Create `LauncherActionRow.qml` with required properties and `activated()` signal
  - REQs: REQ-F-013, REQ-F-030
  - Check: `LauncherActionRow` renders action name and parent app name in a styled frame and emits `activated()` when clicked.

- [x] T-018: Create `LauncherRightPanelBrowse.qml` with RECENT and CATEGORIES sections
  - REQs: REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-026, REQ-F-031, REQ-F-033, REQ-F-034
  - Check: RECENT section shows up to 5 apps ordered newest-first with name/icon, CATEGORIES lists all 12 curated categories with counts, and clicking a category filters the left list.

- [x] T-019: Create `LauncherRightPanelSearch.qml` with FILTERS and SELECTED ITEM sections
  - REQs: REQ-F-015, REQ-F-016, REQ-F-017, REQ-F-018, REQ-F-019, REQ-F-020
  - Check: FILTERS shows app/action counts with clickable toggle buttons, SELECTED ITEM displays name/icon/last-used date and lists desktop actions using `LauncherActionRow`.

- [x] T-020: Restructure `Launcher.qml` body to RowLayout with left/right columns and mode switching
  - REQs: REQ-F-001, REQ-F-006, REQ-F-009, REQ-F-010, REQ-F-015, REQ-F-026, REQ-C-001
  - Check: Left column fills width and switches between alphabetical Browse list and search results with sections; right column (240px) switches between `LauncherRightPanelBrowse` and `LauncherRightPanelSearch`; panel width is 1100px.

- [x] T-021: Wire section headers and filter state in Search mode left column
  - REQs: REQ-F-009, REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-013, REQ-F-016, REQ-F-037
  - Check: Search results show "BEST MATCH" header, app rows grouped under "APPLICATIONS", action rows under "ACTIONS"; filter chips in right panel hide/show sections correctly.

- [x] T-022: Update `CMakeLists.txt` with new QML files in alphabetical order
  - REQs: REQ-NF-001
  - Check: `task configure` and `task qml-lint` pass; the three new Launcher QML files are listed alphabetically within the Launcher group in `HOLONIGHT_QML_FILES`.

## Verification

- [x] T-023: Build project and run `qml-lint` — verify zero errors
  - REQs: REQ-NF-003, REQ-NF-004
  - Check: `task build` completes without errors and `task qml-lint` reports zero warnings/errors on all Launcher QML files.

- [ ] T-024: Manual smoke test — Browse mode, Search mode, Recent tracking, action launching
  - REQs: REQ-F-001, REQ-F-005, REQ-F-007, REQ-F-009, REQ-F-021, REQ-NF-001, REQ-NF-002
  - Check: Opening launcher shows alphabetical app list in Browse; typing "fire" transitions to Search with BEST MATCH/APPLICATIONS/ACTIONS sections; launching an app adds it to Recent; launching a desktop action records the parent app in Recent.
