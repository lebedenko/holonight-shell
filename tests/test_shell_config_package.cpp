#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>
#include <holonight_shell_config/config_parsers.h>
#include <holonight_shell_config/config_path.h>
#include <holonight_shell_config/config_writer.h>

namespace {

using HoloNight::ShellConfig::MissingDefaults;
using HoloNight::ShellConfig::parseConfigTable;
using HoloNight::ShellConfig::ProductConfig;
using HoloNight::ShellConfig::ProductConfigWriter;
using HoloNight::ShellConfig::resolveProductConfigPath;

TEST(ShellConfigPackageTest, ProductPathPrefersXdgConfigHome) {
  EXPECT_EQ(resolveProductConfigPath({{QStringLiteral("XDG_CONFIG_HOME"), QStringLiteral("/xdg")},
                                      {QStringLiteral("HOME"), QStringLiteral("/home/test")}}),
            QStringLiteral("/xdg/holonight/config.toml"));
}

TEST(ShellConfigPackageTest, ProductPathFallsBackToHomeConfig) {
  EXPECT_EQ(resolveProductConfigPath({{QStringLiteral("XDG_CONFIG_HOME"), QString()},
                                      {QStringLiteral("HOME"), QStringLiteral("/home/test")}}),
            QStringLiteral("/home/test/.config/holonight/config.toml"));
}

TEST(ShellConfigPackageTest, ProductPathIsEmptyWithoutResolvableBase) {
  EXPECT_TRUE(resolveProductConfigPath({}).isEmpty());
  EXPECT_TRUE(
      resolveProductConfigPath({{QStringLiteral("XDG_CONFIG_HOME"), QString()}, {QStringLiteral("HOME"), QString()}})
          .isEmpty());
}

TEST(ShellConfigPackageTest, LegacyAppearanceTableIsInert) {
  const auto table = toml::parse(R"(
[appearance]
ui_font = "Legacy Font"
transparency = 12

[bar.workspaces]
count = 7
)");

  MissingDefaults missing;
  const ProductConfig config = parseConfigTable(table, missing);

  EXPECT_EQ(config.bar_workspaces.count, 7);
}

TEST(ShellConfigPackageTest, CanonicalWriterOmitsLegacyAppearanceTable) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("config.toml"));

  ASSERT_TRUE(ProductConfigWriter::write(ProductConfig{}, path));

  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray contents = file.readAll();
  EXPECT_FALSE(contents.contains("[appearance]"));
  EXPECT_FALSE(contents.contains("[theme]"));
  EXPECT_TRUE(contents.contains("[bar.workspaces]"));
}

}  // namespace
