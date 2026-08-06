#pragma once

#include "CalendarTypes.h"

#include <QDateTime>
#include <QList>
#include <QString>

// SQLite-backed local cache for calendar events.
// Schema version 2 — WAL mode, file at $XDG_CACHE_HOME/holonight-shell/calendar/calendar.sqlite.
// Retains events from (now - 30d) to (now + 180d); pruned after each sync.
// Use a separate CalendarCache instance per thread (UUID-named QSqlDatabase connection).
class CalendarCache {
 public:
  CalendarCache() = default;
  ~CalendarCache();

  CalendarCache(const CalendarCache&) = delete;
  CalendarCache& operator=(const CalendarCache&) = delete;
  CalendarCache(CalendarCache&&) = delete;
  CalendarCache& operator=(CalendarCache&&) = delete;

  // Opens the database, migrates schema if needed, returns false on failure.
  [[nodiscard]] bool open(const QString& db_path);
  void close();
  [[nodiscard]] bool isOpen() const { return open_; }

  // Returns events with start_time >= from and start_time < until, ordered by start_time.
  [[nodiscard]] QList<CalendarEvent> queryRange(const QDateTime& from, const QDateTime& until) const;

  // Inserts or replaces events. Primary key is (uid, provider_type, account_name).
  bool upsertEvents(const QList<CalendarEvent>& events);

  // Upserts the account row and returns whether the config_hash changed (triggers cache invalidation).
  bool upsertAccount(const QString& provider_type, const QString& account_name, const QString& config_hash);

  // Deletes all events for a given account (called when config changes).
  bool clearAccountEvents(const QString& provider_type, const QString& account_name);

  // Deletes cached events for (provider_type, account_name) whose uid is absent from
  // fresh_events — i.e. events the upstream server no longer reports (cancelled/deleted meetings).
  // Called after every successful sync, in addition to pruneExpired()'s unrelated date-window prune.
  bool reconcileAccountEvents(const QString& provider_type, const QString& account_name,
                              const QList<CalendarEvent>& fresh_events);

  // Removes events outside the -30d/+180d retention window.
  bool pruneExpired();

  // Stores per-account sync bookkeeping (last_sync_time, error_message, next_sync_time).
  // Pass empty error_message on success; pass "" for next_sync_time to clear it.
  bool storeSyncState(const QString& provider_type, const QString& account_name, const QDateTime& last_sync_time,
                      const QString& error_message, const QDateTime& next_sync_time);

  // Returns stored sync state for the given account. All QDateTime fields are invalid if not yet synced.
  struct SyncState {
    QDateTime last_sync_time;
    QString error_message;
    QDateTime next_sync_time;
  };
  [[nodiscard]] SyncState loadSyncState(const QString& provider_type, const QString& account_name) const;

  // Removes the sync_state and all events for accounts no longer in config.
  bool removeStaleAccounts(const QStringList& active_provider_account_keys);

  // SHA-256 based config hash for change detection. For CalDAV: url+username. For ICS: url.
  [[nodiscard]] static QString configHash(const QString& url, const QString& extra = {});

  // Retention/fetch window shared with CalendarSyncManager, which fetches events over the same
  // range that pruneExpired() retains, so a sync never fetches data it would immediately prune.
  static constexpr int kRetainPastDays{30};
  static constexpr int kRetainFutureDays{180};

 private:
  QString connection_name_;
  bool open_{false};

  static constexpr int kSchemaVersion{3};
};
