#pragma once

#include "CalendarTypes.h"

#include <QList>
#include <QString>

// Minimal read-only iCalendar (RFC 5545) parser.
// Extracts VEVENT instances from VCALENDAR text. RRULE is parsed but never expanded.
// Thread-safe: pure static utility with no mutable state.
class ICalParser {
 public:
  // Parses iCalendar text and returns all valid VEVENT instances found.
  // Events with missing UID or DTSTART are skipped (qCDebug logged).
  // RRULE properties are silently ignored (out of scope per design).
  [[nodiscard]] static QList<CalendarEvent> parseEvents(const QString& ical_text, const QString& account_name,
                                                        const QString& provider_type);
};
