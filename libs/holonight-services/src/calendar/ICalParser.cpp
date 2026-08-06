#include "ICalParser.h"

#include <QDate>
#include <QHash>
#include <QLoggingCategory>
#include <QTimeZone>

Q_LOGGING_CATEGORY(lcICalParser, "holonight.calendar.ical")

namespace {

// RFC 5545 §3.1: join continuation lines (lines starting with SPACE or TAB to previous line).
QStringList unfoldLines(const QString& text) {
  const QList<QStringView> raw = QStringView{text}.split(QLatin1Char('\n'));
  QStringList result;
  result.reserve(raw.size());
  for (QStringView line : raw) {
    if (line.endsWith(QLatin1Char('\r'))) {
      line = line.chopped(1);
    }
    if (line.isEmpty()) {
      continue;
    }
    if ((line.at(0) == QLatin1Char(' ') || line.at(0) == QLatin1Char('\t')) && !result.isEmpty()) {
      result.last() += line.mid(1).toString();
    } else {
      result << line.toString();
    }
  }
  return result;
}

struct PropLine {
  QString name;    // upper-cased, e.g. "DTSTART"
  QString params;  // raw params string, e.g. "TZID=America/New_York"
  QString value;
};

// Splits a logical iCal content line into (name, params, value).
// Returns an empty-name PropLine on malformed input.
PropLine parsePropLine(const QString& line) {
  const qsizetype colon = line.indexOf(QLatin1Char(':'));
  if (colon < 0) {
    return {};
  }
  const QString header = line.left(colon);
  const QString value = line.mid(colon + 1);
  const qsizetype semi = header.indexOf(QLatin1Char(';'));
  if (semi >= 0) {
    return {.name = header.left(semi).toUpper(), .params = header.mid(semi + 1), .value = value};
  }
  return {.name = header.toUpper(), .params = {}, .value = value};
}

// Parses a DTSTART/DTEND value according to RFC 5545 §3.3.4 / §3.3.5.
// Sets is_all_day if VALUE=DATE (not DATE-TIME) is in params.
// Floating times (no Z, no TZID) are parsed as local time.
QDateTime parseDtValue(const QString& value, const QString& params, bool& is_all_day) {
  const bool is_date_type = params.contains(QLatin1String("VALUE=DATE"), Qt::CaseInsensitive) &&
                            !params.contains(QLatin1String("VALUE=DATE-TIME"), Qt::CaseInsensitive);

  if (is_date_type) {
    is_all_day = true;
    const QDate date = QDate::fromString(value, QStringLiteral("yyyyMMdd"));
    return date.isValid() ? date.startOfDay(QTimeZone::utc()) : QDateTime{};
  }

  is_all_day = false;

  if (value.endsWith(QLatin1Char('Z'))) {
    const QDateTime local_rep = QDateTime::fromString(value.chopped(1), QStringLiteral("yyyyMMddTHHmmss"));
    if (!local_rep.isValid()) {
      return {};
    }
    return {local_rep.date(), local_rep.time(), QTimeZone::utc()};
  }

  // TZID-annotated or floating local time — parse as local.
  return QDateTime::fromString(value, QStringLiteral("yyyyMMddTHHmmss"));
}

struct VEventData {
  QString uid;
  QString summary;
  QString dtstart_value;
  QString dtstart_params;
  QString dtend_value;
  QString dtend_params;
  QString description;
  QString location;
  QString dtstamp_value;

  // Remaining standard VEVENT fields
  QString rrule;
  QString duration;
  QString access_class;
  QString created_value;
  QString last_modified_value;
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

  void clear() { *this = VEventData{}; }
};

QDateTime addDuration(const QDateTime& start,  // NOLINT(readability-function-cognitive-complexity)
                      const QString& duration_str) {
  if (duration_str.isEmpty() || !start.isValid()) {
    return {};
  }
  bool is_negative = duration_str.startsWith(QLatin1Char('-'));
  QString duration_cleaned = is_negative ? duration_str.mid(1) : duration_str;
  if (duration_cleaned.startsWith(QLatin1Char('+'))) {
    duration_cleaned = duration_cleaned.mid(1);
  }
  if (!duration_cleaned.startsWith(QLatin1Char('P'))) {
    return {};
  }
  duration_cleaned = duration_cleaned.mid(1);  // strip 'P'

  int years = 0;
  int months = 0;
  int weeks = 0;
  int days = 0;
  int hours = 0;
  int minutes = 0;
  int seconds = 0;
  bool in_time = false;
  QString num_buf;
  for (int i = 0; i < duration_cleaned.length(); ++i) {
    QChar chr = duration_cleaned.at(i);
    if (chr == QLatin1Char('T')) {
      in_time = true;
      continue;
    }
    if (chr.isDigit()) {
      num_buf.append(chr);
    } else {
      int val = num_buf.toInt();
      num_buf.clear();
      if (!in_time) {
        if (chr == QLatin1Char('Y')) {
          years = val;
        } else if (chr == QLatin1Char('M')) {
          months = val;
        } else if (chr == QLatin1Char('W')) {
          weeks = val;
        } else if (chr == QLatin1Char('D')) {
          days = val;
        }
      } else {
        if (chr == QLatin1Char('H')) {
          hours = val;
        } else if (chr == QLatin1Char('M')) {
          minutes = val;
        } else if (chr == QLatin1Char('S')) {
          seconds = val;
        }
      }
    }
  }

  QDateTime res = start;
  int sign = is_negative ? -1 : 1;
  if (years != 0) {
    res = res.addYears(sign * years);
  }
  if (months != 0) {
    res = res.addMonths(sign * months);
  }
  if (weeks != 0) {
    res = res.addDays(sign * weeks * 7);
  }
  if (days != 0) {
    res = res.addDays(sign * days);
  }
  if (hours != 0) {
    res = res.addSecs(sign * hours * 3600);
  }
  if (minutes != 0) {
    res = res.addSecs(sign * minutes * 60);
  }
  if (seconds != 0) {
    res = res.addSecs(sign * seconds);
  }
  return res;
}

std::optional<CalendarEvent> buildCalendarEvent(const VEventData& vevt, const QString& account_name,
                                                const QString& provider_type) {
  if (vevt.uid.isEmpty()) {
    qCDebug(lcICalParser) << "Skipping VEVENT: missing UID";
    return std::nullopt;
  }
  if (vevt.dtstart_value.isEmpty()) {
    qCDebug(lcICalParser) << "Skipping VEVENT uid=" << vevt.uid << ": missing DTSTART";
    return std::nullopt;
  }

  bool is_all_day = false;
  const QDateTime start = parseDtValue(vevt.dtstart_value, vevt.dtstart_params, is_all_day);
  if (!start.isValid()) {
    qCDebug(lcICalParser) << "Skipping VEVENT uid=" << vevt.uid << ": unparseable DTSTART" << vevt.dtstart_value;
    return std::nullopt;
  }

  bool end_all_day = false;
  QDateTime end;
  if (!vevt.dtend_value.isEmpty()) {
    end = parseDtValue(vevt.dtend_value, vevt.dtend_params, end_all_day);
  } else if (!vevt.duration.isEmpty()) {
    end = addDuration(start, vevt.duration);
  }

  CalendarEvent evt;
  evt.uid = vevt.uid;
  evt.provider_type = provider_type;
  evt.account_name = account_name;
  evt.title = vevt.summary;
  evt.start_time = start;
  evt.end_time = end;
  evt.is_all_day = is_all_day;
  evt.description = vevt.description;
  evt.location = vevt.location;
  if (!vevt.dtstamp_value.isEmpty()) {
    bool dummy = false;
    evt.dtstamp = parseDtValue(vevt.dtstamp_value, {}, dummy);
  }

  // Set remaining standard VEVENT fields
  evt.rrule = vevt.rrule;
  evt.duration = vevt.duration;
  evt.access_class = vevt.access_class;
  if (!vevt.created_value.isEmpty()) {
    bool dummy = false;
    evt.created = parseDtValue(vevt.created_value, {}, dummy);
  }
  if (!vevt.last_modified_value.isEmpty()) {
    bool dummy = false;
    evt.last_modified = parseDtValue(vevt.last_modified_value, {}, dummy);
  }
  evt.organizer = vevt.organizer;
  evt.attendees = vevt.attendees;
  evt.categories = vevt.categories;
  evt.status = vevt.status;
  evt.transparency = vevt.transparency;
  evt.url = vevt.url;
  evt.geo = vevt.geo;
  evt.sequence = vevt.sequence;
  evt.recurrence_id = vevt.recurrence_id;
  evt.exdates = vevt.exdates;
  evt.rdates = vevt.rdates;
  evt.attachments = vevt.attachments;
  evt.comments = vevt.comments;
  evt.contacts = vevt.contacts;
  evt.related_to = vevt.related_to;
  evt.resources = vevt.resources;
  evt.alarms = vevt.alarms;

  return evt;
}

enum class VEventProperty : quint8 {
  Uid,
  Summary,
  DtStart,
  DtEnd,
  Description,
  Location,
  DtStamp,
  RRule,
  Duration,
  AccessClass,
  Created,
  LastModified,
  Organizer,
  Attendee,
  Categories,
  Status,
  Transparency,
  Url,
  Geo,
  Sequence,
  RecurrenceId,
  ExDate,
  RDate,
  Attachment,
  Comment,
  Contact,
  RelatedTo,
  Resources,
};

const QHash<QString, VEventProperty>& vEventProperties() {
  static const QHash<QString, VEventProperty> properties{
      {QStringLiteral("UID"), VEventProperty::Uid},
      {QStringLiteral("SUMMARY"), VEventProperty::Summary},
      {QStringLiteral("DTSTART"), VEventProperty::DtStart},
      {QStringLiteral("DTEND"), VEventProperty::DtEnd},
      {QStringLiteral("DESCRIPTION"), VEventProperty::Description},
      {QStringLiteral("LOCATION"), VEventProperty::Location},
      {QStringLiteral("DTSTAMP"), VEventProperty::DtStamp},
      {QStringLiteral("RRULE"), VEventProperty::RRule},
      {QStringLiteral("DURATION"), VEventProperty::Duration},
      {QStringLiteral("CLASS"), VEventProperty::AccessClass},
      {QStringLiteral("CREATED"), VEventProperty::Created},
      {QStringLiteral("LAST-MODIFIED"), VEventProperty::LastModified},
      {QStringLiteral("ORGANIZER"), VEventProperty::Organizer},
      {QStringLiteral("ATTENDEE"), VEventProperty::Attendee},
      {QStringLiteral("CATEGORIES"), VEventProperty::Categories},
      {QStringLiteral("STATUS"), VEventProperty::Status},
      {QStringLiteral("TRANSP"), VEventProperty::Transparency},
      {QStringLiteral("URL"), VEventProperty::Url},
      {QStringLiteral("GEO"), VEventProperty::Geo},
      {QStringLiteral("SEQUENCE"), VEventProperty::Sequence},
      {QStringLiteral("RECURRENCE-ID"), VEventProperty::RecurrenceId},
      {QStringLiteral("EXDATE"), VEventProperty::ExDate},
      {QStringLiteral("RDATE"), VEventProperty::RDate},
      {QStringLiteral("ATTACH"), VEventProperty::Attachment},
      {QStringLiteral("COMMENT"), VEventProperty::Comment},
      {QStringLiteral("CONTACT"), VEventProperty::Contact},
      {QStringLiteral("RELATED-TO"), VEventProperty::RelatedTo},
      {QStringLiteral("RESOURCES"), VEventProperty::Resources},
  };
  return properties;
}

// Accumulates a single property into the current VEventData.
void accumulateVEventProp(const PropLine& prop,  // NOLINT(readability-function-cognitive-complexity)
                          VEventData& current) {
  const auto property = vEventProperties().constFind(prop.name);
  if (property == vEventProperties().constEnd()) {
    return;
  }

  switch (*property) {
    case VEventProperty::Uid:
      current.uid = prop.value;
      break;
    case VEventProperty::Summary:
      current.summary = prop.value;
      break;
    case VEventProperty::DtStart:
      current.dtstart_value = prop.value;
      current.dtstart_params = prop.params;
      break;
    case VEventProperty::DtEnd:
      current.dtend_value = prop.value;
      current.dtend_params = prop.params;
      break;
    case VEventProperty::Description:
      current.description = prop.value;
      current.description.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
      current.description.replace(QStringLiteral("\\,"), QStringLiteral(","));
      current.description.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
      break;
    case VEventProperty::Location:
      current.location = prop.value;
      current.location.replace(QStringLiteral("\\,"), QStringLiteral(","));
      current.location.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
      break;
    case VEventProperty::DtStamp:
      current.dtstamp_value = prop.value;
      break;
    case VEventProperty::RRule:
      current.rrule = prop.value;
      break;
    case VEventProperty::Duration:
      current.duration = prop.value;
      break;
    case VEventProperty::AccessClass:
      current.access_class = prop.value;
      break;
    case VEventProperty::Created:
      current.created_value = prop.value;
      break;
    case VEventProperty::LastModified:
      current.last_modified_value = prop.value;
      break;
    case VEventProperty::Organizer:
      current.organizer = prop.params.isEmpty() ? prop.value : prop.params + QLatin1Char(':') + prop.value;
      break;
    case VEventProperty::Attendee:
      current.attendees.append(prop.params.isEmpty() ? prop.value : prop.params + QLatin1Char(':') + prop.value);
      break;
    case VEventProperty::Categories:
      current.categories.append(prop.value.split(QLatin1Char(',')));
      break;
    case VEventProperty::Status:
      current.status = prop.value;
      break;
    case VEventProperty::Transparency:
      current.transparency = prop.value;
      break;
    case VEventProperty::Url:
      current.url = prop.value;
      break;
    case VEventProperty::Geo:
      current.geo = prop.value;
      break;
    case VEventProperty::Sequence:
      current.sequence = prop.value.toInt();
      break;
    case VEventProperty::RecurrenceId:
      current.recurrence_id = prop.value;
      break;
    case VEventProperty::ExDate:
      current.exdates.append(prop.value.split(QLatin1Char(',')));
      break;
    case VEventProperty::RDate:
      current.rdates.append(prop.value.split(QLatin1Char(',')));
      break;
    case VEventProperty::Attachment:
      current.attachments.append(prop.value);
      break;
    case VEventProperty::Comment:
      current.comments.append(prop.value);
      break;
    case VEventProperty::Contact:
      current.contacts.append(prop.value);
      break;
    case VEventProperty::RelatedTo:
      current.related_to.append(prop.value);
      break;
    case VEventProperty::Resources:
      current.resources.append(prop.value.split(QLatin1Char(',')));
      break;
  }
}

struct ParseState {
  bool in_vcalendar{false};
  bool in_vevent{false};
  bool in_valarm{false};
  VEventData current;
  QString current_alarm;
};

// Processes one logical content line; appends to result if VEVENT is complete.
void processLine(const PropLine& prop, ParseState& state, QList<CalendarEvent>& result, const QString& account_name,
                 const QString& provider_type) {
  if (prop.name == QLatin1String("BEGIN")) {
    if (prop.value == QLatin1String("VCALENDAR")) {
      state.in_vcalendar = true;
    } else if (prop.value == QLatin1String("VEVENT") && state.in_vcalendar) {
      state.in_vevent = true;
      state.current.clear();
    } else if (prop.value == QLatin1String("VALARM") && state.in_vevent) {
      state.in_valarm = true;
      state.current_alarm.clear();
    }
    return;
  }

  if (prop.name == QLatin1String("END")) {
    if (prop.value == QLatin1String("VALARM") && state.in_valarm) {
      state.in_valarm = false;
      if (!state.current_alarm.isEmpty()) {
        state.current.alarms.append(state.current_alarm);
      }
    } else if (prop.value == QLatin1String("VEVENT") && state.in_vevent) {
      state.in_vevent = false;
      if (auto evt = buildCalendarEvent(state.current, account_name, provider_type)) {
        result << std::move(*evt);
      }
    } else if (prop.value == QLatin1String("VCALENDAR")) {
      state.in_vcalendar = false;
    }
    return;
  }

  if (state.in_valarm) {
    QString line_reconstructed = prop.name;
    if (!prop.params.isEmpty()) {
      line_reconstructed += QLatin1Char(';') + prop.params;
    }
    line_reconstructed += QLatin1Char(':') + prop.value;
    state.current_alarm += line_reconstructed + QStringLiteral("\n");
  } else if (state.in_vevent) {
    accumulateVEventProp(prop, state.current);
  }
}

}  // namespace

QList<CalendarEvent> ICalParser::parseEvents(const QString& ical_text, const QString& account_name,
                                             const QString& provider_type) {
  const QStringList lines = unfoldLines(ical_text);
  QList<CalendarEvent> result;
  ParseState state;

  for (const QString& line : lines) {
    const PropLine prop = parsePropLine(line);
    if (prop.name.isEmpty()) {
      continue;
    }
    processLine(prop, state, result, account_name, provider_type);
  }

  return result;
}
