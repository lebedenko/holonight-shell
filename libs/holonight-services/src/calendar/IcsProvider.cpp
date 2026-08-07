#include "IcsProvider.h"

#include "ICalParser.h"

#include <QHash>
#include <QLoggingCategory>
#include <QNetworkRequest>

Q_LOGGING_CATEGORY(lcIcsProvider, "holonight.calendar.ics")

namespace {

SyncError makeConnectError(const QString& account, const QString& detail) {
  return {.kind = SyncError::Kind::ConnectError, .message = QStringLiteral("ICS %1: %2").arg(account, detail)};
}

// Decodes the response body as UTF-8, falling back to ISO-8859-1 on decode failure.
QString decodeBody(const QByteArray& body) {
  QString utf8 = QString::fromUtf8(body);
  // If UTF-8 decoding produced replacement characters, fall back to Latin-1.
  if (utf8.contains(QChar::ReplacementCharacter)) {
    return QString::fromLatin1(body);
  }
  return utf8;
}

}  // namespace

IcsProvider::IcsProvider(HoloNight::ShellConfig::CalendarIcsAccountConfig config)
    : config_(std::move(config)), http_client_(config_.account_name) {}

std::expected<QByteArray, SyncError> IcsProvider::httpGet() const {
  const QUrl feed_url(config_.url);
  QNetworkRequest req{feed_url};
  return http_client_.get(req);
}

QList<CalendarEvent> IcsProvider::deduplicateByUid(QList<CalendarEvent> events) {
  // Within a single ICS feed, keep the event with the latest DTSTAMP if UIDs collide.
  // If DTSTAMP is missing or invalid on one or both, we default to the last occurrence
  // by parse order (which approximates "last DTSTAMP wins" for feeds that list newer overrides last).
  QHash<QString, int> uid_index;
  QList<CalendarEvent> result;
  result.reserve(events.size());
  for (auto& evt : events) {
    const auto uid_pos = uid_index.find(evt.uid);
    if (uid_pos != uid_index.end()) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      auto& existing = result[uid_pos.value()];
      if (evt.dtstamp.isValid() && existing.dtstamp.isValid()) {
        if (evt.dtstamp > existing.dtstamp) {
          existing = std::move(evt);
        }
      } else {
        existing = std::move(evt);
      }
    } else {
      uid_index.insert(evt.uid, static_cast<int>(result.size()));
      result << std::move(evt);
    }
  }
  return result;
}

std::expected<void, SyncError> IcsProvider::testConnection() {
  const auto body_result = httpGet();
  if (!body_result) {
    return std::unexpected(body_result.error());
  }
  const QString text = decodeBody(*body_result);
  if (!text.startsWith(QLatin1String("BEGIN:VCALENDAR"), Qt::CaseInsensitive)) {
    return std::unexpected(makeConnectError(config_.account_name, QStringLiteral("response is not a VCALENDAR")));
  }
  return {};
}

std::expected<QList<CalendarEvent>, SyncError> IcsProvider::fetchEvents(const DateRange& range) {
  const auto body_result = httpGet();
  if (!body_result) {
    return std::unexpected(body_result.error());
  }

  const QString text = decodeBody(*body_result);
  if (!text.contains(QLatin1String("BEGIN:VCALENDAR"), Qt::CaseInsensitive)) {
    qCWarning(lcIcsProvider) << "ICS feed" << config_.account_name << "did not return a valid VCALENDAR response";
    return std::unexpected(makeConnectError(config_.account_name, QStringLiteral("response is not a VCALENDAR")));
  }

  QList<CalendarEvent> events = ICalParser::parseEvents(text, config_.account_name, QStringLiteral("ics"));
  events = deduplicateByUid(std::move(events));

  // Filter to the requested time window.
  QList<CalendarEvent> in_range;
  in_range.reserve(events.size());
  for (const auto& evt : events) {
    if (evt.start_time >= range.start_utc && evt.start_time < range.end_utc) {
      in_range << evt;
    }
  }

  qCDebug(lcIcsProvider) << "ICS" << config_.account_name << ": fetched" << in_range.size() << "events in range";
  return in_range;
}
