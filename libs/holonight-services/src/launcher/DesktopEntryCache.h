#pragma once

#include "DesktopEntryScanner.h"

#include <QSqlQuery>
#include <QString>
#include <QVector>

#include <optional>

class DesktopEntryCache {
 public:
  struct FileMeta {
    qint64 mtime{};
    qint64 size{};
  };

  DesktopEntryCache() = default;
  ~DesktopEntryCache();

  DesktopEntryCache(const DesktopEntryCache&) = delete;
  DesktopEntryCache& operator=(const DesktopEntryCache&) = delete;
  DesktopEntryCache(DesktopEntryCache&&) = delete;
  DesktopEntryCache& operator=(DesktopEntryCache&&) = delete;

  // Opens (or creates) the DB at db_path. Creates parent directory if missing.
  // Must be called on the thread that will use this instance.
  // Returns false on failure; logs qCWarning.
  [[nodiscard]] bool open(const QString& db_path);

  void close();

  // Reads all rows and deserializes JSON blobs. Returns empty on DB error.
  [[nodiscard]] QVector<DesktopEntry> loadAll() const;

  // Returns cached mtime+size for path, or nullopt if not present.
  [[nodiscard]] std::optional<FileMeta> metadata(const QString& path) const;

  // INSERT OR REPLACE row for entry.
  bool upsert(const DesktopEntry& entry, qint64 mtime, qint64 size);

  // DELETE row by path.
  bool remove(const QString& path);

  bool beginTransaction();
  bool commitTransaction();
  void rollbackTransaction();

  [[nodiscard]] bool isOpen() const { return open_; }

 private:
  void clearQueries();

  QString connection_name_;
  mutable QSqlQuery metadata_query_;
  QSqlQuery upsert_query_;
  bool open_{false};
};
