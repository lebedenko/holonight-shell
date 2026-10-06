#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <gtest/gtest.h>
#include <holonight_shell_config/config_parsers.h>
#include <holonight_shell_config/config_path.h>
#include <holonight_shell_config/config_schema.h>
#include <holonight_shell_config/config_writer.h>
#include <stdexcept>

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

TEST(ShellConfigPackageTest, TaskbarDefaultsAndRoundTrip) {
  MissingDefaults missing;
  const auto defaults = parseConfigTable(toml::table{}, missing);
  EXPECT_TRUE(defaults.taskbar.enabled);
  EXPECT_TRUE(defaults.taskbar.grouped);
  EXPECT_TRUE(defaults.taskbar.overview_access);
  EXPECT_FALSE(defaults.taskbar.desktop_menu);
  auto config = defaults;
  config.taskbar = {.enabled = false, .grouped = false, .overview_access = false, .desktop_menu = true};
  QTemporaryDir directory;
  const auto path = directory.filePath("config.toml");
  ASSERT_TRUE(ProductConfigWriter::write(config, path));
  EXPECT_EQ(parseConfigTable(toml::parse_file(path.toStdString()), missing).taskbar, config.taskbar);
}

TEST(ShellConfigPackageTest, SchemaRejectsKnownInvalidValuesAndCollections) {
  const std::vector<std::string> invalid_documents{
      "[bar.workspaces]\ncount = 99\n",
      "[bar.taskbar]\nenabled = 1\n",
      "weather = false\n",
      "[weather]\nlatitude = 91.0\n",
      "[weather]\nprovider = 'unknown'\n",
      "[background]\nimages = ['one', 2]\n",
      "[calendar.caldav.work]\nurl = 'https://example.com'\n",
      "[tray.icon_overrides.app]\nicon = 'app'\n",
      "[[widget]]\ntype = 'clock'\nshow_seconds = 1\n",
      "[[widget]]\ntype = 'mpris'\npause_hide_minutes = 0\n",
      "[osd]\nposition = 'unknown'\n",
  };
  for (const auto& bytes : invalid_documents) {
    SCOPED_TRACE(bytes);
    const auto snapshot = HoloNight::Config::parseDocument(bytes);
    ASSERT_TRUE(snapshot);
    EXPECT_FALSE(HoloNight::ShellConfig::decodeDocument(*snapshot.value));
    EXPECT_FALSE(
        HoloNight::Config::validateDocument(*snapshot.value, HoloNight::ShellConfig::documentSchema()).empty());
    MissingDefaults missing;
    EXPECT_THROW(static_cast<void>(parseConfigTable(toml::parse(bytes), missing)), std::invalid_argument);
  }
}

TEST(ShellConfigPackageTest, SparseEditingPreservesUnknownContentAndResetUsesDefaults) {
  using namespace HoloNight::Config;
  const auto snapshot = parseDocument("# keep\n[bar.workspaces]\ncount = 7 # explicit\n[future]\nvalue = 'kept'\n");
  ASSERT_TRUE(snapshot);
  const auto reset = patchDocument(
      *snapshot.value, {{.key = {"bar", "workspaces", "count"}, .baseline = std::int64_t{7}, .pending = std::nullopt}},
      HoloNight::ShellConfig::documentSchema());
  ASSERT_EQ(reset.status, SaveStatus::Success);
  ASSERT_TRUE(reset.snapshot);
  EXPECT_NE(reset.snapshot->revision.bytes.find("# explicit"), std::string::npos);
  EXPECT_NE(reset.snapshot->revision.bytes.find("value = 'kept'"), std::string::npos);
  const auto config = HoloNight::ShellConfig::decodeDocument(*reset.snapshot);
  ASSERT_TRUE(config);
  EXPECT_EQ(config.value->bar_workspaces.count, ProductConfig{}.bar_workspaces.count);
}

TEST(ShellConfigPackageTest, ExportedMetadataSharesDefaultAndRangeDeclarations) {
  const auto& fields = HoloNight::ShellConfig::settingMetadata();
  const auto field = std::ranges::find_if(
      fields, [](const auto& item) { return item.key == HoloNight::Config::KeyPath{"bar", "workspaces", "count"}; });
  ASSERT_NE(field, fields.end());
  ASSERT_TRUE(field->default_value);
  EXPECT_EQ(std::get<std::int64_t>(*field->default_value), ProductConfig{}.bar_workspaces.count);
  EXPECT_EQ(field->minimum, HoloNight::ShellConfig::BarWorkspacesConfig::kMinCount);
  EXPECT_EQ(field->maximum, HoloNight::ShellConfig::BarWorkspacesConfig::kMaxCount);
}

TEST(ShellConfigPackageTest, TemperatureMetadataRetainsTheSettingsKelvinChoice) {
  const auto snapshot = HoloNight::Config::parseDocument("[weather]\ntemp_unit = 'kelvin'\n");
  ASSERT_TRUE(snapshot);
  const auto decoded = HoloNight::ShellConfig::decodeDocument(*snapshot.value);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded.value->weather.temp_unit, QStringLiteral("kelvin"));
}
