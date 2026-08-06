#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <cstdint>

struct DateRange {
  QDateTime start_utc;
  QDateTime end_utc;
};

struct CalendarEvent {
  QString uid;
  QString provider_type;  // "caldav" | "ics"
  QString account_name;
  QString title;
  QDateTime start_time;
  QDateTime end_time;
  bool is_all_day{false};
  QString description;
  QString location;
  QDateTime dtstamp;

  // Remaining standard VEVENT fields
  QString rrule;
  QString duration;
  QString access_class;
  QDateTime created;
  QDateTime last_modified;
  QString organizer;
  QStringList attendees;
  QStringList categories;
  QString status;
  QString transparency;
  QString url;
  QString geo;
  int sequence{0};
  QString recurrence_id;
  QStringList exdates;
  QStringList rdates;
  QStringList attachments;
  QStringList comments;
  QStringList contacts;
  QStringList related_to;
  QStringList resources;
  QStringList alarms;

  bool operator==(const CalendarEvent&) const = default;
};

struct SyncError {
  enum class Kind : std::uint8_t {
    ConnectError,
    NetworkError,
    ParseError,
    StorageError,
  };
  Kind kind{Kind::ConnectError};
  QString message;
};
