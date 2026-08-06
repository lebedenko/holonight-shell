// Integration test (T-028): AppearanceService reads its font properties from
// ConfigService on startup and live-updates them when the config file changes.
#include "AppearanceService.h"
#include "ConfigService.h"
#include "SettingsPortalBackend.h"
#include "ThemeService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {

QString setTempXdg(const QTemporaryDir& tmp) {
  qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
  return tmp.path() + "/holonight/config.toml";
}

void writeConfig(const QString& path, const QByteArray& content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write(content);
}

constexpr auto kCustomConfig =
    "[appearance]\n"
    "ui_font = \"Fira Code\"\n"
    "ui_font_size = 14\n"
    "fixed_font = \"Inconsolata\"\n"
    "fixed_font_size = 13\n"
    "clock_font = \"Orbitron\"\n"
    "clock_font_size = 20\n"
    "title_font = \"Exo\"\n"
    "title_font_size = 9\n";

}  // namespace

class ThemeConfigIntegrationTest : public ::testing::Test {
 protected:
  QTemporaryDir tmp;    // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  QString config_path;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

  void SetUp() override { config_path = setTempXdg(tmp); }
  void TearDown() override { qunsetenv("XDG_CONFIG_HOME"); }
};

// REQ-F-015, REQ-F-016: AppearanceService mirrors ConfigService::appearance() at construction.
TEST_F(ThemeConfigIntegrationTest, StartupReadsFontsFromConfig) {
  writeConfig(config_path, kCustomConfig);

  ConfigService config;
  AppearanceService appearance(&config);

  EXPECT_EQ(appearance.uiFont(), config.appearance().ui_font);
  EXPECT_EQ(appearance.uiFont(), QStringLiteral("Fira Code"));
  EXPECT_EQ(appearance.uiFontSize(), 14);
  EXPECT_EQ(appearance.clockFont(), QStringLiteral("Orbitron"));
  EXPECT_EQ(appearance.clockFontSize(), 20);
  EXPECT_EQ(appearance.titleFontSize(), 9);
}

// REQ-F-017: editing the file and reloading updates the property and fires its NOTIFY signal.
TEST_F(ThemeConfigIntegrationTest, LiveReloadUpdatesPropertyAndEmitsNotify) {
  writeConfig(config_path, kCustomConfig);

  ConfigService config;
  AppearanceService appearance(&config);
  ASSERT_EQ(appearance.clockFontSize(), 20);

  QSignalSpy clock_size_spy(&appearance, &AppearanceService::clockFontSizeChanged);
  QSignalSpy ui_font_spy(&appearance, &AppearanceService::uiFontChanged);

  QByteArray edited(kCustomConfig);
  edited.replace("clock_font_size = 20", "clock_font_size = 30");
  edited.replace("ui_font = \"Fira Code\"", "ui_font = \"Cascadia Code\"");
  writeConfig(config_path, edited);
  QMetaObject::invokeMethod(&config, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(appearance.clockFontSize(), 30);
  EXPECT_EQ(appearance.uiFont(), QStringLiteral("Cascadia Code"));
  EXPECT_EQ(clock_size_spy.count(), 1);
  EXPECT_EQ(ui_font_spy.count(), 1);
}

// REQ-F-017: unrelated reloads do not churn AppearanceService NOTIFY signals.
TEST_F(ThemeConfigIntegrationTest, UnchangedReloadDoesNotEmitNotify) {
  writeConfig(config_path, kCustomConfig);

  ConfigService config;
  AppearanceService appearance(&config);

  QSignalSpy clock_size_spy(&appearance, &AppearanceService::clockFontSizeChanged);
  QSignalSpy ui_font_spy(&appearance, &AppearanceService::uiFontChanged);

  // Re-parse identical content: appearanceChanged must not fire, so appearance stays quiet.
  QMetaObject::invokeMethod(&config, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(clock_size_spy.count(), 0);
  EXPECT_EQ(ui_font_spy.count(), 0);
}

TEST_F(ThemeConfigIntegrationTest, ThemeConfChangeRequestsPaletteReload) {
  writeConfig(config_path, kCustomConfig);
  const QString theme_config_path = tmp.path() + "/holonight/theme.conf";
  writeConfig(theme_config_path, "[appearance]\nmode=dark\n");

  ThemeService theme;

  QSignalSpy reload_spy(&theme, &ThemeService::paletteReloadRequested);

  writeConfig(theme_config_path, "[appearance]\nmode=light\n");
  QMetaObject::invokeMethod(&theme, "onThemeConfigPathChanged", Qt::DirectConnection,
                            Q_ARG(QString, theme_config_path));

  EXPECT_EQ(reload_spy.count(), 1);
}

TEST_F(ThemeConfigIntegrationTest, ThemeConfChangeUpdatesSettingsPortalBackend) {
  writeConfig(config_path, kCustomConfig);
  const QString theme_config_path = tmp.path() + "/holonight/theme.conf";
  writeConfig(theme_config_path, "[appearance]\nscheme=holonight-dark\naccent=cyan\nmode=dark\n");

  ThemeService theme;
  auto* backend = theme.findChild<SettingsPortalBackend*>();
  ASSERT_NE(backend, nullptr);
  QSignalSpy setting_spy(backend, &SettingsPortalBackend::SettingChanged);

  writeConfig(theme_config_path, "[appearance]\nscheme=holonight-light\naccent=yellow\nmode=light\n");
  QMetaObject::invokeMethod(&theme, "onThemeConfigPathChanged", Qt::DirectConnection,
                            Q_ARG(QString, theme_config_path));

  EXPECT_EQ(backend->values().color_scheme, 2);
  EXPECT_EQ(backend->values().accent_color, QColor(0xe0, 0xaf, 0x68));
  EXPECT_EQ(setting_spy.count(), 2);
}
