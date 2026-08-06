#include "LogindSessionResolver.h"

#include <gtest/gtest.h>

TEST(LogindSessionResolverTest, PrefersSessionPathForCurrentPid) {
  bool active_session_checked = false;
  bool session_id_checked = false;
  LogindSessionResolverHooks hooks;
  hooks.session_path_for_pid = [] { return QStringLiteral("/org/freedesktop/login1/session/_32"); };
  hooks.active_session_id = [&active_session_checked] {
    active_session_checked = true;
    return QStringLiteral("3");
  };
  hooks.session_path_for_id = [&session_id_checked](const QString&) {
    session_id_checked = true;
    return QStringLiteral("/org/freedesktop/login1/session/_33");
  };

  EXPECT_EQ(resolveActiveLogindSessionPath(hooks), QStringLiteral("/org/freedesktop/login1/session/_32"));
  EXPECT_FALSE(active_session_checked);
  EXPECT_FALSE(session_id_checked);
}

TEST(LogindSessionResolverTest, FallsBackThroughActiveSeatSessionId) {
  QString resolved_session_id;
  LogindSessionResolverHooks hooks;
  hooks.session_path_for_pid = [] { return QString{}; };
  hooks.active_session_id = [] { return QStringLiteral("7"); };
  hooks.session_path_for_id = [&resolved_session_id](const QString& session_id) {
    resolved_session_id = session_id;
    return QStringLiteral("/org/freedesktop/login1/session/_37");
  };

  EXPECT_EQ(resolveActiveLogindSessionPath(hooks), QStringLiteral("/org/freedesktop/login1/session/_37"));
  EXPECT_EQ(resolved_session_id, QStringLiteral("7"));
}

TEST(LogindSessionResolverTest, ReturnsEmptyWhenBothResolutionPathsFail) {
  bool session_id_checked = false;
  LogindSessionResolverHooks hooks;
  hooks.session_path_for_pid = [] { return QString{}; };
  hooks.active_session_id = [] { return QString{}; };
  hooks.session_path_for_id = [&session_id_checked](const QString&) {
    session_id_checked = true;
    return QStringLiteral("/org/freedesktop/login1/session/_37");
  };

  EXPECT_TRUE(resolveActiveLogindSessionPath(hooks).isEmpty());
  EXPECT_FALSE(session_id_checked);
}

// Smoke test only: no live logind session is guaranteed in the offscreen/CI environment, and the
// function's real strategy involves live D-Bus + a loginctl subprocess that cannot be asserted
// end-to-end. The deterministic hook tests above cover resolver control flow.
TEST(LogindSessionResolverTest, ResolvesIdempotently) {
  const QString first = resolveActiveLogindSessionPath();
  const QString second = resolveActiveLogindSessionPath();

  EXPECT_EQ(first, second);
}
