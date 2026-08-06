#pragma once

#include <QString>

#include <functional>

struct LogindSessionResolverHooks {
  std::function<QString()> session_path_for_pid;
  std::function<QString()> active_session_id;
  std::function<QString(const QString& session_id)> session_path_for_id;
};

// Resolves the active logind session object path for this process, using the UWSM-safe fallback
// documented in CLAUDE.md: GetSessionByPID fails when the process is not in a session-N.scope
// cgroup (systemd user service launch), so a `loginctl show-seat` subprocess fallback resolves the
// active session id, which is then handed to GetSession. Returns an empty string if neither
// resolution path succeeds.
[[nodiscard]] QString resolveActiveLogindSessionPath();

[[nodiscard]] QString resolveActiveLogindSessionPath(const LogindSessionResolverHooks& hooks);
