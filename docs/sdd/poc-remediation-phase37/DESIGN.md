# Phase 37 — Launcher Cache Resilience and Throughput Design

**Status**: Complete — automated checks and user verification passed.

**Input**: `poc-remediation-phase37/SPEC.md`

## 1. Scope and Baseline

The accepted Phase 36 baseline is `0046640` (`docs: close phase 36
remediation`). Revalidation at current `main` confirms that
`DesktopEntryCache::metadata()` and `upsert()` each create and prepare a fresh
`QSqlQuery` for every scanned entry, while `open()` trusts a matching
`PRAGMA user_version` without checking the table it subsequently queries.

Phase 35 (`9a56a32`) already made cache-connection cleanup RAII-safe. This
phase extends that lifecycle carefully: cached query objects must release their
database references before `closeConnection()` calls `removeDatabase()`.

## 2. Design

### 2.1 Connection-owned prepared statements

After a database connection opens and its schema is ready, prepare the
invariant metadata lookup and upsert SQL once and retain those queries in
`DesktopEntryCache` for that open connection. Each operation clears/rebinds
its query values before executing, preserving its current return and logging
contract.

`close()` resets the retained query objects before closing and removing the
uniquely named connection. A failed open follows the same cleanup discipline
before returning `false`. This preserves Qt's requirement that no live query
reference remains when removing a connection, and respects the class's
existing same-thread usage contract.

### 2.2 Schema fingerprint before use

Define the required `desktop_entries` column names beside the existing schema
definition. During `open()`, read SQLite's table metadata and compare it to
that required set after reading the user version.

If the stored version is older, or the current-version table lacks any
required column, drop the disposable table and recreate it with the existing
schema SQL. Only a successful SQLite connection with an identifiable schema
mismatch takes this rebuild path. Failure to open the database remains visible
to the caller and leaves the file untouched, matching the existing corrupted
database behavior.

The check intentionally verifies compatibility rather than requiring an exact
column order: SQLite may retain harmless additional columns, whereas missing or
renamed required columns would break the cache's queries.

## 3. Test Strategy

- Extend `DesktopEntryCache` coverage with multiple metadata/upsert operations
  across a close/reopen cycle, exercising the reusable query path through the
  public API.
- Build a temporary SQLite database with `user_version` equal to the current
  version but an incomplete `desktop_entries` table. Open it through
  `DesktopEntryCache`, persist an entry, reopen it, and confirm the entry can
  be read.
- Retain existing transaction, empty-category, corrupted-open, and destructor
  cleanup tests as regression coverage.

## 4. Alternatives Rejected

- **Prepare queries in `LauncherService`**: this exposes SQLite details and
  divides cache behavior between two classes. The cache owns the connection and
  should own its statements.
- **Delete the cache after any SQLite open failure**: risks discarding data for
  permissions, filesystem, or transient failures and changes the current
  failure contract.
- **Use only `PRAGMA user_version`**: cannot distinguish a legitimate current
  version from an externally altered table.
