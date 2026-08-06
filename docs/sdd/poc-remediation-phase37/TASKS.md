# Phase 37 — Launcher Cache Resilience and Throughput Tasks

**Status**: Complete — automated checks and user verification passed.

## Tasks

- [x] T-250: Revalidate U-06 I-07 and I-08 against current `main`.
  - Check: confirm `DesktopEntryCache::metadata()` and `upsert()` still prepare
    invariant SQL for each invocation, and `open()` still relies on
    `PRAGMA user_version` without a required-column check.

- [x] T-251: Retain connection-owned prepared metadata and upsert statements.
  - Check: repeated public cache operations reuse statements prepared after a
    successful open; query objects release before connection removal; failed
    opens, explicit closes, transactions, and reopens retain current behavior.

- [x] T-252: Detect and rebuild a current-version incomplete cache schema.
  - Check: a current `user_version` database missing a required
    `desktop_entries` column is recreated into a readable/writable cache;
    valid current cache rows survive reopening.

- [x] T-253: Add deterministic focused cache regression coverage.
  - Check: tests exercise multi-entry persistence/reopen and an incomplete
    current-version schema through the public `DesktopEntryCache` API, without
    a live desktop or compositor.

- [ ] T-254: Validate the implementation.
  - Check: run focused launcher-cache tests, `task test`,
    `task architecture-check`, `task format-check`, `clang-format --dry-run
    --Werror` on changed C++ files, and `git diff --check`; record any
    pre-existing repository-wide check failures separately.
  - Result: focused `DesktopEntryCache.*` coverage passed 7/7; `task test`
    passed 953/953; `task architecture-check`, changed-file clang-format, and
    `git diff --check` passed. `task format-check` fails only on four
    pre-existing `HyprlandWorkspaceService.cpp` formatting violations (lines
    56, 232, 257, and 295); none are in this phase's diff.

- [x] T-255: Obtain user verification of launcher startup/cache behavior.
  - Check: user confirms normal launcher results remain available after a
    restart; no compositor-specific assertion is required for the automatic
    cache-recovery path.
  - Result: user verified normal launcher results remain available after a
    restart.

- [x] T-256: Finalize the phase after verification.
  - Check: mark the SDD status Complete, record validation and the
    implementation commit, and reduce the queued Low-severity backlog from 29
    to 27 candidates.
  - Result: user verification passed. `d793747` (`fix: harden launcher cache
    schema`) implements U-06 I-07 and I-08; 27 Low-severity candidates remain
    queued.
