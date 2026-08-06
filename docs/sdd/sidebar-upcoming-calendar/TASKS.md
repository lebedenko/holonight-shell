# SDD Tasks — sidebar-upcoming-calendar (CalDAV + ICS)

- [x] T-001: Delete GoogleCalendarProvider.h and GoogleCalendarProvider.cpp
  - REQs: REQ-C-001
  - Check: Files src/services/calendar/GoogleCalendarProvider.h and .cpp no longer exist

- [x] T-002: Delete MsGraphCalendarProvider.h and MsGraphCalendarProvider.cpp
  - REQs: REQ-C-001
  - Check: Files src/services/calendar/MsGraphCalendarProvider.h and .cpp no longer exist

- [x] T-003: Delete OAuthRedirectServer.h and OAuthRedirectServer.cpp
  - REQs: REQ-C-001
  - Check: Files src/services/calendar/OAuthRedirectServer.h and .cpp no longer exist

- [x] T-004: Delete ITokenStorage.h
  - REQs: REQ-C-001
  - Check: File src/services/calendar/ITokenStorage.h no longer exists

- [x] T-005: Delete SqliteTokenStorage.h and SqliteTokenStorage.cpp
  - REQs: REQ-C-001
  - Check: Files src/services/calendar/SqliteTokenStorage.h and .cpp no longer exist

- [x] T-006: Delete LibsecretTokenStorage.h and LibsecretTokenStorage.cpp
  - REQs: REQ-C-001
  - Check: Files src/services/calendar/LibsecretTokenStorage.h and .cpp no longer exist

- [x] T-007: Create CalendarTypes.h with CalendarEvent::uid and account_name fields
  - REQs: REQ-F-018, REQ-F-021
  - Check: CalendarEvent struct has uid (QString), account_name (QString), and description (QString) fields

- [x] T-008: Update ICalendarProvider.h interface to remove OAuth methods and add testConnection() + fetchEvents()
  - REQs: REQ-F-017, REQ-C-001
  - Check: ICalendarProvider has testConnection() and fetchEvents(DateRange) methods returning std::expected; no startAuthentication(), authenticate(), exchangeCode(), isAuthenticated(), or refreshTokenIfNeeded() methods exist

- [x] T-009: Create CalendarTypes.h enum UpcomingState with Loading, Ready, ConnectError, Offline values
  - REQs: REQ-F-022
  - Check: UpcomingState enum defined with four values in CalendarTypes.h

- [x] T-010: Update ConfigService.h to replace CalendarMicrosoftConfig and CalendarGoogleConfig with CalendarCaldavAccountConfig and CalendarIcsAccountConfig
  - REQs: REQ-F-001, REQ-F-003
  - Check: CalendarCaldavAccountConfig has account_name, url, username, password_keyring_key, include (QStringList), exclude (QStringList) fields; CalendarIcsAccountConfig has account_name, url, label fields; CalendarConfig::caldav_accounts and ics_accounts (QList) exist

- [x] T-011: Update ConfigParsers.cpp to parse [calendar.caldav.<name>] and [calendar.ics.<name>] TOML subtables
  - REQs: REQ-F-001, REQ-F-003
  - Check: parseCalendar() iterates caldav and ics subtables; validation logs qCWarning for missing url or username (CalDAV) or missing url (ICS)

- [x] T-012: Create CalendarCache.h schema v2 with upsertAccount(), configHash(), clearAccountEvents(), storeSyncState(), loadSyncState() methods
  - REQs: REQ-F-018, REQ-F-019, REQ-F-020
  - Check: CalendarCache has method signatures for upsertAccount(provider_type, account_name, config_hash), configHash(url, username or url), clearAccountEvents(account_name), storeSyncState(provider_type, account_name, last_sync_time, error_message, next_sync_time), loadSyncState(account_name)

- [x] T-013: Implement CalendarCache.cpp schema v2 DDL with accounts, events, sync_state tables and indexes
  - REQs: REQ-F-018, REQ-F-019
  - Check: CREATE TABLE IF NOT EXISTS statements for accounts (id, provider_type, account_name UNIQUE, config_hash, created_at), events (uid, provider_type, account_name, title, dtstart, dtend, is_all_day, description, cached_at; PRIMARY KEY uid+provider_type+account_name), sync_state (provider_type, account_name, last_sync_time, error_message, next_sync_time; PRIMARY KEY provider_type+account_name); indexes on (dtstart, dtend) and (provider_type, account_name); PRAGMA journal_mode=WAL

- [x] T-014: Implement CalendarCache migration from schema v1 to v2 in CalendarCache::open()
  - REQs: REQ-F-019
  - Check: CalendarCache::open() detects version 1, deletes old event/sync_state tables, creates v2 tables, updates version to 2; migration failures logged at qCWarning without blocking

- [x] T-015: Implement CalendarCache::pruneExpired() to delete events outside -30d/+180d window
  - REQs: REQ-F-014
  - Check: pruneExpired() executes DELETE FROM events WHERE dtstart < (today - 30 days) OR dtend > (today + 180 days)

- [x] T-016: Create ICalParser.h with parseEvents(ical_text, account_name, provider_type) static method
  - REQs: REQ-F-008, REQ-F-011
  - Check: ICalParser::parseEvents() returns QList<CalendarEvent>; method signature is static

- [x] T-017: Implement ICalParser.cpp line-unfolding, tokenizer, VCALENDAR/VEVENT state machine
  - REQs: REQ-F-008, REQ-F-011, REQ-C-002
  - Check: ICalParser unfolds continuation lines, tokenizes at first colon, tracks in_vcalendar/in_vevent state, extracts UID/SUMMARY/DTSTART/DTEND/DESCRIPTION/DTSTAMP, skips events with missing UID or DTSTART (qCDebug logged), parses DATE/DATETIME according to RFC 5545, does not expand RRULE

- [x] T-018: Implement ICalParser datetime parsing for UTC (Z-suffix), floating local time, and DATE (all-day) formats
  - REQs: REQ-F-008, REQ-F-011
  - Check: ICalParser::parseEvents() sets is_all_day=true for VALUE=DATE, handles DTSTART/DTEND with Z, and parses floating time without Z as local; test with RFC 5545 examples

- [x] T-019: Create LibsecretCredentialStorage.h with lookupPassword(key) method returning std::optional<QString>
  - REQs: REQ-F-002, REQ-F-006
  - Check: LibsecretCredentialStorage has isServiceAvailable() bool method and lookupPassword(const QString& key) returning std::optional<QString>

- [x] T-020: Implement LibsecretCredentialStorage.cpp to wrap libsecret secret_password_lookup_sync() API
  - REQs: REQ-F-002, REQ-F-006
  - Check: Constructor probes libsecret availability; lookupPassword() calls secret_password_lookup_sync() with schema attribute {"key", SECRET_SCHEMA_ATTRIBUTE_STRING}; no password logged

- [x] T-021: Create CalDavProvider.h declaration with .well-known discovery, PROPFIND, REPORT capabilities
  - REQs: REQ-F-004, REQ-F-005, REQ-F-007
  - Check: CalDavProvider class inherits ICalendarProvider; holds CalendarCaldavAccountConfig, LibsecretCredentialStorage*

- [x] T-022: Implement CalDavProvider.cpp .well-known/caldav discovery with XML parsing
  - REQs: REQ-F-004
  - Check: CalDavProvider::fetchEvents() issues GET to /.well-known/caldav; parses XML for principal URL; logs qCDebug on discovery failure and falls back to configured url without error

- [x] T-023: Implement CalDavProvider::testConnection() PROPFIND on principal URL with HTTP Basic Auth
  - REQs: REQ-F-005, REQ-F-016
  - Check: testConnection() sends PROPFIND with Depth:1 header, Authorization: Basic header, parses XML response; returns std::expected<void, SyncError> with ConnectError on HTTP/XML failure

- [x] T-024: Implement CalDavProvider calendar list discovery via PROPFIND with include/exclude filtering
  - REQs: REQ-F-005
  - Check: CalDavProvider extracts calendar collections from PROPFIND XML by resourcetype=calendar; applies include filter first (if non-empty, only keep listed calendars), then exclude filter; logs qCWarning on PROPFIND failure and returns ConnectError

- [x] T-025: Implement CalDavProvider REPORT query with CALDAV:calendar-query time-range filter
  - REQs: REQ-F-007
  - Check: CalDavProvider::fetchEvents() sends REPORT with CALDAV:calendar-query body; time range is now-30d to now+180d UTC; parses XML multistat response extracting VCALENDAR/VEVENT from cal:calendar-data; logs qCWarning on per-calendar REPORT failure and continues with next calendar

- [x] T-026: Implement CalDavProvider XML parsing to extract VEVENT from REPORT responses and call ICalParser
  - REQs: REQ-F-008
  - Check: CalDavProvider extracts VEVENT text blocks from cal:calendar-data XML elements; calls ICalParser::parseEvents() with extracted text; deduplicates across calendars by UID (same provider+account, latest DTSTAMP wins)

- [x] T-027: Create IcsProvider.h declaration for HTTP GET + ICS parse
  - REQs: REQ-F-010, REQ-F-011
  - Check: IcsProvider class inherits ICalendarProvider; holds CalendarIcsAccountConfig

- [x] T-028: Implement IcsProvider.cpp HTTP GET with 10-second timeout and UTF-8 fallback to ISO-8859-1
  - REQs: REQ-F-010
  - Check: IcsProvider::fetchEvents() sends GET to configured url with QNetworkRequest::SocketTimeoutAttribute 10000ms; decodes response as UTF-8 first, fallback ISO-8859-1; returns ConnectError on HTTP error or timeout

- [x] T-029: Implement IcsProvider to call ICalParser and detect non-VCALENDAR responses
  - REQs: REQ-F-010, REQ-F-011
  - Check: IcsProvider::fetchEvents() parses response as iCalendar; if response does not start with VCALENDAR or ICalParser returns empty list with non-VCALENDAR text present, logs qCWarning and returns ConnectError

- [x] T-030: Implement IcsProvider UID deduplication by DTSTAMP (most recent wins)
  - REQs: REQ-F-013
  - Check: IcsProvider::fetchEvents() detects duplicate UIDs within same feed; keeps event with higher DTSTAMP; if DTSTAMP absent on both, keeps first parsed instance; applies -30d/+180d window filter during parsing

- [x] T-031: Implement IcsProvider::testConnection() with HTTP HEAD or GET to verify feed URL reachability
  - REQs: REQ-F-016
  - Check: IcsProvider::testConnection() sends HTTP HEAD (fallback GET if HEAD not supported) with 10s timeout; returns ConnectError on HTTP error

- [x] T-032: Create CalendarSyncManager.h with dual timers (15 min CalDAV, 60 min ICS), per-account concurrency guards, backoff state, notification dedup set
  - REQs: REQ-F-009, REQ-F-012, REQ-NF-005, REQ-F-027, REQ-F-028, REQ-F-029
  - Check: CalendarSyncManager declares caldav_timer_, ics_timer_, in_progress_ (QHash<QString, bool>), backoff_ (QHash<QString, BackoffState>), fired_notifications_ (QSet), notification_timer_

- [x] T-033: Implement CalendarSyncManager runCalDavSync() with battery gate and per-account concurrency guard
  - REQs: REQ-F-009, REQ-NF-005, REQ-F-030
  - Check: runCalDavSync() checks battery < 10% and skips if true; checks in_progress_[account_name] and skips if true; sets flag and runs provider.fetchEvents() on QtConcurrent worker; clears flag in QFutureWatcher::finished

- [x] T-034: Implement CalendarSyncManager runIcsSync() with battery gate and per-account concurrency guard
  - REQs: REQ-F-012, REQ-NF-005, REQ-F-030
  - Check: runIcsSync() checks battery < 10% and skips if true; checks in_progress_[account_name] and skips if true; sets flag and runs provider.fetchEvents() on QtConcurrent worker; clears flag in QFutureWatcher::finished

- [x] T-035: Implement CalendarSyncManager sync result handling with CalendarCache::upsertEvents() and pruneExpired()
  - REQs: REQ-F-009, REQ-F-012, REQ-F-014
  - Check: On sync completion, runCalDavSync() and runIcsSync() call cache.upsertEvents(), cache.pruneExpired(), emit eventsUpdated()

- [x] T-036: Implement CalendarSyncManager error handling with exponential backoff (5 min initial, capped at sync interval)
  - REQs: REQ-F-009, REQ-F-012, REQ-F-015
  - Check: On SyncError, backoff_[account_name].backoff_ms is initialized to 5 min (300000 ms) if zero; exponential increase on each failure; capped at 15 min (CalDAV) or 60 min (ICS); reset to zero on success

- [x] T-037: Implement CalendarSyncManager sync_state persistence with storeSyncState() and loadSyncState()
  - REQs: REQ-F-015
  - Check: After each sync, storeSyncState() is called with last_sync_time (completion time, not start), error_message (empty on success, brief message on failure), next_sync_time (computed retry or normal interval); loadSyncState() called at startup

- [x] T-038: Implement CalendarSyncManager testConnection() startup call for all providers asynchronously
  - REQs: REQ-F-016
  - Check: CalendarSyncManager constructor queues QTimer::singleShot(0) to runTestConnections(); runs all provider.testConnection() on QtConcurrent workers; collects results via QFutureWatcher; all-pass → Ready state and immediate full sync; any fail → ConnectError state with error message

- [ ] T-039: Implement CalendarSyncManager::onConfigReloaded() to rebuild providers and re-run testConnection()
  - REQs: REQ-F-016, REQ-C-008
  - Check: onConfigReloaded() clears old providers, builds new providers from updated config, re-runs testConnection() on all, clears old sync_state entries for removed accounts, prunes cache entries for removed accounts

- [x] T-040: Implement CalendarSyncManager::notifySidebarOpened() to trigger sync if next_sync_time is past
  - REQs: REQ-F-026
  - Check: notifySidebarOpened() loads sync_state.next_sync_time for all accounts; for each account where next_sync_time <= now, calls runCalDavSync() or runIcsSync() on worker thread (non-blocking); sidebar shows cached events immediately

- [x] T-041: Implement CalendarSyncManager notification timer (60-second interval) and dispatchNotifications() loop
  - REQs: REQ-F-027, REQ-F-028, REQ-F-030
  - Check: notification_timer_ fires every 60 seconds; dispatchNotifications() queries cache for events in [now, now+15min]; for each non-all-day event, checks 15-min and start-time windows; deduplication keys checked; D-Bus Notify called with appropriate expire_timeout (5000 for 15-min, -1 for start)

- [x] T-042: Implement CalendarSyncManager notification deduplication with (uid:account_name, tag) keys
  - REQs: REQ-F-029
  - Check: fired_notifications_ set stores pairs of (uid+":"+account_name, "15min" or "now"); dispatchNotifications() checks membership before calling D-Bus Notify; keys are inserted after notification sent

- [x] T-043: Implement CalendarSyncManager battery gate connecting to BatteryService percent changes
  - REQs: REQ-F-030
  - Check: Constructor connects to BatteryService::percentChanged; stops caldav_timer_, ics_timer_, notification_timer_ when percent < 10 && !charging; resumes when percent >= 20 || charging

- [x] T-044: Update CalendarEventModel to add DescriptionRole with truncation to 100 characters
  - REQs: REQ-F-021
  - Check: CalendarEventModel::roleNames() includes "description"; data() method returns event.description.left(100) for DescriptionRole

- [x] T-045: Update CalendarService.h to rename AuthError → ConnectError, add lastError property, remove startAuthentication()
  - REQs: REQ-F-022, REQ-F-024
  - Check: UpcomingState enum in CalendarService has ConnectError (no AuthError); Q_PROPERTY lastError with READ and NOTIFY signals; no startAuthentication() method; notifySidebarOpened() Q_INVOKABLE added

- [x] T-046: Update CalendarService.cpp to build CalDavProvider and IcsProvider from config
  - REQs: REQ-F-001, REQ-F-003, REQ-F-017
  - Check: CalendarService::initSyncManager() iterates caldav_accounts and ics_accounts from config; constructs CalDavProvider/IcsProvider with appropriate configs; passes LibsecretCredentialStorage* to providers

- [x] T-047: Connect ConfigService::configReloaded() signal to CalendarService::onConfigReloaded()
  - REQs: REQ-C-008
  - Check: CalendarService connects in constructor: QObject::connect(configService, &ConfigService::calendarConfigChanged, this, &CalendarService::onConfigReloaded)

- [x] T-048: Update SidebarOverviewUpcoming.qml to rename AuthError state to ConnectError and display CalendarService.lastError
  - REQs: REQ-F-022, REQ-F-024
  - Check: SidebarOverviewUpcoming.qml no longer references CalendarService.AuthError; ConnectError state displays error text from CalendarService.lastError (e.g. "CalDAV work: HTTP 401")

- [x] T-049: Update SidebarOverviewUpcoming.qml to display 1-minute timer for relative time updates
  - REQs: REQ-F-025
  - Check: Timer with 1-minute interval active only when SidebarOverviewUpcoming visible; updates startTime display every minute; stops and starts with sidebar visibility changes

- [x] T-050: Add DescriptionRole display to SidebarOverviewUpcoming.qml event delegate
  - REQs: REQ-F-021
  - Check: Repeater delegate shows event title and first 100 characters of description (or empty if no description); layout accommodates description text alongside title

- [x] T-051: Add loading spinner to SidebarOverviewUpcoming.qml for Loading state
  - REQs: REQ-F-023
  - Check: SidebarOverviewUpcoming visible when upcomingState == Loading; spinner or animated icon displayed; spinner hidden and event list shown when state transitions to Ready/ConnectError

- [x] T-052: Update CMakeLists.txt to add new CalDavProvider, IcsProvider, ICalParser, LibsecretCredentialStorage sources
  - REQs: REQ-F-017
  - Check: HOLONIGHT_SERVICES_SOURCES includes src/services/calendar/CalDavProvider.{h,cpp}, IcsProvider.{h,cpp}, ICalParser.{h,cpp}, LibsecretCredentialStorage.{h,cpp}

- [x] T-053: Update CMakeLists.txt to remove deleted OAuth sources (GoogleCalendarProvider, MsGraphCalendarProvider, OAuthRedirectServer, ITokenStorage, SqliteTokenStorage, LibsecretTokenStorage)
  - REQs: REQ-C-001
  - Check: HOLONIGHT_SERVICES_SOURCES no longer lists any deleted files

- [x] T-054: Create unit test for ICalParser VEVENT parsing with all-day detection and missing UID skip
  - REQs: REQ-F-008, REQ-F-011
  - Check: tests/test_calendar_integration.cpp (or new tests/test_ical_parser.cpp) includes test case for valid VEVENT with UID/SUMMARY/DTSTART/DTEND/DESCRIPTION; test case for all-day event (VALUE=DATE); test case for missing UID (logged qCDebug, event skipped); test case for missing DTSTART (logged qCDebug, event skipped); test case for DTSTAMP deduplication

- [x] T-055: Create CalDavProvider XML parsing fixture test with multiple server response samples (Nextcloud, iCloud)
  - REQs: REQ-F-005, REQ-F-007
  - Check: tests/test_calendar_integration.cpp includes fixture data for real PROPFIND and REPORT XML responses; test verifies namespace-aware XML parsing; test covers include/exclude filter logic

- [x] T-056: Create integration test verifying end-to-end CalDAV sync cache population and query performance
  - REQs: REQ-F-007, REQ-NF-004
  - Check: tests/test_calendar_integration.cpp has test mocking CalDAV HTTP responses, verifying cache.upsertEvents() called, cache.queryRange() returns events in order, query completes < 100 ms with 1000 events

- [x] T-057: Create integration test verifying ICS feed sync with UID deduplication and window filtering
  - REQs: REQ-F-010, REQ-F-013, REQ-F-014
  - Check: tests/test_calendar_integration.cpp has test with mock ICS response containing duplicates (different DTSTAMP); verifies higher DTSTAMP kept; verifies events outside -30d/+180d window excluded from cache

- [x] T-058: Create integration test verifying config reload invalidates cache for changed accounts
  - REQs: REQ-F-020, REQ-C-008
  - Check: tests/test_calendar_integration.cpp has test: populate cache, change config (e.g. URL), call onConfigReloaded(), verify old events deleted and config_hash updated

- [x] T-059: Build project and verify no compilation errors for all new sources
  - REQs: REQ-F-017
  - Check: task build completes without errors; task qmltypes-check verifies CalendarService in generated qmltypes with UpcomingState enum and properties

- [x] T-060: Run all calendar integration tests and verify test suite passes
  - REQs: REQ-F-008, REQ-F-013, REQ-F-020
  - Check: ctest -R test_calendar_integration passes all assertions

- [ ] T-061: End-to-end verification: build, run shell, open sidebar, verify calendar widget displays cached events
  - REQs: REQ-F-021, REQ-F-023, REQ-F-025
  - Check: task run launches shell; open sidebar via control socket; SidebarOverviewUpcoming displays up to 3 upcoming events with: left accent bar, absolute HH:mm time, title, duration•location second row (or all-day secondary text); relative time updates every minute; at-event-start notification is persistent (not auto-dismissed)
