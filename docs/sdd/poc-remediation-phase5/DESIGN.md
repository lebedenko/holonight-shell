# POC Remediation Phase 5 — Design

Stage 2 (Design) for `docs/sdd/poc-remediation-phase5/SPEC.md`. Satisfies REQ-F-001 through
REQ-F-012, REQ-NF-002, and REQ-C-001 through REQ-C-004. Five scope items — four deletions
(PopupGeometry, `sidebarSurfaceWidth()`, KdeCompatService, `SidebarSessionBar.qml`) and one wire-in
(`LauncherService::desktop_file_index_` → O(1) lookup). SPEC frames this phase as "predominantly
code cleanup"; this design confirms that framing for four of five items and documents one
genuinely non-trivial internal restructuring for the fifth (Item 4).

Grounded against the actual current source, read in full for every touched file:
`libs/holonight-surfaces/src/PopupGeometry.{h,cpp}`, `SidebarSurfacePolicy.{h,cpp}`,
`libs/holonight-services/src/kde-compat/KdeCompatService.{h,cpp}`,
`libs/holonight-services/src/launcher/LauncherService.{h,cpp}` and `LauncherModel.{h,cpp}`,
`apps/shell/qml/RightSidebar/Tabs/System/SidebarSessionBar.qml` and its siblings, plus every
`CMakeLists.txt` in the dependency chain and `apps/shell/app/ShellApplication.{h,cpp}`.

**Headline correction to SPEC's own framing**: SPEC's REQ-F-005/006 describe KdeCompatService as
having "zero QML consumers" and scope its deletion to header/cpp/registration-macro/CMakeLists.
Direct source reading shows it also has a **production C++ consumer**: `ShellApplication.cpp`
constructs it (`kde_compat_service_(new KdeCompatService(this))`) and manually registers it as a
QML singleton via `qmlRegisterSingletonType` in `registerQmlTypes()`. "Zero QML consumers" is
correct (no `.qml` file ever references it) but incomplete — REQ-F-006's own broader verification
step ("no other source file... references KdeCompatService") already covers this, so no SPEC
requirement is violated, but Stage-4 implementation would miss `ShellApplication.{h,cpp}` if it
scoped the change to REQ-F-005's literal file list alone. See Item 3 below for the exhaustive list.

---

## Component Map

| Item | File(s) touched | File(s) deleted | CMakeLists.txt edits needed? |
|---|---|---|---|
| 1. Delete PopupGeometry | `libs/holonight-surfaces/CMakeLists.txt`, `tests/CMakeLists.txt` | `PopupGeometry.h`, `PopupGeometry.cpp`, `tests/test_popup_geometry.cpp` | **Yes** — both are explicit source lists |
| 2. Delete `sidebarSurfaceWidth()` | `SidebarSurfacePolicy.h`, `SidebarSurfacePolicy.cpp`, `tests/test_sidebar_surface_policy.cpp` | none (file retained) | No |
| 3. Delete KdeCompatService | `apps/shell/app/ShellApplication.h`, `ShellApplication.cpp`, `libs/holonight-services/CMakeLists.txt` (include-dir line only), `tests/CMakeLists.txt` | `KdeCompatService.h`, `.cpp`, `tests/test_kde_compat_service.cpp`, `src/kde-compat/` dir | Only `tests/CMakeLists.txt` (explicit list); `holonight-services` CMakeLists.txt is `GLOB_RECURSE` for sources — no source-list edit required |
| 4. Wire `desktop_file_index_` | `LauncherModel.h`, `LauncherModel.cpp`, `LauncherService.h`, `LauncherService.cpp`, `tests/test_launcher_service.cpp` | none | No |
| 5. Delete `SidebarSessionBar.qml` | none besides the deleted file | `SidebarSessionBar.qml` | No — `apps/shell/CMakeLists.txt` is `GLOB_RECURSE` for QML |

Two CMakeLists.txt files in this repo use an **explicit** source list — `libs/holonight-surfaces/CMakeLists.txt`
and `tests/CMakeLists.txt` — and both need literal line removal for Items 1 and 3. Everything else
touched by this phase (`libs/holonight-services/CMakeLists.txt`'s source list, `apps/shell/CMakeLists.txt`'s
QML file list) is `file(GLOB_RECURSE ... CONFIGURE_DEPENDS ...)`, so those need **no** edit for
files that simply cease to exist — CMake re-globs automatically on the next configure.

---

## Item 1 — Delete PopupGeometry

### Context recap

`PopupGeometry` (`libs/holonight-surfaces/src/PopupGeometry.h`/`.cpp`) is a 4-field struct plus one
free function `sessionPopupGeometry()` returning fixed constants (`width=160, height=150,
top_margin=4, right_margin=8`). Confirmed via repo-wide grep: its only production include site
would have been a popup surface class, and there is none — `StatusPopupGeometry`/`StatusPopupSurface`
(same directory) is the actively-used pair. The sole reference to `PopupGeometry` anywhere in the
tree is its own dedicated test, `tests/test_popup_geometry.cpp`.

### Exact removal

- Delete `libs/holonight-surfaces/src/PopupGeometry.h` and `PopupGeometry.cpp` (17 lines combined).
- `libs/holonight-surfaces/CMakeLists.txt` — this target's source list is **explicit**, not GLOB.
  Remove lines 21–22:
  ```
      src/PopupGeometry.h
      src/PopupGeometry.cpp
  ```
- Delete `tests/test_popup_geometry.cpp` (13 lines, one `TEST(PopupGeometry, ...)`).
- `tests/CMakeLists.txt` — remove line 133 (`test_popup_geometry.cpp`) from the
  `holonight_add_test_exe(test_holonight_surfaces ...)` call's file list (lines 130–147).

### Key decisions

- **No replacement/forwarding shim.** `sessionPopupGeometry()` has no callers to redirect —
  `StatusPopupGeometry` is a structurally different, independently-evolved API (not a drop-in
  superset), so there is nothing to migrate call sites *to*; this is a pure subtraction.

### Risks

- None beyond the standard "missed a reference" build-break risk, mitigated by the grep step in
  Verification below — the blast radius is two files with a combined 30 lines and one test.

### Requirement map

REQ-F-001, REQ-F-002, REQ-C-001.

---

## Item 2 — Delete `sidebarSurfaceWidth()` Function Only

### Context recap

`SidebarSurfacePolicy.h`/`.cpp` declare three free functions. `sidebarDefaultHeight()` and
`boundedSidebarHeight()` are both actively called from `SidebarManager.cpp`. `sidebarSurfaceWidth()`
(`return kSidebarMaxContentWidth + kSidebarTabBarWidth;`) has zero callers — the sidebar layer
surface uses compositor auto-sizing plus a QML-side dynamic `panelWidth` binding instead. The file
and its other two functions are retained; this is a function-level deletion, not a file deletion —
unlike Items 1, 3, and 5, `libs/holonight-surfaces/CMakeLists.txt` is **not** touched by this item
(the file stays in the source list).

### Exact removal

`SidebarSurfacePolicy.h` — remove line 3 only:
```cpp
[[nodiscard]] int sidebarSurfaceWidth();
```
`SidebarSurfacePolicy.h` after: 4 lines (pragma, blank, `sidebarDefaultHeight()`, `boundedSidebarHeight()`).

`SidebarSurfacePolicy.cpp` — remove line 7 only:
```cpp
int sidebarSurfaceWidth() { return kSidebarMaxContentWidth + kSidebarTabBarWidth; }
```
The `#include "ShellConstants.h"` and `#include <algorithm>` stay — both `kSidebarDefaultHeight`
(used by `sidebarDefaultHeight()`) and `kSidebarMinHeight`/`kSidebarTopMargin`/`kSidebarBottomMargin`/
`std::clamp`/`std::max` (used by `boundedSidebarHeight()`) are still needed. `kSidebarMaxContentWidth`
and `kSidebarTabBarWidth` (from `ShellConstants.h`) become unused **by this file** once the deleted
line is gone — but `ShellConstants.h` is a shared header; those constants may still be referenced
elsewhere (e.g. QML-facing width bindings) and are out of this item's scope to prune. No action
needed in `ShellConstants.h`.

`tests/test_sidebar_surface_policy.cpp` — remove **only** line 5:
```cpp
TEST(SidebarSurfacePolicy, PanelWidthCombinesContentAndTabBarWidth) { EXPECT_EQ(sidebarSurfaceWidth(), 464); }
```
This is the file's only reference to `sidebarSurfaceWidth()`. Lines 7 (`DefaultHeightMatchesInitialPanelHeight`)
and 9–19 (`BoundsHeightToMinimumAndScreenAvailableSpace`, `BoundsHeightToDefaultWhenScreenIsMissing`)
are untouched — they exercise only `sidebarDefaultHeight()`/`boundedSidebarHeight()` and contain no
mention of the deleted function.

### Key decisions

- **Deleted the single-expression function body, not commented it out or `[[deprecated]]`-marked
  it.** SPEC frames this as permanent dead-code removal (REQ-F-003's acceptance is a zero-match
  grep), not a soft deprecation cycle — there is no external consumer (this is an internal static
  library) that would need a deprecation window.

### Risks

- Low — the function body is a single return statement with no side effects and no test beyond the
  one deleted assertion; there is no partial-removal hazard.

### Requirement map

REQ-F-003, REQ-F-004, REQ-C-002.

---

## Item 3 — Delete KdeCompatService Entirely

### Context recap

`KdeCompatService` (`libs/holonight-services/src/kde-compat/`) is a `QML_ELEMENT`/`QML_SINGLETON`
diagnostic service: on construction it probes for `kbuildsycoca6` on `PATH`, checks whether
`XDG_MENU_PREFIX` is set, and exposes `kdeWarningActive`/`rebuildInProgress` properties plus a
`rebuildCaches()` invokable that shells out to `update-desktop-database` then `kbuildsycoca6` via
the shared `GuardedProcessRunner` (Phase 3 infrastructure). `SessionIntegrationService`
(`libs/holonight-services/src/session-integration/`) now provides richer, actively-wired
diagnostics of the same class (visible in `SidebarSystem.qml`'s "Session Integration" section).

**Exhaustive reference list**, from a repo-wide grep for `KdeCompatService` and `kde-compat` across
`.h`/`.cpp`/`.qml`/`CMakeLists.txt`:

| File | Reference | Action |
|---|---|---|
| `libs/holonight-services/src/kde-compat/KdeCompatService.h` | class definition | delete file |
| `libs/holonight-services/src/kde-compat/KdeCompatService.cpp` | implementation | delete file |
| `libs/holonight-services/CMakeLists.txt:33` | `${CMAKE_CURRENT_SOURCE_DIR}/src/kde-compat` include-dir entry | remove line (dir will be empty/gone) |
| `apps/shell/app/ShellApplication.h:16` | `class KdeCompatService;` forward decl | remove line |
| `apps/shell/app/ShellApplication.h:137` | `KdeCompatService* kde_compat_service_ = nullptr;` member | remove line |
| `apps/shell/app/ShellApplication.cpp:16` | `#include "KdeCompatService.h"` | remove line |
| `apps/shell/app/ShellApplication.cpp:128` | `kde_compat_service_(new KdeCompatService(this)),` constructor init | remove line |
| `apps/shell/app/ShellApplication.cpp:151` | `reg(kde_compat_service_, "KdeCompatService");` in `registerQmlTypes()` | remove line |
| `tests/test_kde_compat_service.cpp` | dedicated test file (2 `TEST()`s covering `GuardedProcessRunner` timeout behavior for both subprocess stages) | delete file |
| `tests/CMakeLists.txt:111` | `test_kde_compat_service.cpp` in `test_holonight_services`'s file list | remove line |

No `qmldir` file exists in this repo — QML singleton registration for every C++ service (not just
KdeCompatService) goes exclusively through the `reg()` lambda inside
`ShellApplication::registerQmlTypes()` (see CLAUDE.md: "`holonight-shell` combines metatypes from
several static-library targets through Qt internal CMake APIs"). The `QML_ELEMENT`/`QML_SINGLETON`
macros on the class itself only feed the `qmltypes` metadata generator (for `qmllint`/IDE tooling);
the actual runtime `HolonightShell` module registration is the explicit `reg()` call. Both layers
are deleted together — removing only the `reg()` call while leaving the macros (or vice versa) would
leave a dangling half-registration.

`libs/holonight-services/src/kde-compat/` is a `GLOB_RECURSE` source directory (see Component Map) —
once `KdeCompatService.h`/`.cpp` are deleted, the directory is empty and CMake's next `CONFIGURE_DEPENDS`
regen simply finds nothing there; **no edit to the `HOLONIGHT_SERVICE_SOURCES` glob itself is
needed.** The directory itself may be left in place (empty) or removed with `rmdir` — REQ-F-005's
acceptance criterion ("only CMakeLists.txt or empty directory may remain") explicitly allows either.

### Does this affect `task qmltypes-check`?

No. `scripts/check-qmltypes.sh`'s `required_types` array is `("AudioService" "NotificationService"
"WorkspaceModel")` — `KdeCompatService` was never in the required set, so its absence from the
generated `holonight-shell.qmltypes` after deletion does not fail the check. The script only fails
if a *required* type goes missing or the file is malformed/empty; removing an unlisted singleton is
invisible to it. REQ-C-003's acceptance criterion is satisfied trivially, not by coincidence: this
was verified by reading the script, not assumed.

### Does this affect `scripts/check-architecture-boundaries.sh`?

No, for two independent reasons, both confirmed by reading the script:
1. It only inspects `#include` lines **inside `libs/holonight-surfaces/src/`** for references to
   `libs/holonight-services/src/*.h` basenames. `KdeCompatService.h` was never included from
   `holonight-surfaces` (it's a services-internal diagnostic, only ever consumed by
   `ShellApplication.cpp` at the `apps/shell/app/` composition root, which this script does not
   scan).
2. Its `service_headers` set is computed dynamically via `find ... -name '*.h'` over
   `libs/holonight-services/src` at script run time — not a hardcoded list. Deleting
   `KdeCompatService.h` automatically shrinks that set; no script edit is needed regardless.

Same conclusion applies to Items 1 and 2 (`PopupGeometry`/`SidebarSurfacePolicy` are
`holonight-surfaces`-internal files, not `holonight-services` headers being included *by*
`holonight-surfaces` — the script's direction of concern is the opposite one).

### Key decisions

- **Delete `ShellApplication`'s three touch points in the same change as the header/cpp deletion,
  not as a follow-up.** SPEC's REQ-F-005 doesn't name `ShellApplication.{h,cpp}` explicitly, but
  REQ-F-006 ("verify no other code references KdeCompatService... run a grep across the entire
  project") makes this non-optional — a grep after only deleting the two `kde-compat/` files would
  immediately fail (5 remaining matches in `ShellApplication.{h,cpp}`). Treating this as one atomic
  change avoids a build-broken intermediate state.
- **Do not add a `SessionIntegrationService`-side "recheck diagnostics" shim to preserve
  `KdeCompatService`'s exact behavior.** SPEC's Overview explicitly frames this as "superseded by
  SessionIntegrationService" and Non-Goal #2/#3 rule out behavior changes and new features — the
  correct read is that `SessionIntegrationService` already covers this need in production, and
  `KdeCompatService`'s specific `kbuildsycoca6`-presence + `XDG_MENU_PREFIX` probe was never
  QML-observable to any user in the first place (zero QML consumers), so there is no observable
  behavior to preserve.

### Risks

- **Missing a reference during deletion silently leaves a dangling forward declaration or unused
  include that still compiles** (e.g., leaving `class KdeCompatService;` in the header while
  removing the member would still compile, hiding an incomplete cleanup). Mitigated by running the
  REQ-F-006 grep as a hard gate, not a courtesy step — see Verification & Rollback.
- **`GuardedProcessRunner` (Phase 3 shared infrastructure) itself is untouched and must remain.**
  `KdeCompatService.cpp` was one of `GuardedProcessRunner`'s two production callers; confirm (via
  grep) that other callers exist before/after this deletion so `GuardedProcessRunner.{h,cpp}` and
  its own tests are not mistakenly identified as now-dead code by a future pass — out of scope to
  verify exhaustively here, flagged for implementer awareness only.

### Requirement map

REQ-F-005, REQ-F-006, REQ-F-007, REQ-C-003.

---

## Item 4 — Wire `LauncherService::desktop_file_index_` for O(1) Lookup

### Context recap

`LauncherModel::findEntryByDesktopFile(const QString&)` (`LauncherModel.cpp:176-183`) is a linear
`for` loop over `entries_`, executed on every call. It has two production call sites, both in
`LauncherService.cpp`, both **unchanged by this design**:
- `entryInfoForDesktopFile()` (`.cpp:333-346`) — `model_.findEntryByDesktopFile(desktop_file)`
- `launchDesktopFile()` (`.cpp:424-435`) — same call

`LauncherService` separately owns `QHash<QString, int> desktop_file_index_` (`LauncherService.h:139`),
rebuilt by the private method `rebuildDesktopFileIndex()` (`.cpp:348-365`) every time `model_` emits
`modelReset` (wired in the constructor, `.cpp:190-194`). Reading `rebuildDesktopFileIndex()` in
full shows it does **four** things in one method, not one:
1. Clears and rebuilds `desktop_file_index_` (the dead index SPEC targets).
2. `invalidateCategoryCache()` — `category_counts_dirty_ = true`, consumed by `countForCategory()`.
3. `invalidateDefaultAppsCache()` — `default_apps_cache_dirty_ = true`, consumed by `cachedDefaultApps()`.
4. Builds `available_desktop_files` and calls `recent_apps_tracker_->removeUnavailableDesktopFiles(...)`
   — prunes recent-launches entries for desktop files that no longer exist on disk.

Only responsibility (1) is dead; (2)–(4) are live, currently-exercised behavior. This means the
fix is **not** "delete `rebuildDesktopFileIndex()` and read `desktop_file_index_` from
`findEntryByDesktopFile()`" — `desktop_file_index_` as a `LauncherService` member indexes into
`entries_` (via `model_.allEntryAt(idx)`), which `LauncherModel::findEntryByDesktopFile()` has no
access path to from outside the class (it's a private member), and passing it in via a parameter or
friend declaration would only relocate the same ownership mismatch SPEC flags as the open question.

### Decision: move `desktop_file_index_` from `LauncherService` into `LauncherModel`

`LauncherModel` already owns `entries_` and already has exactly one method that mutates it —
`setEntries(QVector<DesktopEntry> entries)` (`.cpp:145-148`) — called from three places in
`LauncherService.cpp`: the synchronous cache-load path in `start()`, the cache-rebuild fallback path
in `start()`, and the async scan-completion handler in `runValidator()`. Every path that changes
which desktop files exist goes through this one function. This is the "natural hook" SPEC's own
framing asks to identify, and it exists already — no new signal or callback plumbing is needed.

**New `LauncherModel` state and behavior:**

```cpp
// LauncherModel.h — new include, new private member, new private helper
#include <QHash>
...
 private:
  ...
  void rebuildDesktopFileIndex();   // NEW — builds desktop_file_index_ from entries_

  QVector<DesktopEntry> entries_;
  QVector<ScoredEntry> results_;
  QHash<QString, int> desktop_file_index_;   // NEW — desktop_file -> index into entries_
  QString query_;
  QString active_category_;
```

```cpp
// LauncherModel.cpp
void LauncherModel::setEntries(QVector<DesktopEntry> entries) {
  entries_ = std::move(entries);
  rebuildDesktopFileIndex();   // NEW — one line, before the existing rebuildResults() call
  rebuildResults();
}

void LauncherModel::rebuildDesktopFileIndex() {
  desktop_file_index_.clear();
  desktop_file_index_.reserve(entries_.size());
  for (int idx = 0; idx < entries_.size(); ++idx) {
    desktop_file_index_.insert(entries_.at(idx).desktop_file, idx);
  }
}

const DesktopEntry* LauncherModel::findEntryByDesktopFile(const QString& desktop_file) const {
  const auto index_it = desktop_file_index_.constFind(desktop_file);
  if (index_it == desktop_file_index_.constEnd()) {
    return nullptr;
  }
  return allEntryAt(index_it.value());   // reuses the existing bounds-checked accessor (REQ-F-009a)
}
```

`allEntryAt(int idx)` (`LauncherModel.cpp:185-190`) already exists, is already public, and is
already bounds-checked (`idx < 0 || idx >= entries_.size()` → `nullptr`). Routing the O(1) lookup's
result through it — rather than indexing `entries_` directly inside `findEntryByDesktopFile()` —
satisfies REQ-F-009's accessor requirement for free: no new accessor is needed, and the one that
exists already has the correct bounds-checked, null-safe contract SPEC asks for.

**`LauncherService` side — what changes and what doesn't:**

`entryInfoForDesktopFile()` and `launchDesktopFile()` in `LauncherService.cpp` call
`model_.findEntryByDesktopFile(desktop_file)` today and continue to call exactly that after this
change — **zero lines differ** at either call site. This is what REQ-NF-002's acceptance criterion
("a diff of the calling code in LauncherService.cpp shows only the internal lookup mechanism
changed") requires, and it holds because the O(1) rewrite is entirely internal to `LauncherModel`.

`LauncherService::desktop_file_index_` (the `QHash<QString, int>` member, `.h:139`) is deleted —
its data now lives in `LauncherModel`. The `#include <QHash>` in `LauncherService.h` is deleted too:
grepping the rest of the header shows no other `QHash`-typed member (`category_counts_cache_` is a
`QMap`, not a `QHash`), so the include becomes dead along with the member.

`LauncherService::rebuildDesktopFileIndex()` (`.cpp:348-365`) cannot simply be deleted — three of
its four responsibilities (cache invalidation ×2, recent-apps pruning) are live. It is **renamed**
to `LauncherService::onModelEntriesReset()` and stripped of only the index-building lines:

```cpp
// LauncherService.h — private section
void onModelEntriesReset();   // was: rebuildDesktopFileIndex()
```

```cpp
// LauncherService.cpp
void LauncherService::onModelEntriesReset() {
  invalidateCategoryCache();
  invalidateDefaultAppsCache();
  QSet<QString> available_desktop_files;
  const int total = model_.allEntriesCount();
  available_desktop_files.reserve(total);
  for (int idx = 0; idx < total; ++idx) {
    const DesktopEntry* entry = model_.allEntryAt(idx);
    if (entry != nullptr) {
      available_desktop_files.insert(entry->desktop_file);
    }
  }
  if (recent_apps_tracker_ != nullptr) {
    recent_apps_tracker_->removeUnavailableDesktopFiles(available_desktop_files);
  }
}
```

The constructor's `modelReset` connection (`.cpp:190-194`) updates its one call site accordingly:
```cpp
connect(&model_, &QAbstractItemModel::modelReset, this, [this] {
  onModelEntriesReset();
  refreshSelectionAfterModelReset();
  emit resultCountChanged();
});
```

### Data flow

**Before**: `findEntryByDesktopFile()` → O(n) scan over `entries_` on every call, both from
`entryInfoForDesktopFile()` and `launchDesktopFile()`. `LauncherService::desktop_file_index_` is
rebuilt on every `modelReset` (including redundant rebuilds when only `query_`/`active_category_`
changed and `entries_` did not) but is never read by anything.

**After**: `LauncherModel::desktop_file_index_` is rebuilt only when `entries_` actually changes
(inside `setEntries()`), not on every `modelReset` — `rebuildResults()` alone (triggered by
`setQuery()`/`setActiveCategory()`) no longer redundantly touches the desktop-file index, since
those paths don't call `setEntries()`. `findEntryByDesktopFile()` becomes an O(1) hash lookup
through `allEntryAt()`. `LauncherService`'s renamed `onModelEntriesReset()` keeps running on every
`modelReset` exactly as before, since cache invalidation and recent-apps pruning are legitimately
needed even when only the *filtered/sorted view* (`results_`) changes, not just `entries_` — this
preserves today's (slightly conservative) invalidation behavior unchanged.

### Key decisions

- **Index ownership moves to `LauncherModel`, not passed in as a parameter from `LauncherService`.**
  Alternative considered: keep `desktop_file_index_` on `LauncherService`, add a new
  `LauncherModel::findEntryByDesktopFile(const QString&, const QHash<QString,int>&)` overload taking
  the index as a parameter. Rejected: `LauncherModel::entries_` is private, and the index is
  meaningless without a synchronized view of exactly the `entries_` vector it was built against — a
  parameter-passing design creates a staleness hazard the moment `LauncherModel::setEntries()` is
  called without the caller *also* remembering to rebuild and re-pass a fresh index (nothing in the
  type system would catch a stale index being passed in). Owning the index next to the data it
  indexes, with rebuild wired directly into the one mutator that changes that data, makes the
  "index and data can never disagree" invariant structural rather than a discipline the caller must
  maintain. This is also the smaller diff: it needs zero API signature changes on the two production
  call sites in `LauncherService.cpp` (REQ-NF-002), whereas a parameter-passing design would require
  every caller of `findEntryByDesktopFile()` to also thread the index through.
- **Renamed `LauncherService::rebuildDesktopFileIndex()` to `onModelEntriesReset()` rather than
  keeping the old name.** The method's only remaining code has nothing to do with building any
  index once the index moves to `LauncherModel` — keeping the old name would be actively misleading
  to the next reader (a private-method rename is a zero-external-impact, single-call-site change,
  unlike a public API rename). Considered keeping the old name to minimize diff noise; rejected
  because "correct but confusingly named" is a worse outcome for a maintenance-focused phase than a
  one-line-extra rename.
- **`desktop_file_index_` rebuild is hooked into `setEntries()`, not into `rebuildResults()`.**
  `rebuildResults()` runs on every query/category change too (three call sites: `setEntries()`,
  `setQuery()`, `setActiveCategory()`), but the index only needs to reflect `entries_`, which only
  `setEntries()` mutates. Hooking into `rebuildResults()` would rebuild the hash on every keystroke
  in the launcher search box for zero benefit — `setEntries()` is the more precise and cheaper hook.

### Alternatives considered

- **Keep `desktop_file_index_` a `LauncherService` member, add a `friend class LauncherService;`
  declaration to `LauncherModel` so it can reach into `entries_` directly.** Rejected: `friend`
  declarations widen a class's effective public surface in a way that's invisible at the call site;
  the existing `allEntryAt()`/`entryAt()` public-accessor pattern already gives `LauncherService`
  everything it needs without breaching encapsulation, and REQ-F-009 explicitly frames the accessor
  requirement as "public or friend-accessible" — public was chosen as the less surprising option,
  consistent with every other `LauncherModel` accessor already being plain public methods.
- **Leave `LauncherService::desktop_file_index_` in place but unused, and add the index fresh in
  `LauncherModel` as a second, separate hash.** Rejected: SPEC explicitly asks to "wire" the
  existing member, not add a parallel one — carrying two hashes with the same conceptual content
  (one dead, one live) is exactly the kind of confusion Phase 5's Overview says this cleanup exists
  to remove.

### Risks

- **`desktop_file_index_` (new, in `LauncherModel`) going stale if a future entries-mutation path
  bypasses `setEntries()`.** `LauncherModel` currently has exactly one entry-mutating method, so
  this risk is zero today by inspection — flagged so a future change that adds, e.g., an incremental
  `addEntry()`/`removeEntry()` API does not forget to also touch the index. `rebuildDesktopFileIndex()`
  is `private`, giving the compiler no way to enforce this by construction; it is a code-review
  invariant, not a type-system one — same category of risk `category_counts_dirty_`/
  `default_apps_cache_dirty_` already carry in this codebase (dirty-flag caches, not auto-invalidated).
- **Rename of a private method (`rebuildDesktopFileIndex()` → `onModelEntriesReset()`) is
  low-risk but must update its one call site inside the same constructor lambda** — a partial rename
  (declaration renamed, call site not updated, or vice versa) is a compile error, not a silent bug,
  so this fails loudly if missed.

### Requirement map

REQ-F-008, REQ-F-009, REQ-NF-002, REQ-F-010.

---

## Item 5 — Delete SidebarSessionBar.qml

### Context recap

`apps/shell/qml/RightSidebar/Tabs/System/SidebarSessionBar.qml` (89 lines) is a `Row` of four
44×44 icon buttons (Log out / Sleep / Reboot / Shut down) wired directly to `SessionService.logout()`/
`.sleep()`/`.reboot()`/`.shutdown()`. A repo-wide grep for `SidebarSessionBar` across every `.qml`,
`CMakeLists.txt`, and build-config file returns **zero** matches outside the file itself — it is not
imported, not instantiated via `Loader`, and not referenced by id anywhere, including in
`SidebarSystem.qml` (the tab it would conceptually belong to) or `SidebarTabBar.qml`.

The canonical, live implementation is `SidebarTabBar.qml`'s `sessionMenu` (`Controls.Menu`, opened
from a profile-button click at `.qml:272`, items at `.qml:278-356`): `Lock` → `SessionService.lockScreen()`,
`Log out` → `.logout()`, `Reboot` → `.reboot()`, `Shut down` → `.shutdown()` — a superset (adds Lock)
of `SidebarSessionBar.qml`'s four actions, using the same `SessionService` singleton.

### Exact removal

- Delete `apps/shell/qml/RightSidebar/Tabs/System/SidebarSessionBar.qml`.
- No CMakeLists.txt edit: `apps/shell/CMakeLists.txt`'s `HOLONIGHT_QML_FILES` is
  `file(GLOB_RECURSE ... CONFIGURE_DEPENDS qml/*.qml)` (line 44-49) — the file's removal is picked
  up automatically on the next CMake configure, both from the `qt6_add_qml_module(... QML_FILES
  ${HOLONIGHT_QML_FILES})` call and from the generated `HOLONIGHT_QML_TEST_ENTRIES` list
  (`GeneratedQmlFiles.h.in`), which is built from the same glob.
- No further `.qml` edits: `SidebarSystem.qml` and `SidebarTabBar.qml` were confirmed (by direct
  read) to contain no reference to `SidebarSessionBar` to remove.

### Key decisions

- **No `Loader`/dead-import cleanup needed beyond the file deletion itself**, since none exists —
  this item is a pure file deletion with no call-site surgery, unlike Items 1 and 3.

### Risks

- Negligible. The file has no test coverage of its own (no `tst_SidebarSessionBar.qml` exists to
  also delete — confirmed via `tests/qml/` listing) and no production reference to sever.

### Requirement map

REQ-F-011, REQ-F-012 (SPEC's own numbering — note this REQ-F-012 belongs to Item 5, distinct from
Item 4's index-consistency REQ-F-010; SPEC's requirement IDs are not item-scoped-sequential), REQ-C-004.

---

## Testing Approach

| Item | Test file | New/existing | What it covers |
|---|---|---|---|
| 1 | `tests/test_popup_geometry.cpp` | **Deleted** | N/A — REQ-F-002's acceptance is the file's *absence*, verified by `ctest` no longer listing `PopupGeometry.*` and a build/grep pass, not a passing test. |
| 2 | `tests/test_sidebar_surface_policy.cpp` | **Existing, one assertion removed** | The two retained `TEST()`s (`DefaultHeightMatchesInitialPanelHeight`, `BoundsHeightToMinimumAndScreenAvailableSpace` + `BoundsHeightToDefaultWhenScreenIsMissing`) run unmodified and must still pass — this is the direct proof REQ-F-004's "preserve all assertions for the other two functions" holds. |
| 3 | `tests/test_kde_compat_service.cpp` | **Deleted** | N/A — same shape as Item 1. `tests/test_shell_application.cpp` (`test_holonight_app` target) is not modified by this design since `ShellApplication`'s existing tests do not assert on `KdeCompatService` by name (confirm during implementation via grep as a sanity check, since this design does not re-derive that file's full contents). |
| 4 | `tests/test_launcher_service.cpp` | **Existing, extended** | New `TEST(LauncherModel, ...)` cases, added alongside the file's existing `LauncherService`-focused tests (the file already `#include`s `LauncherService.h`, which transitively includes `LauncherModel.h` — no new `#include` needed). Reuses the file's existing `makeEntry(name, exec, desktop_file)` helper (line 67) in a loop to build a synthetic 1000-entry `QVector<DesktopEntry>`, constructs a bare `LauncherModel` directly (`LauncherModel model; model.setEntries(std::move(entries));`) — bypassing `LauncherService`'s file-scanning/async-cache machinery entirely, since REQ-F-008's own acceptance text asks for "a LauncherModel with 1000 test entries," not a `LauncherService`. `QElapsedTimer` around a loop of repeated `findEntryByDesktopFile()` calls proves sub-millisecond aggregate time; a not-found case asserts `nullptr`. A second new test proves REQ-F-010 index consistency across a `setEntries()` replacement (add one entry, drop another) — `LauncherModel` has no incremental add/remove API, so "trigger a rescan" is naturally satisfied by a second `setEntries()` call, which is the only entries-mutation path that exists. Existing tests (`ScansSearchesAndRanksApplications`, `LaunchSelectedDispatchesToActionWhenActionRowSelected`, and the one existing `launchDesktopFile()` call at line 395) run unmodified, proving REQ-NF-002. |
| 5 | none | N/A | No dedicated QML test exists for `SidebarSessionBar.qml` today (verified: no `tst_SidebarSessionBar.qml` under `tests/qml/`), so there is nothing to delete on the test side. Coverage is grep + build + `qmltypes-check` only, per SPEC's own Non-Goals #4. |

**Correction to SPEC's test-strategy language, Item 4 (REQ-F-010)**: SPEC's acceptance text says "The
test uses spy or log assertions to confirm `rebuildDesktopFileIndex()` is called and the index state
is consistent afterward." After this design's restructuring, there is no public, spy-able signal for
this — `LauncherModel::rebuildDesktopFileIndex()` is a `private` method with no `Q_SIGNAL` wrapper
(spying on a private method is not possible with `QSignalSpy`, which only observes signals). This
design substitutes **behavioral** verification: call `findEntryByDesktopFile()` before and after a
`setEntries()` replacement and assert the pre-replacement entry becomes unreachable while the
post-replacement entry becomes reachable — this proves the index was rebuilt (an unrebuilt index
would either wrongly still resolve the removed entry or wrongly fail to resolve the added one) without
requiring a new signal whose only purpose would be to make a private implementation detail
test-observable. Mirrors Phase 3/4 DESIGN.md's established convention of flagging where SPEC's
illustrative test-mechanism wording doesn't map onto this codebase's actual seams.

---

## Sequencing

All five items are independent — zero file overlap between any two items' Component Map rows,
confirmed above. They may be implemented, reviewed, and committed in any order or in parallel, as
SPEC's own Verification Strategy implies (each item's acceptance criteria are self-contained grep/
build/test checks).

**Recommendation: one commit per item (5 commits total), not one combined commit.** Rationale:
- Each item's `git revert` should be independently safe — a regression discovered in, say, Item 4's
  O(1) lookup (a real behavior-adjacent change, unlike Items 1/2/3/5's pure deletions) should be
  revertable without also reverting the four unrelated deletions.
- Items 1–3 and 5 touch zero overlapping lines with Item 4 or each other, so there is no merge-order
  hazard or rebase friction from splitting them.
- This also matches the level of atomicity `git blame`/`git bisect` benefit from most: if
  `qmltypes-check` or a `ctest` run regresses after this phase lands, a 5-commit history localizes
  the cause to one item immediately; a single squashed commit would require re-deriving which of the
  five unrelated changes was responsible.

If the implementer prefers a single commit for a phase this size, that is also acceptable per SPEC's
own framing ("predominantly code cleanup") — the requirement is independent *revertability of intent*,
not necessarily separate commits; a single commit with five clearly-delimited hunks reviewed together
is a reasonable alternative. This design's recommendation is for five, but does not block a
single-commit choice at implementation time.

---

## Verification & Rollback

For every item: file-existence check, grep-for-zero-references check, then build, then the relevant
test target. Exact commands:

### Item 1 — PopupGeometry

```bash
ls libs/holonight-surfaces/src/PopupGeometry.h libs/holonight-surfaces/src/PopupGeometry.cpp  # both: No such file
grep -r "PopupGeometry" libs/ apps/ tests/                                                     # zero matches
task build
ctest -N | grep -i popup_geometry                                                              # zero matches (test no longer registered)
```
Revert: `git revert <item-1-commit>` restores both source files and both CMakeLists.txt entries in
one step if committed atomically.

### Item 2 — sidebarSurfaceWidth()

```bash
grep -rn "sidebarSurfaceWidth" libs/ apps/                    # zero matches
grep -n "sidebarDefaultHeight\|boundedSidebarHeight" libs/holonight-surfaces/src/SidebarManager.cpp  # 2+ matches each, unchanged
task build
ctest -R test_sidebar_surface_policy --verbose                # passes, 3 remaining TEST()s
```

### Item 3 — KdeCompatService

```bash
ls libs/holonight-services/src/kde-compat/                                           # empty or removed
grep -r "KdeCompatService" libs/ apps/ tests/ --include="*.h" --include="*.cpp" \
     --include="*.qml" --include="CMakeLists.txt"                                    # zero matches
task configure && task build
task qmltypes-check                                                                  # passes (KdeCompatService was never in required_types)
ctest -N | grep -i kde_compat                                                        # zero matches
```

### Item 4 — desktop_file_index_ wiring

```bash
task configure-tests && task build
ctest -R test_holonight_services --output-on-failure    # full target; includes the new LauncherModel tests + unmodified LauncherService tests
grep -n "rebuildDesktopFileIndex\|onModelEntriesReset" libs/holonight-services/src/launcher/*.cpp libs/holonight-services/src/launcher/*.h
  # confirms: rebuildDesktopFileIndex now only in LauncherModel.cpp; onModelEntriesReset only in LauncherService.{h,cpp}
```
This item is the one place a straight revert is slightly less trivial than the four deletions — a
revert restores the O(n) scan and the dead `LauncherService::desktop_file_index_` member exactly as
they were pre-Phase-5; no data migration or persisted state is involved (the index is in-memory
only, rebuilt from `entries_` on every `setEntries()` call), so `git revert` alone is sufficient with
no follow-up cleanup step.

### Item 5 — SidebarSessionBar.qml

```bash
ls apps/shell/qml/RightSidebar/Tabs/System/SidebarSessionBar.qml    # No such file
grep -r "SidebarSessionBar" apps/shell/qml/ --include="*.qml"       # zero matches
task build
task qmltypes-check                                                 # passes
```

### Whole-phase gate

```bash
task format-check   # verify attribution against `git status` per CLAUDE.md — pre-existing drift in untouched files is not this phase's responsibility
task tidy            # same attribution caveat
task test             # full ctest suite, all targets
```

---

## Cross-Cutting Risks Summary

| Risk | Item | Severity | Mitigation / status |
|---|---|---|---|
| KdeCompatService has a non-QML production consumer (`ShellApplication.cpp`) that SPEC's REQ-F-005 file list doesn't name explicitly | 3 | Medium | Exhaustive reference table above covers it; REQ-F-006's whole-project grep is the acceptance gate that would catch an incomplete deletion regardless |
| `LauncherModel::desktop_file_index_` going stale if a future change adds an entries-mutation path that bypasses `setEntries()` | 4 | Low (structural today, zero live paths bypass it) | Flagged for future-change awareness; `rebuildDesktopFileIndex()` is private with no compiler-enforced call obligation, same class of risk as this codebase's existing dirty-flag caches |
| `libs/holonight-surfaces/CMakeLists.txt` and `tests/CMakeLists.txt` use explicit source lists (unlike `holonight-services`/`apps/shell`, which GLOB) — a missed line removal here fails silently at configure time only if `CONFIGURE_DEPENDS` doesn't force a re-scan, and explicit lists don't self-heal like GLOB does | 1, 3 (test file only) | Low | `task build` after `rm` will fail loudly (missing source file) if a CMakeLists.txt line is forgotten — explicit lists fail *louder*, not silently, unlike GLOB which would just silently stop building a still-present file if one were kept by mistake |
| SPEC's REQ-F-010 illustrative "spy on rebuildDesktopFileIndex()" framing doesn't map onto a private, signal-less method | 4 | Low | Documented substitution: behavioral before/after assertion across a `setEntries()` replacement, in Testing Approach above |

## Non-Goals Respected

This design changes no observable behavior of `entryInfoForDesktopFile()`/`launchDesktopFile()`
(Non-Goal #2) — both call sites are textually unchanged. No new feature is added anywhere (Non-Goal
#3) — Item 4 is a pure internal performance change with identical external contract. No test in this
phase requires a live compositor, Hyprland, or manual sidebar interaction (Non-Goal #4) — every
verification step above is `grep`, `task build`, `ctest`, or `task qmltypes-check`. Nothing outside
the five named items is touched — `GuardedProcessRunner`, `SessionIntegrationService`,
`StatusPopupGeometry`/`StatusPopupSurface`, `SidebarManager`, and `SidebarTabBar.qml`'s `sessionMenu`
are all read for context in this design but not modified (Non-Goal #5).
