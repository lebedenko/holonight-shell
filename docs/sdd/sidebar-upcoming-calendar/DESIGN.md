# Sidebar Upcoming Calendar — Architecture Design

**Version**: 2.0
**Date**: 2026-06-22
**Author**: Andrii L
**Status**: Draft
**Spec**: `docs/sdd/sidebar-upcoming-calendar/SPEC.md`

> This document supersedes the v1.0 design (OAuth/Microsoft/Google). The new system uses CalDAV
> with HTTP Basic Auth and plain ICS HTTP feeds. OAuth, `OAuthRedirectServer`, `MsGraphCalendarProvider`,
> `GoogleCalendarProvider`, `SqliteTokenStorage`, and `ITokenStorage` are **removed**.

---

## 1. Overview

The calendar feature fetches events from two read-only sources — CalDAV servers (RFC 4791, HTTP
Basic Auth) and plain ICS URL feeds (RFC 5545, unauthenticated or token-in-URL) — caches them in
SQLite, and exposes up to three upcoming events to a QML sidebar widget. `CalendarSyncManager`
owns all providers, drives separate periodic timers (15 min for CalDAV, 60 min for ICS), applies a
battery gate, enforces per-account concurrency guards, and dispatches D-Bus desktop notifications
at 15 minutes before and at event start. Credentials for CalDAV are stored in the system keyring
via libsecret (key format `holonight-shell/caldav/<account_name>`); ICS URLs live in plaintext
TOML because they may embed authentication tokens. The system boundary stops at the existing
`CalendarService` QML singleton, which exposes `upcomingEvents` (a `QAbstractListModel*`) and
`upcomingState` (`UpcomingState` enum) to `SidebarOverviewUpcoming.qml`.

---

## 2. Component Map

### Config Layer

| File | New / Modified | Purpose |
|------|---------------|---------|
| `src/core/ConfigService.h` | Modified | Replace `CalendarMicrosoftConfig`/`CalendarGoogleConfig` with `CalendarCaldavAccountConfig` and `CalendarIcsAccountConfig`; restructure `CalendarConfig` |
| `src/core/ConfigParsers.cpp` | Modified | Parse `[calendar.caldav.<name>]` and `[calendar.ics.<name>]` TOML subtables |

### Service / Provider Layer

| File | New / Modified | Purpose |
|------|---------------|---------|
| `src/services/calendar/CalendarTypes.h` | Modified | Extend `CalendarEvent` with `uid` and `account_name`; update `SyncError`; remove `TokenData` |
| `src/services/calendar/ICalendarProvider.h` | Modified | Remove OAuth methods; add `testConnection()`, `fetchEvents(range)` returning `std::expected`; remove `deltaSync()`, `authenticate()`, `exchangeCode()`, `isAuthenticated()`, `refreshTokenIfNeeded()` |
| `src/services/calendar/CalDavProvider.h/.cpp` | New | CalDAV provider: `.well-known` discovery, PROPFIND calendar list, REPORT time-range query, Basic Auth |
| `src/services/calendar/IcsProvider.h/.cpp` | New | ICS feed provider: HTTP GET, minimal iCal parser, UID deduplication by DTSTAMP |
| `src/services/calendar/ICalParser.h/.cpp` | New | In-house minimal iCalendar tokenizer/parser (VCALENDAR/VEVENT; no RRULE expansion) |
| `src/services/calendar/CalendarCache.h` | Modified | Add `accounts`, updated `events` and `sync_state` schemas matching SPEC REQ-F-018; add `upsertAccount()`, `configHash()`, `clearAccountEvents()`, `storeSyncState()`, `loadSyncState()` |
| `src/services/calendar/CalendarCache.cpp` | Modified | Implement new schema DDL, config-hash invalidation, pruning, sync-state persistence |
| `src/services/calendar/CalendarEventModel.h/.cpp` | Unchanged | `QAbstractListModel` for QML; existing roles sufficient |
| `src/services/calendar/CalendarSyncManager.h/.cpp` | Modified | Replace single 15-min timer + OAuth logic with dual timers (CalDAV/ICS), per-account concurrency flags, exponential backoff, `testConnection()` startup call, sidebar-open triggered sync |
| `src/services/calendar/LibsecretCredentialStorage.h/.cpp` | New | Thin wrapper around libsecret for CalDAV Basic Auth password lookup (replaces `LibsecretTokenStorage`; no `TokenData` JSON — stores plain password string) |
| `src/services/CalendarService.h` | Modified | Remove `startAuthentication()`; rename `AuthError` → `ConnectError` in `UpcomingState`; remove `onAuthRequired()` slot |
| `src/services/CalendarService.cpp` | Modified | Build `CalDavProvider`/`IcsProvider` from new config; remove OAuth plumbing; add `onConfigReloaded()` |

### Removed Files (dead code from OAuth era)

| File | Action |
|------|--------|
| `src/services/calendar/GoogleCalendarProvider.h/.cpp` | Delete |
| `src/services/calendar/MsGraphCalendarProvider.h/.cpp` | Delete |
| `src/services/calendar/OAuthRedirectServer.h/.cpp` | Delete |
| `src/services/calendar/ITokenStorage.h` | Delete |
| `src/services/calendar/SqliteTokenStorage.h/.cpp` | Delete |
| `src/services/calendar/LibsecretTokenStorage.h/.cpp` | Delete (replaced by `LibsecretCredentialStorage`) |

### QML Layer

| File | New / Modified | Purpose |
|------|---------------|---------|
| `src/qml/RightSidebar/SidebarOverviewUpcoming.qml` | Modified | Replace `AuthError` state with `ConnectError`; update error message text to show account name + error detail from `CalendarService.lastError` |
| `src/qml/RightSidebar/SidebarOverview.qml` | Unchanged | Already contains `SidebarOverviewUpcoming` |

### Build

| File | New / Modified | Purpose |
|------|---------------|---------|
| `CMakeLists.txt` | Modified | Add `CalDavProvider`, `IcsProvider`, `ICalParser`, `LibsecretCredentialStorage` sources; remove deleted OAuth sources; verify libsecret linkage remains |

---

## 3. Data Flow

### 3.1 CalDAV Sync (end-to-end)

```
[config.toml]  [calendar.caldav.work]
      │  url, username, password_keyring_key
      ▼
ConfigService::calendarConfig() → CalendarConfig::caldav_accounts (QList<CalendarCaldavAccountConfig>)
      │
      ▼  CalendarService::initSyncManager()
CalDavProvider constructed with (account_config, LibsecretCredentialStorage*)

CalendarSyncManager::runCalDavSync()
      │
      ├─ battery gate: BatteryService::percent() < 10 → skip
      ├─ per-account in_progress_ flag → skip if already running
      │
      ▼  QtConcurrent::run (worker thread)
LibsecretCredentialStorage::lookupPassword("holonight-shell/caldav/work")
      │ returns QString password (or nullopt → SyncError::AuthError)
      │
      ▼
CalDavProvider::testConnection()  [on startup / config reload only]
      PROPFIND <principal-url> Depth:0, Authorization: Basic ...
      ├─ 2xx + valid XML → UpcomingState::Ready
      └─ error           → UpcomingState::ConnectError (error stored in sync_state.error_message)

CalDavProvider::fetchEvents(DateRange{now-30d, now+180d})
      │
      ├─ 1. GET /.well-known/caldav  → parse XML for principal URL
      │      fail/missing → use configured url (no error, qCDebug)
      │
      ├─ 2. PROPFIND <principal-url> Depth:1
      │      Authorization: Basic base64(username:password)
      │      Parse XML: extract calendar collections with resourcetype=calendar
      │      Apply include/exclude filters from account config
      │      fail → SyncError::ConnectError, abort account
      │
      └─ 3. For each calendar collection:
              REPORT <calendar-url>
              Body: CALDAV:calendar-query, CALDAV:time-range
                    start=NOW-30d end=NOW+180d, comp-filter VEVENT
              Authorization: Basic ...
              Parse XML multistat: extract VCALENDAR/VEVENT from <cal:calendar-data>
              ICalParser::parseEvents(ical_text) → QList<CalendarEvent>
              fail → log qCWarning, continue with next calendar

      ▼  (back on main thread via QFutureWatcher::finished)
CalendarCache::upsertEvents(events, account_name="work", provider_type="caldav")
CalendarCache::pruneExpired()
CalendarCache::storeSyncState(provider_type, account_name, last_sync_time, "", next_sync_time)
CalendarCache::queryRange(now, now+180d) LIMIT 3
      │
      ▼
CalendarEventModel::setEvents(events)
CalendarService → emit upcomingEventsChanged(), emit upcomingStateChanged()
      │
      ▼
SidebarOverviewUpcoming.qml  (Repeater bound to CalendarService.upcomingEvents)
```

### 3.2 ICS Sync (end-to-end)

```
[config.toml]  [calendar.ics.holidays]
      │  url, label
      ▼
ConfigService::calendarConfig() → CalendarConfig::ics_accounts (QList<CalendarIcsAccountConfig>)
      │
      ▼  CalendarService::initSyncManager()
IcsProvider constructed with (account_config)

CalendarSyncManager::runIcsSync()
      │
      ├─ battery gate: BatteryService::percent() < 10 → skip
      ├─ per-account in_progress_ flag → skip if already running
      │
      ▼  QtConcurrent::run (worker thread)
QNetworkAccessManager::get(url)   [10s timeout]
      │
      ├─ network/HTTP error → SyncError::ConnectError
      ├─ body not VCALENDAR → SyncError::ParseError → ConnectError
      └─ ok (UTF-8, fallback ISO-8859-1)
            │
            ▼
ICalParser::parseEvents(ical_text)
      Parse VCALENDAR → extract VEVENT blocks
      Extract: UID, SUMMARY, DTSTART, DTEND, DESCRIPTION, DTSTAMP
      Deduplicate by UID: keep higher DTSTAMP (REQ-F-013)
      Filter: keep events with DTSTART in [now-30d, now+180d]
      → QList<CalendarEvent>

      ▼  (back on main thread)
CalendarCache::upsertEvents(events, account_name="holidays", provider_type="ics")
CalendarCache::pruneExpired()
CalendarCache::storeSyncState(...)
CalendarCache::queryRange(now, now+180d) LIMIT 3
      │
      ▼
CalendarEventModel::setEvents(events)
CalendarService → emit upcomingEventsChanged()
```

### 3.3 Startup Flow

```
ShellApplication::init()
      │
      ▼
CalendarService ctor
      │  reads ConfigService::calendarConfig()
      ├─ build CalDavProvider for each caldav_accounts entry
      ├─ build IcsProvider for each ics_accounts entry
      └─ CalendarSyncManager(providers, battery, this)
            │
            ├─ open CalendarCache (main-thread connection for reads)
            ├─ start caldav_timer_ (15 min, not yet fired)
            ├─ start ics_timer_ (60 min, not yet fired)
            └─ QTimer::singleShot(0) → runTestConnections()
                    │  (async, QtConcurrent::run per provider)
                    ├─ CalDavProvider::testConnection() → PROPFIND
                    └─ IcsProvider::testConnection() → HTTP HEAD or GET
                          │
                          ▼  QFutureWatcher::finished
                    update UpcomingState (Ready / ConnectError)
                    emit CalendarService::upcomingStateChanged()
                    if all ok → runCalDavSync() + runIcsSync() immediately
```

### 3.4 Sidebar-Open Triggered Sync

```
SidebarOverviewUpcoming.qml :: Component.onCompleted
      → CalendarService.notifySidebarOpened()
            → CalendarSyncManager::notifySidebarOpened()
                  │
                  ├─ load sync_state.next_sync_time per account from CalendarCache
                  └─ for each account where next_sync_time <= now:
                          ├─ CalDAV → runCalDavSync(account) (non-blocking)
                          └─ ICS    → runIcsSync(account)   (non-blocking)
                    (cached events already shown; model updates when sync completes)
```

---

## 4. Key Interfaces

### 4.1 ICalendarProvider

The interface is stripped of all OAuth methods. Implementations are synchronous, non-QObject, and
run on worker threads via `QtConcurrent::run`.

```cpp
// src/services/calendar/ICalendarProvider.h
#pragma once

#include "CalendarTypes.h"

#include <QList>
#include <QString>

#include <expected>

struct DateRange {
  QDateTime start_utc;
  QDateTime end_utc;
};

// Pure-virtual interface for read-only calendar data providers.
// All methods are synchronous and run on a worker thread (QtConcurrent::run).
// No QObject inheritance — providers hold no event loop ownership.
class ICalendarProvider {
 public:
  virtual ~ICalendarProvider() = default;

  ICalendarProvider(const ICalendarProvider&) = delete;
  ICalendarProvider& operator=(const ICalendarProvider&) = delete;
  ICalendarProvider(ICalendarProvider&&) = delete;
  ICalendarProvider& operator=(ICalendarProvider&&) = delete;

  // Returns the account name (TOML section key, e.g. "work", "holidays").
  [[nodiscard]] virtual QString accountName() const = 0;

  // Returns the provider type tag ("caldav" or "ics").
  [[nodiscard]] virtual QString providerType() const = 0;

  // Verifies connectivity and credentials without fetching all events.
  // CalDAV: PROPFIND on principal URL. ICS: HTTP HEAD or GET with 10s timeout.
  // Called once on startup and after config reload, on a worker thread.
  [[nodiscard]] virtual std::expected<void, SyncError> testConnection() = 0;

  // Fetches all calendar events in [range.start_utc, range.end_utc).
  // CalDAV: PROPFIND calendar list + REPORT per calendar.
  // ICS: HTTP GET + parse full feed (filter by range in-memory).
  // Returns parsed events; failures carry SyncError with a human-readable message.
  [[nodiscard]] virtual std::expected<QList<CalendarEvent>, SyncError>
  fetchEvents(const DateRange& range) = 0;

 protected:
  ICalendarProvider() = default;
};
```

### 4.2 CalDAV HTTP Layer

**Choice: `QNetworkAccessManager` with `QEventLoop` (not `QtConcurrent`).**

CalDAV providers are already running on a `QtConcurrent` worker thread (launched by
`CalendarSyncManager`). Inside that synchronous context, `QNetworkAccessManager` sends a request
and a local `QEventLoop` spins until `QNetworkReply::finished` fires. This avoids nested
`QtConcurrent` pools and keeps the provider code straightforward sequential C++.

```
// Inside CalDavProvider::fetchEvents (worker thread):
QNetworkAccessManager nam;  // created on the worker thread
QNetworkRequest req(url);
req.setRawHeader("Authorization", basicAuthHeader(username, password));
req.setRawHeader("Depth", "1");
req.setAttribute(QNetworkRequest::SocketTimeoutAttribute, 10000);

QNetworkReply* reply = nam.sendCustomRequest(req, "PROPFIND", body);

QEventLoop loop;
QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
QTimer::singleShot(10000, &loop, &QEventLoop::quit);  // hard timeout guard
loop.exec();

if (reply->error() != QNetworkReply::NoError) { ... }
```

Each HTTP verb (PROPFIND for discovery, REPORT for events) follows this same pattern. The `QNAM`
is constructed fresh per `fetchEvents()` call on the worker thread to avoid cross-thread QNAM
access (Qt requires QNAM to be used from the thread that owns it).

**Rationale**: The alternative — calling `QtConcurrent::run` for each individual HTTP request
from within an already-concurrent lambda — creates nested thread-pool jobs with no benefit.
`QEventLoop` inside a worker thread is safe and idiomatic for blocking HTTP in Qt.

### 4.3 ICS Parsing

**Choice: in-house minimal iCalendar parser (`ICalParser`).**

RRULE expansion is out of scope (REQ-C-002). The only structures needed are `VCALENDAR` and
`VEVENT` with five properties each. Linking `libical` (~300 KB) or `KCalendarCore` (a KDE
framework dependency) would be disproportionate.

`ICalParser` implements a line-by-line tokenizer following RFC 5545 §3.1:

1. **Unfold**: join continuation lines (lines starting with `SPACE` or `TAB` are appended to the
   previous content line).
2. **Tokenize**: split each content line at the first `:` to get `property-name` (with optional
   `param-list` after `;`) and `value`.
3. **State machine**: track `in_vcalendar` / `in_vevent` flags on `BEGIN:VCALENDAR`,
   `BEGIN:VEVENT`, `END:VEVENT`, `END:VCALENDAR`.
4. **Property extraction**: for each VEVENT, collect `UID`, `SUMMARY`, `DTSTART`, `DTEND`,
   `DESCRIPTION`, `DTSTAMP`. `RRULE` is parsed but discarded (not expanded, REQ-C-002).
5. **DateTime parsing**: handles `DTSTART;TZID=...`, `DTSTART;VALUE=DATE` (all-day), and bare
   `DTSTART` (UTC `Z` suffix or floating local time).

```cpp
// src/services/calendar/ICalParser.h
#pragma once

#include "CalendarTypes.h"

#include <QList>
#include <QString>

class ICalParser {
 public:
  // Parses iCalendar text; returns all VEVENT instances found.
  // Events with missing UID or DTSTART are silently skipped (qCDebug logged).
  // RRULE is parsed and stored in CalendarEvent::rrule for reference but never expanded.
  [[nodiscard]] static QList<CalendarEvent> parseEvents(const QString& ical_text,
                                                        const QString& account_name,
                                                        const QString& provider_type);
};
```

The parser is a pure static utility — no QObject, no state between calls. Thread-safe by
construction.

---

## 5. SQLite Schema

The schema targets the requirements in REQ-F-018 and REQ-F-019. The existing `CalendarCache`
schema (version 1, from the OAuth era) is replaced by version 2 via migration.

```sql
-- Schema version tracking
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER NOT NULL
);

-- Provider accounts (one row per configured account; used for config-hash invalidation)
CREATE TABLE IF NOT EXISTS accounts (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    provider_type   TEXT    NOT NULL,           -- 'caldav' | 'ics'
    account_name    TEXT    NOT NULL UNIQUE,     -- TOML section name, e.g. 'work'
    config_hash     TEXT    NOT NULL DEFAULT '', -- SHA-256 of url+username (caldav) or url (ics)
    created_at      TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
);

-- Cached calendar events
CREATE TABLE IF NOT EXISTS events (
    uid             TEXT    NOT NULL,
    provider_type   TEXT    NOT NULL,           -- 'caldav' | 'ics'
    account_name    TEXT    NOT NULL,
    title           TEXT    NOT NULL,
    dtstart         TEXT    NOT NULL,           -- ISO 8601 UTC
    dtend           TEXT,                       -- ISO 8601 UTC; NULL for untimed events
    is_all_day      INTEGER NOT NULL DEFAULT 0,
    description     TEXT    NOT NULL DEFAULT '',
    cached_at       TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    PRIMARY KEY (uid, provider_type, account_name)
);

-- Composite index covering range queries and per-account pruning
CREATE INDEX IF NOT EXISTS idx_events_dtstart
    ON events (dtstart, dtend);

CREATE INDEX IF NOT EXISTS idx_events_account
    ON events (provider_type, account_name);

-- Per-account sync bookkeeping
CREATE TABLE IF NOT EXISTS sync_state (
    provider_type   TEXT    NOT NULL,
    account_name    TEXT    NOT NULL,
    last_sync_time  TEXT,                       -- ISO 8601 UTC; NULL = never synced
    error_message   TEXT    NOT NULL DEFAULT '',
    next_sync_time  TEXT,                       -- ISO 8601 UTC; NULL = sync immediately
    PRIMARY KEY (provider_type, account_name)
);

CREATE INDEX IF NOT EXISTS idx_sync_state_account
    ON sync_state (provider_type, account_name);
```

**WAL mode**: `PRAGMA journal_mode=WAL` is set on every connection open. This allows one writer
(sync worker thread) and one reader (main thread for display queries) to operate concurrently
without blocking.

**UUID-named connections**: the same `QUuid::createUuid().toString(QUuid::WithoutBraces)` pattern
from the existing `CalendarCache` is retained. Each `CalendarCache` instance generates a unique
connection name in its constructor and removes it in the destructor, matching Qt's per-thread
`QSqlDatabase` ownership rules.

**Schema migration**: `CalendarCache::open()` reads `schema_version.version`. Version 1 databases
(OAuth era) are detected and the `events`, `sync_state` columns are migrated: existing event rows
are dropped (OAuth events are incompatible), new tables are created, and version is updated to 2.
Migration failures are logged at `qCWarning`; the cache continues without history (non-fatal per
REQ-F-019).

---

## 6. Config Structure

### C++ Structs (additions/replacements to `ConfigService.h`)

```cpp
// Replaces CalendarMicrosoftConfig and CalendarGoogleConfig

struct CalendarCaldavAccountConfig {
  QString account_name;           // TOML section key
  QString url;                    // CalDAV server URL (principal or calendar URL)
  QString username;
  QString password_keyring_key;   // libsecret lookup key, e.g. "holonight-shell/caldav/work"
  QStringList include;            // empty = include all calendars
  QStringList exclude;            // applied after include filter

  bool operator==(const CalendarCaldavAccountConfig&) const = default;
};

struct CalendarIcsAccountConfig {
  QString account_name;           // TOML section key
  QString url;                    // ICS feed URL (may contain embedded auth token)
  QString label;                  // optional display name override

  bool operator==(const CalendarIcsAccountConfig&) const = default;
};

struct CalendarConfig {
  WeekStartDay week_start_day{WeekStartDay::Monday};
  QList<CalendarCaldavAccountConfig> caldav_accounts;  // empty = CalDAV disabled
  QList<CalendarIcsAccountConfig> ics_accounts;        // empty = ICS disabled

  bool operator==(const CalendarConfig&) const = default;
};
```

### TOML Format (user-facing)

```toml
[calendar]
# week_start_day = "Mon"   # or "Sun"; default Mon

# CalDAV account — one section per account, arbitrary name
[calendar.caldav.work]
url      = "https://caldav.example.com/"   # server root or principal URL
username = "alice@example.com"
# Password is NOT stored here — store it in the system keyring:
#   secret-tool store --label="HoloNight CalDAV work" \
#     key holonight-shell/caldav/work
password_keyring_key = "holonight-shell/caldav/work"
# Optional calendar filters (matched against CalDAV display name):
include = ["Personal", "Holidays"]         # omit or [] = all calendars
exclude = ["Birthdays"]                    # applied after include

[calendar.caldav.personal]
url      = "https://nextcloud.home.lan/remote.php/dav/principals/users/bob/"
username = "bob"
password_keyring_key = "holonight-shell/caldav/personal"

# ICS feed — one section per feed, arbitrary name
[calendar.ics.holidays]
url   = "https://calendar.google.com/calendar/ical/en.uk.official%23holiday%40group.v.calendar.google.com/public/basic.ics"
label = "UK Public Holidays"

[calendar.ics.work-oncall]
url   = "https://pagerduty.example.com/api/v1/users/PXXXXXX/calendar?token=SECRETTOKEN"
# label omitted → account_name is used as display name
```

### ConfigParsers Changes

`parseCalendar()` iterates `tbl["calendar"]["caldav"]` and `tbl["calendar"]["ics"]` as
`toml::table`s. Each sub-table key becomes `account_name`. Missing `url` or `username` (CalDAV) /
missing `url` (ICS) are logged with `qCWarning` and the account is skipped. The `providers`
`QStringList` field from the old config is dropped; presence of entries in `caldav_accounts` or
`ics_accounts` enables those providers.

---

## 7. Libsecret Integration

### `LibsecretCredentialStorage`

This is a thin replacement for the OAuth-era `LibsecretTokenStorage`. Instead of serializing a
`TokenData` JSON blob, it stores and retrieves a plain UTF-8 password string.

```cpp
// src/services/calendar/LibsecretCredentialStorage.h
#pragma once

#include <QString>
#include <optional>

// Reads CalDAV Basic Auth passwords from the system Secret Service (libsecret).
// Key format: the string configured in password_keyring_key (e.g. "holonight-shell/caldav/work").
// If libsecret is unavailable, isServiceAvailable() returns false and all lookups return nullopt.
class LibsecretCredentialStorage {
 public:
  LibsecretCredentialStorage();
  ~LibsecretCredentialStorage() = default;

  LibsecretCredentialStorage(const LibsecretCredentialStorage&) = delete;
  LibsecretCredentialStorage& operator=(const LibsecretCredentialStorage&) = delete;

  [[nodiscard]] bool isServiceAvailable() const { return service_available_; }

  // Looks up the password for the given key. Returns nullopt on missing entry or error.
  // Thread-safe: libsecret sync API is called on the worker thread; no signal/slot.
  [[nodiscard]] std::optional<QString> lookupPassword(const QString& key) const;

 private:
  bool service_available_{false};
};
```

**Lifecycle**: `LibsecretCredentialStorage` is constructed once in `CalendarService::initSyncManager()`
on the main thread (for the `isServiceAvailable()` D-Bus probe). The `lookupPassword()` method is
called lazily on worker threads at sync time — not at startup. `secret_password_lookup_sync()` is
re-entrant and thread-safe per libsecret documentation.

**Key format**: The key is taken verbatim from `CalendarCaldavAccountConfig::password_keyring_key`
(e.g. `"holonight-shell/caldav/work"`). The `SecretSchema` attribute used is `"key"`, matching the
existing schema in `LibsecretTokenStorage.cpp` (`io.github.holonight.shell.token` schema with
attribute `{"key", SECRET_SCHEMA_ATTRIBUTE_STRING}`). This ensures passwords set by users with
`secret-tool` or Seahorse under that key are found.

**Unavailable libsecret**: if `LibsecretCredentialStorage::isServiceAvailable()` is false,
`CalendarSyncManager` skips all CalDAV providers and logs `qCWarning`. ICS providers are
unaffected (no credential needed). The sidebar shows `ConnectError` for CalDAV accounts with a
message explaining the keyring is unavailable.

**Password never logged**: `lookupPassword()` returns a `QString`; the caller passes it directly
to the `Authorization: Basic` header construction and does not log it. All log lines referencing
credentials use only the account name.

---

## 8. Sync Manager

### `CalendarSyncManager` Responsibilities

`CalendarSyncManager` is a `QObject` living on the main thread. It owns all providers and drives
all background work via `QtConcurrent::run` + `QFutureWatcher`.

```cpp
// src/services/calendar/CalendarSyncManager.h (outline)

class CalendarSyncManager : public QObject {
  Q_OBJECT
 public:
  explicit CalendarSyncManager(
      std::vector<std::unique_ptr<ICalendarProvider>> providers,
      LibsecretCredentialStorage* credentials,   // non-owning; owned by CalendarService
      BatteryService* battery,
      QObject* parent = nullptr);
  ~CalendarSyncManager() override;

  [[nodiscard]] CalendarEventModel* eventModel() { return &model_; }

  // Called by CalendarService when sidebar opens.
  void notifySidebarOpened();

  // Called by CalendarService on config reload.
  void onConfigReloaded(const CalendarConfig& new_config);

 Q_SIGNALS:
  void eventsUpdated();
  void syncStateChanged(SyncError error);

 private:
  // Separate timers: CalDAV fires every 15 min, ICS every 60 min.
  QTimer caldav_timer_;
  QTimer ics_timer_;

  // Per-account in-progress flags (keyed by account_name).
  QHash<QString, bool> in_progress_;

  // Per-account backoff state.
  struct BackoffState {
    int backoff_ms{0};           // 0 = not in backoff
    QTimer* timer{nullptr};      // singleShot timer, owned by this object
  };
  QHash<QString, BackoffState> backoff_;

  // Fired notification deduplication set: (uid + account_name, "15min" | "now")
  QSet<std::pair<QString, QString>> fired_notifications_;

  // 1-minute timer for notification dispatch loop.
  QTimer notification_timer_;

  std::vector<std::unique_ptr<ICalendarProvider>> providers_;
  LibsecretCredentialStorage* credentials_;
  BatteryService* battery_;
  CalendarCache cache_;
  CalendarEventModel model_;

  static constexpr int kCaldavIntervalMs{15 * 60 * 1000};
  static constexpr int kIcsIntervalMs{60 * 60 * 1000};
  static constexpr int kSidebarSyncThresholdMs{5 * 60 * 1000};
  static constexpr int kLowBatteryThreshold{10};
  static constexpr int kBackoffInitialMs{5 * 60 * 1000};
  static constexpr int kNotifyWindowSecs{15 * 60};
};
```

### Timer Strategy

- `caldav_timer_` fires every 15 minutes (interval set after each sync *completes*, not on a fixed
  schedule — `QTimer::setSingleShot(false)` with restart after completion).
- `ics_timer_` fires every 60 minutes (same pattern).
- Both timers are stopped when `battery_->percent() < 10` and restarted when battery recovers.

Actually, to implement "15 min after completion", both timers use `QTimer::singleShot()` restart
pattern: after `onSyncFinished()`, compute the next fire time and schedule a single-shot. This
matches REQ-F-009 ("15 minutes after the previous one ended") and REQ-F-012.

### Battery Gate

`CalendarSyncManager` connects to `BatteryService::percentChanged` and
`BatteryService::chargingChanged` at construction. If `percent < 10 && !charging`, both timers are
stopped and `notification_timer_` is also stopped (REQ-F-030). Timers resume when
`percent >= 20 || charging`.

### Per-Account Concurrency Guard

`in_progress_[account_name]` is set true at the start of each provider sync and cleared in the
`QFutureWatcher::finished` handler. If a sync trigger fires while `in_progress_` is true, that
account is skipped with `qCDebug`. Concurrent syncs for different accounts are permitted (separate
`QFutureWatcher` instances per account, or batch per provider type).

### Exponential Backoff

On `SyncError` (network or parse failure):
- If no prior backoff: next retry in `kBackoffInitialMs` (5 min).
- Each successive failure: double the interval, capped at the normal sync interval (15 min for
  CalDAV, 60 min for ICS).
- On success: `backoff_[account_name].backoff_ms = 0`; normal timer resumes.
- Backoff uses `QTimer::singleShot(backoff_ms, ...)` so the regular interval timer continues
  independently and the account rejoins the normal cadence after recovery.

### `testConnection()` on Startup / Config Reload

On construction, `CalendarSyncManager` queues an immediate `testConnection()` call for each
provider via `QTimer::singleShot(0, ...)`. Each test runs on a `QtConcurrent` worker. Results
are collected via a `QFutureWatcher`; all-pass → schedule first full sync immediately; any fail →
set `UpcomingState::ConnectError` and store the error message in `sync_state`.

On config reload (`onConfigReloaded()`), providers are rebuilt and `testConnection()` is re-run
for all accounts.

---

## 9. QML Integration

### CalendarService Properties

```cpp
// CalendarService (existing, modified)

enum class UpcomingState : std::uint8_t {
  Loading      = 0,  // sync in progress or initial startup
  Ready        = 1,  // all accounts OK, at least attempted one sync
  ConnectError = 2,  // one or more accounts failed testConnection or last sync
  Offline      = 3,  // reserved; may merge with ConnectError
};
Q_ENUM(UpcomingState)

Q_PROPERTY(QAbstractListModel* upcomingEvents READ upcomingEvents
           NOTIFY upcomingEventsChanged FINAL)
Q_PROPERTY(UpcomingState upcomingState READ upcomingState
           NOTIFY upcomingStateChanged FINAL)
Q_PROPERTY(QString lastError READ lastError
           NOTIFY upcomingStateChanged FINAL)   // human-readable error from sync_state

Q_INVOKABLE void notifySidebarOpened();
// startAuthentication() removed (no OAuth)
```

`lastError` is populated by `CalendarSyncManager` from `sync_state.error_message` after a failed
sync, and cleared on success. `CalendarService` reads it from `CalendarSyncManager` and exposes it
as a `Q_PROPERTY` so `SidebarOverviewUpcoming.qml` can display `"CalDAV work: Connection refused"`
without hardcoding provider names in QML.

### CalendarEventModel Role Names

| Role constant | `roleNames()` key | QML accessor | Type |
|---|---|---|---|
| `EventIdRole` | `"eventId"` | `model.eventId` | `QString` |
| `TitleRole` | `"title"` | `model.title` | `QString` |
| `StartTimeRole` | `"startTime"` | `model.startTime` | `QDateTime` (UTC) |
| `EndTimeRole` | `"endTime"` | `model.endTime` | `QDateTime` (UTC) |
| `IsAllDayRole` | `"isAllDay"` | `model.isAllDay` | `bool` |
| `ProviderRole` | `"provider"` | `model.provider` | `QString` (account_name) |

These roles already exist in the current `CalendarEventModel`. The `DescriptionRole` is not
currently exposed; the spec (REQ-F-021) requires the first 100 characters of description in the
delegate. Add:

| `DescriptionRole` | `"description"` | `model.description` | `QString` |

`CalendarEventModel::data()` returns `event.description.left(100)` for this role (truncation
happens in C++ to keep QML delegates simple).

### SidebarOverviewUpcoming.qml State Mapping

```
CalendarService.upcomingState
  Loading      → "Loading events…" text (or spinner)
  ConnectError → CalendarService.lastError text (e.g. "CalDAV work: HTTP 401")
  Offline      → "No connection — showing cached events" text
  Ready        →
      upcomingEvents.rowCount() == 0 → "No upcoming events"
      upcomingEvents.rowCount() > 0  → Repeater (up to 3 rows)
```

The existing `SidebarOverviewUpcoming.qml` uses `CalendarService.AuthError` which is renamed to
`CalendarService.ConnectError`. The error text is changed from the hardcoded `"Calendar: sign-in
required"` to `CalendarService.lastError`.

---

## 10. Notifications

### Dispatch Loop

`CalendarSyncManager` runs a `QTimer` (`notification_timer_`) with a 60-second interval on the
main thread. Each tick calls `dispatchNotifications()`:

```
dispatchNotifications()
    │
    ├─ battery gate: percent < 10 → return (REQ-F-030)
    │
    └─ load events from CalendarCache::queryRange(now, now + 16min)
           for each non-all-day event:
               │
               ├─ "15 min before":
               │   secs_to_start in (0, 15*60]
               │   key = (uid + ":" + account_name, "15min")
               │   not in fired_notifications_ → send D-Bus notification, insert key
               │
               └─ "at start":
                   secs_to_start in (-60, 0]
                   key = (uid + ":" + account_name, "now")
                   not in fired_notifications_ → send D-Bus notification, insert key
```

### Deduplication Key

```cpp
// key first element: uid + ":" + account_name (avoids collision between accounts with same UID)
// key second element: "15min" | "now"
auto key = std::make_pair(event.uid + QLatin1Char(':') + event.account_name, tag);
```

The `fired_notifications_` set is in-memory only (session-scoped). On restart, notifications may
re-fire for events that already notified. This is the same acceptable trade-off as the existing
implementation (REQ-F-029 allows in-memory dedup; SQLite persistence is optional).

### D-Bus Call

Uses `QDBusInterface("org.freedesktop.Notifications", "/org/freedesktop/Notifications",
"org.freedesktop.Notifications")` calling `Notify`. Parameters:
- 15-min reminder: `expire_timeout = 5000` ms (REQ-F-027).
- Start notification: `expire_timeout = -1` (user must dismiss, REQ-F-028).
- Body includes account name for multi-account disambiguation.

---

## 11. Key Decisions

### D1: In-house iCal parser vs. linking `libical` or `KCalendarCore`

| | |
|---|---|
| **Decision** | Write `ICalParser` as a minimal in-house tokenizer |
| **Rationale** | RRULE is explicitly out of scope (REQ-C-002). Only 5 VEVENT properties are needed. `libical` is ~300 KB of additional link; `KCalendarCore` drags in KDE Framework dependencies that conflict with the shell's build constraints. The iCal format for our use (line-unfolding + property extraction) is simple enough to implement correctly in ~200 lines. |
| **Alternatives** | `libical` (LGPL), `KCalendarCore` (LGPL, Qt6 port available) |
| **Rejected because** | Dependency weight vs. functionality ratio is unfavorable; RRULE expansion would be pulled in even though unused; licensing requirements (LGPL) complicate static linking if ever needed |

### D2: `QNetworkAccessManager` + `QEventLoop` vs. `QtConcurrent` for HTTP

| | |
|---|---|
| **Decision** | QNAM + `QEventLoop` inside the `QtConcurrent` worker |
| **Rationale** | Providers already run on a worker thread; a local `QEventLoop` is the standard Qt pattern for blocking async operations on non-main threads. Creating QNAM on the worker thread avoids cross-thread access. No nested `QtConcurrent::run` nesting. |
| **Alternative** | Nested `QtConcurrent::run` per HTTP request; or a `QThread` with a resident event loop |
| **Rejected because** | Nested QtConcurrent jobs reduce available thread-pool slots; a dedicated `QThread` per provider adds lifecycle overhead for periodic batch work |

### D3: SQLite WAL mode + UUID-named connections

| | |
|---|---|
| **Decision** | Retain the existing `CalendarCache` WAL + UUID connection pattern; extend schema to version 2 |
| **Rationale** | WAL allows the main-thread read (for QML display) and worker-thread writes (sync) to proceed concurrently. UUID-named connections satisfy Qt's one-connection-per-thread rule. This pattern is already proven in the existing `CalendarCache` and the launcher's `DesktopEntryCache`. |
| **Alternative** | Single connection with mutex; or a read-only main-thread copy |
| **Rejected because** | Mutex serializes reads and writes, harming UI responsiveness during sync; read copy adds bookkeeping complexity |

### D4: Config location — embedded in `config.toml` vs. separate `calendar.toml`

| | |
|---|---|
| **Decision** | Embed calendar config in `config.toml` under `[calendar.caldav.*]` and `[calendar.ics.*]` |
| **Rationale** | Consistent with all other features (weather, notifications, bar, widgets are all in one file). `ConfigService` already watches `config.toml` for changes and emits `calendarConfigChanged`. Adding a second watched file adds `QFileSystemWatcher` complexity and two-file synchronization (if both change simultaneously). |
| **Alternative** | Separate `~/.config/holonight-shell/calendar.toml` (per REQ-C-005 which states this path) |
| **Note** | REQ-C-005 specifies `calendar.toml` but the existing codebase consolidates all config in one file. The SPEC path should be re-evaluated at implementation time; if stakeholders confirm separate file, `ConfigService` gets a second watcher and `parseCalendarFile()` is extracted. For this design we document the embedded approach as the default path. |

---

## 12. Files Changed / Created

| File | New / Modified | Purpose |
|------|---------------|---------|
| `src/core/ConfigService.h` | Modified | Replace MS/Google config structs with `CalendarCaldavAccountConfig`, `CalendarIcsAccountConfig`; restructure `CalendarConfig` |
| `src/core/ConfigParsers.cpp` | Modified | Parse `[calendar.caldav.*]` and `[calendar.ics.*]` subtable iteration |
| `src/services/calendar/CalendarTypes.h` | Modified | Add `uid`, `account_name` fields to `CalendarEvent`; update `SyncError`; remove `TokenData` |
| `src/services/calendar/ICalendarProvider.h` | Modified | Remove OAuth methods; new clean interface with `testConnection()` + `fetchEvents(DateRange)` |
| `src/services/calendar/CalDavProvider.h` | New | CalDAV provider declaration |
| `src/services/calendar/CalDavProvider.cpp` | New | .well-known discovery, PROPFIND, REPORT, Basic Auth, XML parsing |
| `src/services/calendar/IcsProvider.h` | New | ICS feed provider declaration |
| `src/services/calendar/IcsProvider.cpp` | New | HTTP GET, calls `ICalParser`, UID deduplication |
| `src/services/calendar/ICalParser.h` | New | Minimal iCal parser declaration |
| `src/services/calendar/ICalParser.cpp` | New | Line-unfolding tokenizer, VEVENT state machine, DTSTART/DTEND parsing |
| `src/services/calendar/LibsecretCredentialStorage.h` | New | Plain password lookup via libsecret |
| `src/services/calendar/LibsecretCredentialStorage.cpp` | New | libsecret C API wrapper (same `#pragma push_macro("signals")` pattern) |
| `src/services/calendar/CalendarCache.h` | Modified | New schema v2 methods: `upsertAccount`, `storeSyncState`, `loadSyncState`, `clearAccountEvents`, `configHash`; remove `deltaLink`/`storeDeltaLink` |
| `src/services/calendar/CalendarCache.cpp` | Modified | Schema v2 DDL, migration from v1, updated upsert/prune logic |
| `src/services/calendar/CalendarEventModel.h` | Modified | Add `DescriptionRole` |
| `src/services/calendar/CalendarEventModel.cpp` | Modified | Return `description.left(100)` for `DescriptionRole` |
| `src/services/calendar/CalendarSyncManager.h` | Modified | Dual timers, per-account concurrency, backoff, `testConnection()` wiring, remove OAuth fields |
| `src/services/calendar/CalendarSyncManager.cpp` | Modified | Rewrite sync loop for CalDAV/ICS providers; backoff; notification timer |
| `src/services/CalendarService.h` | Modified | Rename `AuthError` → `ConnectError`; add `lastError` property; remove `startAuthentication()` and `onAuthRequired()` |
| `src/services/CalendarService.cpp` | Modified | Build providers from new config; remove OAuth plumbing; connect `configReloaded` |
| `src/qml/RightSidebar/SidebarOverviewUpcoming.qml` | Modified | Rename `AuthError` → `ConnectError`; show `CalendarService.lastError` in error state |
| `CMakeLists.txt` | Modified | Add new sources; remove deleted OAuth sources |
| `src/services/calendar/GoogleCalendarProvider.h/.cpp` | Deleted | OAuth era, no longer used |
| `src/services/calendar/MsGraphCalendarProvider.h/.cpp` | Deleted | OAuth era, no longer used |
| `src/services/calendar/OAuthRedirectServer.h/.cpp` | Deleted | OAuth era, no longer used |
| `src/services/calendar/ITokenStorage.h` | Deleted | Replaced by simpler `LibsecretCredentialStorage` |
| `src/services/calendar/SqliteTokenStorage.h/.cpp` | Deleted | No token refresh needed for Basic Auth |
| `src/services/calendar/LibsecretTokenStorage.h/.cpp` | Deleted | Replaced by `LibsecretCredentialStorage` |

---

## 13. Risks

### RISK-1: CalDAV server `.well-known` redirect chains

**Description**: Some CalDAV servers (e.g. Nextcloud) redirect `/.well-known/caldav` through one
or more HTTP redirects before reaching the principal URL. QNAM follows redirects by default, but
some servers redirect to an `https` URL from an `http` initial request (protocol upgrade). Qt's
QNAM does not follow HTTP→HTTPS redirects by default (`QNetworkRequest::NoLessSafeRedirectPolicy`).

**Mitigation**: Set `QNetworkRequest::RedirectPolicy` to
`QNetworkRequest::SameOriginRedirectPolicy` for `.well-known` discovery requests. If discovery
still fails, `CalDavProvider` falls back to the configured `url` field (REQ-F-004) with a
`qCDebug` log. The fall-back path is always exercised, so discovery failures do not break the
feature.

### RISK-2: XML namespace handling in PROPFIND/REPORT responses

**Description**: CalDAV servers return XML with namespace prefixes that vary by implementation
(e.g. `<D:multistatus>` vs `<d:multistatus>` vs `<multistatus xmlns="DAV:">`). Qt's `QXmlStreamReader`
reports the namespace URI separately from the local name, but naive element-name matching will
break on prefix differences.

**Mitigation**: `CalDavProvider` XML parsing always compares `QXmlStreamReader::namespaceUri()`
+ `QXmlStreamReader::name()` pairs rather than qualified names. The relevant namespace URIs are
constants (`"DAV:"`, `"urn:ietf:params:xml:ns:caldav"`). Unit tests use fixtures from at least
two real-server response samples (Nextcloud, iCloud) to cover prefix variation.

### RISK-3: ICS feed timezone handling for local-time DTSTART

**Description**: RFC 5545 §3.3.5 allows `DTSTART` without a `Z` suffix to mean "floating local
time" (interpreted in the `VTIMEZONE` component's timezone). `ICalParser` does not implement
`VTIMEZONE` parsing. Events with floating `DTSTART` will be stored as local-time `QDateTime`
which may be wrong for users in non-UTC timezones.

**Mitigation**: For this version, floating `DTSTART` values are parsed as local time using
`QDateTime::fromString(value, Qt::ISODate)` (which Qt interprets as local if no `Z` or offset is
present). A `qCInfo` message is logged once per feed when floating times are detected. RRULE
expansion is already out of scope, and timezone-aware parsing can be added later via `VTIMEZONE`
table lookup in `ICalParser`. ICS feeds from major providers (Google Calendar, Fastmail) emit
`Z`-suffixed UTC times in practice.

### RISK-4: libsecret availability in session startup race

**Description**: On some login managers, the GNOME Keyring / KDE Wallet D-Bus service is not yet
available when `holonight-shell` starts (particularly on fast logins before the session bus is
fully initialized). `LibsecretCredentialStorage` probes availability at construction; if it returns
false, all CalDAV accounts are skipped with `ConnectError`.

**Mitigation**: `CalendarService::initSyncManager()` defers construction of
`LibsecretCredentialStorage` by 3 seconds via `QTimer::singleShot(3000, ...)` if the initial
probe fails. A second probe at that point typically succeeds. If the second probe also fails, the
`ConnectError` state is shown and the user is informed via `lastError`. This matches the existing
3-second layer-shell fallback timer pattern in the shell.

### RISK-5: Large ICS feeds exceeding memory bounds (REQ-NF-003)

**Description**: Public ICS feeds (national holiday calendars, conference schedules) can contain
thousands of events. `ICalParser::parseEvents()` returns all events in a `QList` before filtering;
for a 5,000-event feed this may temporarily allocate ~5 MB of `CalendarEvent` structs in memory.

**Mitigation**: `ICalParser` applies the `-30d / +180d` window filter during parsing (not after),
so events outside the range are never added to the result list. For feeds that are structurally
large but mostly out-of-range (e.g. multi-year conference archives), memory usage stays bounded to
the window's worth of events. `REQ-NF-003`'s 1,000-event cap across all providers is enforced in
`CalendarCache::upsertEvents()` which rejects inserts once the per-account row count exceeds the
limit (logging a `qCWarning`).

---

## Related Documents

- **Specification**: `docs/sdd/sidebar-upcoming-calendar/SPEC.md`
- **Existing CalendarCache**: `src/services/calendar/CalendarCache.{h,cpp}` (pattern reference)
- **Existing LibsecretTokenStorage**: `src/services/calendar/LibsecretTokenStorage.{h,cpp}` (libsecret C API pattern)
- **HoloNight Design System**: `assets/dont-commit/` (color tokens, icon sets)
- **Notification D-Bus Pattern**: `src/services/calendar/CalendarSyncManager.cpp::sendNotification()` (existing)
