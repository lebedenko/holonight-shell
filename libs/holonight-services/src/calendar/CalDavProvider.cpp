#include "CalDavProvider.h"

#include "ICalParser.h"

#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QUrl>
#include <QXmlStreamReader>

Q_LOGGING_CATEGORY(lcCalDav, "holonight.calendar.caldav")

namespace {

const QString kDavNs = QStringLiteral("DAV:");
const QString kCalDavNs = QStringLiteral("urn:ietf:params:xml:ns:caldav");

// Parses the PROPFIND Depth:0 response to extract the current-user-principal or redirected URL.
// Returns empty string if not found.
QString extractPrincipalFromWellKnown(const QByteArray& xml) {
  QXmlStreamReader reader(xml);
  bool in_href = false;
  bool after_current_principal = false;
  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement()) {
      if (reader.namespaceUri() == kDavNs && reader.name() == QLatin1String("current-user-principal")) {
        after_current_principal = true;
      }
      if (after_current_principal && reader.namespaceUri() == kDavNs && reader.name() == QLatin1String("href")) {
        in_href = true;
      }
    } else if (reader.isCharacters() && in_href) {
      return reader.text().toString().trimmed();
    } else if (reader.isEndElement()) {
      in_href = false;
      if (reader.namespaceUri() == kDavNs && reader.name() == QLatin1String("current-user-principal")) {
        after_current_principal = false;
      }
    }
  }
  return {};
}

struct CalendarEntry {
  QString url;
  QString display_name;
};

struct CalendarListParseState {
  CalendarEntry current;
  bool in_response{false};
  bool has_calendar_type{false};
  bool in_href{false};
  bool in_displayname{false};
};

void handleCalendarListStartElement(QXmlStreamReader& reader, CalendarListParseState& state) {
  const auto nsuri = reader.namespaceUri();
  const auto elem = reader.name();
  if (nsuri == kDavNs && elem == QLatin1String("response")) {
    state.in_response = true;
    state.current = {};
    state.has_calendar_type = false;
  } else if (state.in_response && nsuri == kDavNs && elem == QLatin1String("href")) {
    state.in_href = true;
  } else if (state.in_response && nsuri == kCalDavNs && elem == QLatin1String("calendar")) {
    state.has_calendar_type = true;
  } else if (state.in_response && nsuri == kDavNs && elem == QLatin1String("displayname")) {
    state.in_displayname = true;
  }
}

// Parses a PROPFIND multistatus response and returns calendar collection entries.
QList<CalendarEntry> parseCalendarList(const QByteArray& xml, const QUrl& base_url) {
  QXmlStreamReader reader(xml);
  QList<CalendarEntry> result;
  CalendarListParseState state;

  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement()) {
      handleCalendarListStartElement(reader, state);
    } else if (reader.isCharacters()) {
      if (state.in_href) {
        state.current.url = base_url.resolved(QUrl(reader.text().toString().trimmed())).toString();
      }
      if (state.in_displayname) {
        state.current.display_name = reader.text().toString().trimmed();
      }
    } else if (reader.isEndElement()) {
      const auto nsuri = reader.namespaceUri();
      const auto elem = reader.name();
      state.in_href = false;
      state.in_displayname = false;
      if (nsuri == kDavNs && elem == QLatin1String("response")) {
        state.in_response = false;
        if (state.has_calendar_type && !state.current.url.isEmpty()) {
          result << state.current;
        }
      }
    }
  }
  return result;
}

// Extracts VCALENDAR text blocks from a REPORT multistatus response.
QStringList parseCalendarData(const QByteArray& xml) {
  QXmlStreamReader reader(xml);
  QStringList result;
  bool in_calendar_data = false;

  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement() && reader.namespaceUri() == kCalDavNs &&
        reader.name() == QLatin1String("calendar-data")) {
      in_calendar_data = true;
    } else if (reader.isCharacters() && in_calendar_data) {
      const QString text = reader.text().toString();
      if (!text.trimmed().isEmpty()) {
        result << text;
      }
    } else if (reader.isEndElement() && reader.namespaceUri() == kCalDavNs &&
               reader.name() == QLatin1String("calendar-data")) {
      in_calendar_data = false;
    }
  }
  return result;
}

QString buildPropfindBody() {
  return QStringLiteral(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<D:propfind xmlns:D=\"DAV:\" xmlns:C=\"urn:ietf:params:xml:ns:caldav\">"
      "  <D:prop><D:resourcetype/><D:displayname/></D:prop>"
      "</D:propfind>");
}

QString buildReportBody(const QDateTime& start, const QDateTime& end) {
  const QString start_str = start.toUTC().toString(QStringLiteral("yyyyMMddTHHmmssZ"));
  const QString end_str = end.toUTC().toString(QStringLiteral("yyyyMMddTHHmmssZ"));
  return QStringLiteral(
             "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
             "<C:calendar-query xmlns:D=\"DAV:\" xmlns:C=\"urn:ietf:params:xml:ns:caldav\">"
             "  <D:prop><D:getetag/><C:calendar-data/></D:prop>"
             "  <C:filter>"
             "    <C:comp-filter name=\"VCALENDAR\">"
             "      <C:comp-filter name=\"VEVENT\">"
             "        <C:time-range start=\"%1\" end=\"%2\"/>"
             "      </C:comp-filter>"
             "    </C:comp-filter>"
             "  </C:filter>"
             "</C:calendar-query>")
      .arg(start_str, end_str);
}

SyncError makeConnectError(const QString& account, const QString& detail) {
  return {.kind = SyncError::Kind::ConnectError, .message = QStringLiteral("CalDAV %1: %2").arg(account, detail)};
}

}  // namespace

CalDavProvider::CalDavProvider(CalendarCaldavAccountConfig config, const LibsecretCredentialStorage* credentials)
    : config_(std::move(config)), credentials_(credentials), http_client_(config_.account_name) {}

QString CalDavProvider::basicAuthHeader(const QString& username, const QString& password) {
  const QByteArray credentials = (username + QLatin1Char(':') + password).toUtf8().toBase64();
  return QStringLiteral("Basic ") + QString::fromLatin1(credentials);
}

QString CalDavProvider::resolvePrincipalUrl(const QString& password) const {
  const QUrl server_url(config_.url);
  const QUrl well_known =
      QUrl(server_url.scheme() + QStringLiteral("://") + server_url.host() +
           (server_url.port() != -1 ? QStringLiteral(":") + QString::number(server_url.port()) : QString{}) +
           QStringLiteral("/.well-known/caldav"));

  QNetworkRequest req(well_known);
  req.setRawHeader("Authorization", basicAuthHeader(config_.username, password).toUtf8());
  req.setRawHeader("Depth", "0");
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml"));

  const auto result = http_client_.sendCustomRequest(req, "PROPFIND", buildPropfindBody().toUtf8());
  if (!result) {
    qCDebug(lcCalDav) << "Well-known discovery failed for" << config_.account_name << "— using configured URL";
    return config_.url;
  }

  const QString principal = extractPrincipalFromWellKnown(*result);
  if (principal.isEmpty()) {
    qCDebug(lcCalDav) << "No principal URL in well-known response for" << config_.account_name
                      << "— using configured URL";
    return config_.url;
  }

  return well_known.resolved(QUrl(principal)).toString();
}

std::expected<QStringList, SyncError> CalDavProvider::discoverCalendars(const QString& principal_url,
                                                                        const QString& password) const {
  const QUrl principal_qurl(principal_url);
  QNetworkRequest req{principal_qurl};
  req.setRawHeader("Authorization", basicAuthHeader(config_.username, password).toUtf8());
  req.setRawHeader("Depth", "1");
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml"));

  const auto result = http_client_.sendCustomRequest(req, "PROPFIND", buildPropfindBody().toUtf8());
  if (!result) {
    return std::unexpected(result.error());
  }

  const QList<CalendarEntry> entries = parseCalendarList(*result, QUrl(principal_url));

  QStringList calendar_urls;
  for (const auto& entry : entries) {
    if (!config_.include.isEmpty() && !config_.include.contains(entry.display_name)) {
      continue;
    }
    if (config_.exclude.contains(entry.display_name)) {
      continue;
    }
    calendar_urls << entry.url;
  }

  qCDebug(lcCalDav) << "Discovered" << calendar_urls.size() << "calendar(s) for" << config_.account_name;
  return calendar_urls;
}

std::expected<QList<CalendarEvent>, SyncError> CalDavProvider::fetchCalendarEvents(const QString& calendar_url,
                                                                                   const QString& password,
                                                                                   const DateRange& range) const {
  const QUrl cal_qurl(calendar_url);
  QNetworkRequest req{cal_qurl};
  req.setRawHeader("Authorization", basicAuthHeader(config_.username, password).toUtf8());
  req.setRawHeader("Depth", "1");
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml"));

  const QString report_body = buildReportBody(range.start_utc, range.end_utc);
  const auto result = http_client_.sendCustomRequest(req, "REPORT", report_body.toUtf8());
  if (!result) {
    qCWarning(lcCalDav) << "REPORT failed for" << calendar_url << ":" << result.error().message;
    return std::unexpected(result.error());
  }

  const QStringList ical_blocks = parseCalendarData(*result);
  QList<CalendarEvent> events;
  for (const QString& ical : ical_blocks) {
    const auto parsed = ICalParser::parseEvents(ical, config_.account_name, QStringLiteral("caldav"));
    events << parsed;
  }
  return events;
}

std::expected<void, SyncError> CalDavProvider::testConnection() {
  const auto password_opt = credentials_->lookupPassword(config_.password_keyring_key);
  if (!password_opt) {
    return std::unexpected(makeConnectError(config_.account_name, QStringLiteral("keyring lookup failed")));
  }

  const QUrl config_qurl(config_.url);
  QNetworkRequest req{config_qurl};
  req.setRawHeader("Authorization", basicAuthHeader(config_.username, *password_opt).toUtf8());
  req.setRawHeader("Depth", "0");
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml"));

  const auto result = http_client_.sendCustomRequest(req, "PROPFIND", buildPropfindBody().toUtf8());
  if (!result) {
    return std::unexpected(result.error());
  }
  return {};
}

std::expected<QList<CalendarEvent>, SyncError> CalDavProvider::fetchEvents(const DateRange& range) {
  const auto password_opt = credentials_->lookupPassword(config_.password_keyring_key);
  if (!password_opt) {
    return std::unexpected(makeConnectError(config_.account_name, QStringLiteral("keyring lookup failed")));
  }
  const QString& password = *password_opt;

  const QString principal_url = resolvePrincipalUrl(password);

  const auto calendars_result = discoverCalendars(principal_url, password);
  if (!calendars_result) {
    return std::unexpected(calendars_result.error());
  }

  QList<CalendarEvent> all_events;

  for (const QString& cal_url : *calendars_result) {
    const auto events_result = fetchCalendarEvents(cal_url, password, range);
    if (!events_result) {
      return std::unexpected(events_result.error());
    }
    for (const auto& evt : *events_result) {
      all_events << evt;
    }
  }

  return all_events;
}
