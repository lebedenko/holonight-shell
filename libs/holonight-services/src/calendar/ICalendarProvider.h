#pragma once

#include "CalendarTypes.h"

#include <QList>
#include <QString>

#include <expected>

// Pure-virtual interface for read-only calendar data providers (CalDAV, ICS).
// Implementations are non-QObject and run synchronously on worker threads via QtConcurrent::run.
class ICalendarProvider {
 public:
  virtual ~ICalendarProvider() = default;

  ICalendarProvider(const ICalendarProvider&) = delete;
  ICalendarProvider& operator=(const ICalendarProvider&) = delete;
  ICalendarProvider(ICalendarProvider&&) = delete;
  ICalendarProvider& operator=(ICalendarProvider&&) = delete;

  // Returns the TOML section key (e.g. "work", "holidays").
  [[nodiscard]] virtual QString accountName() const = 0;

  // Returns the provider type tag ("caldav" or "ics").
  [[nodiscard]] virtual QString providerType() const = 0;

  // Verifies connectivity and credentials without fetching all events.
  // CalDAV: PROPFIND on principal URL. ICS: HTTP HEAD or GET with 10s timeout.
  // Called once on startup and after config reload, on a worker thread.
  [[nodiscard]] virtual std::expected<void, SyncError> testConnection() = 0;

  // Fetches all calendar events in [range.start_utc, range.end_utc).
  // CalDAV: .well-known discovery + PROPFIND calendar list + REPORT per calendar.
  // ICS: HTTP GET + parse full feed (filter by range in-memory).
  [[nodiscard]] virtual std::expected<QList<CalendarEvent>, SyncError> fetchEvents(const DateRange& range) = 0;

 protected:
  ICalendarProvider() = default;
};
