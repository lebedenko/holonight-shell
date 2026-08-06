# Phase 35 — Launcher Model and Cache Hygiene: Design

**Input**: `poc-remediation-phase35/SPEC.md`
**Baseline**: Phase 34 accepted in `b4ffc39`.
**Status**: Complete — automated checks and user verification passed.

## 1. Change Map

| Requirement | Primary implementation | Coverage |
|---|---|---|
| F-01 | `LauncherModel.{h,cpp}` result-row representation and lookup | launcher model and service tests for app/action data, selection, and launching |
| F-02 | `LauncherModel.cpp` role switch | existing role-contract test plus unknown-role regression assertion |
| F-03 | `DesktopEntryCache.{h,cpp}` destructor lifecycle | cache lifetime test using Qt's SQL connection registry |

## 2. Design Decisions

### 2.1 Index results into the stable entry collection

`entries_` owns the desktop entries for the complete lifetime of a result set.
Replace `ScoredEntry`'s copied `DesktopEntry` with an `entry_index`, and resolve
the entry through the current collection wherever row data, ordering, or
launcher actions need it. `setEntries()` already replaces the owner and rebuilds
results as one model reset, so no result index can cross into a prior set.

This removes repeated entry-value copies during each browse/search rebuild
without introducing pointers whose validity can outlive an entry replacement.

### 2.2 Leave a fallback after an exhaustive switch

Remove `default:` from the `Role` switch and retain an empty return after the
switch. The post-switch fallback preserves the existing response for Qt roles
or arbitrary integer inputs, while a future `LauncherModel::Role` enumerator
causes compiler diagnostics until its model data is deliberately defined.

### 2.3 Make close idempotent RAII cleanup

Declare a non-defaulted `DesktopEntryCache` destructor that calls the existing
idempotent `close()`. `close()` already checks `open_`, closes the connection,
and removes its unique connection name, so explicit callers retain their
current behavior and unwinding/early returns gain the same cleanup guarantee.

The regression test observes Qt's SQL connection registry before and after a
nested cache lifetime. This validates the public Qt resource boundary rather
than coupling to a private helper's call count.

## 3. Risks and Boundaries

- An invalid result index could change QML data or launch behavior; all
  construction and dereference sites must preserve bounds checks and be covered
  with app and action rows.
- A result sort comparator must resolve the index against the current entry
  store only; no entry pointer may be captured across a model reset.
- Qt warns when a database is removed while handles still exist. The existing
  scoped database handles in `closeConnection()` must remain destroyed before
  `removeDatabase()` is called.
- This phase intentionally does not redesign cache queries or schema recovery;
  its only cache behavior change is reliable resource release.
