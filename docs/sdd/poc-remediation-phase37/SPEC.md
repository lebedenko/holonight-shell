# Phase 37 — Launcher Cache Resilience and Throughput

**Status**: Complete — automated checks and user verification passed.

## Objective

Remediate the two remaining cache-local launcher items from the Phase 7
backlog. This tranche reuses SQL statements on the background validation path
and makes an externally altered but version-current cache rebuild itself.

| Source | Phase 37 item | Impact |
|---|---|---|
| U-06 I-07 | Reuse prepared cache queries | Revalidating many desktop entries does not re-prepare identical SQLite statements for every row. |
| U-06 I-08 | Recover from a version-current altered schema | The launcher recreates its disposable cache when its table no longer matches the schema it needs. |

## Functional Requirements

### REQ-F-01 — Reuse prepared statements for cache validation

`DesktopEntryCache` shall prepare its path-metadata lookup and entry upsert
statements once per successfully opened database connection, then reuse them
for repeated cache operations on that instance.

- `metadata()` and `upsert()` retain their existing results, parameter binding,
  error reporting, and transaction behavior.
- A reopened cache creates statements associated with its new connection;
  closing an instance releases every statement before the connection is
  removed.
- Failed opens and unopened caches retain their current safe failure behavior.
- The cache remains confined to the thread on which it was opened.

**Acceptance**: focused launcher-cache coverage persists and reopens multiple
entries successfully, and implementation review confirms repeated validation
does not prepare the two invariant statements per entry.

### REQ-F-02 — Validate the actual cache table schema

When opening a cache, `DesktopEntryCache` shall verify that the
`desktop_entries` table supplies the current required column set as well as
checking `PRAGMA user_version`.

- A version older than `kCurrentVersion` continues to rebuild the cache.
- A database whose version is current but whose table is missing, renamed, or
  otherwise lacks a required column is treated as stale and rebuilt.
- A valid current schema preserves its existing cached rows.
- The cache remains an implementation detail: a failed SQLite open still
  returns `false` and is not silently replaced or deleted.

**Acceptance**: a test creates a current-version but incomplete
`desktop_entries` table, then proves opening the cache restores a writable,
readable schema; a valid cache still survives a reopen unchanged.

## Constraints and Verification

- Keep changes within `DesktopEntryCache` and focused launcher tests; do not
  change desktop-entry scanning, JSON serialization, result ranking, QML, or
  the cache file location.
- Treat the SQLite cache as disposable only after a successful connection and
  explicit schema mismatch detection; do not remove a database solely because
  it failed to open.
- Preserve the current RAII connection cleanup introduced in Phase 35.
- Use deterministic temporary SQLite databases in tests; no live desktop
  directory, D-Bus, or compositor interaction is required for automated
  coverage.
- Run focused launcher tests, `task test`, `task architecture-check`,
  `task format-check`, and `git diff --check`.

## Out of Scope

- DesktopEntry field-list consolidation (U-06 I-09), launcher selection or
  ranking behavior, and cache schema version changes.
- Notification-rule limits and role-enum consistency (U-06 I-05/I-06).
- Weather, portal, tray, QML, and every other queued Phase 7 Low-severity
  candidate.
