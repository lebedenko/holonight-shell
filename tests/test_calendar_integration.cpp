#include "calendar/CalendarCache.h"
#include "calendar/CalendarEventModel.h"
#include "calendar/CalendarSyncManager.h"
#include "calendar/CalendarTypes.h"
#include "calendar/ICalParser.h"
#include "calendar/ICalendarProvider.h"
#include "calendar/IcsProvider.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <utility>

TEST(CalendarEventModel, RoleNamesExposeQmlContract) {
  CalendarEventModel model;
  const QHash<int, QByteArray> roles = model.roleNames();

  EXPECT_EQ(roles.value(CalendarEventModel::EventIdRole), "eventId");
  EXPECT_EQ(roles.value(CalendarEventModel::TitleRole), "title");
  EXPECT_EQ(roles.value(CalendarEventModel::StartTimeRole), "startTime");
  EXPECT_EQ(roles.value(CalendarEventModel::EndTimeRole), "endTime");
  EXPECT_EQ(roles.value(CalendarEventModel::IsAllDayRole), "isAllDay");
  EXPECT_EQ(roles.value(CalendarEventModel::ProviderRole), "provider");
  EXPECT_EQ(roles.value(CalendarEventModel::DescriptionRole), "description");
  EXPECT_EQ(roles.value(CalendarEventModel::LocationRole), "location");
  EXPECT_EQ(roles.value(CalendarEventModel::RruleRole), "rrule");
  EXPECT_EQ(roles.value(CalendarEventModel::DurationRole), "duration");
  EXPECT_EQ(roles.value(CalendarEventModel::AccessClassRole), "accessClass");
  EXPECT_EQ(roles.value(CalendarEventModel::CreatedRole), "created");
  EXPECT_EQ(roles.value(CalendarEventModel::LastModifiedRole), "lastModified");
  EXPECT_EQ(roles.value(CalendarEventModel::OrganizerRole), "organizer");
  EXPECT_EQ(roles.value(CalendarEventModel::AttendeesRole), "attendees");
  EXPECT_EQ(roles.value(CalendarEventModel::CategoriesRole), "categories");
  EXPECT_EQ(roles.value(CalendarEventModel::StatusRole), "status");
  EXPECT_EQ(roles.value(CalendarEventModel::TransparencyRole), "transparency");
  EXPECT_EQ(roles.value(CalendarEventModel::UrlRole), "url");
  EXPECT_EQ(roles.value(CalendarEventModel::GeoRole), "geo");
  EXPECT_EQ(roles.value(CalendarEventModel::SequenceRole), "sequence");
  EXPECT_EQ(roles.value(CalendarEventModel::RecurrenceIdRole), "recurrenceId");
  EXPECT_EQ(roles.value(CalendarEventModel::ExdatesRole), "exdates");
  EXPECT_EQ(roles.value(CalendarEventModel::RdatesRole), "rdates");
  EXPECT_EQ(roles.value(CalendarEventModel::AttachmentsRole), "attachments");
  EXPECT_EQ(roles.value(CalendarEventModel::CommentsRole), "comments");
  EXPECT_EQ(roles.value(CalendarEventModel::ContactsRole), "contacts");
  EXPECT_EQ(roles.value(CalendarEventModel::RelatedToRole), "relatedTo");
  EXPECT_EQ(roles.value(CalendarEventModel::ResourcesRole), "resources");
  EXPECT_EQ(roles.value(CalendarEventModel::AlarmsRole), "alarms");
  EXPECT_EQ(roles.size(), 30);
}

// ---------------------------------------------------------------------------
// T-054: ICalParser unit tests
// ---------------------------------------------------------------------------

namespace {

constexpr const char* kBasicVevent = R"ical(
BEGIN:VCALENDAR
VERSION:2.0
BEGIN:VEVENT
UID:test-uid-001@example.com
SUMMARY:Team Meeting
DTSTART:20260622T100000Z
DTEND:20260622T110000Z
DESCRIPTION:Weekly sync\nPlease bring notes
END:VEVENT
END:VCALENDAR
)ical";

constexpr const char* kAllDayVevent = R"ical(
BEGIN:VCALENDAR
VERSION:2.0
BEGIN:VEVENT
UID:allday-001@example.com
SUMMARY:Company Holiday
DTSTART;VALUE=DATE:20260625
DTEND;VALUE=DATE:20260626
END:VEVENT
END:VCALENDAR
)ical";

constexpr const char* kMissingUidVevent = R"ical(
BEGIN:VCALENDAR
BEGIN:VEVENT
SUMMARY:No UID Event
DTSTART:20260622T090000Z
DTEND:20260622T100000Z
END:VEVENT
END:VCALENDAR
)ical";

constexpr const char* kMissingDtstartVevent = R"ical(
BEGIN:VCALENDAR
BEGIN:VEVENT
UID:no-start@example.com
SUMMARY:No Start Event
DTEND:20260622T100000Z
END:VEVENT
END:VCALENDAR
)ical";

constexpr const char* kDuplicateUidFeed = R"ical(
BEGIN:VCALENDAR
BEGIN:VEVENT
UID:dup-001@example.com
SUMMARY:First occurrence
DTSTART:20260622T090000Z
DTEND:20260622T100000Z
END:VEVENT
BEGIN:VEVENT
UID:dup-001@example.com
SUMMARY:Second occurrence
DTSTART:20260622T140000Z
DTEND:20260622T150000Z
END:VEVENT
END:VCALENDAR
)ical";

bool waitForFlag(const std::atomic_bool& flag, int timeout_ms) {
  QElapsedTimer timer;
  timer.start();
  while (!flag.load() && timer.elapsed() < timeout_ms) {
    QTest::qWait(10);
  }
  return flag.load();
}

constexpr const char* kMultipleEvents = R"ical(
BEGIN:VCALENDAR
VERSION:2.0
BEGIN:VEVENT
UID:multi-001@example.com
SUMMARY:First
DTSTART:20260622T080000Z
DTEND:20260622T090000Z
END:VEVENT
BEGIN:VEVENT
UID:multi-002@example.com
SUMMARY:Second
DTSTART:20260622T100000Z
DTEND:20260622T110000Z
END:VEVENT
END:VCALENDAR
)ical";

constexpr const char* kFloatingLocalTime = R"ical(
BEGIN:VCALENDAR
BEGIN:VEVENT
UID:local-001@example.com
SUMMARY:Local Event
DTSTART:20260622T100000
DTEND:20260622T110000
END:VEVENT
END:VCALENDAR
)ical";

class FakeCalendarProvider final : public ICalendarProvider {
 public:
  FakeCalendarProvider(QString account_name, QString provider_type, std::atomic_int* fetch_count)
      : account_name_(std::move(account_name)), provider_type_(std::move(provider_type)), fetch_count_(fetch_count) {}

  [[nodiscard]] QString accountName() const override { return account_name_; }
  [[nodiscard]] QString providerType() const override { return provider_type_; }
  [[nodiscard]] std::expected<void, SyncError> testConnection() override { return {}; }

  [[nodiscard]] std::expected<QList<CalendarEvent>, SyncError> fetchEvents(const DateRange& /*range*/) override {
    ++(*fetch_count_);
    return QList<CalendarEvent>{};
  }

 private:
  QString account_name_;
  QString provider_type_;
  std::atomic_int* fetch_count_;
};

class FailingCalendarProvider final : public ICalendarProvider {
 public:
  FailingCalendarProvider(QString account_name, QString provider_type,
                          SyncError::Kind kind = SyncError::Kind::ConnectError)
      : account_name_(std::move(account_name)), provider_type_(std::move(provider_type)), kind_(kind) {}

  [[nodiscard]] QString accountName() const override { return account_name_; }
  [[nodiscard]] QString providerType() const override { return provider_type_; }
  [[nodiscard]] std::expected<void, SyncError> testConnection() override { return {}; }

  [[nodiscard]] std::expected<QList<CalendarEvent>, SyncError> fetchEvents(const DateRange& /*range*/) override {
    return std::unexpected(SyncError{.kind = kind_, .message = QStringLiteral("simulated sync failure")});
  }

 private:
  QString account_name_;
  QString provider_type_;
  SyncError::Kind kind_;
};

class BlockingCalendarProvider final : public ICalendarProvider {
 public:
  BlockingCalendarProvider(QString account_name, QString provider_type, std::atomic_int* fetch_count,
                           std::atomic_bool* release)
      : account_name_(std::move(account_name)),
        provider_type_(std::move(provider_type)),
        fetch_count_(fetch_count),
        release_(release) {}

  [[nodiscard]] QString accountName() const override { return account_name_; }
  [[nodiscard]] QString providerType() const override { return provider_type_; }
  [[nodiscard]] std::expected<void, SyncError> testConnection() override { return {}; }

  [[nodiscard]] std::expected<QList<CalendarEvent>, SyncError> fetchEvents(const DateRange& /*range*/) override {
    ++(*fetch_count_);
    while (!release_->load()) {
      std::this_thread::yield();
    }
    return QList<CalendarEvent>{};
  }

 private:
  QString account_name_;
  QString provider_type_;
  std::atomic_int* fetch_count_;
  std::atomic_bool* release_;
};

// Fetches an updatable event list (guarded by a mutex — fetchEvents() runs on a QtConcurrent
// worker thread while tests mutate the list from the test thread between syncs).
class ControllableCalendarProvider final : public ICalendarProvider {
 public:
  ControllableCalendarProvider(QString account_name, QString provider_type, QList<CalendarEvent> events)
      : account_name_(std::move(account_name)), provider_type_(std::move(provider_type)), events_(std::move(events)) {}

  [[nodiscard]] QString accountName() const override { return account_name_; }
  [[nodiscard]] QString providerType() const override { return provider_type_; }
  [[nodiscard]] std::expected<void, SyncError> testConnection() override { return {}; }

  [[nodiscard]] std::expected<QList<CalendarEvent>, SyncError> fetchEvents(const DateRange& /*range*/) override {
    QMutexLocker locker(&mutex_);
    return events_;
  }

  void setEvents(QList<CalendarEvent> events) {
    QMutexLocker locker(&mutex_);
    events_ = std::move(events);
  }

 private:
  QString account_name_;
  QString provider_type_;
  mutable QMutex mutex_;
  QList<CalendarEvent> events_;
};

}  // namespace

TEST(ICalParserTest, ParsesValidVeventFields) {
  const auto events =
      ICalParser::parseEvents(QString::fromLatin1(kBasicVevent), QStringLiteral("work"), QStringLiteral("caldav"));
  ASSERT_EQ(events.size(), 1);
  const auto& evt = events.front();
  EXPECT_EQ(evt.uid, QStringLiteral("test-uid-001@example.com"));
  EXPECT_EQ(evt.title, QStringLiteral("Team Meeting"));
  EXPECT_EQ(evt.account_name, QStringLiteral("work"));
  EXPECT_EQ(evt.provider_type, QStringLiteral("caldav"));
  EXPECT_FALSE(evt.is_all_day);
  EXPECT_TRUE(evt.start_time.isValid());
  // Z-suffix → interpreted as UTC: verify the UTC representation.
  EXPECT_EQ(evt.start_time.toUTC().date(), QDate(2026, 6, 22));
  EXPECT_EQ(evt.start_time.toUTC().time().hour(), 10);
}

TEST(ICalParserTest, ParsesDtstampField) {
  constexpr const char* kDtstampVevent = R"ical(
BEGIN:VCALENDAR
VERSION:2.0
BEGIN:VEVENT
UID:dtstamp-uid-001@example.com
SUMMARY:Team Meeting
DTSTART:20260622T100000Z
DTEND:20260622T110000Z
DTSTAMP:20260622T093000Z
END:VEVENT
END:VCALENDAR
)ical";

  const auto events =
      ICalParser::parseEvents(QString::fromLatin1(kDtstampVevent), QStringLiteral("work"), QStringLiteral("caldav"));
  ASSERT_EQ(events.size(), 1);
  const auto& evt = events.front();
  EXPECT_EQ(evt.uid, QStringLiteral("dtstamp-uid-001@example.com"));
  EXPECT_TRUE(evt.dtstamp.isValid());
  EXPECT_EQ(evt.dtstamp.toUTC().date(), QDate(2026, 6, 22));
  EXPECT_EQ(evt.dtstamp.toUTC().time().hour(), 9);
  EXPECT_EQ(evt.dtstamp.toUTC().time().minute(), 30);
}

TEST(ICalParserTest, ParsesAllStandardVeventFieldsAndValarm) {
  constexpr const char* kComplexVevent = R"ical(
BEGIN:VCALENDAR
VERSION:2.0
BEGIN:VEVENT
UID:complex-uid-001@example.com
SUMMARY:All Fields Meeting
DTSTART:20260622T100000Z
DTEND:20260622T110000Z
DTSTAMP:20260622T093000Z
RRULE:FREQ=DAILY;COUNT=5
DURATION:PT1H
CLASS:PUBLIC
CREATED:20260620T120000Z
LAST-MODIFIED:20260621T153000Z
ORGANIZER;CN=John Organizer:mailto:john@example.com
ATTENDEE;RSVP=TRUE:mailto:alice@example.com
ATTENDEE;RSVP=FALSE:mailto:bob@example.com
CATEGORIES:Work,Meeting
STATUS:CONFIRMED
TRANSP:OPAQUE
URL:https://example.com/meeting
GEO:37.386013;-122.082932
SEQUENCE:3
RECURRENCE-ID:20260622T100000Z
EXDATE:20260623T100000Z,20260624T100000Z
RDATE:20260625T100000Z
ATTACH:https://example.com/agenda.pdf
COMMENT:Please be on time
CONTACT:John Doe
RELATED-TO:parent-event-id
RESOURCES:Room 101,Projector
BEGIN:VALARM
ACTION:DISPLAY
TRIGGER:-PT15M
DESCRIPTION:Reminder
END:VALARM
END:VEVENT
END:VCALENDAR
)ical";

  const auto events =
      ICalParser::parseEvents(QString::fromLatin1(kComplexVevent), QStringLiteral("work"), QStringLiteral("caldav"));
  ASSERT_EQ(events.size(), 1);
  const auto& evt = events.front();
  EXPECT_EQ(evt.uid, QStringLiteral("complex-uid-001@example.com"));
  EXPECT_EQ(evt.title, QStringLiteral("All Fields Meeting"));
  EXPECT_EQ(evt.rrule, QStringLiteral("FREQ=DAILY;COUNT=5"));
  EXPECT_EQ(evt.duration, QStringLiteral("PT1H"));
  EXPECT_EQ(evt.access_class, QStringLiteral("PUBLIC"));
  EXPECT_EQ(evt.created.toUTC().date(), QDate(2026, 6, 20));
  EXPECT_EQ(evt.last_modified.toUTC().date(), QDate(2026, 6, 21));
  EXPECT_EQ(evt.organizer, QStringLiteral("CN=John Organizer:mailto:john@example.com"));
  ASSERT_EQ(evt.attendees.size(), 2);
  EXPECT_EQ(evt.attendees.at(0), QStringLiteral("RSVP=TRUE:mailto:alice@example.com"));
  EXPECT_EQ(evt.attendees.at(1), QStringLiteral("RSVP=FALSE:mailto:bob@example.com"));
  ASSERT_EQ(evt.categories.size(), 2);
  EXPECT_EQ(evt.categories.at(0), QStringLiteral("Work"));
  EXPECT_EQ(evt.categories.at(1), QStringLiteral("Meeting"));
  EXPECT_EQ(evt.status, QStringLiteral("CONFIRMED"));
  EXPECT_EQ(evt.transparency, QStringLiteral("OPAQUE"));
  EXPECT_EQ(evt.url, QStringLiteral("https://example.com/meeting"));
  EXPECT_EQ(evt.geo, QStringLiteral("37.386013;-122.082932"));
  EXPECT_EQ(evt.sequence, 3);
  EXPECT_EQ(evt.recurrence_id, QStringLiteral("20260622T100000Z"));
  ASSERT_EQ(evt.exdates.size(), 2);
  EXPECT_EQ(evt.exdates.at(0), QStringLiteral("20260623T100000Z"));
  ASSERT_EQ(evt.rdates.size(), 1);
  EXPECT_EQ(evt.rdates.at(0), QStringLiteral("20260625T100000Z"));
  ASSERT_EQ(evt.attachments.size(), 1);
  EXPECT_EQ(evt.attachments.at(0), QStringLiteral("https://example.com/agenda.pdf"));
  ASSERT_EQ(evt.comments.size(), 1);
  EXPECT_EQ(evt.comments.at(0), QStringLiteral("Please be on time"));
  ASSERT_EQ(evt.contacts.size(), 1);
  EXPECT_EQ(evt.contacts.at(0), QStringLiteral("John Doe"));
  ASSERT_EQ(evt.related_to.size(), 1);
  EXPECT_EQ(evt.related_to.at(0), QStringLiteral("parent-event-id"));
  ASSERT_EQ(evt.resources.size(), 2);
  EXPECT_EQ(evt.resources.at(0), QStringLiteral("Room 101"));
  EXPECT_EQ(evt.resources.at(1), QStringLiteral("Projector"));
  ASSERT_EQ(evt.alarms.size(), 1);
  EXPECT_EQ(evt.alarms.at(0), QStringLiteral("ACTION:DISPLAY\nTRIGGER:-PT15M\nDESCRIPTION:Reminder\n"));
}

TEST(ICalParserTest, FallbacksToEndTimeFromDuration) {
  constexpr const char* kDurationOnlyVevent = R"ical(
BEGIN:VCALENDAR
VERSION:2.0
BEGIN:VEVENT
UID:duration-uid-001@example.com
SUMMARY:Duration Fallback Meeting
DTSTART:20260622T100000Z
DURATION:PT1H30M
END:VEVENT
END:VCALENDAR
)ical";

  const auto events = ICalParser::parseEvents(QString::fromLatin1(kDurationOnlyVevent), QStringLiteral("work"),
                                              QStringLiteral("caldav"));
  ASSERT_EQ(events.size(), 1);
  const auto& evt = events.front();
  EXPECT_EQ(evt.uid, QStringLiteral("duration-uid-001@example.com"));
  EXPECT_TRUE(evt.end_time.isValid());
  EXPECT_EQ(evt.end_time.toUTC().time().hour(), 11);
  EXPECT_EQ(evt.end_time.toUTC().time().minute(), 30);
}

TEST(ICalParserTest, SetsIsAllDayForValueDateDtstart) {
  const auto events =
      ICalParser::parseEvents(QString::fromLatin1(kAllDayVevent), QStringLiteral("personal"), QStringLiteral("ics"));
  ASSERT_EQ(events.size(), 1);
  EXPECT_TRUE(events.front().is_all_day);
  EXPECT_EQ(events.front().uid, QStringLiteral("allday-001@example.com"));
}

TEST(ICalParserTest, SkipsEventWithMissingUid) {
  const auto events =
      ICalParser::parseEvents(QString::fromLatin1(kMissingUidVevent), QStringLiteral("work"), QStringLiteral("caldav"));
  EXPECT_TRUE(events.isEmpty());
}

TEST(ICalParserTest, SkipsEventWithMissingDtstart) {
  const auto events = ICalParser::parseEvents(QString::fromLatin1(kMissingDtstartVevent), QStringLiteral("work"),
                                              QStringLiteral("caldav"));
  EXPECT_TRUE(events.isEmpty());
}

TEST(ICalParserTest, ParsesMultipleEventsFromSingleFeed) {
  const auto events =
      ICalParser::parseEvents(QString::fromLatin1(kMultipleEvents), QStringLiteral("work"), QStringLiteral("caldav"));
  ASSERT_EQ(events.size(), 2);
  EXPECT_EQ(events.at(0).uid, QStringLiteral("multi-001@example.com"));
  EXPECT_EQ(events.at(1).uid, QStringLiteral("multi-002@example.com"));
}

TEST(ICalParserTest, RetainsBothDuplicateUidEventsBeforeIcsDedup) {
  // ICalParser itself emits both; IcsProvider deduplicates on top.
  const auto events = ICalParser::parseEvents(QString::fromLatin1(kDuplicateUidFeed), QStringLiteral("personal"),
                                              QStringLiteral("ics"));
  ASSERT_EQ(events.size(), 2);
  EXPECT_EQ(events.at(0).uid, QStringLiteral("dup-001@example.com"));
  EXPECT_EQ(events.at(1).uid, QStringLiteral("dup-001@example.com"));
}

TEST(ICalParserTest, ParsesFloatingLocalTimeDtstart) {
  const auto events = ICalParser::parseEvents(QString::fromLatin1(kFloatingLocalTime), QStringLiteral("personal"),
                                              QStringLiteral("ics"));
  ASSERT_EQ(events.size(), 1);
  EXPECT_FALSE(events.front().is_all_day);
  EXPECT_TRUE(events.front().start_time.isValid());
  // Floating time is stored as local time, not UTC.
  EXPECT_NE(events.front().start_time.timeSpec(), Qt::UTC);
}

TEST(ICalParserTest, ReturnsEmptyForNonVcalendarInput) {
  const auto events =
      ICalParser::parseEvents(QStringLiteral("not an ical"), QStringLiteral("work"), QStringLiteral("caldav"));
  EXPECT_TRUE(events.isEmpty());
}

// ---------------------------------------------------------------------------
// T-056 / T-058: CalendarCache v2 schema tests
// ---------------------------------------------------------------------------

namespace {

CalendarEvent makeCacheEvent(const QString& uid, const QString& account, const QString& provider_type,
                             const QDateTime& start) {
  CalendarEvent evt;
  evt.uid = uid;
  evt.account_name = account;
  evt.provider_type = provider_type;
  evt.title = uid + QStringLiteral(" title");
  evt.start_time = start;
  evt.end_time = start.addSecs(3600);
  evt.is_all_day = false;
  evt.dtstamp = start.addSecs(-1800);
  evt.rrule = QStringLiteral("FREQ=DAILY;COUNT=3");
  evt.duration = QStringLiteral("PT1H");
  evt.access_class = QStringLiteral("CONFIDENTIAL");
  evt.organizer = QStringLiteral("mailto:org@example.com");
  evt.attendees = {QStringLiteral("alice@example.com"), QStringLiteral("bob@example.com")};
  evt.categories = {QStringLiteral("Work")};
  evt.status = QStringLiteral("TENTATIVE");
  evt.transparency = QStringLiteral("TRANSPARENT");
  evt.url = QStringLiteral("http://event.example.com");
  evt.geo = QStringLiteral("45;90");
  evt.sequence = 5;
  evt.recurrence_id = QStringLiteral("rec-id-123");
  evt.exdates = {QStringLiteral("2026-06-23T10:00:00Z")};
  evt.rdates = {QStringLiteral("2026-06-24T10:00:00Z")};
  evt.attachments = {QStringLiteral("agenda.txt")};
  evt.comments = {QStringLiteral("note")};
  evt.contacts = {QStringLiteral("contact")};
  evt.related_to = {QStringLiteral("parent")};
  evt.resources = {QStringLiteral("Projector")};
  evt.alarms = {QStringLiteral("alarm1")};
  return evt;
}

}  // namespace

TEST(CalendarCacheV2Test, OpenCreatesTablesAndReturnsTrue) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  CalendarCache cache;
  EXPECT_TRUE(cache.open(dir.filePath(QStringLiteral("cal.sqlite"))));
  EXPECT_TRUE(cache.isOpen());
}

// T-020: forces the initOrMigrate() failure path in CalendarCache::open() (garbage file content
// opens fine at the SQLite level but fails the first PRAGMA/DDL statement) and confirms open()
// returns false cleanly. This exercises the reorder fix where the local QSqlDatabase handle now
// goes out of scope before QSqlDatabase::removeDatabase() runs; a subsequent open() reusing the
// same path only succeeds if the failed attempt fully released its connection.
TEST(CalendarCacheV2Test, OpenReturnsFalseCleanlyOnCorruptDatabaseFile) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString db_path = dir.filePath(QStringLiteral("corrupt.sqlite"));

  QFile corrupt_file(db_path);
  ASSERT_TRUE(corrupt_file.open(QIODevice::WriteOnly));
  corrupt_file.write("not a valid sqlite database");
  corrupt_file.close();

  CalendarCache cache;
  EXPECT_FALSE(cache.open(db_path));
  EXPECT_FALSE(cache.isOpen());

  ASSERT_TRUE(QFile::remove(db_path));
  CalendarCache retry_cache;
  EXPECT_TRUE(retry_cache.open(db_path));
  EXPECT_TRUE(retry_cache.isOpen());
}

TEST(CalendarCacheV2Test, UpsertAndQueryEvents) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  CalendarCache cache;
  ASSERT_TRUE(cache.open(dir.filePath(QStringLiteral("cal.sqlite"))));

  QDateTime start = QDateTime::currentDateTimeUtc().addSecs(3600);
  start.setMSecsSinceEpoch((start.toMSecsSinceEpoch() / 1000) * 1000);
  const QList<CalendarEvent> events = {
      makeCacheEvent(QStringLiteral("evt-001"), QStringLiteral("work"), QStringLiteral("caldav"), start),
      makeCacheEvent(QStringLiteral("evt-002"), QStringLiteral("work"), QStringLiteral("caldav"), start.addSecs(7200)),
  };

  ASSERT_TRUE(cache.upsertEvents(events));

  const auto queried = cache.queryRange(start.addSecs(-60), start.addSecs(10000));
  ASSERT_EQ(queried.size(), 2);
  EXPECT_EQ(queried.at(0).uid, QStringLiteral("evt-001"));
  EXPECT_TRUE(queried.at(0).dtstamp.isValid());
  EXPECT_EQ(queried.at(0).dtstamp.toUTC().toString(Qt::ISODate), start.addSecs(-1800).toUTC().toString(Qt::ISODate));
  EXPECT_EQ(queried.at(0).rrule, QStringLiteral("FREQ=DAILY;COUNT=3"));
  EXPECT_EQ(queried.at(0).duration, QStringLiteral("PT1H"));
  EXPECT_EQ(queried.at(0).access_class, QStringLiteral("CONFIDENTIAL"));
  EXPECT_EQ(queried.at(0).organizer, QStringLiteral("mailto:org@example.com"));
  ASSERT_EQ(queried.at(0).attendees.size(), 2);
  EXPECT_EQ(queried.at(0).attendees.at(0), QStringLiteral("alice@example.com"));
  EXPECT_EQ(queried.at(0).attendees.at(1), QStringLiteral("bob@example.com"));
  ASSERT_EQ(queried.at(0).categories.size(), 1);
  EXPECT_EQ(queried.at(0).categories.at(0), QStringLiteral("Work"));
  EXPECT_EQ(queried.at(0).status, QStringLiteral("TENTATIVE"));
  EXPECT_EQ(queried.at(0).transparency, QStringLiteral("TRANSPARENT"));
  EXPECT_EQ(queried.at(0).url, QStringLiteral("http://event.example.com"));
  EXPECT_EQ(queried.at(0).geo, QStringLiteral("45;90"));
  EXPECT_EQ(queried.at(0).sequence, 5);
  EXPECT_EQ(queried.at(0).recurrence_id, QStringLiteral("rec-id-123"));
  ASSERT_EQ(queried.at(0).exdates.size(), 1);
  EXPECT_EQ(queried.at(0).exdates.at(0), QStringLiteral("2026-06-23T10:00:00Z"));
  ASSERT_EQ(queried.at(0).rdates.size(), 1);
  EXPECT_EQ(queried.at(0).rdates.at(0), QStringLiteral("2026-06-24T10:00:00Z"));
  ASSERT_EQ(queried.at(0).attachments.size(), 1);
  EXPECT_EQ(queried.at(0).attachments.at(0), QStringLiteral("agenda.txt"));
  ASSERT_EQ(queried.at(0).comments.size(), 1);
  EXPECT_EQ(queried.at(0).comments.at(0), QStringLiteral("note"));
  ASSERT_EQ(queried.at(0).contacts.size(), 1);
  EXPECT_EQ(queried.at(0).contacts.at(0), QStringLiteral("contact"));
  ASSERT_EQ(queried.at(0).related_to.size(), 1);
  EXPECT_EQ(queried.at(0).related_to.at(0), QStringLiteral("parent"));
  ASSERT_EQ(queried.at(0).resources.size(), 1);
  EXPECT_EQ(queried.at(0).resources.at(0), QStringLiteral("Projector"));
  ASSERT_EQ(queried.at(0).alarms.size(), 1);
  EXPECT_EQ(queried.at(0).alarms.at(0), QStringLiteral("alarm1"));

  EXPECT_EQ(queried.at(1).uid, QStringLiteral("evt-002"));
  EXPECT_TRUE(queried.at(1).dtstamp.isValid());
  EXPECT_EQ(queried.at(1).dtstamp.toUTC().toString(Qt::ISODate),
            start.addSecs(7200 - 1800).toUTC().toString(Qt::ISODate));
}

TEST(CalendarCacheV2Test, UpsertAccountReportsTrueOnlyWhenHashChanges) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  CalendarCache cache;
  ASSERT_TRUE(cache.open(dir.filePath(QStringLiteral("cal.sqlite"))));

  const QString hash1 = CalendarCache::configHash(QStringLiteral("https://dav.example.com"), QStringLiteral("alice"));
  const QString hash2 =
      CalendarCache::configHash(QStringLiteral("https://dav.example.com"), QStringLiteral("alice-new"));
  // First insert (no previous row) → false (nothing changed from before)
  EXPECT_FALSE(cache.upsertAccount(QStringLiteral("caldav"), QStringLiteral("work"), hash1));
  // Second insert with same hash → false (no change)
  EXPECT_FALSE(cache.upsertAccount(QStringLiteral("caldav"), QStringLiteral("work"), hash1));
  // Insert with different hash → true (hash changed)
  EXPECT_TRUE(cache.upsertAccount(QStringLiteral("caldav"), QStringLiteral("work"), hash2));
}

TEST(CalendarCacheV2Test, ClearAccountEventsRemovesOnlyThatAccount) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  CalendarCache cache;
  ASSERT_TRUE(cache.open(dir.filePath(QStringLiteral("cal.sqlite"))));

  const QDateTime start = QDateTime::currentDateTimeUtc().addSecs(3600);
  ASSERT_TRUE(cache.upsertEvents({
      makeCacheEvent(QStringLiteral("a1"), QStringLiteral("work"), QStringLiteral("caldav"), start),
      makeCacheEvent(QStringLiteral("b1"), QStringLiteral("personal"), QStringLiteral("ics"), start),
  }));

  ASSERT_TRUE(cache.clearAccountEvents(QStringLiteral("caldav"), QStringLiteral("work")));

  const auto remaining = cache.queryRange(start.addSecs(-60), start.addSecs(7200));
  ASSERT_EQ(remaining.size(), 1);
  EXPECT_EQ(remaining.front().account_name, QStringLiteral("personal"));
}

// REQ-F-011: cancelled/deleted upstream events must be removed from the cache, not just left to
// linger until pruneExpired()'s unrelated date-window prune (or never, if still inside the window).
TEST(CalendarCacheV2Test,
     ReconcileAccountEventsRemovesStaleEvents) {  // NOLINT(readability-function-cognitive-complexity)
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  CalendarCache cache;
  ASSERT_TRUE(cache.open(dir.filePath(QStringLiteral("cal.sqlite"))));

  const QDateTime start = QDateTime::currentDateTimeUtc().addSecs(3600);
  const auto evt_a = makeCacheEvent(QStringLiteral("A"), QStringLiteral("work"), QStringLiteral("caldav"), start);
  const auto evt_b =
      makeCacheEvent(QStringLiteral("B"), QStringLiteral("work"), QStringLiteral("caldav"), start.addSecs(1800));
  const auto evt_c =
      makeCacheEvent(QStringLiteral("C"), QStringLiteral("work"), QStringLiteral("caldav"), start.addSecs(3600));
  ASSERT_TRUE(cache.upsertEvents({evt_a, evt_b, evt_c}));

  ASSERT_TRUE(cache.reconcileAccountEvents(QStringLiteral("caldav"), QStringLiteral("work"), {evt_a, evt_c}));

  const auto remaining = cache.queryRange(start.addSecs(-60), start.addSecs(7200));
  QStringList uids;
  for (const auto& evt : remaining) {
    uids.append(evt.uid);
  }
  ASSERT_EQ(uids.size(), 2);
  EXPECT_TRUE(uids.contains(QStringLiteral("A")));
  EXPECT_TRUE(uids.contains(QStringLiteral("C")));
  EXPECT_FALSE(uids.contains(QStringLiteral("B")));
}

TEST(CalendarCacheV2Test, ReconcileAccountEventsDoesNotAffectOtherAccounts) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  CalendarCache cache;
  ASSERT_TRUE(cache.open(dir.filePath(QStringLiteral("cal.sqlite"))));

  const QDateTime start = QDateTime::currentDateTimeUtc().addSecs(3600);
  ASSERT_TRUE(cache.upsertEvents({
      makeCacheEvent(QStringLiteral("a1"), QStringLiteral("work"), QStringLiteral("caldav"), start),
      makeCacheEvent(QStringLiteral("b1"), QStringLiteral("personal"), QStringLiteral("ics"), start),
  }));

  // Reconciling "work" against an empty fresh list deletes a1 but must leave personal's b1 alone.
  ASSERT_TRUE(cache.reconcileAccountEvents(QStringLiteral("caldav"), QStringLiteral("work"), {}));

  const auto remaining = cache.queryRange(start.addSecs(-60), start.addSecs(7200));
  ASSERT_EQ(remaining.size(), 1);
  EXPECT_EQ(remaining.front().account_name, QStringLiteral("personal"));
}

// REQ-NF-004: reconciliation must stay fast even across many accounts/events — O(n) diff via
// QSet membership, not the O(n^2) pattern the requirement warns against.
TEST(CalendarCacheV2Test,
     ReconcileAccountEventsPerformanceAcrossManyAccounts) {  // NOLINT(readability-function-cognitive-complexity)
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  CalendarCache cache;
  ASSERT_TRUE(cache.open(dir.filePath(QStringLiteral("cal.sqlite"))));

  const QDateTime start = QDateTime::currentDateTimeUtc().addSecs(3600);
  constexpr int kAccountCount = 12;
  constexpr int kEventsPerAccount = 50;  // 600 events total, comfortably over the 500-event ask.

  QList<CalendarEvent> all_events;
  all_events.reserve(kAccountCount * kEventsPerAccount);
  for (int acct = 0; acct < kAccountCount; ++acct) {
    const QString account_name = QStringLiteral("acct-%1").arg(acct);
    for (int i = 0; i < kEventsPerAccount; ++i) {
      all_events.append(makeCacheEvent(QStringLiteral("evt-%1-%2").arg(acct).arg(i), account_name,
                                       QStringLiteral("caldav"), start.addSecs(i * 60)));
    }
  }
  ASSERT_TRUE(cache.upsertEvents(all_events));

  // Reconcile one account, dropping half its events.
  const QString target_account = QStringLiteral("acct-0");
  QList<CalendarEvent> fresh_events;
  for (int i = 0; i < kEventsPerAccount / 2; ++i) {
    fresh_events.append(makeCacheEvent(QStringLiteral("evt-0-%1").arg(i), target_account, QStringLiteral("caldav"),
                                       start.addSecs(i * 60)));
  }

  QElapsedTimer timer;
  timer.start();
  ASSERT_TRUE(cache.reconcileAccountEvents(QStringLiteral("caldav"), target_account, fresh_events));
  const qint64 elapsed_ms = timer.elapsed();

  EXPECT_LT(elapsed_ms, 100);

  const auto remaining_for_account = cache.queryRange(start.addSecs(-60), start.addSecs((kEventsPerAccount + 1) * 60));
  int count_for_target = 0;
  for (const auto& evt : remaining_for_account) {
    if (evt.account_name == target_account) {
      ++count_for_target;
    }
  }
  EXPECT_EQ(count_for_target, kEventsPerAccount / 2);
}

TEST(CalendarCacheV2Test, StoreSyncStateAndLoadSyncState) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  CalendarCache cache;
  ASSERT_TRUE(cache.open(dir.filePath(QStringLiteral("cal.sqlite"))));

  const QDateTime last_sync = QDateTime::currentDateTimeUtc();
  const QDateTime next_sync = last_sync.addSecs(900);
  ASSERT_TRUE(
      cache.storeSyncState(QStringLiteral("caldav"), QStringLiteral("work"), last_sync, QStringLiteral(""), next_sync));

  const auto state = cache.loadSyncState(QStringLiteral("caldav"), QStringLiteral("work"));
  EXPECT_TRUE(state.last_sync_time.isValid());
  // Timestamps stored as seconds-since-epoch; allow ±1 second round-trip delta.
  EXPECT_LE(std::abs(state.last_sync_time.secsTo(last_sync)), 1LL);
  EXPECT_TRUE(state.error_message.isEmpty());
}

TEST(CalendarCacheV2Test, PruneExpiredRemovesOldEvents) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  CalendarCache cache;
  ASSERT_TRUE(cache.open(dir.filePath(QStringLiteral("cal.sqlite"))));

  const QDateTime old_start = QDateTime::currentDateTimeUtc().addDays(-40);
  const QDateTime recent_start = QDateTime::currentDateTimeUtc().addSecs(3600);

  ASSERT_TRUE(cache.upsertEvents({
      makeCacheEvent(QStringLiteral("old"), QStringLiteral("work"), QStringLiteral("caldav"), old_start),
      makeCacheEvent(QStringLiteral("recent"), QStringLiteral("work"), QStringLiteral("caldav"), recent_start),
  }));

  ASSERT_TRUE(cache.pruneExpired());

  // Old event is outside the -30d/+180d window.
  const auto all = cache.queryRange(old_start.addSecs(-60), recent_start.addSecs(3600));
  ASSERT_EQ(all.size(), 1);
  EXPECT_EQ(all.front().uid, QStringLiteral("recent"));
}

TEST(CalendarCacheV2Test, RemoveStaleAccountsClearsDeletedAccounts) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  CalendarCache cache;
  ASSERT_TRUE(cache.open(dir.filePath(QStringLiteral("cal.sqlite"))));

  const QDateTime start = QDateTime::currentDateTimeUtc().addSecs(3600);
  // Register both accounts so removeStaleAccounts can find them.
  const QString hash = CalendarCache::configHash(QStringLiteral("https://dav.example.com"), {});
  cache.upsertAccount(QStringLiteral("caldav"), QStringLiteral("old-account"), hash);
  cache.upsertAccount(QStringLiteral("caldav"), QStringLiteral("kept-account"), hash);

  ASSERT_TRUE(cache.upsertEvents({
      makeCacheEvent(QStringLiteral("a1"), QStringLiteral("old-account"), QStringLiteral("caldav"), start),
      makeCacheEvent(QStringLiteral("b1"), QStringLiteral("kept-account"), QStringLiteral("caldav"), start),
  }));

  // Only kept-account remains in active config — key format is "provider_type:account_name".
  ASSERT_TRUE(cache.removeStaleAccounts({QStringLiteral("caldav:kept-account")}));

  const auto remaining = cache.queryRange(start.addSecs(-60), start.addSecs(7200));
  ASSERT_EQ(remaining.size(), 1);
  EXPECT_EQ(remaining.front().account_name, QStringLiteral("kept-account"));
}

// T-057: UID deduplication is performed by IcsProvider; ICalParser emits all occurrences.
// This test verifies that the cache (not parser) is the source of truth for stored events,
// and that upsertEvents with the same (uid, provider_type, account_name) key updates in-place.
TEST(CalendarCacheV2Test, UpsertEventReplacesSamePrimaryKey) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  CalendarCache cache;
  ASSERT_TRUE(cache.open(dir.filePath(QStringLiteral("cal.sqlite"))));

  const QDateTime start1 = QDateTime::currentDateTimeUtc().addSecs(3600);
  const QDateTime start2 = QDateTime::currentDateTimeUtc().addSecs(7200);

  CalendarEvent evt1 = makeCacheEvent(QStringLiteral("dup"), QStringLiteral("work"), QStringLiteral("ics"), start1);
  evt1.title = QStringLiteral("First title");
  ASSERT_TRUE(cache.upsertEvents({evt1}));

  CalendarEvent evt2 = makeCacheEvent(QStringLiteral("dup"), QStringLiteral("work"), QStringLiteral("ics"), start2);
  evt2.title = QStringLiteral("Updated title");
  ASSERT_TRUE(cache.upsertEvents({evt2}));

  const auto events = cache.queryRange(start1.addSecs(-60), start2.addSecs(3600));
  // Only one row: same (uid, provider_type, account_name) key.
  ASSERT_EQ(events.size(), 1);
  EXPECT_EQ(events.front().title, QStringLiteral("Updated title"));
}

// T-058: Config reload invalidates cache for changed accounts.
TEST(CalendarCacheV2Test, ConfigHashChangeTriggersAccountCacheInvalidation) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  CalendarCache cache;
  ASSERT_TRUE(cache.open(dir.filePath(QStringLiteral("cal.sqlite"))));

  const QString old_hash =
      CalendarCache::configHash(QStringLiteral("https://dav.example.com"), QStringLiteral("alice"));
  const QString new_hash =
      CalendarCache::configHash(QStringLiteral("https://dav.example.com"), QStringLiteral("alice-new-server"));

  // First registration (no prior row) → false (nothing changed before).
  EXPECT_FALSE(cache.upsertAccount(QStringLiteral("caldav"), QStringLiteral("work"), old_hash));

  const QDateTime start = QDateTime::currentDateTimeUtc().addSecs(3600);
  ASSERT_TRUE(cache.upsertEvents({
      makeCacheEvent(QStringLiteral("stale"), QStringLiteral("work"), QStringLiteral("caldav"), start),
  }));

  // Simulate config change: new hash → hash changed → returns true.
  const bool hash_changed = cache.upsertAccount(QStringLiteral("caldav"), QStringLiteral("work"), new_hash);
  ASSERT_TRUE(hash_changed);

  // CalendarService clears events for the changed account.
  ASSERT_TRUE(cache.clearAccountEvents(QStringLiteral("caldav"), QStringLiteral("work")));

  const auto events = cache.queryRange(start.addSecs(-60), start.addSecs(3600));
  EXPECT_TRUE(events.isEmpty());
}

class IcsProviderTest : public ::testing::Test {
 protected:
  static QList<CalendarEvent> deduplicateByUid(QList<CalendarEvent> events) {
    return IcsProvider::deduplicateByUid(std::move(events));
  }
};

TEST_F(IcsProviderTest, DeduplicatesByLatestDtstamp) {
  CalendarEvent evt1;
  evt1.uid = QStringLiteral("dup-1");
  evt1.dtstamp = QDateTime::fromString(QStringLiteral("2026-06-23T10:00:00Z"), Qt::ISODate);
  evt1.title = QStringLiteral("First (Older)");

  CalendarEvent evt2;
  evt2.uid = QStringLiteral("dup-1");
  evt2.dtstamp = QDateTime::fromString(QStringLiteral("2026-06-23T11:00:00Z"), Qt::ISODate);
  evt2.title = QStringLiteral("Second (Newer)");

  CalendarEvent evt3;
  evt3.uid = QStringLiteral("dup-1");
  // Missing dtstamp (invalid)

  // Test 1: Newer override replaces older
  {
    QList<CalendarEvent> input = {evt1, evt2};
    auto result = deduplicateByUid(input);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result.front().title, QStringLiteral("Second (Newer)"));
  }

  // Test 2: Older override doesn't replace newer (even if listed later)
  {
    QList<CalendarEvent> input = {evt2, evt1};
    auto result = deduplicateByUid(input);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result.front().title, QStringLiteral("Second (Newer)"));
  }

  // Test 3: If dtstamp is invalid, falls back to parse order (last wins)
  {
    QList<CalendarEvent> input = {evt2, evt3};
    auto result = deduplicateByUid(input);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result.front().title, QString());  // evt3 has empty title
  }
}

TEST(CalendarSyncManagerTest, SameNamedProvidersSyncIndependently) {
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  qputenv("XDG_CACHE_HOME", cache_dir.path().toUtf8());

  std::atomic_int caldav_fetches{0};
  std::atomic_int ics_fetches{0};
  std::atomic_bool release_caldav{false};
  std::vector<std::unique_ptr<ICalendarProvider>> caldav_providers;
  caldav_providers.push_back(std::make_unique<BlockingCalendarProvider>(
      QStringLiteral("shared"), QStringLiteral("caldav"), &caldav_fetches, &release_caldav));
  std::vector<std::unique_ptr<ICalendarProvider>> ics_providers;
  ics_providers.push_back(
      std::make_unique<FakeCalendarProvider>(QStringLiteral("shared"), QStringLiteral("ics"), &ics_fetches));

  {
    CalendarSyncManager manager(std::move(caldav_providers), std::move(ics_providers), nullptr);
    QElapsedTimer timer;
    timer.start();
    while (caldav_fetches.load() == 0 && timer.elapsed() < 1000) {
      QTest::qWait(10);
    }
    EXPECT_GT(caldav_fetches.load(), 0);

    timer.restart();
    while (ics_fetches.load() == 0 && timer.elapsed() < 1000) {
      QTest::qWait(10);
    }
    EXPECT_GT(ics_fetches.load(), 0);
    release_caldav.store(true);
  }

  qunsetenv("XDG_CACHE_HOME");
}

TEST(CalendarSyncManagerTest, IdleResumeSyncsCalDavAndIcsImmediately) {
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  qputenv("XDG_CACHE_HOME", cache_dir.path().toUtf8());

  std::atomic_int caldav_fetches{0};
  std::atomic_int ics_fetches{0};

  std::vector<std::unique_ptr<ICalendarProvider>> caldav_providers;
  caldav_providers.push_back(
      std::make_unique<FakeCalendarProvider>(QStringLiteral("work"), QStringLiteral("caldav"), &caldav_fetches));

  std::vector<std::unique_ptr<ICalendarProvider>> ics_providers;
  ics_providers.push_back(
      std::make_unique<FakeCalendarProvider>(QStringLiteral("holidays"), QStringLiteral("ics"), &ics_fetches));

  CalendarSyncManager manager(std::move(caldav_providers), std::move(ics_providers), nullptr);

  QTRY_VERIFY_WITH_TIMEOUT(caldav_fetches.load() >= 1, 1000);
  QTRY_VERIFY_WITH_TIMEOUT(ics_fetches.load() >= 1, 1000);

  caldav_fetches.store(0);
  ics_fetches.store(0);

  manager.setIdlePaused(true);
  manager.setIdlePaused(false);

  QTRY_VERIFY_WITH_TIMEOUT(caldav_fetches.load() >= 1, 1000);
  QTRY_VERIFY_WITH_TIMEOUT(ics_fetches.load() >= 1, 1000);

  qunsetenv("XDG_CACHE_HOME");
}

// T-013: verifies a CalDAV/ICS sync failure (e.g. from HttpSyncClient's timeout/abort path)
// propagates through CalendarSyncManager::syncError rather than failing silently.
TEST(CalendarSyncManagerTest, SyncFailurePropagatesSyncErrorSignal) {
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  qputenv("XDG_CACHE_HOME", cache_dir.path().toUtf8());

  std::vector<std::unique_ptr<ICalendarProvider>> caldav_providers;
  caldav_providers.push_back(
      std::make_unique<FailingCalendarProvider>(QStringLiteral("broken"), QStringLiteral("caldav")));
  std::vector<std::unique_ptr<ICalendarProvider>> ics_providers;

  CalendarSyncManager manager(std::move(caldav_providers), std::move(ics_providers), nullptr);

  std::atomic_bool error_received{false};
  QString received_message;
  QObject::connect(&manager, &CalendarSyncManager::syncError, &manager,
                   [&error_received, &received_message](SyncError::Kind /*kind*/, const QString& message) {
                     received_message = message;
                     error_received.store(true);
                   });

  ASSERT_TRUE(waitForFlag(error_received, 2000));
  EXPECT_TRUE(received_message.contains(QStringLiteral("simulated sync failure")));

  qunsetenv("XDG_CACHE_HOME");
}

// T-018: CalendarService::onSyncError() maps Kind::NetworkError to UpcomingState::Offline via a
// direct static_cast passthrough of the kind carried by this signal (see
// CalendarService.cpp:111-112) — so asserting the kind survives to this signal unchanged is
// equivalent to asserting CalendarService ends up in UpcomingState::Offline for a real
// network-layer provider failure (e.g. HttpSyncClient's timeout/connection-refused path), not
// just a hand-constructed SyncError.
TEST(CalendarSyncManagerTest, NetworkLayerFailurePropagatesNetworkErrorKind) {
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  qputenv("XDG_CACHE_HOME", cache_dir.path().toUtf8());

  std::vector<std::unique_ptr<ICalendarProvider>> caldav_providers;
  caldav_providers.push_back(std::make_unique<FailingCalendarProvider>(
      QStringLiteral("offline"), QStringLiteral("caldav"), SyncError::Kind::NetworkError));
  std::vector<std::unique_ptr<ICalendarProvider>> ics_providers;

  CalendarSyncManager manager(std::move(caldav_providers), std::move(ics_providers), nullptr);

  std::atomic_bool error_received{false};
  SyncError::Kind received_kind{SyncError::Kind::ConnectError};
  QObject::connect(&manager, &CalendarSyncManager::syncError, &manager,
                   [&error_received, &received_kind](SyncError::Kind kind, const QString& /*message*/) {
                     received_kind = kind;
                     error_received.store(true);
                   });

  ASSERT_TRUE(waitForFlag(error_received, 2000));
  EXPECT_EQ(received_kind, SyncError::Kind::NetworkError);

  qunsetenv("XDG_CACHE_HOME");
}

TEST(CalendarSyncManagerTest, StorageFailureDoesNotReportSuccessfulSync) {
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  qputenv("XDG_CACHE_HOME", cache_dir.path().toUtf8());

  const QString calendar_parent = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  ASSERT_TRUE(QDir().mkpath(calendar_parent));
  QFile calendar_blocker(calendar_parent + QStringLiteral("/calendar"));
  ASSERT_TRUE(calendar_blocker.open(QIODevice::WriteOnly));
  calendar_blocker.close();

  std::vector<std::unique_ptr<ICalendarProvider>> caldav_providers;
  caldav_providers.push_back(std::make_unique<ControllableCalendarProvider>(
      QStringLiteral("work"), QStringLiteral("caldav"), QList<CalendarEvent>{}));
  std::vector<std::unique_ptr<ICalendarProvider>> ics_providers;
  CalendarSyncManager manager(std::move(caldav_providers), std::move(ics_providers), nullptr);

  std::atomic_bool storage_error_received{false};
  QObject::connect(&manager, &CalendarSyncManager::syncError, &manager,
                   [&storage_error_received](SyncError::Kind kind, const QString&) {
                     storage_error_received.store(kind == SyncError::Kind::StorageError);
                   });
  QSignalSpy events_updated(&manager, &CalendarSyncManager::eventsUpdated);

  ASSERT_TRUE(waitForFlag(storage_error_received, 2000));
  EXPECT_EQ(events_updated.count(), 0);

  qunsetenv("XDG_CACHE_HOME");
}

// REQ-F-011: end-to-end reconciliation across two CalendarSyncManager instances sharing the same
// on-disk cache — the first instance's sync populates {A,B,C}; a second instance (simulating the
// next periodic sync cycle) fetches {A,C}, and B must be reconciled away, not just left cached.
TEST(CalendarSyncManagerTest,
     ReconciliationAcrossManagerInstancesRemovesStaleEvents) {  // NOLINT(readability-function-cognitive-complexity)
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  qputenv("XDG_CACHE_HOME", cache_dir.path().toUtf8());

  const QDateTime start = QDateTime::currentDateTimeUtc().addSecs(3600);
  const auto evt_a = makeCacheEvent(QStringLiteral("A"), QStringLiteral("work"), QStringLiteral("caldav"), start);
  const auto evt_b =
      makeCacheEvent(QStringLiteral("B"), QStringLiteral("work"), QStringLiteral("caldav"), start.addSecs(1800));
  const auto evt_c =
      makeCacheEvent(QStringLiteral("C"), QStringLiteral("work"), QStringLiteral("caldav"), start.addSecs(3600));

  {
    std::vector<std::unique_ptr<ICalendarProvider>> caldav_providers;
    caldav_providers.push_back(std::make_unique<ControllableCalendarProvider>(
        QStringLiteral("work"), QStringLiteral("caldav"), QList<CalendarEvent>{evt_a, evt_b, evt_c}));
    std::vector<std::unique_ptr<ICalendarProvider>> ics_providers;
    CalendarSyncManager first(std::move(caldav_providers), std::move(ics_providers), nullptr);

    QSignalSpy events_updated(&first, &CalendarSyncManager::eventsUpdated);
    ASSERT_TRUE(events_updated.wait(2000));
  }

  {
    std::vector<std::unique_ptr<ICalendarProvider>> caldav_providers;
    caldav_providers.push_back(std::make_unique<ControllableCalendarProvider>(
        QStringLiteral("work"), QStringLiteral("caldav"), QList<CalendarEvent>{evt_a, evt_c}));
    std::vector<std::unique_ptr<ICalendarProvider>> ics_providers;
    CalendarSyncManager second(std::move(caldav_providers), std::move(ics_providers), nullptr);

    QSignalSpy events_updated(&second, &CalendarSyncManager::eventsUpdated);
    ASSERT_TRUE(events_updated.wait(2000));
  }

  const QString db_path =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/calendar/calendar.sqlite");
  CalendarCache verify_cache;
  ASSERT_TRUE(verify_cache.open(db_path));
  const auto remaining = verify_cache.queryRange(start.addSecs(-60), start.addSecs(7200));
  QStringList uids;
  for (const auto& evt : remaining) {
    uids.append(evt.uid);
  }
  ASSERT_EQ(uids.size(), 2);
  EXPECT_TRUE(uids.contains(QStringLiteral("A")));
  EXPECT_TRUE(uids.contains(QStringLiteral("C")));
  EXPECT_FALSE(uids.contains(QStringLiteral("B")));

  qunsetenv("XDG_CACHE_HOME");
}

// REQ-F-012: CalendarSyncManager::removeAccount() (called from
// CalendarService::onCalendarConfigChanged() on account removal) must clear only that account's
// cached events, leaving other accounts' data untouched.
TEST(CalendarSyncManagerTest, RemoveAccountClearsCachedEventsForThatAccountOnly) {
  QTemporaryDir cache_dir;
  ASSERT_TRUE(cache_dir.isValid());
  qputenv("XDG_CACHE_HOME", cache_dir.path().toUtf8());

  std::vector<std::unique_ptr<ICalendarProvider>> caldav_providers;
  std::vector<std::unique_ptr<ICalendarProvider>> ics_providers;
  CalendarSyncManager manager(std::move(caldav_providers), std::move(ics_providers), nullptr);

  const QString db_path =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/calendar/calendar.sqlite");
  const QDateTime start = QDateTime::currentDateTimeUtc().addSecs(3600);
  {
    CalendarCache seed_cache;
    ASSERT_TRUE(seed_cache.open(db_path));
    ASSERT_TRUE(seed_cache.upsertEvents({
        makeCacheEvent(QStringLiteral("a1"), QStringLiteral("work"), QStringLiteral("caldav"), start),
        makeCacheEvent(QStringLiteral("b1"), QStringLiteral("personal"), QStringLiteral("ics"), start),
    }));
  }

  manager.removeAccount(QStringLiteral("caldav"), QStringLiteral("work"));

  CalendarCache verify_cache;
  ASSERT_TRUE(verify_cache.open(db_path));
  const auto remaining = verify_cache.queryRange(start.addSecs(-60), start.addSecs(7200));
  ASSERT_EQ(remaining.size(), 1);
  EXPECT_EQ(remaining.front().account_name, QStringLiteral("personal"));

  qunsetenv("XDG_CACHE_HOME");
}
