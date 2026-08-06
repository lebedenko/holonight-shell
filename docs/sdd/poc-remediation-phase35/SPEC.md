# Phase 35 — Launcher Model and Cache Hygiene

**Status**: Complete — automated checks and user verification passed.

## Objective

Remediate three related launcher-internal Low-severity items from the Phase 7
backlog. This tranche keeps result rows as lightweight references to the
authoritative entry store, restores compiler assistance for launcher roles, and
makes the launcher cache release its owned SQLite connection through RAII.

| Source | Phase 35 item | Impact |
|---|---|---|
| U-06 I-03 | Restore exhaustive role-switch checking | Adding a launcher role cannot silently bypass the model's role-data implementation. |
| U-06 I-04 | Avoid per-result `DesktopEntry` copies | Browsing and searching retain one authoritative entry object per desktop file while preserving every row's data and launch target. |
| U-06 I-10 | Close cache connection in the destructor | A cache instance that leaves scope after `open()` removes its uniquely named SQLite connection without depending on caller discipline. |

## Functional Requirements

### REQ-F-01 — Result rows reference authoritative entries

`LauncherModel` shall store an index into `entries_` for each result row rather
than a value copy of `DesktopEntry`.

- Browse and search ordering, category filtering, scores, app/action rows, and
  role values remain unchanged.
- `entryAt()`, action lookup, selection restoration, and launch operations
  continue to resolve the corresponding authoritative entry.
- Replacing entries continues to reset results atomically; a result row shall
  never retain an index into a previous entry collection.

**Acceptance**: focused model and service tests show browse and search rows,
including action rows, expose the same role values and launch the same app or
action as before an entry replacement.

### REQ-F-02 — Role handling remains exhaustive and safe for unknown roles

`LauncherModel::data()` shall not use a `default:` branch that suppresses
compiler exhaustiveness diagnostics for `LauncherModel::Role`.

- Every declared role remains explicitly handled.
- Unknown integer roles continue to return an empty `QVariant`.
- No QML role name, value, or display behavior changes.

**Acceptance**: the launcher model builds with its normal warning policy, all
declared role values retain their current results, and an unknown role returns
an empty value.

### REQ-F-03 — Cache connection lifetime follows object lifetime

`DesktopEntryCache` destruction shall close and remove its uniquely named
SQLite connection when the cache is open.

- An explicit `close()` remains safe and idempotent.
- Failed opens and unopened instances retain their current no-op destruction
  behavior.
- The cache's transaction, persistence, and reopen behavior remain unchanged.

**Acceptance**: a cache opened in a nested scope leaves no matching Qt SQL
connection registered after destruction, and an explicitly closed cache remains
safe to destroy.

## Constraints and Verification

- Keep the change within `LauncherModel`, `DesktopEntryCache`, and focused
  launcher tests; do not change QML, cache schema, scan policy, or public QML
  property names.
- Preserve `DesktopEntry` value ownership in `entries_`; result rows may only
  refer to it while that collection is current.
- Do not fold in prepared-statement batching, cache schema self-healing,
  notification-rule limits, or DesktopEntry field-list consolidation.
- Run focused launcher tests, `task test`, `task architecture-check`,
  `task format-check`, and `git diff --check`.

## Out of Scope

- Launcher result selection policy, ranking algorithm, desktop-entry scanning,
  cache schema migration, and QML layout.
- Other queued Phase 7 Low-severity candidates, including cache prepared-query
  reuse and schema-tamper recovery.
