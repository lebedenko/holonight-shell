#pragma once

#include "NotificationFilter.h"

#include <QFutureWatcher>
#include <QList>
#include <QObject>
#include <QString>

// Outcome of one async rule-file write. Same {bool ok; QString reason;} shape as Phase 1's
// SessionCommandResult, but a distinct type — a disk-write diagnostic has no reason to share a
// type with session-command dispatch beyond an incidentally identical shape.
struct RulePersistOutcome {
  bool ok{true};
  QString reason;
};

// Preference store for per-application notification rules. Rules are small user preferences and
// live under XDG_CONFIG_HOME. Writes are async (QtConcurrent) and serialized via a dirty-flag
// pattern so writes never overlap and only the latest snapshot is ever written — same pattern as
// NotificationStore (notification history), ported here rather than shared via a base class.
class NotificationRuleStore : public QObject {
  Q_OBJECT

 public:
  explicit NotificationRuleStore(QObject* parent = nullptr);
  explicit NotificationRuleStore(QString file_path, QObject* parent = nullptr);
  ~NotificationRuleStore() override = default;

  NotificationRuleStore(const NotificationRuleStore&) = delete;
  NotificationRuleStore& operator=(const NotificationRuleStore&) = delete;
  NotificationRuleStore(NotificationRuleStore&&) = delete;
  NotificationRuleStore& operator=(NotificationRuleStore&&) = delete;

  [[nodiscard]] QList<AppNotificationRule> load() const;

  // Schedules an async write. `action` identifies which caller triggered the write, threaded
  // through to persistFailed() on failure — it is a diagnostic label only, never persisted to disk.
  void persist(const QList<AppNotificationRule>& rules, const QString& action);

  [[nodiscard]] QString filePath() const { return file_path_; }

 Q_SIGNALS:
  void writeCompleted();
  void persistFailed(const QString& action, const QString& reason);

 private Q_SLOTS:
  void onWriteFinished();

 private:
  void launchWrite(QList<AppNotificationRule> rules, QString action);
  void ensureDirectoryExists() const;

  QString file_path_;
  bool write_in_flight_{false};
  bool write_dirty_{false};
  QList<AppNotificationRule> pending_rules_;
  QString pending_action_;
  QString in_flight_action_;
  QFutureWatcher<RulePersistOutcome>* watcher_{nullptr};
};
