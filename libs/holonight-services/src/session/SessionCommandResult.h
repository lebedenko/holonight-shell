#pragma once

#include <QString>

#include <utility>

// Outcome of a session-command dispatch (lock/logout/sleep/reboot/shutdown). `reason` is only
// meaningful when ok == false. CommandRunner::run() only reports launch success/failure (bool) —
// launches are detached/fire-and-forget (QProcess::startDetached), so there is no exit code to
// report — so `reason` is a human-readable description built from the program+args that failed
// to launch. Graceful no-ops (e.g. "no locker installed", "already locked") are ok == true:
// REQ-F-B.* requires signaling command DISPATCH failures, not the absence of work.
struct SessionCommandResult {
  bool ok{true};
  QString reason;

  [[nodiscard]] static SessionCommandResult success() { return {}; }
  [[nodiscard]] static SessionCommandResult failure(QString why) { return {.ok = false, .reason = std::move(why)}; }
};
