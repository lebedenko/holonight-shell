#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <functional>

struct GuardedProcessResult {
  bool timed_out{false};
  QProcess::ProcessError error{QProcess::UnknownError};
  bool had_error{false};
  int exit_code{-1};
  QString stdout_text;
  QString stderr_text;
};

// Spawns `program args...`, kills it if it hasn't finished within `timeout_ms`, and invokes
// `callback` exactly once with the outcome, regardless of which path (normal finish, non-crash
// error, or timeout-kill) fired first. The QProcess is heap-allocated and self-deletes
// (deleteLater) after the callback runs; callers never see or own the QProcess pointer. When
// `callback_context` is supplied, the callback is suppressed if that QObject is destroyed before
// completion; the process still cleans itself up in every completion path.
void runGuardedProcess(const QString& program, const QStringList& arguments, int timeout_ms,
                       std::function<void(GuardedProcessResult)> callback, QObject* callback_context = nullptr);
