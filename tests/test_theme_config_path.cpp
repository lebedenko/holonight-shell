#include "ThemeConfigPath.h"

#include <QDir>

#include <gtest/gtest.h>

namespace {

class ScopedEnvVar {
 public:
  explicit ScopedEnvVar(const char* name)
      : name_(name), previous_(qgetenv(name)), had_value_(qEnvironmentVariableIsSet(name)) {}

  ~ScopedEnvVar() {
    if (had_value_) {
      qputenv(name_, previous_);
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
  QByteArray previous_;
  bool had_value_ = false;
};

}  // namespace

TEST(ThemeConfigPathTest, UsesXdgConfigHomeWhenSet) {
  const ScopedEnvVar xdg_config_home("XDG_CONFIG_HOME");
  qputenv("XDG_CONFIG_HOME", QByteArrayLiteral("/tmp/holonight-xdg-test"));

  const ThemeConfigPaths paths = resolveHolonightThemeConfigPaths();

  EXPECT_EQ(paths.dir_path, QStringLiteral("/tmp/holonight-xdg-test/holonight"));
  EXPECT_EQ(paths.file_path, QStringLiteral("/tmp/holonight-xdg-test/holonight/theme.conf"));
}

TEST(ThemeConfigPathTest, FallsBackToDotConfigWhenUnset) {
  const ScopedEnvVar xdg_config_home("XDG_CONFIG_HOME");
  qunsetenv("XDG_CONFIG_HOME");

  const ThemeConfigPaths paths = resolveHolonightThemeConfigPaths();

  EXPECT_EQ(paths.dir_path, QDir::homePath() + QStringLiteral("/.config/holonight"));
  EXPECT_EQ(paths.file_path, QDir::homePath() + QStringLiteral("/.config/holonight/theme.conf"));
}
