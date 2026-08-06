# Phase 35 — Launcher Model and Cache Hygiene: Tasks

**Status**: Complete — automated checks and user verification passed.

## Pre-flight

- [x] T-236: Revalidate U-06 I-03, I-04, and I-10 against current HEAD.
  - Check: confirm result rows still value-copy `DesktopEntry`, the role switch
    has a `default:`, and an opened `DesktopEntryCache` still has a defaulted
    destructor; re-grep all row lookup and launcher call sites.
  - Result: all three candidates remained. `ScoredEntry` owned a full
    `DesktopEntry`, `data()` had a `default:` branch, and an opened cache relied
    on every caller explicitly invoking `close()`.

## Implementation and Tests

- [x] T-237: Replace copied result entries with current-entry indexes.
  - REQs: REQ-F-01
  - Files: `libs/holonight-services/src/launcher/LauncherModel.{h,cpp}`.
  - Check: browse/search sorting and every row-data, `entryAt()`, and action
    access path resolve the same authoritative entry; entry replacement cannot
    leave stale result references.
  - Result: `ScoredEntry` now stores `entry_index`; model data, row lookup, and
    sort comparators resolve it through the current `entries_` collection.

- [x] T-238: Restore exhaustive role-switch checking.
  - REQs: REQ-F-02
  - Files: `libs/holonight-services/src/launcher/LauncherModel.cpp`.
  - Check: remove only the switch `default:`; preserve the empty fallback for
    non-enum role integers and all declared role behavior.
  - Result: the `default:` branch is removed and the existing empty result is
    returned after the exhaustive role switch.

- [x] T-239: Make cache destruction release an open connection.
  - REQs: REQ-F-03
  - Files: `libs/holonight-services/src/launcher/DesktopEntryCache.{h,cpp}`.
  - Check: the destructor delegates to the idempotent existing close path;
    explicit close and failed-open paths remain safe.
  - Result: `DesktopEntryCache::~DesktopEntryCache()` delegates to `close()`,
    retaining the existing idempotent cleanup behavior.

- [x] T-240: Add focused deterministic regression coverage.
  - REQs: REQ-F-01, REQ-F-02, REQ-F-03
  - Files: `tests/test_launcher_service.cpp`.
  - Check: assert representative browse/search action data and launches,
    unknown-role fallback, entry replacement consistency, and no leaked
    uniquely named SQL connection after scope exit.
  - Result: launcher tests now cover an unknown role, app/action rows across
    an entry replacement, and removal of the unique Qt SQL connection after an
    open cache leaves scope. Existing service coverage continues to exercise
    app and action launching.

## Validation and Handoff

- [x] T-241: Run focused and project validation.
  - Check: focused launcher tests, `task test`, `task architecture-check`,
    `task format-check`, and `git diff --check`; record unrelated existing
    failures separately.
  - Result: focused `DesktopEntryCache.*`, `LauncherModel.*`, and
    `LauncherService.*` coverage passed 25/25. `task test` and
    `task architecture-check` passed. Direct changed-file
    `clang-format --dry-run --Werror` and `git diff --check` passed.
    `task format-check` still reports only the four pre-existing formatting
    violations in `libs/holonight-core/src/HyprlandWorkspaceService.cpp`.

- [x] T-242: Record user acceptance and update the Phase 7 handoff.
  - Check: cite the implementation commit and reduce the queued Low backlog
    from 35 to 32 only after acceptance evidence is recorded.
  - Result: user verified launcher search, result data, action launching, and
    cache lifecycle behavior. `9a56a32` (`refactor: streamline launcher result
    storage`) implements the accepted U-06 I-03, I-04, and I-10 items; the
    other 32 Low-severity candidates remain queued.
