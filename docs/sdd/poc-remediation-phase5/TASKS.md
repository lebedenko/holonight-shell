# SDD Tasks — poc-remediation-phase5

## Item 1 — Delete PopupGeometry

- [x] T-001: Delete PopupGeometry header, implementation, and test files; update CMakeLists.txt
  - REQs: REQ-F-001, REQ-F-002, REQ-C-001
  - Check: (1) Delete `libs/holonight-surfaces/src/PopupGeometry.h` and `libs/holonight-surfaces/src/PopupGeometry.cpp`; (2) In `libs/holonight-surfaces/CMakeLists.txt`, remove lines 21–22 (the two lines listing `src/PopupGeometry.h` and `src/PopupGeometry.cpp`); (3) Delete `tests/test_popup_geometry.cpp`; (4) In `tests/CMakeLists.txt`, remove line 133 (the `test_popup_geometry.cpp` entry from the `holonight_add_test_exe(test_holonight_surfaces ...)` call); (5) Run `grep -r "PopupGeometry" libs/ apps/ tests/ --include="*.h" --include="*.cpp"` — result is zero matches; (6) Run `task build` — build completes without unresolved-reference errors; (7) Run `ctest -N | grep -i popup_geometry` — result is zero matches (test no longer registered).

---

## Item 2 — Delete sidebarSurfaceWidth() Function

- [x] T-002: Remove sidebarSurfaceWidth() function and its test assertion
  - REQs: REQ-F-003, REQ-F-004, REQ-C-002
  - Check: (1) In `libs/holonight-surfaces/src/SidebarSurfacePolicy.h`, remove the declaration line `[[nodiscard]] int sidebarSurfaceWidth();` (confirm file has 4 remaining lines: pragma, blank line, `sidebarDefaultHeight()`, `boundedSidebarHeight()`); (2) In `libs/holonight-surfaces/src/SidebarSurfacePolicy.cpp`, remove the function definition line `int sidebarSurfaceWidth() { return kSidebarMaxContentWidth + kSidebarTabBarWidth; }` (confirm `#include` lines and helper constants remain for `sidebarDefaultHeight()`/`boundedSidebarHeight()`); (3) In `tests/test_sidebar_surface_policy.cpp`, remove line 5 only: `TEST(SidebarSurfacePolicy, PanelWidthCombinesContentAndTabBarWidth) { EXPECT_EQ(sidebarSurfaceWidth(), 464); }` (preserve lines 7, 9–19 with `DefaultHeightMatchesInitialPanelHeight`, `BoundsHeightToMinimumAndScreenAvailableSpace`, `BoundsHeightToDefaultWhenScreenIsMissing`); (4) Run `grep -rn "sidebarSurfaceWidth" libs/ apps/` — result is zero matches; (5) Run `grep -n "sidebarDefaultHeight\|boundedSidebarHeight" libs/holonight-surfaces/src/SidebarManager.cpp` — result is 2+ matches each (confirming both sibling functions remain in use); (6) Run `task build` — build completes without errors; (7) Run `ctest -R test_sidebar_surface_policy --verbose` — test passes, exactly 3 `TEST()` blocks remain.

---

## Item 3 — Delete KdeCompatService

- [x] T-003: Delete KdeCompatService source files and wiring from ShellApplication
  - REQs: REQ-F-005, REQ-F-006
  - Check: (1) Delete `libs/holonight-services/src/kde-compat/KdeCompatService.h` and `libs/holonight-services/src/kde-compat/KdeCompatService.cpp`; (2) In `libs/holonight-services/CMakeLists.txt`, remove line 33 (the `${CMAKE_CURRENT_SOURCE_DIR}/src/kde-compat` include-dir entry); (3) In `apps/shell/app/ShellApplication.h`, remove line 16 (the forward declaration `class KdeCompatService;`) and remove line 137 (the member `KdeCompatService* kde_compat_service_ = nullptr;`); (4) In `apps/shell/app/ShellApplication.cpp`, remove line 16 (the `#include "KdeCompatService.h"` directive), remove line 128 (the `kde_compat_service_(new KdeCompatService(this)),` constructor-init line), and remove line 151 (the `reg(kde_compat_service_, "KdeCompatService");` call in `registerQmlTypes()`); (5) Run `ls libs/holonight-services/src/kde-compat/` — directory should be empty or not exist; (6) Run `grep -r "KdeCompatService" libs/holonight-services/CMakeLists.txt` — result is zero matches.

- [x] T-004: Delete KdeCompatService test file and verify no dangling references
  - REQs: REQ-F-007, REQ-C-003
  - Check: (1) Delete `tests/test_kde_compat_service.cpp`; (2) In `tests/CMakeLists.txt`, remove line 111 (the `test_kde_compat_service.cpp` entry from the `holonight_add_test_exe(test_holonight_services ...)` call); (3) Run `grep -r "KdeCompatService" libs/ apps/ tests/ --include="*.h" --include="*.cpp" --include="*.qml" --include="CMakeLists.txt"` — result is zero matches; (4) Run `task configure && task build` — build completes without errors; (5) Run `task qmltypes-check` — passes (confirms `KdeCompatService` was never in the required_types list, so its absence does not fail the check); (6) Run `ctest -N | grep -i kde_compat` — result is zero matches.

---

## Item 4 — Wire LauncherService::desktop_file_index_ for O(1) Lookup

- [x] T-005: Implement desktop_file_index_ member and rebuild hook in LauncherModel
  - REQs: REQ-F-008, REQ-F-009
  - Check: (1) In `LauncherModel.h`, add `#include <QHash>` (if not already present); (2) In the `private:` section of `LauncherModel.h`, add new member `QHash<QString, int> desktop_file_index_;` and declare new private method `void rebuildDesktopFileIndex();`; (3) In `LauncherModel.cpp`, implement `rebuildDesktopFileIndex()` to clear the hash, reserve space for `entries_.size()`, and populate with `insert(entries_.at(idx).desktop_file, idx)` for each index; (4) In `LauncherModel::setEntries()`, call `rebuildDesktopFileIndex();` after `entries_ = std::move(entries);` and before `rebuildResults();`; (5) Rewrite `LauncherModel::findEntryByDesktopFile()` (`LauncherModel.cpp:176-183`) to use `desktop_file_index_.constFind()`, return `nullptr` if not found, or return `allEntryAt(index_it.value())` if found (reusing the existing public bounds-checked accessor); (6) Run `grep -n "allEntryAt" libs/holonight-services/src/launcher/LauncherModel.cpp` — confirm the method exists and is public (should show definition and call site).

- [x] T-006: Update LauncherService to remove dead index member and rename rebuildDesktopFileIndex()
  - REQs: REQ-NF-002, REQ-F-010
  - Check: (1) In `LauncherService.h`, delete the member `QHash<QString, int> desktop_file_index_;` (line 139) and delete the `#include <QHash>` from the top of the file (confirm no other `QHash`-typed member exists in the header — `category_counts_cache_` should be `QMap`, not `QHash`); (2) In `LauncherService.cpp`, locate the private method `rebuildDesktopFileIndex()` and rename it to `onModelEntriesReset()`; (3) Rewrite the body of `onModelEntriesReset()` to remove all index-building lines (clear and rebuild of `desktop_file_index_`), keeping only the three surviving responsibilities: `invalidateCategoryCache()`, `invalidateDefaultAppsCache()`, and the `available_desktop_files` loop that calls `recent_apps_tracker_->removeUnavailableDesktopFiles()`; (4) In the same file, locate the constructor's `modelReset` connection (around `.cpp:190-194`) and update the lambda call site from `rebuildDesktopFileIndex();` to `onModelEntriesReset();`; (5) Run `grep -n "rebuildDesktopFileIndex\|onModelEntriesReset" libs/holonight-services/src/launcher/*.cpp libs/holonight-services/src/launcher/*.h` — confirm `rebuildDesktopFileIndex` appears only in `LauncherModel.cpp` and `onModelEntriesReset` appears only in `LauncherService.{h,cpp}`; (6) Run `task build` — build completes without unresolved-reference or missing-declaration errors; (7) Manually inspect diff: both call sites to `model_.findEntryByDesktopFile()` in `LauncherService.cpp` (`entryInfoForDesktopFile()` and `launchDesktopFile()`) must be textually unchanged (verify diff shows zero lines changed at those locations).

- [x] T-007: Extend test suite with O(1) lookup performance and index consistency verification
  - REQs: REQ-F-008, REQ-F-010, REQ-NF-002
  - Check: (1) In `tests/test_launcher_service.cpp`, add new `TEST(LauncherModel, O1LookupPerformance)`: construct a 1000-entry `QVector<DesktopEntry>` by looping `makeEntry()` with unique desktop-file names, instantiate a bare `LauncherModel`, call `model.setEntries(std::move(entries))`, use `QElapsedTimer` to measure repeated `findEntryByDesktopFile()` calls, assert aggregate time is <1ms; (2) Verify `findEntryByDesktopFile("nonexistent.desktop")` returns `nullptr`; (3) Add new `TEST(LauncherModel, IndexConsistencyAfterSetEntries)`: construct model with initial entries, call `rebuildDesktopFileIndex()` (via `setEntries()`), call `findEntryByDesktopFile()` on a known entry, verify returned entry matches; repeat with a second `setEntries()` call that adds a new entry and drops an old one, call `findEntryByDesktopFile()` on the new entry, verify it is found; call with the removed entry, verify `nullptr` is returned; (4) Verify existing tests unmodified: `ScansSearchesAndRanksApplications`, `LaunchSelectedDispatchesToActionWhenActionRowSelected`, and the existing `launchDesktopFile()` test (line 395) pass without changes; (5) Run `task configure-tests && task build`; (6) Run `ctest -R test_holonight_services --output-on-failure` — all tests pass, including new LauncherModel tests and existing LauncherService tests.

---

## Item 5 — Delete SidebarSessionBar.qml

- [x] T-008: Delete SidebarSessionBar.qml and verify no references
  - REQs: REQ-F-011, REQ-F-012, REQ-C-004
  - Check: (1) Delete `apps/shell/qml/RightSidebar/Tabs/System/SidebarSessionBar.qml`; (2) Run `ls apps/shell/qml/RightSidebar/Tabs/System/SidebarSessionBar.qml` — result is "No such file"; (3) Run `grep -r "SidebarSessionBar" apps/shell/qml/ --include="*.qml"` — result is zero matches; (4) Confirm `apps/shell/CMakeLists.txt` line 44-49 shows `file(GLOB_RECURSE ... CONFIGURE_DEPENDS qml/*.qml)` (file is picked up automatically on next CMake configure); (5) Manually inspect `apps/shell/qml/RightSidebar/SidebarTabBar.qml` and `apps/shell/qml/RightSidebar/Tabs/System/SidebarSystem.qml` — confirm zero references to `SidebarSessionBar` in either file; (6) Run `task build` — build completes without errors; (7) Run `task qmltypes-check` — passes (confirms QML module registration is intact).

---

## Whole-Phase Verification Gate

- [x] T-009: Run complete build, format, and test suite
  - REQs: REQ-C-001, REQ-C-002, REQ-C-003, REQ-C-004
  - Check: (1) Run `task format-check` — verify attribution against `git status` (pre-existing drift in untouched files is not this phase's responsibility per CLAUDE.md); (2) Run `task tidy` — same attribution caveat; (3) Run `task test` — full `ctest` suite, all targets pass without regression; (4) No unresolved-reference or missing-declaration linker errors.

---

## Summary

**9 tasks covering all 16 requirements:**

| Item | Count | Task IDs | Requirement IDs |
|------|-------|----------|-----------------|
| 1. Delete PopupGeometry | 1 | T-001 | REQ-F-001, REQ-F-002, REQ-C-001 |
| 2. Delete sidebarSurfaceWidth() | 1 | T-002 | REQ-F-003, REQ-F-004, REQ-C-002 |
| 3. Delete KdeCompatService | 2 | T-003, T-004 | REQ-F-005, REQ-F-006, REQ-F-007, REQ-C-003 |
| 4. Wire desktop_file_index_ | 3 | T-005, T-006, T-007 | REQ-F-008, REQ-F-009, REQ-NF-002, REQ-F-010 |
| 5. Delete SidebarSessionBar.qml | 1 | T-008 | REQ-F-011, REQ-F-012, REQ-C-004 |
| Verification Gate | 1 | T-009 | REQ-C-001, REQ-C-002, REQ-C-003, REQ-C-004 |
| **Total** | **9** | **T-001–T-009** | **All 16** |

**Sequencing:** All items independent; may be implemented in any order or in parallel. Recommendation: one commit per item (5 commits), with T-009 verification gate run after all changes (per DESIGN.md § Sequencing). Each item's `git revert` is independently safe.
