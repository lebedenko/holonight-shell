#include "CalendarCache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

#include <array>

Q_LOGGING_CATEGORY(lcCalendarCache, "holonight.calendar.cache")

namespace {

bool execQuery(QSqlQuery& query, const char* ctx) {
  if (!query.exec()) {
    qCWarning(lcCalendarCache) << ctx << "failed:" << query.lastError().text();
    return false;
  }
  return true;
}

bool execDdl(QSqlDatabase& database, const char* sql) {
  QSqlQuery query(database);
  if (!query.exec(QString::fromLatin1(sql))) {
    qCWarning(lcCalendarCache) << "DDL failed:" << query.lastError().text() << "\nSQL:" << sql;
    return false;
  }
  return true;
}

QString stringListToJson(const QStringList& list) {
  QJsonArray arr;
  for (const auto& str : list) {
    arr.append(str);
  }
  return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QStringList jsonToStringList(const QString& json_str) {
  const auto doc = QJsonDocument::fromJson(json_str.toUtf8());
  if (!doc.isArray()) {
    return {};
  }
  QStringList list;
  for (const auto& val : doc.array()) {
    list.append(val.toString());
  }
  return list;
}

bool createSchemaV2(QSqlDatabase& database) {
  if (!execDdl(database, "PRAGMA journal_mode=WAL")) {
    return false;
  }
  execDdl(database, "PRAGMA foreign_keys=ON");

  static constexpr std::array kDdl{
      "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL)",

      "CREATE TABLE IF NOT EXISTS accounts ("
      "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  provider_type TEXT    NOT NULL,"
      "  account_name  TEXT    NOT NULL UNIQUE,"
      "  config_hash   TEXT    NOT NULL DEFAULT '',"
      "  created_at    TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))"
      ")",

      "CREATE TABLE IF NOT EXISTS events ("
      "  uid           TEXT    NOT NULL,"
      "  provider_type TEXT    NOT NULL,"
      "  account_name  TEXT    NOT NULL,"
      "  title         TEXT    NOT NULL,"
      "  dtstart       TEXT    NOT NULL,"
      "  dtend         TEXT,"
      "  is_all_day    INTEGER NOT NULL DEFAULT 0,"
      "  description   TEXT    NOT NULL DEFAULT '',"
      "  location      TEXT    NOT NULL DEFAULT '',"
      "  dtstamp       TEXT,"
      "  rrule         TEXT,"
      "  duration      TEXT,"
      "  access_class  TEXT,"
      "  created       TEXT,"
      "  last_modified TEXT,"
      "  organizer     TEXT,"
      "  attendees     TEXT    NOT NULL DEFAULT '[]',"
      "  categories    TEXT    NOT NULL DEFAULT '[]',"
      "  status        TEXT,"
      "  transparency  TEXT,"
      "  url           TEXT,"
      "  geo           TEXT,"
      "  sequence      INTEGER NOT NULL DEFAULT 0,"
      "  recurrence_id TEXT,"
      "  exdates       TEXT    NOT NULL DEFAULT '[]',"
      "  rdates        TEXT    NOT NULL DEFAULT '[]',"
      "  attachments   TEXT    NOT NULL DEFAULT '[]',"
      "  comments      TEXT    NOT NULL DEFAULT '[]',"
      "  contacts      TEXT    NOT NULL DEFAULT '[]',"
      "  related_to    TEXT    NOT NULL DEFAULT '[]',"
      "  resources     TEXT    NOT NULL DEFAULT '[]',"
      "  alarms        TEXT    NOT NULL DEFAULT '[]',"
      "  cached_at     TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),"
      "  PRIMARY KEY (uid, provider_type, account_name)"
      ")",

      "CREATE INDEX IF NOT EXISTS idx_events_dtstart ON events (dtstart, dtend)",
      "CREATE INDEX IF NOT EXISTS idx_events_account ON events (provider_type, account_name)",

      "CREATE TABLE IF NOT EXISTS sync_state ("
      "  provider_type  TEXT NOT NULL,"
      "  account_name   TEXT NOT NULL,"
      "  last_sync_time TEXT,"
      "  error_message  TEXT NOT NULL DEFAULT '',"
      "  next_sync_time TEXT,"
      "  PRIMARY KEY (provider_type, account_name)"
      ")",

      "CREATE INDEX IF NOT EXISTS idx_sync_state_account ON sync_state (provider_type, account_name)",
  };

  for (const auto* sql : kDdl) {
    if (!execDdl(database, sql)) {
      return false;
    }
  }
  return true;
}

int readSchemaVersion(QSqlDatabase& database) {
  QSqlQuery query(database);
  if (!query.exec(QStringLiteral("SELECT version FROM schema_version LIMIT 1")) || !query.next()) {
    return 0;
  }
  return query.value(0).toInt();
}

void writeSchemaVersion(QSqlDatabase& database, int version) {
  QSqlQuery del(database);
  del.prepare(QStringLiteral("DELETE FROM schema_version"));
  del.exec();
  QSqlQuery ins(database);
  ins.prepare(QStringLiteral("INSERT INTO schema_version (version) VALUES (:ver)"));
  ins.bindValue(QStringLiteral(":ver"), version);
  ins.exec();
}

bool migrateToV2(QSqlDatabase& database) {
  static constexpr std::array kDrops{
      "DROP TABLE IF EXISTS events",
      "DROP TABLE IF EXISTS sync_state",
      "DROP TABLE IF EXISTS accounts",
  };
  for (const auto* sql : kDrops) {
    execDdl(database, sql);
  }
  return true;
}

bool initOrMigrate(QSqlDatabase& database) {  // NOLINT(readability-function-cognitive-complexity)
  // Ensure schema_version table exists before reading.
  execDdl(database, "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL)");

  const int version = readSchemaVersion(database);

  if (version < 5) {
    if (version == 1) {
      qCInfo(lcCalendarCache) << "Migrating calendar cache from schema v1 to v5 (dropping OAuth-era tables)";
      if (!migrateToV2(database)) {
        qCWarning(lcCalendarCache) << "v1→v5 migration failed; continuing with empty cache";
      }
    }
    if (!createSchemaV2(database)) {
      return false;
    }
    if (version == 2) {
      qCInfo(lcCalendarCache)
          << "Migrating calendar cache from schema v2 to v5 (adding location, dtstamp, and remaining fields)";
      execDdl(database, "ALTER TABLE events ADD COLUMN location TEXT NOT NULL DEFAULT ''");
      execDdl(database, "ALTER TABLE events ADD COLUMN dtstamp TEXT");
    } else if (version == 3) {
      qCInfo(lcCalendarCache) << "Migrating calendar cache from schema v3 to v5 (adding dtstamp and remaining fields)";
      execDdl(database, "ALTER TABLE events ADD COLUMN dtstamp TEXT");
    }

    if (version >= 2 && version <= 4) {
      qCInfo(lcCalendarCache) << "Migrating calendar cache to schema v5 (adding all remaining standard VEVENT fields)";
      static constexpr std::array kNewColumns{
          "ALTER TABLE events ADD COLUMN rrule TEXT",
          "ALTER TABLE events ADD COLUMN duration TEXT",
          "ALTER TABLE events ADD COLUMN access_class TEXT",
          "ALTER TABLE events ADD COLUMN created TEXT",
          "ALTER TABLE events ADD COLUMN last_modified TEXT",
          "ALTER TABLE events ADD COLUMN organizer TEXT",
          "ALTER TABLE events ADD COLUMN attendees TEXT NOT NULL DEFAULT '[]'",
          "ALTER TABLE events ADD COLUMN categories TEXT NOT NULL DEFAULT '[]'",
          "ALTER TABLE events ADD COLUMN status TEXT",
          "ALTER TABLE events ADD COLUMN transparency TEXT",
          "ALTER TABLE events ADD COLUMN url TEXT",
          "ALTER TABLE events ADD COLUMN geo TEXT",
          "ALTER TABLE events ADD COLUMN sequence INTEGER NOT NULL DEFAULT 0",
          "ALTER TABLE events ADD COLUMN recurrence_id TEXT",
          "ALTER TABLE events ADD COLUMN exdates TEXT NOT NULL DEFAULT '[]'",
          "ALTER TABLE events ADD COLUMN rdates TEXT NOT NULL DEFAULT '[]'",
          "ALTER TABLE events ADD COLUMN attachments TEXT NOT NULL DEFAULT '[]'",
          "ALTER TABLE events ADD COLUMN comments TEXT NOT NULL DEFAULT '[]'",
          "ALTER TABLE events ADD COLUMN contacts TEXT NOT NULL DEFAULT '[]'",
          "ALTER TABLE events ADD COLUMN related_to TEXT NOT NULL DEFAULT '[]'",
          "ALTER TABLE events ADD COLUMN resources TEXT NOT NULL DEFAULT '[]'",
          "ALTER TABLE events ADD COLUMN alarms TEXT NOT NULL DEFAULT '[]'",
      };
      for (const auto* sql : kNewColumns) {
        if (!execDdl(database, sql)) {
          qCWarning(lcCalendarCache) << "v4→v5 migration failed on:" << sql;
        }
      }
    }
    writeSchemaVersion(database, 5);
    execDdl(database, "PRAGMA journal_mode=WAL");
    return true;
  }

  // Already at v5 — just ensure WAL is enabled.
  execDdl(database, "PRAGMA journal_mode=WAL");
  return true;
}

}  // namespace

CalendarCache::~CalendarCache() { close(); }

bool CalendarCache::open(const QString& db_path) {
  const QDir parent_dir = QFileInfo(db_path).absoluteDir();
  if (!parent_dir.exists() && !QDir().mkpath(parent_dir.absolutePath())) {
    qCWarning(lcCalendarCache) << "Failed to create cache directory:" << parent_dir.absolutePath();
    return false;
  }

  connection_name_ = QStringLiteral("holonight_calendar_cache_") + QUuid::createUuid().toString(QUuid::WithoutBraces);

  bool init_ok = false;
  {
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_name_);
    database.setDatabaseName(db_path);
    if (!database.open()) {
      qCWarning(lcCalendarCache) << "Failed to open calendar cache:" << database.lastError().text();
    } else if (!initOrMigrate(database)) {
      qCWarning(lcCalendarCache) << "Schema init/migration failed";
      database.close();
    } else {
      init_ok = true;
    }
  }
  // `database` (and any other QSqlDatabase handle to this connection) must go out of scope
  // before removeDatabase() — Qt logs "connection still in use" if a live handle remains.
  if (!init_ok) {
    QSqlDatabase::removeDatabase(connection_name_);
    return false;
  }

  open_ = true;
  return true;
}

void CalendarCache::close() {
  if (!open_) {
    return;
  }
  {
    QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
    if (database.isOpen()) {
      database.close();
    }
  }
  QSqlDatabase::removeDatabase(connection_name_);
  open_ = false;
}

QList<CalendarEvent> CalendarCache::queryRange(const QDateTime& from, const QDateTime& until) const {
  if (!open_) {
    return {};
  }
  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  QSqlQuery query(database);
  query.prepare(QStringLiteral(
      "SELECT uid, provider_type, account_name, title, dtstart, dtend, is_all_day, description, location, dtstamp, "
      "rrule, duration, access_class, created, last_modified, organizer, attendees, categories, status, "
      "transparency, url, geo, sequence, recurrence_id, exdates, rdates, attachments, comments, contacts, "
      "related_to, resources, alarms"
      " FROM events WHERE dtstart >= :from AND dtstart < :until"
      " ORDER BY dtstart ASC"));
  query.bindValue(QStringLiteral(":from"), from.toUTC().toString(Qt::ISODate));
  query.bindValue(QStringLiteral(":until"), until.toUTC().toString(Qt::ISODate));
  if (!query.exec()) {
    qCWarning(lcCalendarCache) << "queryRange failed:" << query.lastError().text();
    return {};
  }
  QList<CalendarEvent> result;
  while (query.next()) {
    CalendarEvent evt;
    evt.uid = query.value(0).toString();
    evt.provider_type = query.value(1).toString();
    evt.account_name = query.value(2).toString();
    evt.title = query.value(3).toString();
    evt.start_time = QDateTime::fromString(query.value(4).toString(), Qt::ISODate);
    const QString dtend_str = query.value(5).toString();
    if (!dtend_str.isEmpty()) {
      evt.end_time = QDateTime::fromString(dtend_str, Qt::ISODate);
    }
    evt.is_all_day = query.value(6).toInt() != 0;
    evt.description = query.value(7).toString();
    evt.location = query.value(8).toString();
    const QString dtstamp_str = query.value(9).toString();
    if (!dtstamp_str.isEmpty()) {
      evt.dtstamp = QDateTime::fromString(dtstamp_str, Qt::ISODate);
    }
    evt.rrule = query.value(10).toString();
    evt.duration = query.value(11).toString();
    evt.access_class = query.value(12).toString();
    const QString created_str = query.value(13).toString();
    if (!created_str.isEmpty()) {
      evt.created = QDateTime::fromString(created_str, Qt::ISODate);
    }
    const QString last_modified_str = query.value(14).toString();
    if (!last_modified_str.isEmpty()) {
      evt.last_modified = QDateTime::fromString(last_modified_str, Qt::ISODate);
    }
    evt.organizer = query.value(15).toString();
    evt.attendees = jsonToStringList(query.value(16).toString());
    evt.categories = jsonToStringList(query.value(17).toString());
    evt.status = query.value(18).toString();
    evt.transparency = query.value(19).toString();
    evt.url = query.value(20).toString();
    evt.geo = query.value(21).toString();
    evt.sequence = query.value(22).toInt();
    evt.recurrence_id = query.value(23).toString();
    evt.exdates = jsonToStringList(query.value(24).toString());
    evt.rdates = jsonToStringList(query.value(25).toString());
    evt.attachments = jsonToStringList(query.value(26).toString());
    evt.comments = jsonToStringList(query.value(27).toString());
    evt.contacts = jsonToStringList(query.value(28).toString());
    evt.related_to = jsonToStringList(query.value(29).toString());
    evt.resources = jsonToStringList(query.value(30).toString());
    evt.alarms = jsonToStringList(query.value(31).toString());
    result.append(evt);
  }
  return result;
}

bool CalendarCache::upsertEvents(  // NOLINT(readability-function-cognitive-complexity)
    const QList<CalendarEvent>& events) {
  if (!open_) {
    return false;
  }
  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  if (!database.transaction()) {
    qCWarning(lcCalendarCache) << "upsertEvents: failed to begin transaction";
    return false;
  }

  QSqlQuery upsert(database);
  upsert.prepare(QStringLiteral(
      "INSERT INTO events (uid, provider_type, account_name, title, dtstart, dtend, is_all_day, description, location, "
      "dtstamp, "
      "rrule, duration, access_class, created, last_modified, organizer, attendees, categories, status, "
      "transparency, url, geo, sequence, recurrence_id, exdates, rdates, attachments, comments, contacts, "
      "related_to, resources, alarms)"
      " VALUES (:uid, :ptype, :acct, :title, :start, :end, :allday, :desc, :loc, :dtstamp, "
      ":rrule, :dur, :aclass, :created, :lmod, :org, :att, :cat, :status, :trans, :url, :geo, :seq, :recid, :exd, :rd, "
      ":attch, :comm, :cont, :rel, :res, :alarm)"
      " ON CONFLICT(uid, provider_type, account_name) DO UPDATE SET"
      "   title = excluded.title, dtstart = excluded.dtstart, dtend = excluded.dtend,"
      "   is_all_day = excluded.is_all_day, description = excluded.description,"
      "   location = excluded.location, dtstamp = excluded.dtstamp,"
      "   rrule = excluded.rrule, duration = excluded.duration, access_class = excluded.access_class,"
      "   created = excluded.created, last_modified = excluded.last_modified, organizer = excluded.organizer,"
      "   attendees = excluded.attendees, categories = excluded.categories, status = excluded.status,"
      "   transparency = excluded.transparency, url = excluded.url, geo = excluded.geo,"
      "   sequence = excluded.sequence, recurrence_id = excluded.recurrence_id, exdates = excluded.exdates,"
      "   rdates = excluded.rdates, attachments = excluded.attachments, comments = excluded.comments,"
      "   contacts = excluded.contacts, related_to = excluded.related_to, resources = excluded.resources,"
      "   alarms = excluded.alarms,"
      "   cached_at = strftime('%Y-%m-%dT%H:%M:%SZ','now')"));

  for (const auto& evt : events) {
    upsert.bindValue(QStringLiteral(":uid"), evt.uid);
    upsert.bindValue(QStringLiteral(":ptype"), evt.provider_type);
    upsert.bindValue(QStringLiteral(":acct"), evt.account_name);
    upsert.bindValue(QStringLiteral(":title"), evt.title);
    upsert.bindValue(QStringLiteral(":start"), evt.start_time.toUTC().toString(Qt::ISODate));
    upsert.bindValue(QStringLiteral(":end"),
                     evt.end_time.isValid() ? QVariant{evt.end_time.toUTC().toString(Qt::ISODate)} : QVariant{});
    upsert.bindValue(QStringLiteral(":allday"), evt.is_all_day ? 1 : 0);
    upsert.bindValue(QStringLiteral(":desc"), evt.description.isNull() ? QStringLiteral("") : evt.description);
    upsert.bindValue(QStringLiteral(":loc"), evt.location.isNull() ? QStringLiteral("") : evt.location);
    upsert.bindValue(QStringLiteral(":dtstamp"),
                     evt.dtstamp.isValid() ? QVariant{evt.dtstamp.toUTC().toString(Qt::ISODate)} : QVariant{});
    upsert.bindValue(QStringLiteral(":rrule"), evt.rrule.isNull() ? QVariant{} : evt.rrule);
    upsert.bindValue(QStringLiteral(":dur"), evt.duration.isNull() ? QVariant{} : evt.duration);
    upsert.bindValue(QStringLiteral(":aclass"), evt.access_class.isNull() ? QVariant{} : evt.access_class);
    upsert.bindValue(QStringLiteral(":created"),
                     evt.created.isValid() ? QVariant{evt.created.toUTC().toString(Qt::ISODate)} : QVariant{});
    upsert.bindValue(QStringLiteral(":lmod"), evt.last_modified.isValid()
                                                  ? QVariant{evt.last_modified.toUTC().toString(Qt::ISODate)}
                                                  : QVariant{});
    upsert.bindValue(QStringLiteral(":org"), evt.organizer.isNull() ? QVariant{} : evt.organizer);
    upsert.bindValue(QStringLiteral(":att"), stringListToJson(evt.attendees));
    upsert.bindValue(QStringLiteral(":cat"), stringListToJson(evt.categories));
    upsert.bindValue(QStringLiteral(":status"), evt.status.isNull() ? QVariant{} : evt.status);
    upsert.bindValue(QStringLiteral(":trans"), evt.transparency.isNull() ? QVariant{} : evt.transparency);
    upsert.bindValue(QStringLiteral(":url"), evt.url.isNull() ? QVariant{} : evt.url);
    upsert.bindValue(QStringLiteral(":geo"), evt.geo.isNull() ? QVariant{} : evt.geo);
    upsert.bindValue(QStringLiteral(":seq"), evt.sequence);
    upsert.bindValue(QStringLiteral(":recid"), evt.recurrence_id.isNull() ? QVariant{} : evt.recurrence_id);
    upsert.bindValue(QStringLiteral(":exd"), stringListToJson(evt.exdates));
    upsert.bindValue(QStringLiteral(":rd"), stringListToJson(evt.rdates));
    upsert.bindValue(QStringLiteral(":attch"), stringListToJson(evt.attachments));
    upsert.bindValue(QStringLiteral(":comm"), stringListToJson(evt.comments));
    upsert.bindValue(QStringLiteral(":cont"), stringListToJson(evt.contacts));
    upsert.bindValue(QStringLiteral(":rel"), stringListToJson(evt.related_to));
    upsert.bindValue(QStringLiteral(":res"), stringListToJson(evt.resources));
    upsert.bindValue(QStringLiteral(":alarm"), stringListToJson(evt.alarms));

    if (!execQuery(upsert, "upsertEvents")) {
      database.rollback();
      return false;
    }
  }

  if (!database.commit()) {
    qCWarning(lcCalendarCache) << "upsertEvents commit failed:" << database.lastError().text();
    database.rollback();
    return false;
  }
  return true;
}

bool CalendarCache::upsertAccount(const QString& provider_type, const QString& account_name,
                                  const QString& config_hash) {
  if (!open_) {
    return false;
  }
  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);

  // Check existing hash to detect config changes.
  QSqlQuery check(database);
  check.prepare(QStringLiteral("SELECT config_hash FROM accounts WHERE account_name = :acct"));
  check.bindValue(QStringLiteral(":acct"), account_name);
  bool hash_changed = false;
  if (check.exec() && check.next()) {
    hash_changed = (check.value(0).toString() != config_hash);
  }

  QSqlQuery upsert(database);
  upsert.prepare(
      QStringLiteral("INSERT INTO accounts (provider_type, account_name, config_hash)"
                     " VALUES (:ptype, :acct, :hash)"
                     " ON CONFLICT(account_name) DO UPDATE SET"
                     "   provider_type = excluded.provider_type, config_hash = excluded.config_hash"));
  upsert.bindValue(QStringLiteral(":ptype"), provider_type);
  upsert.bindValue(QStringLiteral(":acct"), account_name);
  upsert.bindValue(QStringLiteral(":hash"), config_hash);
  if (!execQuery(upsert, "upsertAccount")) {
    return false;
  }
  return hash_changed;
}

bool CalendarCache::reconcileAccountEvents(const QString& provider_type, const QString& account_name,
                                           const QList<CalendarEvent>& fresh_events) {
  if (!open_) {
    return false;
  }
  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);

  QSqlQuery cached(database);
  cached.prepare(QStringLiteral("SELECT uid FROM events WHERE provider_type = :ptype AND account_name = :acct"));
  cached.bindValue(QStringLiteral(":ptype"), provider_type);
  cached.bindValue(QStringLiteral(":acct"), account_name);
  if (!execQuery(cached, "reconcileAccountEvents:select")) {
    return false;
  }

  QSet<QString> fresh_uids;
  fresh_uids.reserve(fresh_events.size());
  for (const CalendarEvent& evt : fresh_events) {
    fresh_uids.insert(evt.uid);
  }

  QStringList stale_uids;
  while (cached.next()) {
    const QString uid = cached.value(0).toString();
    if (!fresh_uids.contains(uid)) {
      stale_uids.append(uid);
    }
  }

  if (stale_uids.isEmpty()) {
    return true;
  }

  if (!database.transaction()) {
    qCWarning(lcCalendarCache) << "reconcileAccountEvents: failed to begin transaction";
    return false;
  }

  QSqlQuery del(database);
  del.prepare(
      QStringLiteral("DELETE FROM events WHERE provider_type = :ptype AND account_name = :acct AND uid = :uid"));
  bool all_ok = true;
  for (const QString& uid : stale_uids) {
    del.bindValue(QStringLiteral(":ptype"), provider_type);
    del.bindValue(QStringLiteral(":acct"), account_name);
    del.bindValue(QStringLiteral(":uid"), uid);
    all_ok = execQuery(del, "reconcileAccountEvents:delete") && all_ok;
  }

  if (!all_ok) {
    database.rollback();
    return false;
  }
  if (!database.commit()) {
    qCWarning(lcCalendarCache) << "reconcileAccountEvents commit failed:" << database.lastError().text();
    database.rollback();
    return false;
  }
  return true;
}

bool CalendarCache::clearAccountEvents(const QString& provider_type, const QString& account_name) {
  if (!open_) {
    return false;
  }
  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  QSqlQuery query(database);
  query.prepare(QStringLiteral("DELETE FROM events WHERE provider_type = :ptype AND account_name = :acct"));
  query.bindValue(QStringLiteral(":ptype"), provider_type);
  query.bindValue(QStringLiteral(":acct"), account_name);
  return execQuery(query, "clearAccountEvents");
}

bool CalendarCache::pruneExpired() {
  if (!open_) {
    return false;
  }
  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  const QDateTime cutoff_past = QDateTime::currentDateTimeUtc().addDays(-kRetainPastDays);
  const QDateTime cutoff_future = QDateTime::currentDateTimeUtc().addDays(kRetainFutureDays);
  QSqlQuery query(database);
  query.prepare(QStringLiteral("DELETE FROM events WHERE dtstart < :past OR dtstart > :future"));
  query.bindValue(QStringLiteral(":past"), cutoff_past.toString(Qt::ISODate));
  query.bindValue(QStringLiteral(":future"), cutoff_future.toString(Qt::ISODate));
  return execQuery(query, "pruneExpired");
}

bool CalendarCache::storeSyncState(const QString& provider_type, const QString& account_name,
                                   const QDateTime& last_sync_time, const QString& error_message,
                                   const QDateTime& next_sync_time) {
  if (!open_) {
    return false;
  }
  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  QSqlQuery query(database);
  query.prepare(QStringLiteral(
      "INSERT INTO sync_state (provider_type, account_name, last_sync_time, error_message, next_sync_time)"
      " VALUES (:ptype, :acct, :last, :err, :next)"
      " ON CONFLICT(provider_type, account_name) DO UPDATE SET"
      "   last_sync_time = excluded.last_sync_time,"
      "   error_message  = excluded.error_message,"
      "   next_sync_time = excluded.next_sync_time"));
  query.bindValue(QStringLiteral(":ptype"), provider_type);
  query.bindValue(QStringLiteral(":acct"), account_name);
  query.bindValue(QStringLiteral(":last"),
                  last_sync_time.isValid() ? QVariant{last_sync_time.toUTC().toString(Qt::ISODate)} : QVariant{});
  query.bindValue(QStringLiteral(":err"), error_message.isNull() ? QStringLiteral("") : error_message);
  query.bindValue(QStringLiteral(":next"),
                  next_sync_time.isValid() ? QVariant{next_sync_time.toUTC().toString(Qt::ISODate)} : QVariant{});
  return execQuery(query, "storeSyncState");
}

CalendarCache::SyncState CalendarCache::loadSyncState(const QString& provider_type, const QString& account_name) const {
  SyncState state;
  if (!open_) {
    return state;
  }
  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  QSqlQuery query(database);
  query.prepare(
      QStringLiteral("SELECT last_sync_time, error_message, next_sync_time"
                     " FROM sync_state WHERE provider_type = :ptype AND account_name = :acct"));
  query.bindValue(QStringLiteral(":ptype"), provider_type);
  query.bindValue(QStringLiteral(":acct"), account_name);
  if (!query.exec() || !query.next()) {
    return state;
  }
  const QString last_str = query.value(0).toString();
  if (!last_str.isEmpty()) {
    state.last_sync_time = QDateTime::fromString(last_str, Qt::ISODate);
  }
  state.error_message = query.value(1).toString();
  const QString next_str = query.value(2).toString();
  if (!next_str.isEmpty()) {
    state.next_sync_time = QDateTime::fromString(next_str, Qt::ISODate);
  }
  return state;
}

bool CalendarCache::removeStaleAccounts(const QStringList& active_provider_account_keys) {
  if (!open_) {
    return false;
  }
  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);

  QSqlQuery sel(database);
  if (!sel.exec(QStringLiteral("SELECT provider_type, account_name FROM accounts"))) {
    return false;
  }
  // Build list of (provider_type, account_name) pairs for stale accounts.
  QList<std::pair<QString, QString>> stale;
  while (sel.next()) {
    const QString ptype = sel.value(0).toString();
    const QString acct = sel.value(1).toString();
    if (!active_provider_account_keys.contains(ptype + QLatin1Char(':') + acct)) {
      stale.emplace_back(ptype, acct);
    }
  }

  bool all_ok = true;
  for (const auto& [ptype, name] : stale) {
    QSqlQuery del_events(database);
    del_events.prepare(QStringLiteral("DELETE FROM events WHERE provider_type = :ptype AND account_name = :acct"));
    del_events.bindValue(QStringLiteral(":ptype"), ptype);
    del_events.bindValue(QStringLiteral(":acct"), name);
    all_ok = execQuery(del_events, "removeStaleAccounts:events") && all_ok;

    QSqlQuery del_sync(database);
    del_sync.prepare(QStringLiteral("DELETE FROM sync_state WHERE provider_type = :ptype AND account_name = :acct"));
    del_sync.bindValue(QStringLiteral(":ptype"), ptype);
    del_sync.bindValue(QStringLiteral(":acct"), name);
    all_ok = execQuery(del_sync, "removeStaleAccounts:sync_state") && all_ok;

    QSqlQuery del_acct(database);
    del_acct.prepare(QStringLiteral("DELETE FROM accounts WHERE account_name = :acct"));
    del_acct.bindValue(QStringLiteral(":acct"), name);
    all_ok = execQuery(del_acct, "removeStaleAccounts:accounts") && all_ok;
  }
  return all_ok;
}

QString CalendarCache::configHash(const QString& url, const QString& extra) {
  const QString input = url + extra;
  return QString::fromLatin1(QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256).toHex());
}
