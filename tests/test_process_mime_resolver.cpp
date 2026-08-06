// Regression coverage for MimeService's ProcessMimeResolver (REQ-F-003): after migrating its
// three QProcess call sites to the shared GuardedProcessRunner helper, exercise the real resolver
// end-to-end (real subprocesses, real PATH lookup) via MimeService's public API and verify the
// same three scenarios (success, unavailable, timeout-kill) produce the same output/error
// categorization as before the refactor.

#include "mime/MimeService.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

namespace {

// Saves and restores an environment variable across a test, so PATH/XDG overrides used to
// sandbox a fake xdg-mime/xdg-settings never leak into other tests in this binary.
class ScopedEnvVar {
 public:
  ScopedEnvVar(const char* name, const QString& value) : name_(name), had_value_(qEnvironmentVariableIsSet(name)) {
    if (had_value_) {
      original_ = qEnvironmentVariable(name);
    }
    qputenv(name_, value.toUtf8());
  }
  ~ScopedEnvVar() {
    if (had_value_) {
      qputenv(name_, original_.toUtf8());
    } else {
      qunsetenv(name_);
    }
  }

  ScopedEnvVar(const ScopedEnvVar&) = delete;
  ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;
  ScopedEnvVar(ScopedEnvVar&&) = delete;
  ScopedEnvVar& operator=(ScopedEnvVar&&) = delete;

 private:
  const char* name_;
  bool had_value_;
  QString original_;
};

void writeFakeExecutable(const QDir& dir, const QString& name, const QString& script_body) {
  const QString path = dir.filePath(name);
  QFile file(path);
  const bool opened = file.open(QIODevice::WriteOnly | QIODevice::Text);
  ASSERT_TRUE(opened) << "failed to write fake executable: " << path.toStdString();
  file.write(QStringLiteral("#!/bin/bash\n").toUtf8());
  file.write(script_body.toUtf8());
  file.close();
  ASSERT_TRUE(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
}

}  // namespace

TEST(ProcessMimeResolver, SuccessfulQueryPopulatesRoleFromRealSubprocess) {
  QTemporaryDir bin_dir;
  ASSERT_TRUE(bin_dir.isValid());
  writeFakeExecutable(QDir(bin_dir.path()), QStringLiteral("xdg-mime"),
                      QStringLiteral("echo success.desktop\nexit 0\n"));
  writeFakeExecutable(QDir(bin_dir.path()), QStringLiteral("xdg-settings"),
                      QStringLiteral("if [ \"$1\" = \"get\" ]; then echo success.desktop; exit 0; fi\n"
                                     "if [ \"$1\" = \"check\" ]; then echo yes; exit 0; fi\n"));

  ScopedEnvVar path_guard("PATH", bin_dir.path());

  MimeService service;
  QSignalSpy terminal_spy(&service, &MimeService::defaultTerminalChanged);
  QSignalSpy browser_spy(&service, &MimeService::defaultBrowserChanged);

  ASSERT_TRUE(terminal_spy.wait(2000));
  ASSERT_TRUE(browser_spy.wait(2000));

  EXPECT_EQ(service.defaultTerminal(), QStringLiteral("success.desktop"));
  EXPECT_EQ(service.defaultBrowser(), QStringLiteral("success.desktop"));
}

TEST(ProcessMimeResolver, UnavailableBinaryLeavesRoleEmptyWithoutCrashing) {
  QTemporaryDir bin_dir;
  ASSERT_TRUE(bin_dir.isValid());
  // Deliberately empty: xdg-mime/xdg-settings are not on PATH, so QProcess::start() fails
  // (FailedToStart), exercising the errorOccurred/had_error mapping path.

  ScopedEnvVar path_guard("PATH", bin_dir.path());

  MimeService service;
  QTest::qWait(300);

  EXPECT_TRUE(service.defaultTerminal().isEmpty());
  EXPECT_TRUE(service.defaultBrowser().isEmpty());
}

TEST(ProcessMimeResolver, HangingSubprocessIsKilledAtTimeoutAndRoleStaysEmpty) {
  QTemporaryDir bin_dir;
  ASSERT_TRUE(bin_dir.isValid());
  writeFakeExecutable(QDir(bin_dir.path()), QStringLiteral("xdg-mime"), QStringLiteral("sleep 8\n"));
  writeFakeExecutable(QDir(bin_dir.path()), QStringLiteral("xdg-settings"), QStringLiteral("sleep 8\n"));

  ScopedEnvVar path_guard("PATH", bin_dir.path());

  MimeService service;
  // The hardcoded 5000ms guard timeout (kMimeProcessTimeoutMs) must kill the hung process well
  // before the 8s sleep would otherwise return — bound this wait comfortably above the timeout.
  QTest::qWait(5600);

  EXPECT_TRUE(service.defaultTerminal().isEmpty());
  EXPECT_TRUE(service.defaultBrowser().isEmpty());
}

TEST(ProcessMimeResolver, SetDefaultSuccessWritesConfigAndUpdatesRole) {
  QTemporaryDir bin_dir;
  QTemporaryDir config_dir;
  ASSERT_TRUE(bin_dir.isValid());
  ASSERT_TRUE(config_dir.isValid());
  // Stateful fake: "query default" reads back whatever the last "default" call wrote, so the
  // test can observe a real empty → set-by-test.desktop transition (and thus a real
  // defaultTerminalChanged signal) rather than the query round-trip trivially reconfirming a
  // value the initial refreshAllRoles() pass already cached.
  writeFakeExecutable(
      QDir(bin_dir.path()), QStringLiteral("xdg-mime"),
      QStringLiteral("state_file=\"$(dirname \"$0\")/state.txt\"\n"
                     "if [ \"$1\" = \"default\" ]; then printf '%s' \"$2\" > \"$state_file\"; exit 0; fi\n"
                     "if [ \"$1\" = \"query\" ]; then "
                     "[ -f \"$state_file\" ] && cat \"$state_file\"; exit 0; fi\n"));
  writeFakeExecutable(QDir(bin_dir.path()), QStringLiteral("xdg-settings"), QStringLiteral("exit 1\n"));

  ScopedEnvVar path_guard("PATH", bin_dir.path());
  ScopedEnvVar config_guard("XDG_CONFIG_HOME", config_dir.path());
  ScopedEnvVar desktop_guard("XDG_CURRENT_DESKTOP", QString{});

  MimeService service;
  QTest::qWait(300);  // let the initial refreshAllRoles() pass settle first

  QSignalSpy terminal_spy(&service, &MimeService::defaultTerminalChanged);
  service.setDefaultTerminal(QStringLiteral("set-by-test.desktop"));
  ASSERT_TRUE(terminal_spy.wait(2000));

  EXPECT_EQ(service.defaultTerminal(), QStringLiteral("set-by-test.desktop"));
  const QString mimeapps_path = QDir(config_dir.path()).filePath(QStringLiteral("mimeapps.list"));
  EXPECT_TRUE(QFile::exists(mimeapps_path));
}
