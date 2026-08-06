#pragma once

#include "ConfigService.h"
#include "NotificationTypes.h"

#include <QFutureWatcher>
#include <QList>
#include <QObject>
#include <QString>

// Owns disk persistence for notification history. All writes are async
// (QtConcurrent) and serialized via a dirty-flag pattern so writes never
// overlap and the full list is always written (no delta tracking needed).
class NotificationStore : public QObject {
  Q_OBJECT

 public:
  explicit NotificationStore(const NotificationHistoryConfig& config, QObject* parent = nullptr);
  ~NotificationStore() override = default;

  NotificationStore(const NotificationStore&) = delete;
  NotificationStore& operator=(const NotificationStore&) = delete;
  NotificationStore(NotificationStore&&) = delete;
  NotificationStore& operator=(NotificationStore&&) = delete;

  // Synchronous load at startup — reads history.json and returns parsed entries.
  // Invalid/malformed entries are logged and skipped; never throws.
  [[nodiscard]] QList<NotificationHistoryItem> load();

  // Schedules an async write of the current history snapshot. If a write is
  // already in flight the snapshot is buffered and written when the current one
  // completes (dirty-flag serialization).
  void persist(QList<NotificationHistoryItem> snapshot);
  void updateConfig(const NotificationHistoryConfig& config);

  [[nodiscard]] QString historyFilePath() const { return file_path_; }

 Q_SIGNALS:
  void writeCompleted();

 private Q_SLOTS:
  void onWriteFinished();

 private:
  void launchWrite(QList<NotificationHistoryItem> snapshot);
  void ensureDirectoryExists();

  NotificationHistoryConfig config_;
  QString file_path_;
  bool write_in_flight_{false};
  bool write_dirty_{false};
  QList<NotificationHistoryItem> pending_snapshot_;
  QFutureWatcher<void>* watcher_{nullptr};
};
