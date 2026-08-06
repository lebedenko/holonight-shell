# Sidebar Upcoming Calendar — EARS Requirements Specification

**Feature:** CalDAV and ICS calendar event retrieval and display  
**Version:** 1.0  
**Date:** 2026-06-22  
**Status:** Active Development

---

## Non-Goals

The following are explicitly out of scope:

- **OAuth authentication** — CalDAV uses HTTP Basic Auth only; ICS feeds are unauthenticated URLs
- **RRULE / recurrence expansion** — Only explicitly listed VEVENT instances are displayed; recurring event expansion is not performed
- **Event creation, editing, or deletion** — This is a read-only implementation
- **CalDAV Digest Auth** — Only HTTP Basic Auth is supported
- **Multiple accounts of the same type in single config entry** — Each provider account must have its own config section; multiple independent CalDAV or ICS accounts are allowed if configured separately

---

## Configuration and CalDAV Secrets

### REQ-F-001: CalDAV provider configuration

**Statement:** The system shall support CalDAV provider configuration via a TOML config file with fields for `url`, `username`, `password_keyring_key`, and optional `include` / `exclude` calendar lists.

**Acceptance criteria:**
- TOML parsing accepts `[calendar.caldav.<account_name>]` sections
- Each section stores `url` (string), `username` (string), `password_keyring_key` (string), optional `include` (array of calendar display names), optional `exclude` (array of calendar display names)
- Config reload re-reads all CalDAV accounts from TOML without requiring application restart
- Missing `url` or `username` fields in a CalDAV section trigger a config error message with the account name

### REQ-F-002: CalDAV HTTP Basic Auth credentials storage in libsecret

**Statement:** The system shall store CalDAV credentials in the libsecret keyring under the key format `holonight-shell/caldav/<account_name>`, where `<account_name>` is the TOML config section name.

**Acceptance criteria:**
- Credentials are written to libsecret with service name `holonight-shell` and label matching the account name
- Credentials are retrieved from libsecret at sync time; a missing keyring entry logs a warning and transitions to `ConnectError` state
- Plaintext password never appears in config files, logs, or process memory dumps
- Multiple CalDAV accounts use distinct keyring entries (e.g., `holonight-shell/caldav/work` and `holonight-shell/caldav/personal`)

### REQ-F-003: ICS provider configuration

**Statement:** The system shall support ICS provider configuration via TOML config file with fields for `url` and optional calendar name label.

**Acceptance criteria:**
- TOML parsing accepts `[calendar.ics.<account_name>]` sections
- Each section stores `url` (string, may contain embedded auth tokens) and optional `label` (display name override)
- URLs are stored in plaintext TOML (not in keyring) because some URLs embed authentication tokens
- Missing `url` field in an ICS section triggers a config error message with the account name

---

## CalDAV Discovery and Principal Resolution

### REQ-F-004: CalDAV server discovery via .well-known

**Statement:** When a CalDAV account is initialized, the system shall attempt to discover the CalDAV principal URL via `/.well-known/caldav` on the configured server.

**Acceptance criteria:**
- Discovery HTTP request is made to `https://<host>/.well-known/caldav` (or HTTP if URL scheme is HTTP)
- Discovery response is parsed as XML; if valid and contains a principal URL reference, that URL is used
- If discovery fails (HTTP error, invalid XML, no principal URL in response), the system falls back to the configured `url` field without blocking
- Fallback does not produce an error message; it logs a debug message and continues with the configured URL

### REQ-F-005: Calendar list discovery from CalDAV principal

**Statement:** When a CalDAV principal URL is resolved, the system shall fetch all accessible calendars using a PROPFIND request with depth 1 on the principal URL.

**Acceptance criteria:**
- PROPFIND request targets the principal URL with `Depth: 1` header and HTTP Basic Auth credentials
- Response is parsed as XML; calendar collection resources are extracted by type (e.g., `resourcetype` contains `calendar`)
- Calendar display name and collection URL are stored in memory for sync operations
- Calendars are filtered by the `include` and `exclude` lists from config (if present); calendars listed in `exclude` are removed, and if `include` is present, only calendars in `include` are kept
- If PROPFIND fails (HTTP error, invalid XML, authentication error), sync fails; state transitions to `ConnectError` and error message is logged with the account name

### REQ-F-006: HTTP Basic Auth credentials in CalDAV requests

**Statement:** The system shall include HTTP Basic Auth credentials in all CalDAV requests (PROPFIND, REPORT, OPTIONS, etc.).

**Acceptance criteria:**
- Authorization header is set to `Basic <base64(username:password)>` for each request
- Credentials are retrieved from libsecret keyring lookup at request time
- If keyring lookup fails, the request is not sent; error is logged and state transitions to `ConnectError`
- All CalDAV requests to the same account use identical credentials (no per-calendar auth variation)

---

## CalDAV Sync and REPORT Queries

### REQ-F-007: CalDAV event fetch with time-range REPORT

**Statement:** The system shall query each discovered CalDAV calendar using a REPORT request with a `CALDAV:calendar-query` to fetch events in the range -30 days to +180 days from today.

**Acceptance criteria:**
- REPORT request uses `CALDAV:calendar-query` with `CALDAV:time-range` filter
- Time range is computed as `NOW - 30 days` to `NOW + 180 days` in UTC
- Request body contains VEVENT filter and time range boundaries; response is parsed as XML/iCalendar
- Events (VEVENT components) are extracted and inserted into the SQLite cache
- If REPORT fails (HTTP error, malformed calendar, timeout), that calendar is skipped; error is logged; sync continues with other calendars in the same account

### REQ-F-008: CalDAV event parsing and cache insertion

**Statement:** The system shall parse VEVENT instances from CalDAV REPORT responses and extract `UID`, `SUMMARY`, `DTSTART`, `DTEND`, and `DESCRIPTION` properties.

**Acceptance criteria:**
- Each VEVENT is deserialized from the REPORT XML response; required properties (UID, DTSTART) are validated
- If a required property is missing, the event is skipped with a debug message logged
- DTSTART/DTEND are parsed as UTC timestamps or local times (according to RFC 5545); all-day events (DATE not DATETIME) are handled with `is_all_day` flag
- Extracted events are inserted into the SQLite `events` table with `provider_type = 'caldav'` and `account_name` set to the config section name

### REQ-F-009: CalDAV sync interval

**Statement:** The system shall perform CalDAV sync every 15 minutes.

**Acceptance criteria:**
- A background timer is started after each CalDAV sync completes
- Next CalDAV sync begins 15 minutes after the previous one ended (not 15 minutes after it started)
- If a sync takes longer than 15 minutes, the next sync starts immediately after the previous one ends
- CalDAV sync is skipped if the device is on a low-battery state (battery percentage < 10% or power profile is "power-saver")

---

## ICS HTTP Feed Sync

### REQ-F-010: ICS feed HTTP fetch and parse

**Statement:** The system shall fetch ICS calendar feeds via HTTP GET request to the configured `url` and parse the response as iCalendar format (RFC 5545).

**Acceptance criteria:**
- HTTP GET request is made to the ICS `url` with a timeout of 10 seconds
- Response is read as plain text (UTF-8 with fallback to ISO-8859-1) and parsed as iCalendar (VCALENDAR component)
- If HTTP request fails (network error, 4xx/5xx status), sync fails; state transitions to `ConnectError` and error is logged with the account name
- If response is not valid iCalendar, sync fails; error is logged; state transitions to `ConnectError`

### REQ-F-011: ICS event parsing and cache insertion

**Statement:** The system shall parse VEVENT instances from ICS feed responses and extract `UID`, `SUMMARY`, `DTSTART`, `DTEND`, and `DESCRIPTION` properties.

**Acceptance criteria:**
- Each VEVENT from the ICS file is deserialized; required properties (UID, DTSTART) are validated
- If a required property is missing, the event is skipped with a debug message logged
- DTSTART/DTEND are parsed as UTC or local times (according to RFC 5545); all-day events are handled with `is_all_day` flag set
- Extracted events are inserted into the SQLite `events` table with `provider_type = 'ics'` and `account_name` set to the config section name

### REQ-F-012: ICS sync interval

**Statement:** The system shall perform ICS feed sync every 60 minutes.

**Acceptance criteria:**
- A background timer is started after each ICS sync completes
- Next ICS sync begins 60 minutes after the previous one ended (not 60 minutes after it started)
- If an ICS sync takes longer than 60 minutes, the next sync starts immediately after the previous one ends
- ICS sync is skipped if the device is on a low-battery state (battery percentage < 10% or power profile is "power-saver")

### REQ-F-013: ICS UID conflict handling

**Statement:** When parsing an ICS feed, if two VEVENT instances have the same UID, the system shall use the one with the most recent `DTSTAMP` value.

**Acceptance criteria:**
- During ICS parsing, duplicate UIDs are detected
- When a duplicate is found, the event with the earlier DTSTAMP is discarded
- This deduplication applies within a single ICS feed only; different ICS feeds may have overlapping UIDs (they are tracked separately by account)
- If DTSTAMP is absent from both events, the first parsed instance is kept

---

## Shared Sync Orchestration

### REQ-F-014: Event window caching and pruning

**Statement:** The system shall cache events in the range -30 days to +180 days from today in SQLite and prune events outside this window after each sync.

**Acceptance criteria:**
- Cache stores events with `dtstart` >= (today - 30 days) and `dtend` <= (today + 180 days)
- After each sync (CalDAV or ICS), events outside this window are deleted from the `events` table
- Cache is queried for display without re-fetching from servers
- Pruning happens automatically and does not require user action

### REQ-F-015: Sync state tracking

**Statement:** The system shall track the last successful sync timestamp, error state, and next scheduled sync time for each provider account.

**Acceptance criteria:**
- A `sync_state` table stores `provider_type` (caldav|ics), `account_name`, `last_sync_time`, `error_message`, `next_sync_time`
- After each sync, `last_sync_time` is updated to the sync completion time (not start time)
- If a sync fails, `error_message` is set to a brief error description and `next_sync_time` is set to retry after a backoff period (5 minutes for first failure, capping at the normal sync interval)
- On successful sync, `error_message` is cleared to empty string
- Entries in `sync_state` persist across application restarts

### REQ-F-016: testConnection() on startup and config reload

**Statement:** When the application starts or config is reloaded, the system shall call `testConnection()` on each provider account to verify connectivity and credentials.

**Acceptance criteria:**
- For CalDAV: perform a PROPFIND on the principal URL (or configured URL) with credentials; if successful (HTTP 2xx, valid XML response), transition to `Ready` state; if failed, transition to `ConnectError` with error message
- For ICS: perform an HTTP HEAD or GET on the feed URL; if successful (HTTP 2xx), transition to `Ready` state; if failed, transition to `ConnectError` with error message
- Test results are cached in-memory; do not block application startup (run in background thread)
- `UpcomingState` enum reflects the result: `ConnectError` if any account fails; `Ready` if all pass; `Loading` while tests are in progress

### REQ-F-017: Provider interface abstraction

**Statement:** The system shall implement an `ICalendarProvider` interface with methods `testConnection()` and `fetchEvents(dateRange)` for both CalDAV and ICS provider types.

**Acceptance criteria:**
- `testConnection()` returns a boolean or error struct indicating success/failure; no exceptions thrown
- `fetchEvents(dateRange)` returns a vector of parsed VEVENT objects with UIDs, titles, start/end times, and descriptions
- Implementations handle their respective auth and fetch logic (CalDAV: PROPFIND + REPORT with Basic Auth; ICS: HTTP GET + parse)
- Both methods use the provider's configured account name and credentials
- Both methods are non-blocking; long-running operations use background threads

---

## SQLite Cache Schema

### REQ-F-018: SQLite cache schema and initialization

**Statement:** The system shall create and maintain SQLite database tables for accounts, events, and sync state.

**Acceptance criteria:**
- `accounts` table: `(id INTEGER PRIMARY KEY, provider_type TEXT NOT NULL, account_name TEXT NOT NULL UNIQUE, config_hash TEXT, created_at DATETIME DEFAULT CURRENT_TIMESTAMP)`
- `events` table: `(uid TEXT NOT NULL, provider_type TEXT NOT NULL, account_name TEXT NOT NULL, title TEXT NOT NULL, dtstart DATETIME NOT NULL, dtend DATETIME, is_all_day BOOLEAN DEFAULT 0, description TEXT, cached_at DATETIME DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (uid, provider_type, account_name))`
- `sync_state` table: `(provider_type TEXT NOT NULL, account_name TEXT NOT NULL, last_sync_time DATETIME, error_message TEXT, next_sync_time DATETIME, PRIMARY KEY (provider_type, account_name))`
- Database file is at `$XDG_CACHE_HOME/holonight-shell/calendar.db` (or fallback to `~/.cache/holonight-shell/calendar.db`)
- WAL mode is enabled for concurrent read/write safety
- Indexes are created on `(dtstart, dtend)` in `events` table and `(provider_type, account_name)` in `sync_state` table for query performance

### REQ-F-019: Cache initialization and schema versioning

**Statement:** The system shall initialize the cache database on first launch and apply schema migrations on version updates.

**Acceptance criteria:**
- If the database file does not exist, all tables are created with the schema from REQ-F-018
- A `schema_version` integer is stored (e.g., in a metadata table or pragma); current version is 1
- If the database exists with an older schema version, migrations are applied automatically
- Migration failures are logged; the application continues with a warning (missing cache is non-fatal)

### REQ-F-020: Cache invalidation on config change

**Statement:** When the config is reloaded, the system shall invalidate cached events for accounts whose configuration has changed.

**Acceptance criteria:**
- A hash of each provider account's config (URL, username for CalDAV; URL for ICS) is computed and stored in the `accounts` table
- On config reload, hashes are compared for each existing account
- If a hash changes (e.g., URL updated), all events for that account are deleted from the `events` table
- New events are fetched and cached on the next sync cycle
- Account additions/deletions in config do not cause errors; new accounts are added to `accounts` table and deleted accounts are cleaned up (events deleted, sync_state removed)

---

## QML User Interface

### REQ-F-021: SidebarOverviewUpcoming QML component

**Statement:** The system shall provide a QML component `SidebarOverviewUpcoming.qml` that displays the next upcoming events from all calendar providers.

**Acceptance criteria:**
- Component is a QML file in `src/qml/RightSidebar/SidebarOverviewUpcoming.qml`
- Component queries the calendar cache for events in the range `[NOW, NOW + 180 days]` ordered by `dtstart`
- Component displays a maximum of 3 upcoming events
- Each event displays: start date/time, title, and optional description excerpt (first 100 characters or until newline)
- Component is imported and rendered by `SidebarOverview.qml` alongside other sidebar sections

### REQ-F-022: UpcomingState enum

**Statement:** The system shall expose an `UpcomingState` enumeration with values `Loading`, `Ready`, `ConnectError`, and `Offline`.

**Acceptance criteria:**
- `Loading`: sync is in progress or cache is being queried
- `Ready`: all provider accounts have successful `testConnection()` result and at least one account has events in cache
- `ConnectError`: at least one provider account failed `testConnection()` or the most recent sync attempt failed for that account
- `Offline`: (optional, may be combined with `ConnectError` if network is truly unavailable)
- QML component binds to this state and adjusts display (spinner for Loading, error text for ConnectError, event list for Ready)

### REQ-F-023: Calendar sync loading indicator

**Statement:** While the system is performing a sync, the system shall display a loading spinner in `SidebarOverviewUpcoming`.

**Acceptance criteria:**
- When CalDAV or ICS sync begins, `UpcomingState` transitions to `Loading`
- A spinner (or animated icon) is displayed in place of the event list
- When sync completes (success or error), `UpcomingState` transitions to `Ready` or `ConnectError`
- Spinner is replaced with the event list or error message

### REQ-F-024: Calendar error message display

**Statement:** If `UpcomingState` is `ConnectError`, the system shall display an error message with the account name and error details.

**Acceptance criteria:**
- Error message is constructed from the account name and `sync_state.error_message`
- Message indicates which account failed (e.g., "CalDAV myserver.com: Connection refused" or "ICS personal: HTTP 401")
- Error message is visible in the sidebar without expanding or navigating away
- User can dismiss the error message or it auto-dismisses after 5 seconds

### REQ-F-025: Real-time event time display with minute updates

**Statement:** The system shall update event start times to reflect elapsed time with minute-level precision while the sidebar is open.

**Acceptance criteria:**
- A QML Timer with 1-minute interval updates the displayed start times
- Timer is active only when `SidebarOverviewUpcoming` is visible (not when sidebar is collapsed or closed)
- Relative time display (e.g., "in 5 minutes", "Today at 3:30 PM") updates automatically every minute
- Timer is stopped and started as sidebar visibility changes to avoid unnecessary CPU usage when sidebar is hidden

### REQ-F-026: Calendar integration with sidebar lifecycle

**Statement:** When the sidebar is opened after being closed, the system shall trigger a sync for any provider whose next scheduled sync time is in the past.

**Acceptance criteria:**
- Before showing `SidebarOverviewUpcoming`, check `sync_state.next_sync_time` for all accounts
- If `next_sync_time <= NOW` for any account, invoke sync immediately (non-blocking, background thread)
- Do not block sidebar open animation on sync completion
- Display events from cache immediately; update if sync completes before the sidebar closes
- A loading spinner indicates sync is in progress while cached events are displayed

---

## Notifications

### REQ-F-027: Event reminder notifications at 15 minutes

**Statement:** The system shall emit a desktop notification 15 minutes before an event start time.

**Acceptance criteria:**
- A background task monitors upcoming events every minute
- When an event's start time minus 15 minutes equals the current time (to within 1 minute), a notification is sent via D-Bus `org.freedesktop.Notifications`
- Notification includes the event title, start time formatted for display, and account name
- Notification is only sent once per event (deduplication by UID + account_name + "15min" label)
- Notification timeout is set to 5 seconds (auto-dismiss)

### REQ-F-028: Event start notifications

**Statement:** The system shall emit a notification at the event start time.

**Acceptance criteria:**
- A background task monitors upcoming events every minute
- When an event's start time equals the current time (to within 1 minute), a notification is sent via D-Bus `org.freedesktop.Notifications`
- Notification includes the event title, account name, and a brief indicator (e.g., "Starting now")
- Notification is only sent once per event (deduplication by UID + account_name + "now" label)
- Notification has no timeout (user must dismiss it manually)

### REQ-F-029: Notification deduplication

**Statement:** The system shall use a persistent deduplication set to avoid sending duplicate notifications for the same event and reminder type.

**Acceptance criteria:**
- Deduplication key is constructed as `hash(UID + account_name + event_start_time + reminder_type)` where reminder_type is "15min" or "now"
- After a notification is sent, the key is stored in an in-memory set (or SQLite table for persistence across restarts)
- On application startup, the deduplication set is rebuilt from recent notification history (events in the last 7 days)
- If a deduplication key already exists, no notification is sent; no error is logged

### REQ-F-030: Notification suppression on low battery

**Statement:** If the device is on low battery, the system shall not send calendar event notifications.

**Acceptance criteria:**
- Before sending a notification, check the battery state via `BatteryService`
- If `batteryPercentage < 10%` or `powerState == LowPower` or battery is critically low, suppress the notification
- Log a debug message indicating the notification was suppressed due to battery state
- Resume notifications when battery recovers above 10% or power state changes to normal

---

## Non-Functional Requirements

### REQ-NF-001: CalDAV sync performance

**Statement:** The system shall complete a CalDAV REPORT query and cache update for a single calendar within 3 seconds under normal network conditions.

**Acceptance criteria:**
- Measure time from REPORT request start to cache insert completion (excluding network latency variance)
- Average completion time across 10 sequential syncs on the same calendar is ≤ 3 seconds
- HTTP request timeout is set to 10 seconds; if exceeded, sync fails with a timeout error

### REQ-NF-002: ICS feed fetch performance

**Statement:** The system shall fetch and parse an ICS feed within 5 seconds under normal network conditions.

**Acceptance criteria:**
- Measure time from HTTP GET start to cache insert completion
- Average completion time across 10 sequential fetches is ≤ 5 seconds
- HTTP request timeout is set to 10 seconds; if exceeded, fetch fails with a timeout error

### REQ-NF-003: Memory usage bounds

**Statement:** The system shall limit in-memory cache of events to a maximum of 1000 events across all providers.

**Acceptance criteria:**
- Events are loaded into memory only when queried for display (lazy load from SQLite)
- Total event count across all providers is tracked; if exceeded, oldest events are evicted from cache
- Measure peak memory usage during a full sync cycle with 500+ events; should not exceed 50 MB for calendar data structures
- No memory leaks detected over 24-hour runtime with repeated sync cycles

### REQ-NF-004: Database query performance

**Statement:** The system shall retrieve the next 3 upcoming events within 100 milliseconds from the SQLite cache.

**Acceptance criteria:**
- Query: `SELECT * FROM events WHERE dtstart >= NOW ORDER BY dtstart LIMIT 3`
- Measure query execution time; average across 100 executions is ≤ 100 ms
- With 1000 events in cache, 99th percentile query time is < 200 ms
- Indexes are created on `(dtstart, dtend)` to optimize range queries

### REQ-NF-005: Sync concurrency

**Statement:** The system shall not run concurrent syncs for the same provider account.

**Acceptance criteria:**
- A mutex or boolean flag is maintained per provider account
- If a sync is already in progress for an account, the next scheduled sync is skipped with a debug log message
- Concurrent syncs for different accounts may run in parallel without blocking each other

---

## Constraints

### REQ-C-001: HTTP Basic Auth only for CalDAV

**Statement:** The system shall not implement or support OAuth, OAuth2, Digest authentication, or any other authentication method for CalDAV.

**Acceptance criteria:**
- Only HTTP Basic Auth is supported for CalDAV
- Configuration does not include OAuth client ID, redirect URI, token, or scope fields
- No OAuth libraries are linked (no libcurl with HTTPS_PROXY env var tricks, etc.)
- Tests do not verify OAuth token flow

### REQ-C-002: No RRULE expansion

**Statement:** The system shall not expand recurring events (RRULE) into individual instances.

**Acceptance criteria:**
- RRULE properties in VEVENT are parsed but ignored (not expanded)
- Only explicitly listed VEVENT instances (in CalDAV REPORT or ICS file) are cached and displayed
- Recurring event instances created by server expansion (e.g., CalDAV server expands RRULE in REPORT response) are treated as individual events
- Documentation states: "Recurrence expansion is not supported; subscribe to recurring calendars and the server will send individual instances"

### REQ-C-003: Read-only calendar data

**Statement:** The system shall not create, modify, or delete calendar events.

**Acceptance criteria:**
- No MKCALENDAR, PUT, DELETE, or POST requests are sent to CalDAV servers
- ICS feeds are fetched with GET only; no write operations are attempted
- Configuration provides no UI for event editing or creation
- Code review verifies no write intent in provider implementations

### REQ-C-004: QML module registration

**Statement:** The system shall register all C++ calendar services with the QML module `HolonightShell` for access from QML components.

**Acceptance criteria:**
- CalendarService C++ class is registered as a QML singleton via `qmlRegisterSingletonType`
- QML can import `HolonightShell` and access `CalendarService` without additional imports
- Property changes in C++ emit QML signals; QML can bind to properties and properties change
- Generated `HolonightShell.qmltypes` includes CalendarService and related types

### REQ-C-005: Configuration file format (TOML)

**Statement:** The system shall use TOML format for calendar configuration.

**Acceptance criteria:**
- Config file is located at `$XDG_CONFIG_HOME/holonight-shell/calendar.toml` (or fallback to `~/.config/holonight-shell/calendar.toml`)
- Sections follow the pattern `[calendar.caldav.<account_name>]` and `[calendar.ics.<account_name>]`
- TOML parser (e.g., toml11 or similar C++23 compatible library) is used
- Invalid TOML causes a clear error message and prevents application startup

### REQ-C-006: Libsecret keyring integration for CalDAV

**Statement:** The system shall use libsecret to store CalDAV credentials.

**Acceptance criteria:**
- libsecret API (or platform-specific keyring API) is called with service name `holonight-shell` and label matching the account name
- Credentials are stored in the user's default keyring (usually the session keyring or GNOME Keyring)
- If libsecret is unavailable (e.g., no D-Bus session), the application logs an error and skips CalDAV sync; ICS syncs continue normally
- No fallback to plaintext files or environment variables for CalDAV passwords

### REQ-C-007: Logging and diagnostics

**Statement:** The system shall use QT_LOGGING_RULES categories for calendar diagnostics.

**Acceptance criteria:**
- Calendar sync events log to category `holonight.calendar` or similar
- Debug-level logs (e.g., CalDAV REPORT XML details, parsed event counts) use `qCDebug`
- Sync errors use `qCWarning` or `qCInfo` (visible by default in logs)
- No tokens, passwords, or auth headers are logged
- Test coverage includes verifying log output for success and error cases

### REQ-C-008: Signal/slot connection for config reload

**Statement:** The system shall use Qt signals and slots to propagate config reload events from `ConfigService` to `CalendarService`.

**Acceptance criteria:**
- `ConfigService::configReloaded()` signal is connected to `CalendarService::onConfigReloaded()`
- On config reload, all provider accounts are re-initialized and `testConnection()` is called asynchronously
- If a provider's config is removed, its cached events are deleted and next sync entry is removed
- Connections are established in the `CalendarService` constructor and never disconnected

### REQ-C-009: Thread safety

**Statement:** The system shall ensure thread-safe access to the SQLite cache and provider state.

**Acceptance criteria:**
- SQLite database is opened in serialized mode (SQLITE_OPEN_FULLMUTEX or WAL mode for concurrency)
- Cache queries and inserts use consistent locking to prevent race conditions
- Provider sync operations can run in parallel for different accounts without data corruption
- No use of global mutable state in provider implementations

### REQ-C-010: Icon and color theming

**Statement:** All UI elements in `SidebarOverviewUpcoming.qml` shall use HoloNight theme tokens from `HoloniightPalette` and SVG icons from the design system; no hardcoded colors or generic icons.

**Acceptance criteria:**
- Inspect QML imports; `import Holonight` is present at the top of the file
- All text colors use `HoloniightPalette.*` tokens (e.g., `HoloniightPalette.textPrimary`)
- All icons reference `qrc:/HolonightShell/` paths (from design system assets)
- No hex color literals (e.g., `#FF0000`) appear in the QML file
- Spinner/loading icon uses an animated SVG or Canvas element styled with palette tokens

---

## Appendix: Term Definitions

| Term | Definition |
|------|-----------|
| CalDAV | Calendar Distribution Access Protocol (RFC 4791); used to access and manage calendar data on a server using WebDAV extensions. |
| ICS | iCalendar format (RFC 5545); a plain-text format for exchanging calendar data in VCALENDAR and VEVENT components. |
| VEVENT | An iCalendar component representing a calendar event with properties like UID, DTSTART, DTEND, SUMMARY, DESCRIPTION. |
| PROPFIND | WebDAV method used to discover resources and their properties; used in CalDAV for listing calendars and principal URLs. |
| REPORT | WebDAV method used to query calendar data; used in CalDAV to fetch events within a time range with CALDAV:calendar-query. |
| UID | Unique identifier for a calendar event (required by RFC 5545); used to deduplicate and track events across syncs. |
| RRULE | Recurrence rule property in iCalendar; specifies how recurring events repeat (out of scope for this implementation). |
| Deduplication | Process of identifying and avoiding duplicate records; in this context, ensuring notifications are not sent twice for the same event. |
| Event window | The time range of events cached and displayed; -30 days to +180 days from today. |
| Keyring | System service for securely storing credentials; on Linux, accessed via libsecret and GNOME Keyring. |
| WAL mode | Write-Ahead Logging in SQLite; improves concurrency by separating reads and writes with separate files. |
| HTTP Basic Auth | Simple HTTP authentication using `Authorization: Basic <base64(username:password)>` header. |
| DTSTAMP | Date-time stamp property in iCalendar; represents when the event/calendar object was last modified (used for conflict resolution). |

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-06-22 | Initial EARS specification; CalDAV HTTP Basic Auth + ICS HTTP feed implementation (no OAuth) |

