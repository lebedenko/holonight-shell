# POC Remediation Phase 5 SPEC

## Overview

Phase 5 addresses "Built-but-unwired API audit" (Gap #6, §5.6) from the POC Readiness Review (docs/sdd/poc-readiness-review/REPORT.md §7). Phases 0–4 are complete. This phase is predominantly code cleanup: four deletions of superseded or dead code, plus one performance wire-in of existing infrastructure. No user-facing behavior changes; all changes are internal refactoring.

**Key goals:**
- Delete PopupGeometry (superseded by StatusPopupGeometry).
- Delete sidebarSurfaceWidth() function (unused; keep sibling functions in same file).
- Delete KdeCompatService entirely (superseded by SessionIntegrationService).
- Wire LauncherService::desktop_file_index_ for O(1) lookup, replacing O(n) linear scans.
- Delete SidebarSessionBar.qml (duplicated by SidebarTabBar.qml sessionMenu).

All acceptance criteria are verification-only: grep for zero references, confirm build succeeds, run existing test suites without regression. No live-compositor tests required.

---

## 1. Delete PopupGeometry

### Context

`PopupGeometry` (header and implementation in `libs/holonight-surfaces/src/PopupGeometry.h` and `.cpp`) is a struct and free function `sessionPopupGeometry()` used for popup geometry calculations. It is superseded by the parallel, actively-used `StatusPopupGeometry` and `StatusPopupSurface` pair, which is currently the canonical popup-positioning mechanism in the codebase. `PopupGeometry` has zero production callers and exists only as a dedicated test `tests/test_popup_geometry.cpp`.

Removing this dead API reduces codebase complexity and eliminates a maintenance burden (two slightly different, competing APIs for the same problem).

### Requirements

**REQ-F-001: Delete PopupGeometry header and implementation files**

*When* the Phase 5 implementation stage begins, *the system* shall remove `libs/holonight-surfaces/src/PopupGeometry.h` and `libs/holonight-surfaces/src/PopupGeometry.cpp` from the repository.

- **Acceptance:** A grep search of `libs/` and `apps/` directories for `PopupGeometry` (excluding deleted-file diffs) returns zero matches. The files no longer exist on disk.

---

**REQ-F-002: Delete PopupGeometry test file and CMakeLists registration**

*After* removing PopupGeometry source files, *the system* shall remove the dedicated test file `tests/test_popup_geometry.cpp` and remove its entry from `tests/CMakeLists.txt`.

- **Acceptance:** `tests/test_popup_geometry.cpp` does not exist. Grep of `tests/CMakeLists.txt` for `test_popup_geometry` returns zero matches. `ctest` output does not list `test_popup_geometry` as a registered test.

---

**REQ-C-001: Ensure build succeeds after PopupGeometry deletion**

*The system* shall confirm that the CMake build remains green after deleting PopupGeometry files and test references.

- **Acceptance:** Run `task build` (or `task configure && task build`) after applying deletions; the build completes without errors or unresolved-reference linker failures.

---

## 2. Delete sidebarSurfaceWidth() Function

### Context

`SidebarSurfacePolicy.h` and `SidebarSurfacePolicy.cpp` declare three functions: `sidebarDefaultHeight()`, `boundedSidebarHeight()` (both actively used by `SidebarManager.cpp`), and `sidebarSurfaceWidth()` (zero callers). The sidebar layer-surface uses compositor auto-sizing (layer-shell `set_size(0,0)`) and QML's dynamic width binding (`panelWidth: tabBar.width + contentArea.width`), making `sidebarSurfaceWidth()` redundant and dead code.

Removing this function simplifies the policy API and eliminates confusion (callers don't need to reason about two competing width-computation mechanisms). The file itself and its other two functions must be retained.

### Requirements

**REQ-F-003: Delete sidebarSurfaceWidth() function only**

*When* Phase 5 implementation begins, *the system* shall remove the `sidebarSurfaceWidth()` free function from `libs/holonight-surfaces/src/SidebarSurfacePolicy.h` and `libs/holonight-surfaces/src/SidebarSurfacePolicy.cpp`, while preserving `sidebarDefaultHeight()` and `boundedSidebarHeight()`.

- **Acceptance:** A grep search for `sidebarSurfaceWidth` in `libs/` and `apps/` (excluding deleted-function diffs) returns zero matches. A grep search for `sidebarDefaultHeight` and `boundedSidebarHeight` in `SidebarManager.cpp` returns 2+ matches each, confirming those functions remain intact.

---

**REQ-F-004: Remove test assertions for sidebarSurfaceWidth() only**

*After* deleting the `sidebarSurfaceWidth()` function, *the system* shall remove only the test assertions in `tests/test_sidebar_surface_policy.cpp` that validate `sidebarSurfaceWidth()`'s behavior, while preserving all assertions for `sidebarDefaultHeight()` and `boundedSidebarHeight()`.

- **Acceptance:** Run `ctest -R test_sidebar_surface_policy --verbose` after changes; the test passes. A manual inspection of `test_sidebar_surface_policy.cpp` confirms:
  1. No assertions referencing `sidebarSurfaceWidth()` remain.
  2. Assertions for `sidebarDefaultHeight()` and `boundedSidebarHeight()` are unchanged.

---

**REQ-C-002: sidebarSurfaceWidth() removal does not break build**

*The system* shall confirm build succeeds after removing `sidebarSurfaceWidth()` and its test references.

- **Acceptance:** Run `task build` after applying deletions; no unresolved-reference or missing-declaration errors appear.

---

## 3. Delete KdeCompatService Entirely

### Context

`KdeCompatService` (header `libs/holonight-services/src/kde-compat/KdeCompatService.h`, implementation `KdeCompatService.cpp`, plus QML `QML_ELEMENT`/`QML_SINGLETON` registration) is a diagnostic service that surfaces KDE-menu and environment metadata (e.g., `kbuildsycoca6` invocation, `XDG_MENU_PREFIX` probing). It was added in an early phase but has zero QML consumers — no QML file ever imported or used it.

The same diagnostic information (and more, with richer state) is now provided by `SessionIntegrationService` (in `libs/holonight-services/src/session-integration/`), which is actively wired into production QML (`apps/shell/qml/RightSidebar/Tabs/System/SidebarSystem.qml`'s "Session Integration" section) and surfaces multiple diagnostics including KDE-menu rebuilds, refresh state, and rebuild-in-progress flags.

Removing `KdeCompatService` eliminates dead code, eliminates maintenance burden, and clarifies that `SessionIntegrationService` is the single canonical source of these diagnostics.

### Requirements

**REQ-F-005: Delete KdeCompatService header, implementation, and registration**

*When* Phase 5 implementation begins, *the system* shall delete:
1. `libs/holonight-services/src/kde-compat/KdeCompatService.h`
2. `libs/holonight-services/src/kde-compat/KdeCompatService.cpp`
3. The `QML_ELEMENT`/`QML_SINGLETON` registration directives for KdeCompatService
4. The entry for KdeCompatService in `libs/holonight-services/CMakeLists.txt` (source file and header listings)

- **Acceptance:** The files no longer exist on disk. A grep search of `libs/holonight-services/CMakeLists.txt` for `KdeCompatService` returns zero matches. A grep of `libs/holonight-services/src/kde-compat/` returns zero `.cpp` or `.h` files (only CMakeLists.txt or empty directory may remain; confirm via `ls`).

---

**REQ-F-006: Verify no other code references KdeCompatService**

*After* deleting KdeCompatService files, *the system* shall confirm that no other source file, header, QML file, or CMakeLists.txt entry references KdeCompatService.

- **Acceptance:** Run a grep across the entire project (`grep -r "KdeCompatService" libs/ apps/ tests/ --include="*.h" --include="*.cpp" --include="*.qml" --include="CMakeLists.txt"`); the result is zero lines (or only deleted-file diffs if using git). Manually inspect `CMakeLists.txt` in parent directories and the qmldir file (if any) to confirm no registration references remain.

---

**REQ-F-007: Delete KdeCompatService test**

*The system* shall remove the dedicated test `tests/test_kde_compat_service.cpp` and remove its entry from `tests/CMakeLists.txt`.

- **Acceptance:** The file `tests/test_kde_compat_service.cpp` does not exist. `grep test_kde_compat_service tests/CMakeLists.txt` returns zero matches.

---

**REQ-C-003: KdeCompatService removal does not break build or QML registration**

*The system* shall confirm that the CMake build succeeds and QML type registration completes (no missing or dangling type references) after deleting KdeCompatService.

- **Acceptance:** Run `task configure && task build`. The build completes without errors. Run `task qmltypes-check` to verify the generated qmltypes file includes all expected C++ singletons and does not contain only an empty `Module {}`. No QML engine-registration errors appear in logs.

---

## 4. Wire LauncherService::desktop_file_index_ for O(1) Lookup

### Context

`LauncherService` maintains a `QHash<QString, int> desktop_file_index_` that maps desktop-file paths to row indices in the launcher model. This index is rebuilt on every launcher scan via `rebuildDesktopFileIndex()` but is never read. Meanwhile, `LauncherModel::findEntryByDesktopFile(const QString&)` performs a naive O(n) linear scan over the model's entries on every call. This function is invoked from two production call sites in `LauncherService.cpp`:
1. `entryInfoForDesktopFile()` (around line 333)
2. `launchDesktopFile()` (around line 424)

Wiring the index into these lookups converts O(n) scans to O(1) hash lookups, improving performance without changing behavior. Existing tests must pass unmodified; a new test verifies index consistency after rebuilds.

### Requirements

**REQ-F-008: Use desktop_file_index_ for O(1) lookup in findEntryByDesktopFile()**

*When* `LauncherModel::findEntryByDesktopFile(const QString& desktop_file)` is called, *the system* shall look up the entry index in `LauncherService::desktop_file_index_` (via `index()` and return `entries_[idx]` if found) instead of performing a linear scan over all entries.

- **Acceptance:**
  1. A unit test constructs a LauncherModel with 1000 test entries.
  2. Calls `findEntryByDesktopFile("some-app.desktop")` (which exists in the model).
  3. Verifies the call returns the correct entry (matching the O(n) result).
  4. Measures that repeated calls complete in negligible time (<1ms aggregate, compared to O(n) baseline of ~10-50ms for 1000 entries).
  5. Calls with a non-existent desktop file; verifies `nullptr` is returned (behavior unchanged).

---

**REQ-F-009: Implement safe index-based accessor or update findEntryByDesktopFile() signature**

*The system* shall provide a mechanism for `findEntryByDesktopFile()` to access `desktop_file_index_` and retrieve entries by index safely (bounds-checked, null-safe).

- **Acceptance:**
  1. The implementation either:
     a. Adds a public or friend-accessible method `LauncherModel::allEntryAt(int idx)` (or similar name) that returns `entries_[idx]` with bounds checking, or
     b. Migrates the lookup logic into a shared location (e.g., a private helper) that both `findEntryByDesktopFile()` and any other future callers can reuse.
  2. A code review confirms the accessor is used correctly (index range is valid, null-return cases are handled).

---

**REQ-NF-002: Preserve exact behavior of entryInfoForDesktopFile() and launchDesktopFile()**

*The system* shall not modify the external behavior of `LauncherService::entryInfoForDesktopFile(const QString&)` or `launchDesktopFile(const QString&)`; the only internal change is using the index for O(1) lookup instead of O(n) scan.

- **Acceptance:**
  1. Existing GTests exercising both functions pass without modification.
  2. Behavior edge cases (entry not found → null/false return; entry found → correct info/command returned) remain identical.
  3. A diff of the calling code in LauncherService.cpp shows only the internal lookup mechanism changed; the return statement and error handling remain unchanged.

---

**REQ-F-010: Verify index consistency after rebuildDesktopFileIndex()**

*After* `rebuildDesktopFileIndex()` is called (or internally when entries are added/removed), *the system* shall ensure the index remains consistent with the actual entries list and subsequent lookups use correct indices.

- **Acceptance:**
  1. A new GTest:
     a. Constructs a LauncherModel with initial entries.
     b. Calls `rebuildDesktopFileIndex()`.
     c. Calls `findEntryByDesktopFile()` on a known entry; verifies the returned entry matches.
     d. Adds a new entry to the model and triggers a rescan (if applicable).
     e. Calls `findEntryByDesktopFile()` on the new entry; verifies the returned entry is correct.
     f. Removes an entry and triggers a rescan.
     g. Calls `findEntryByDesktopFile()` on the removed entry; verifies `nullptr` is returned.
  2. The test uses spy or log assertions to confirm `rebuildDesktopFileIndex()` is called and the index state is consistent afterward.

---

## 5. Delete SidebarSessionBar.qml

### Context

`apps/shell/qml/RightSidebar/Tabs/System/SidebarSessionBar.qml` is a QML component that provides a profile button and dropdown menu with session actions (Lock, Log Out, Reboot, Shut Down) wired to `SessionService`. This functionality is already live and in production in `apps/shell/qml/RightSidebar/SidebarTabBar.qml`, which includes an identical profile-button dropdown (`sessionMenu`) with the same actions. `SidebarSessionBar.qml` has zero references in any parent QML file and is dead code.

Removing this file eliminates duplication and confusion about which session-menu implementation is canonical (now clearly `SidebarTabBar.qml`'s `sessionMenu`).

### Requirements

**REQ-F-011: Delete SidebarSessionBar.qml**

*When* Phase 5 implementation begins, *the system* shall remove `apps/shell/qml/RightSidebar/Tabs/System/SidebarSessionBar.qml` from the repository.

- **Acceptance:** The file no longer exists on disk. A grep search of all `.qml` files for `SidebarSessionBar` (excluding deleted-file diffs) returns zero matches.

---

**REQ-F-012: Verify no QML file references SidebarSessionBar**

*After* removing SidebarSessionBar.qml, *the system* shall confirm that no QML file imports, instantiates, or references it by name (e.g., via `import` directives, loader sources, or id references).

- **Acceptance:** Run `grep -r "SidebarSessionBar" apps/shell/qml/ --include="*.qml"` (excluding deleted-file diffs); result is zero lines. Manually inspect `SidebarTabBar.qml` and any CMake QML resource listings to confirm no explicit file references remain.

---

**REQ-C-004: SidebarSessionBar deletion does not break build or QML module**

*The system* shall confirm that the CMake build and QML module registration remain green after deleting SidebarSessionBar.qml.

- **Acceptance:** Run `task build` and `task qmltypes-check` after deletion; both complete without errors. A visual inspection of the right sidebar in a running shell (if available) or a QML type-check confirms no unresolved type references.

---

## Non-Goals

The following items are **explicitly out of scope** for Phase 5:

1. **CalendarCache account-removal API wiring:** This was completed in Phase 4 (REQ-F-012, "Wire account removal into config-change path"). Phase 5 does not revisit calendar-service wiring.

2. **Behavior changes to currently-wired code:** All Phase 5 items are either deletions (zero impact) or the LauncherService wire-in, which is a pure performance optimization with zero external behavior change. No existing production code paths are altered in their observable behavior.

3. **New features or enhancements:** Phase 5 is cleanup only.

4. **Live-compositor smoke tests:** All acceptance criteria are verification-only (grep, build, unit tests). No Hyprland, sidebar toggles, or manual testing required.

5. **Broader refactoring:** Only the five specified items are in scope. Other stale-backlog tasks (portal-service, idle-management, etc.) remain out of scope per STALE-BACKLOG.md.

---

## Verification Strategy

**Deletion Verification:**

All four deletions (PopupGeometry, sidebarSurfaceWidth(), KdeCompatService, SidebarSessionBar.qml) are verified via:

1. **File existence check:** Confirm file no longer exists on disk (`ls -l <file>` or `find` returns no matches).
2. **Grep verification:** Grep the project (`libs/`, `apps/`, `tests/`) for the deleted entity name; zero matches indicates no lingering references.
3. **Build verification:** `task build` completes without unresolved-reference errors.
4. **CMakeLists.txt verification:** For C++ deletions, inspect relevant `CMakeLists.txt` files; confirm source file and test entries are removed.
5. **QML registration verification:** For KdeCompatService, run `task qmltypes-check` to confirm no dangling type references.

**Wire-In Verification (LauncherService):**

1. **Unit tests (existing + new):** Run existing tests for `entryInfoForDesktopFile()` and `launchDesktopFile()` unmodified; all pass.
2. **Performance test:** New test verifies O(1) lookup speed and correctness after index rebuild.
3. **Index consistency test:** New test exercises add/remove/rescan cycles and confirms lookups remain correct.
4. **Behavior regression test:** Grep confirms external behavior (return types, error cases) is unchanged.

**No Live-Compositor Tests:**

This phase does NOT require:
- Launching Hyprland or Wayland compositor.
- Manually opening sidebars or toggling widgets.
- Live launcher searches or desktop-file lookups.
- Screenshot validation or visual inspection (except optional QML type-check).

---

## Summary

**5 scope items, 16 requirements:**

| Item | Category | Count | IDs |
|------|----------|-------|-----|
| 1. Delete PopupGeometry | F, C | 3 | REQ-F-001, REQ-F-002, REQ-C-001 |
| 2. Delete sidebarSurfaceWidth() | F, C | 3 | REQ-F-003, REQ-F-004, REQ-C-002 |
| 3. Delete KdeCompatService | F, C | 4 | REQ-F-005, REQ-F-006, REQ-F-007, REQ-C-003 |
| 4. Wire desktop_file_index_ | F, NF | 4 | REQ-F-008, REQ-F-009, REQ-NF-002, REQ-F-010 |
| 5. Delete SidebarSessionBar.qml | F, C | 3 | REQ-F-011, REQ-F-012, REQ-C-004 |
| **Total** | | **16** | |

Functional requirements (REQ-F) cover deletion steps, wire-in implementation, and verification of removal. Non-functional and constraint requirements (REQ-NF, REQ-C) ensure build integrity, behavior preservation, and registration completeness. Each requirement is independently falsifiable via grep, build, unit tests, or CMake verification.
