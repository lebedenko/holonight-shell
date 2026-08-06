#include "DesktopEntryCache.h"

#include "DesktopEntrySerializer.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
Q_LOGGING_CATEGORY(lcCache, "holonight.launcher.cache")

// Bump this whenever the JSON blob schema changes in a way that requires re-parsing all .desktop
// files. Old caches with a lower version are wiped and rebuilt on next startup.
constexpr int kCurrentVersion = 2;  // v2: added startup_wm_class field

constexpr QLatin1StringView kSchema{
    "CREATE TABLE IF NOT EXISTS desktop_entries ("
    "  path       TEXT    PRIMARY KEY,"
    "  mtime      INTEGER NOT NULL,"
    "  size       INTEGER NOT NULL,"
    "  name       TEXT    NOT NULL,"
    "  categories TEXT    NOT NULL,"
    "  data       TEXT    NOT NULL"
    ")"};
constexpr QLatin1StringView kMetadataQuery{"SELECT mtime, size FROM desktop_entries WHERE path = :path"};
constexpr QLatin1StringView kUpsertQuery{
    "INSERT OR REPLACE INTO desktop_entries (path, mtime, size, name, categories, data) "
    "VALUES (:path, :mtime, :size, :name, :categories, :data)"};

const QSet<QString>& requiredColumns() {
  static const QSet<QString> kColumns{QStringLiteral("path"), QStringLiteral("mtime"),      QStringLiteral("size"),
                                      QStringLiteral("name"), QStringLiteral("categories"), QStringLiteral("data")};
  return kColumns;
}

bool hasRequiredColumns(const QSqlDatabase& database) {
  QSqlQuery query(database);
  if (!query.exec(QStringLiteral("PRAGMA table_info(desktop_entries)"))) {
    return false;
  }

  QSet<QString> columns;
  while (query.next()) {
    columns.insert(query.value(1).toString());
  }
  return columns.contains(requiredColumns());
}

QString initializeDatabase(const QSqlDatabase& database, QSqlQuery* metadata_query, QSqlQuery* upsert_query) {
  QSqlQuery query(database);
  int stored_version = 0;
  if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next()) {
    stored_version = query.value(0).toInt();
  }
  query.finish();

  const bool schema_outdated = stored_version < kCurrentVersion;
  const bool schema_incomplete = !schema_outdated && !hasRequiredColumns(database);
  if (schema_outdated) {
    qCInfo(lcCache) << "Launcher cache schema version" << stored_version << "< current" << kCurrentVersion
                    << "— rebuilding";
  } else if (schema_incomplete) {
    qCInfo(lcCache) << "Launcher cache schema is incomplete — rebuilding";
  }
  const bool rebuild_schema = schema_outdated || schema_incomplete;
  if (rebuild_schema) {
    query.exec(QStringLiteral("DROP TABLE IF EXISTS desktop_entries"));
  }
  if (!query.exec(QString::fromLatin1(kSchema))) {
    return query.lastError().text();
  }
  if (rebuild_schema && !query.exec(QStringLiteral("PRAGMA user_version = ") + QString::number(kCurrentVersion))) {
    return query.lastError().text();
  }

  *metadata_query = QSqlQuery(database);
  *upsert_query = QSqlQuery(database);
  if (!metadata_query->prepare(QString::fromLatin1(kMetadataQuery))) {
    return metadata_query->lastError().text();
  }
  if (!upsert_query->prepare(QString::fromLatin1(kUpsertQuery))) {
    return upsert_query->lastError().text();
  }
  return {};
}

QSqlDatabase openConnection(const QString& name, const QString& db_path) {
  QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
  database.setDatabaseName(db_path);
  return database;
}

void closeConnection(const QString& name) {
  {
    QSqlDatabase database = QSqlDatabase::database(name, false);
    if (database.isOpen()) {
      database.close();
    }
  }
  QSqlDatabase::removeDatabase(name);
}
}  // namespace

DesktopEntryCache::~DesktopEntryCache() { close(); }

bool DesktopEntryCache::open(const QString& db_path) {
  const QDir parent_dir = QFileInfo(db_path).absoluteDir();
  if (!parent_dir.exists() && !QDir().mkpath(parent_dir.absolutePath())) {
    qCWarning(lcCache) << "Failed to create cache directory:" << parent_dir.absolutePath();
    return false;
  }

  connection_name_ = QStringLiteral("holonight_launcher_") + QUuid::createUuid().toString(QUuid::WithoutBraces);

  QString error;
  {
    QSqlDatabase database = openConnection(connection_name_, db_path);
    if (!database.open()) {
      error = database.lastError().text();
    } else {
      error = initializeDatabase(database, &metadata_query_, &upsert_query_);
    }
  }

  if (!error.isEmpty()) {
    qCWarning(lcCache) << "Failed to open launcher cache DB:" << error;
    clearQueries();
    closeConnection(connection_name_);
    return false;
  }

  open_ = true;
  return true;
}

void DesktopEntryCache::close() {
  if (!open_) {
    return;
  }
  clearQueries();
  closeConnection(connection_name_);
  open_ = false;
}

void DesktopEntryCache::clearQueries() {
  metadata_query_ = QSqlQuery{};
  upsert_query_ = QSqlQuery{};
}

QVector<DesktopEntry> DesktopEntryCache::loadAll() const {
  if (!open_) {
    return {};
  }

  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  if (!database.isOpen()) {
    qCWarning(lcCache) << "loadAll: database not open";
    return {};
  }

  QSqlQuery query(database);
  if (!query.exec(QStringLiteral("SELECT data FROM desktop_entries"))) {
    qCWarning(lcCache) << "loadAll query failed:" << query.lastError().text();
    return {};
  }

  QVector<DesktopEntry> entries;
  while (query.next()) {
    const QByteArray raw = query.value(0).toString().toUtf8();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
      qCWarning(lcCache) << "loadAll: invalid JSON blob, skipping row";
      continue;
    }
    auto entry = DesktopEntrySerializer::fromJson(doc.object());
    if (!entry.has_value()) {
      qCWarning(lcCache) << "loadAll: failed to deserialize entry, skipping row";
      continue;
    }
    entries.append(std::move(*entry));
  }
  return entries;
}

std::optional<DesktopEntryCache::FileMeta> DesktopEntryCache::metadata(const QString& path) const {
  if (!open_) {
    return std::nullopt;
  }

  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  if (!database.isOpen()) {
    return std::nullopt;
  }

  metadata_query_.finish();
  metadata_query_.bindValue(QStringLiteral(":path"), path);
  if (!metadata_query_.exec() || !metadata_query_.next()) {
    return std::nullopt;
  }
  return FileMeta{.mtime = metadata_query_.value(0).toLongLong(), .size = metadata_query_.value(1).toLongLong()};
}

bool DesktopEntryCache::upsert(const DesktopEntry& entry, qint64 mtime, qint64 size) {
  if (!open_) {
    return false;
  }

  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  if (!database.isOpen()) {
    qCWarning(lcCache) << "upsert: database not open";
    return false;
  }

  const QJsonDocument doc(DesktopEntrySerializer::toJson(entry));
  const QString data = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
  QString categories = entry.categories;
  if (categories.isNull()) {
    categories = QStringLiteral("");
  }

  upsert_query_.finish();
  upsert_query_.bindValue(QStringLiteral(":path"), entry.desktop_file);
  upsert_query_.bindValue(QStringLiteral(":mtime"), mtime);
  upsert_query_.bindValue(QStringLiteral(":size"), size);
  upsert_query_.bindValue(QStringLiteral(":name"), entry.name);
  upsert_query_.bindValue(QStringLiteral(":categories"), categories);
  upsert_query_.bindValue(QStringLiteral(":data"), data);

  if (!upsert_query_.exec()) {
    qCWarning(lcCache) << "upsert failed for" << entry.desktop_file << ":" << upsert_query_.lastError().text();
    return false;
  }
  return true;
}

bool DesktopEntryCache::remove(const QString& path) {
  if (!open_) {
    return false;
  }

  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  if (!database.isOpen()) {
    qCWarning(lcCache) << "remove: database not open";
    return false;
  }

  QSqlQuery query(database);
  query.prepare(QStringLiteral("DELETE FROM desktop_entries WHERE path = :path"));
  query.bindValue(QStringLiteral(":path"), path);

  if (!query.exec()) {
    qCWarning(lcCache) << "remove failed for" << path << ":" << query.lastError().text();
    return false;
  }
  return true;
}

bool DesktopEntryCache::beginTransaction() {
  if (!open_) {
    return false;
  }

  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  if (!database.isOpen()) {
    qCWarning(lcCache) << "beginTransaction: database not open";
    return false;
  }

  if (!database.transaction()) {
    qCWarning(lcCache) << "beginTransaction failed:" << database.lastError().text();
    return false;
  }
  return true;
}

bool DesktopEntryCache::commitTransaction() {
  if (!open_) {
    return false;
  }

  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  if (!database.isOpen()) {
    qCWarning(lcCache) << "commitTransaction: database not open";
    return false;
  }

  if (!database.commit()) {
    qCWarning(lcCache) << "commitTransaction failed:" << database.lastError().text();
    return false;
  }
  return true;
}

void DesktopEntryCache::rollbackTransaction() {
  if (!open_) {
    return;
  }

  QSqlDatabase database = QSqlDatabase::database(connection_name_, false);
  if (database.isOpen() && !database.rollback()) {
    qCWarning(lcCache) << "rollbackTransaction failed:" << database.lastError().text();
  }
}
