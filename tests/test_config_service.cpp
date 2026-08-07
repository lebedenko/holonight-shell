#include "ConfigService.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace HoloNight::ShellConfig;

// Helper: set XDG_CONFIG_HOME to a temp dir, return the expected config path.
static QString setTempXdg(const QTemporaryDir& tmp) {
  qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
  return tmp.path() + "/holonight/config.toml";
}

static void writeTempConfig(const QString& path, const QByteArray& content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
  file.write(content);
}

// ---------------------------------------------------------------------------
// T-022: Initialisation scenarios
// ---------------------------------------------------------------------------

class ConfigServiceTest : public ::testing::Test {
 protected:
  QTemporaryDir tmp;    // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  QString config_path;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

  void SetUp() override { config_path = setTempXdg(tmp); }
  void TearDown() override { qunsetenv("XDG_CONFIG_HOME"); }
};

TEST_F(ConfigServiceTest, CreatesDefaultFileOnFirstRun) {
  ASSERT_FALSE(QFile::exists(config_path));
  ConfigService svc;
  EXPECT_TRUE(QFile::exists(config_path));
}

TEST_F(ConfigServiceTest, DefaultValuesMatchStructDefaults) {
  ConfigService svc;
  EXPECT_EQ(svc.barWorkspaces().count, 5);
  EXPECT_EQ(svc.barSystemTray().max_items, 3);
  EXPECT_EQ(svc.widgets().margin, 32);
  EXPECT_TRUE(svc.widgets().definitions.isEmpty());
}

TEST_F(ConfigServiceTest, DefaultTrayIconOverridesAreEmpty) {
  ConfigService svc;
  EXPECT_TRUE(svc.trayIconOverrides().items.isEmpty());
}

TEST_F(ConfigServiceTest, LoadsCompleteConfigFile) {
  writeTempConfig(config_path,
                  "[appearance]\n"
                  "ui_font = \"Fira Code\"\n"
                  "ui_font_size = 14\n"
                  "fixed_font = \"Inconsolata\"\n"
                  "fixed_font_size = 13\n"
                  "clock_font = \"Orbitron\"\n"
                  "clock_font_size = 20\n"
                  "title_font = \"Exo\"\n"
                  "title_font_size = 9\n"
                  "[bar.workspaces]\n"
                  "count = 7\n"
                  "\n"
                  "[bar.systemtray]\n"
                  "max_items = 4\n");

  ConfigService svc;
  EXPECT_EQ(svc.barWorkspaces().count, 7);
  EXPECT_EQ(svc.barSystemTray().max_items, 4);
}

TEST_F(ConfigServiceTest, PartialFileGetsMissingKeysFilledIn) {
  writeTempConfig(config_path,
                  "[appearance]\n"
                  "ui_font = \"Fira Code\"\n"
                  "ui_font_size = 14\n");

  ConfigService svc;
  // Missing product values default.
  EXPECT_EQ(svc.barWorkspaces().count, 5);
  // File should now contain all keys (written back).
  QFile written(config_path);
  ASSERT_TRUE(written.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString content = QString::fromUtf8(written.readAll());
  EXPECT_FALSE(content.contains(QLatin1String("clock_font")));
  EXPECT_FALSE(content.contains(QLatin1String("[theme]")));
  EXPECT_TRUE(content.contains(QLatin1String("[bar.workspaces]")));
  EXPECT_TRUE(content.contains(QLatin1String("count = 5 # accepted: 3-10")));
  EXPECT_TRUE(content.contains(QLatin1String("max_items = 3 # accepted: 2-5")));
}

TEST_F(ConfigServiceTest, InvalidPresentValuesAreCorrectedInMemoryButNotRewritten) {
  writeTempConfig(config_path,
                  "[appearance]\n"
                  "ui_font = \"Fira Code\"\n"
                  "ui_font_size = \"bad\"\n"
                  "fixed_font = \"JetBrains Mono\"\n"
                  "fixed_font_size = 0\n"
                  "clock_font = \"Rajdhani\"\n"
                  "clock_font_size = 24\n"
                  "title_font = \"Audiowide\"\n"
                  "title_font_size = 8\n"
                  "\n"
                  "[bar.workspaces]\n"
                  "count = 99\n"
                  "\n"
                  "[bar.systemtray]\n"
                  "max_items = 0\n");

  ConfigService svc;

  EXPECT_EQ(svc.barWorkspaces().count, BarWorkspacesConfig::kMaxCount);
  EXPECT_EQ(svc.barSystemTray().max_items, BarSystemTrayConfig::kMinMaxItems);

  QFile written(config_path);
  ASSERT_TRUE(written.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString content = QString::fromUtf8(written.readAll());
  EXPECT_TRUE(content.contains(QLatin1String("ui_font_size = \"bad\"")));
  EXPECT_TRUE(content.contains(QLatin1String("fixed_font_size = 0")));
  EXPECT_TRUE(content.contains(QLatin1String("count = 99")));
  EXPECT_TRUE(content.contains(QLatin1String("max_items = 0")));
}

TEST_F(ConfigServiceTest, CorruptTomlUsesDefaults) {
  writeTempConfig(config_path, "[[[ invalid toml ]]]\n");

  ConfigService svc;
  // Must not crash; values are defaults.
  EXPECT_EQ(svc.barWorkspaces().count, 5);
}

TEST_F(ConfigServiceTest, MissingFileDoesNotCrash) {
  // File does not exist; ConfigService must create it and proceed.
  ConfigService svc;
  EXPECT_EQ(svc.barWorkspaces().count, 5);
}

TEST_F(ConfigServiceTest, InstanceGetterReturnsConstructedPointer) {
  ConfigService svc;
  EXPECT_EQ(ConfigService::instance(), &svc);
}

TEST_F(ConfigServiceTest, InstanceGetterClearsAfterDestruction) {
  {
    ConfigService svc;
    EXPECT_EQ(ConfigService::instance(), &svc);
  }
  EXPECT_EQ(ConfigService::instance(), nullptr);
}

// ---------------------------------------------------------------------------
// T-023: Range clamping
// ---------------------------------------------------------------------------

TEST_F(ConfigServiceTest, ParsesTrayIconOverrides) {
  writeTempConfig(config_path,
                  "[tray.icon_overrides.slack]\n"
                  "id = \"Slack_status_icon_1\"\n"
                  "icon = \"slack-indicator\"\n"
                  "attention_icon = \"slack-indicator-attention\"\n");

  ConfigService svc;
  ASSERT_EQ(svc.trayIconOverrides().items.size(), 1);
  const TrayIconOverrideConfig& override = svc.trayIconOverrides().items.at(0);
  EXPECT_EQ(override.name, QStringLiteral("slack"));
  EXPECT_EQ(override.id, QStringLiteral("Slack_status_icon_1"));
  EXPECT_EQ(override.icon, QStringLiteral("slack-indicator"));
  EXPECT_EQ(override.attention_icon, QStringLiteral("slack-indicator-attention"));
}

TEST_F(ConfigServiceTest, InvalidTrayIconOverrideEntriesAreSkipped) {
  writeTempConfig(config_path,
                  "[tray.icon_overrides.no_icon]\n"
                  "id = \"app\"\n"
                  "[tray.icon_overrides.no_matcher]\n"
                  "icon = \"app-icon\"\n");

  ConfigService svc;
  EXPECT_TRUE(svc.trayIconOverrides().items.isEmpty());
}

TEST_F(ConfigServiceTest, TrayIconOverridesChangedEmittedWhenRulesDiffer) {
  writeTempConfig(config_path,
                  "[tray.icon_overrides.slack]\n"
                  "id = \"Slack_status_icon_1\"\n"
                  "icon = \"slack-indicator\"\n");

  ConfigService svc;
  QSignalSpy spy(&svc, &ConfigService::trayIconOverridesChanged);

  writeTempConfig(config_path,
                  "[tray.icon_overrides.slack]\n"
                  "id = \"Slack_status_icon_1\"\n"
                  "icon = \"slack-indicator-updated\"\n");
  QMetaObject::invokeMethod(&svc, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(spy.count(), 1);
  ASSERT_EQ(svc.trayIconOverrides().items.size(), 1);
  EXPECT_EQ(svc.trayIconOverrides().items.at(0).icon, QStringLiteral("slack-indicator-updated"));
}

TEST_F(ConfigServiceTest, LegacyThemeSectionChangesDoNotEmitShellConfigSignals) {
  writeTempConfig(config_path,
                  "[theme]\n"
                  "variant = \"Storm\"\n"
                  "mode = \"dark\"\n"
                  "accent = \"cyan\"\n");

  ConfigService svc;
  QSignalSpy workspace_spy(&svc, &ConfigService::barWorkspacesChanged);

  writeTempConfig(config_path,
                  "[theme]\n"
                  "variant = \"Aurora\"\n"
                  "mode = \"system\"\n"
                  "accent = \"violet\"\n");
  QMetaObject::invokeMethod(&svc, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(workspace_spy.count(), 0);
}

TEST_F(ConfigServiceTest, WorkspaceCountBelowMinClampsToMin) {
  writeTempConfig(config_path,
                  "[bar.workspaces]\n"
                  "count = 1\n");
  ConfigService svc;
  EXPECT_EQ(svc.barWorkspaces().count, BarWorkspacesConfig::kMinCount);
}

TEST_F(ConfigServiceTest, WorkspaceCountAboveMaxClampsToMax) {
  writeTempConfig(config_path,
                  "[bar.workspaces]\n"
                  "count = 99\n");
  ConfigService svc;
  EXPECT_EQ(svc.barWorkspaces().count, BarWorkspacesConfig::kMaxCount);
}

TEST_F(ConfigServiceTest, SystemTrayItemsBelowMinClampsToMin) {
  writeTempConfig(config_path,
                  "[bar.systemtray]\n"
                  "max_items = 0\n");
  ConfigService svc;
  EXPECT_EQ(svc.barSystemTray().max_items, BarSystemTrayConfig::kMinMaxItems);
}

TEST_F(ConfigServiceTest, SystemTrayItemsAboveMaxClampsToMax) {
  writeTempConfig(config_path,
                  "[bar.systemtray]\n"
                  "max_items = 99\n");
  ConfigService svc;
  EXPECT_EQ(svc.barSystemTray().max_items, BarSystemTrayConfig::kMaxMaxItems);
}

TEST_F(ConfigServiceTest, WorkspaceCountAtBoundaryIsNotClamped) {
  writeTempConfig(config_path,
                  "[bar.workspaces]\n"
                  "count = 3\n");
  ConfigService svc;
  EXPECT_EQ(svc.barWorkspaces().count, 3);

  writeTempConfig(config_path,
                  "[bar.workspaces]\n"
                  "count = 10\n");
  ConfigService svc2;
  EXPECT_EQ(svc2.barWorkspaces().count, 10);
}

// ---------------------------------------------------------------------------
// T-024: Signal emission on first load
// ---------------------------------------------------------------------------

TEST_F(ConfigServiceTest, BarWorkspacesChangedEmittedWhenCountDiffers) {
  writeTempConfig(config_path,
                  "[bar.workspaces]\n"
                  "count = 5\n"
                  "[bar.systemtray]\n"
                  "max_items = 3\n");

  ConfigService svc;
  QSignalSpy spy(&svc, &ConfigService::barWorkspacesChanged);

  writeTempConfig(config_path,
                  "[bar.workspaces]\n"
                  "count = 7\n"
                  "[bar.systemtray]\n"
                  "max_items = 3\n");
  QMetaObject::invokeMethod(&svc, "parseFile", Qt::DirectConnection);
  EXPECT_EQ(spy.count(), 1);
  EXPECT_EQ(svc.barWorkspaces().count, 7);
}

// ---------------------------------------------------------------------------
// T-025 / T-026: Live reload keeps state on failure
// ---------------------------------------------------------------------------

TEST_F(ConfigServiceTest, CorruptReloadPreservesLastGoodValues) {
  writeTempConfig(config_path,
                  "[appearance]\n"
                  "ui_font = \"Fira Code\"\n"
                  "ui_font_size = 16\n"
                  "fixed_font = \"JetBrains Mono\"\n"
                  "fixed_font_size = 12\n"
                  "clock_font = \"Rajdhani\"\n"
                  "clock_font_size = 24\n"
                  "title_font = \"Audiowide\"\n"
                  "title_font_size = 8\n"
                  "[bar.workspaces]\n"
                  "count = 6\n"
                  "[bar.systemtray]\n"
                  "max_items = 4\n");

  ConfigService svc;
  ASSERT_EQ(svc.barWorkspaces().count, 6);

  // Now corrupt the file and reload.
  writeTempConfig(config_path, "[[[ broken ]]]\n");
  QMetaObject::invokeMethod(&svc, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(svc.barWorkspaces().count, 6);
}

TEST_F(ConfigServiceTest, SignalsNotEmittedWhenValuesUnchanged) {
  writeTempConfig(config_path,
                  "[appearance]\n"
                  "ui_font = \"Inter\"\n"
                  "ui_font_size = 12\n"
                  "fixed_font = \"JetBrains Mono\"\n"
                  "fixed_font_size = 12\n"
                  "clock_font = \"Rajdhani\"\n"
                  "clock_font_size = 24\n"
                  "title_font = \"Audiowide\"\n"
                  "title_font_size = 8\n"
                  "[bar.workspaces]\n"
                  "count = 5\n"
                  "[bar.systemtray]\n"
                  "max_items = 3\n");

  ConfigService svc;
  QSignalSpy spy_w(&svc, &ConfigService::barWorkspacesChanged);
  QSignalSpy spy_t(&svc, &ConfigService::barSystemTrayChanged);
  QSignalSpy spy_o(&svc, &ConfigService::trayIconOverridesChanged);
  QSignalSpy spy_widgets(&svc, &ConfigService::widgetsConfigChanged);
  QSignalSpy spy_osd(&svc, &ConfigService::osdConfigChanged);

  // Re-parse the same file — values unchanged, no signals.
  QMetaObject::invokeMethod(&svc, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(spy_w.count(), 0);
  EXPECT_EQ(spy_t.count(), 0);
  EXPECT_EQ(spy_o.count(), 0);
  EXPECT_EQ(spy_widgets.count(), 0);
  EXPECT_EQ(spy_osd.count(), 0);
}

// ---------------------------------------------------------------------------
// Desktop widget config parsing
// ---------------------------------------------------------------------------

TEST_F(ConfigServiceTest, ParsesTimeToEventWidgets) {
  writeTempConfig(config_path,
                  "[widgets]\n"
                  "margin = 48\n"
                  "\n"
                  "[[widget]]\n"
                  "type = \"time-to-event\"\n"
                  "title = \"Flight departs\"\n"
                  "deadline = \"2026-07-15T14:30:00\"\n"
                  "position = \"center-top\"\n"
                  "monitors = [\"DP-1\", \"eDP-1\"]\n"
                  "show_seconds = true\n"
                  "\n"
                  "[[widget]]\n"
                  "type = \"time-to-event\"\n"
                  "title = \"Launch\"\n"
                  "deadline = \"2026-08-01\"\n");

  ConfigService svc;

  EXPECT_EQ(svc.widgets().margin, 48);
  ASSERT_EQ(svc.widgets().definitions.size(), 2);

  const WidgetDefinition& first = svc.widgets().definitions.at(0);
  EXPECT_EQ(first.type, WidgetType::TimeToEvent);
  EXPECT_EQ(first.position, WidgetPosition::CenterTop);
  EXPECT_EQ(first.monitors, QStringList({QStringLiteral("DP-1"), QStringLiteral("eDP-1")}));
  EXPECT_EQ(first.time_to_event.title, QStringLiteral("Flight departs"));
  EXPECT_EQ(first.time_to_event.deadline, QDateTime(QDate(2026, 7, 15), QTime(14, 30, 0)));
  EXPECT_TRUE(first.time_to_event.has_time);
  EXPECT_TRUE(first.time_to_event.show_seconds);

  const WidgetDefinition& second = svc.widgets().definitions.at(1);
  EXPECT_EQ(second.position, WidgetPosition::CenterCenter);
  EXPECT_TRUE(second.monitors.isEmpty());
  EXPECT_EQ(second.time_to_event.title, QStringLiteral("Launch"));
  EXPECT_EQ(second.time_to_event.deadline, QDate(2026, 8, 1).startOfDay());
  EXPECT_FALSE(second.time_to_event.has_time);
  EXPECT_FALSE(second.time_to_event.show_seconds);
}

TEST_F(ConfigServiceTest, ParsesClockWidgets) {
  writeTempConfig(config_path,
                  "[[widget]]\n"
                  "type = \"clock\"\n"
                  "position = \"right-bottom\"\n"
                  "monitors = [\"DP-1\"]\n"
                  "show_seconds = false\n"
                  "date_format = \"yyyy-MM-dd\"\n"
                  "locale = \"de_DE\"\n");

  ConfigService svc;

  ASSERT_EQ(svc.widgets().definitions.size(), 1);
  const WidgetDefinition& clock = svc.widgets().definitions.at(0);
  EXPECT_EQ(clock.type, WidgetType::Clock);
  EXPECT_TRUE(clock.enabled);
  EXPECT_EQ(clock.position, WidgetPosition::RightBottom);
  EXPECT_EQ(clock.monitors, QStringList({QStringLiteral("DP-1")}));
  EXPECT_FALSE(clock.clock.show_seconds);
  EXPECT_EQ(clock.clock.date_format, QStringLiteral("yyyy-MM-dd"));
  EXPECT_EQ(clock.clock.locale, QStringLiteral("de_DE"));
}

TEST_F(ConfigServiceTest, DisabledWidgetsBypassTypeSpecificValidation) {
  writeTempConfig(config_path,
                  "[[widget]]\n"
                  "type = \"time-to-event\"\n"
                  "enabled = false\n"
                  "position = \"not-a-position\"\n"
                  "\n"
                  "[[widget]]\n"
                  "type = \"future-widget\"\n"
                  "enabled = false\n"
                  "\n"
                  "[[widget]]\n"
                  "type = \"clock\"\n"
                  "enabled = true\n");

  ConfigService svc;

  ASSERT_EQ(svc.widgets().definitions.size(), 3);
  EXPECT_FALSE(svc.widgets().definitions.at(0).enabled);
  EXPECT_FALSE(svc.widgets().definitions.at(1).enabled);
  EXPECT_TRUE(svc.widgets().definitions.at(2).enabled);
  EXPECT_EQ(svc.widgets().definitions.at(2).type, WidgetType::Clock);
}

TEST_F(ConfigServiceTest, InvalidWidgetEntriesAreSkipped) {
  writeTempConfig(config_path,
                  "[[widget]]\n"
                  "type = \"time-to-event\"\n"
                  "deadline = \"2026-07-15\"\n"
                  "\n"
                  "[[widget]]\n"
                  "type = \"time-to-event\"\n"
                  "title = \"Missing deadline\"\n"
                  "\n"
                  "[[widget]]\n"
                  "type = \"time-to-event\"\n"
                  "title = \"Bad deadline\"\n"
                  "deadline = \"not-a-date\"\n"
                  "\n"
                  "[[widget]]\n"
                  "type = \"future-widget\"\n"
                  "title = \"Future\"\n"
                  "deadline = \"2026-07-15\"\n"
                  "\n"
                  "[[widget]]\n"
                  "type = \"time-to-event\"\n"
                  "title = \"Bad position\"\n"
                  "deadline = \"2026-07-15\"\n"
                  "position = \"freeform\"\n"
                  "\n"
                  "[[widget]]\n"
                  "type = \"time-to-event\"\n"
                  "title = \"Valid\"\n"
                  "deadline = \"2026-07-15\"\n");

  ConfigService svc;

  ASSERT_EQ(svc.widgets().definitions.size(), 1);
  EXPECT_EQ(svc.widgets().definitions.at(0).time_to_event.title, QStringLiteral("Valid"));
}

TEST_F(ConfigServiceTest, WidgetsConfigChangedEmittedWhenDefinitionsDiffer) {
  writeTempConfig(config_path,
                  "[[widget]]\n"
                  "type = \"time-to-event\"\n"
                  "title = \"One\"\n"
                  "deadline = \"2026-07-15\"\n");

  ConfigService svc;
  QSignalSpy spy(&svc, &ConfigService::widgetsConfigChanged);

  writeTempConfig(config_path,
                  "[[widget]]\n"
                  "type = \"time-to-event\"\n"
                  "title = \"Two\"\n"
                  "deadline = \"2026-07-15\"\n");
  QMetaObject::invokeMethod(&svc, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(spy.count(), 1);
  ASSERT_EQ(svc.widgets().definitions.size(), 1);
  EXPECT_EQ(svc.widgets().definitions.at(0).time_to_event.title, QStringLiteral("Two"));
}

// ---------------------------------------------------------------------------
// Background section parsing and write-back
// ---------------------------------------------------------------------------

TEST_F(ConfigServiceTest, ParsesBackgroundImages) {
  writeTempConfig(config_path,
                  "[background]\n"
                  "images = [\"/abs/a.png\", \"/abs/b.jpg\"]\n");
  ConfigService svc;
  ASSERT_EQ(svc.background().images.size(), 2);
  EXPECT_EQ(svc.background().images.at(0), QStringLiteral("/abs/a.png"));
  EXPECT_EQ(svc.background().images.at(1), QStringLiteral("/abs/b.jpg"));
}

TEST_F(ConfigServiceTest, BackgroundTildeExpansionApplied) {
  writeTempConfig(config_path,
                  "[background]\n"
                  "images = [\"~/wall.png\"]\n");
  ConfigService svc;
  ASSERT_EQ(svc.background().images.size(), 1);
  EXPECT_EQ(svc.background().images.at(0), QDir::homePath() + QStringLiteral("/wall.png"));
}

TEST_F(ConfigServiceTest, EmptyBackgroundListGivesEmptyImages) {
  writeTempConfig(config_path,
                  "[background]\n"
                  "images = []\n");
  ConfigService svc;
  EXPECT_TRUE(svc.background().images.isEmpty());
}

TEST_F(ConfigServiceTest, AbsentBackgroundSectionWritesBackEmptyList) {
  writeTempConfig(config_path,
                  "[appearance]\n"
                  "ui_font = \"Inter\"\n");
  ConfigService svc;
  EXPECT_TRUE(svc.background().images.isEmpty());

  QFile written(config_path);
  ASSERT_TRUE(written.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString content = QString::fromUtf8(written.readAll());
  EXPECT_TRUE(content.contains(QLatin1String("[background]")));
  EXPECT_TRUE(content.contains(QLatin1String("images = []")));
}

TEST_F(ConfigServiceTest, BackgroundChangedEmittedWhenImagesDiffer) {
  writeTempConfig(config_path,
                  "[background]\n"
                  "images = [\"/a.png\"]\n");
  ConfigService svc;
  QSignalSpy spy(&svc, &ConfigService::backgroundChanged);

  writeTempConfig(config_path,
                  "[background]\n"
                  "images = [\"/b.png\"]\n");
  QMetaObject::invokeMethod(&svc, "parseFile", Qt::DirectConnection);
  EXPECT_EQ(spy.count(), 1);
}

TEST_F(ConfigServiceTest, BackgroundChangedNotEmittedWhenImagesUnchanged) {
  writeTempConfig(config_path,
                  "[background]\n"
                  "images = [\"/a.png\", \"/b.png\"]\n");
  ConfigService svc;
  QSignalSpy spy(&svc, &ConfigService::backgroundChanged);

  QMetaObject::invokeMethod(&svc, "parseFile", Qt::DirectConnection);
  EXPECT_EQ(spy.count(), 0);
}

TEST_F(ConfigServiceTest, BackgroundChangedEmittedOnReorder) {
  writeTempConfig(config_path,
                  "[background]\n"
                  "images = [\"/a.png\", \"/b.png\"]\n");
  ConfigService svc;
  QSignalSpy spy(&svc, &ConfigService::backgroundChanged);

  // Same set of paths, different order — must register as a change (positional mapping).
  writeTempConfig(config_path,
                  "[background]\n"
                  "images = [\"/b.png\", \"/a.png\"]\n");
  QMetaObject::invokeMethod(&svc, "parseFile", Qt::DirectConnection);
  EXPECT_EQ(spy.count(), 1);
}

// ---------------------------------------------------------------------------
// [logo] section parsing
// ---------------------------------------------------------------------------

TEST_F(ConfigServiceTest, AbsentLogoSectionDefaultsToEmptyFileAndFalseGeneric) {
  writeTempConfig(config_path,
                  "[appearance]\n"
                  "ui_font = \"Inter\"\n");
  ConfigService svc;
  EXPECT_TRUE(svc.logo().file.isEmpty());
  EXPECT_FALSE(svc.logo().generic);
}

TEST_F(ConfigServiceTest, LogoFileTildeExpansionApplied) {
  writeTempConfig(config_path,
                  "[logo]\n"
                  "file = \"~/my-logo.svg\"\n");
  ConfigService svc;
  EXPECT_EQ(svc.logo().file, QDir::homePath() + QStringLiteral("/my-logo.svg"));
}

TEST_F(ConfigServiceTest, LogoGenericTrueIsParsed) {
  writeTempConfig(config_path,
                  "[logo]\n"
                  "generic = true\n");
  ConfigService svc;
  EXPECT_TRUE(svc.logo().generic);
  EXPECT_TRUE(svc.logo().file.isEmpty());
}

TEST_F(ConfigServiceTest, LogoFileAndGenericBothSetAreBothParsed) {
  writeTempConfig(config_path,
                  "[logo]\n"
                  "file = \"/abs/custom-logo.svg\"\n"
                  "generic = true\n");
  ConfigService svc;
  EXPECT_EQ(svc.logo().file, QStringLiteral("/abs/custom-logo.svg"));
  EXPECT_TRUE(svc.logo().generic);
}

TEST(LogoConfigTest, OperatorEqualsComparesFields) {
  const LogoConfig lhs{.file = QStringLiteral("/a.svg"), .generic = false};
  const LogoConfig same{.file = QStringLiteral("/a.svg"), .generic = false};
  const LogoConfig different_file{.file = QStringLiteral("/b.svg"), .generic = false};
  const LogoConfig different_generic{.file = QStringLiteral("/a.svg"), .generic = true};
  EXPECT_TRUE(lhs == same);
  EXPECT_FALSE(lhs == different_file);
  EXPECT_FALSE(lhs == different_generic);
}

// ---------------------------------------------------------------------------
// [osd] section parsing and live reload
// ---------------------------------------------------------------------------

TEST_F(ConfigServiceTest, AbsentOsdSectionYieldsStructDefaults) {
  writeTempConfig(config_path,
                  "[appearance]\n"
                  "ui_font = \"Inter\"\n");
  ConfigService svc;
  EXPECT_EQ(svc.osd(), OsdConfig{});
  EXPECT_TRUE(svc.osd().enabled);
  EXPECT_EQ(svc.osd().timeout_ms, 1500);
  EXPECT_EQ(svc.osd().position, WidgetPosition::CenterBottom);
  EXPECT_TRUE(svc.osd().volume.enabled);
  EXPECT_TRUE(svc.osd().brightness.enabled);
  EXPECT_TRUE(svc.osd().keyboard_layout.enabled);
}

TEST_F(ConfigServiceTest, OsdSectionOverridesAreLoaded) {
  writeTempConfig(config_path,
                  "[osd]\n"
                  "enabled = true\n"
                  "timeout = 900\n"
                  "position = \"center-top\"\n"
                  "\n"
                  "[osd.brightness]\n"
                  "enabled = false\n");
  ConfigService svc;
  EXPECT_TRUE(svc.osd().enabled);
  EXPECT_EQ(svc.osd().timeout_ms, 900);
  EXPECT_EQ(svc.osd().position, WidgetPosition::CenterTop);
  EXPECT_TRUE(svc.osd().volume.enabled);
  EXPECT_FALSE(svc.osd().brightness.enabled);
  EXPECT_TRUE(svc.osd().keyboard_layout.enabled);
}

TEST_F(ConfigServiceTest, OsdConfigChangedEmittedWhenSectionDiffers) {
  writeTempConfig(config_path,
                  "[osd]\n"
                  "timeout = 1500\n");
  ConfigService svc;
  QSignalSpy spy(&svc, &ConfigService::osdConfigChanged);

  writeTempConfig(config_path,
                  "[osd]\n"
                  "timeout = 3000\n");
  QMetaObject::invokeMethod(&svc, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(spy.count(), 1);
  EXPECT_EQ(svc.osd().timeout_ms, 3000);
}

TEST_F(ConfigServiceTest, OsdConfigChangedNotEmittedWhenSectionUnchanged) {
  writeTempConfig(config_path,
                  "[osd]\n"
                  "timeout = 3000\n"
                  "position = \"left-bottom\"\n");
  ConfigService svc;
  ASSERT_EQ(svc.osd().timeout_ms, 3000);
  QSignalSpy spy(&svc, &ConfigService::osdConfigChanged);

  QMetaObject::invokeMethod(&svc, "parseFile", Qt::DirectConnection);

  EXPECT_EQ(spy.count(), 0);
}

// ---------------------------------------------------------------------------
// BackgroundConfig::imageForMonitor positional mapping
// ---------------------------------------------------------------------------

TEST(BackgroundConfigTest, InRangeSelectsImageByIndex) {
  const QStringList images{QStringLiteral("/a.png"), QStringLiteral("/b.png")};
  EXPECT_EQ(BackgroundConfig::imageForMonitor(images, 0), QStringLiteral("/a.png"));
  EXPECT_EQ(BackgroundConfig::imageForMonitor(images, 1), QStringLiteral("/b.png"));
}

TEST(BackgroundConfigTest, OverflowIgnoredViaIndexBound) {
  // 3 images, monitor index 0 still maps to the first image (extra images are never queried).
  const QStringList images{QStringLiteral("/a.png"), QStringLiteral("/b.png"), QStringLiteral("/c.png")};
  EXPECT_EQ(BackgroundConfig::imageForMonitor(images, 0), QStringLiteral("/a.png"));
}

TEST(BackgroundConfigTest, UnderflowRepeatsLast) {
  const QStringList images{QStringLiteral("/a.png")};
  EXPECT_EQ(BackgroundConfig::imageForMonitor(images, 1), QStringLiteral("/a.png"));
  EXPECT_EQ(BackgroundConfig::imageForMonitor(images, 5), QStringLiteral("/a.png"));
}

TEST(BackgroundConfigTest, EmptyListReturnsEmptyString) {
  EXPECT_TRUE(BackgroundConfig::imageForMonitor(QStringList{}, 0).isEmpty());
}

TEST(BackgroundConfigTest, OperatorEqualsIsOrderSensitive) {
  const BackgroundConfig lhs{QStringList{QStringLiteral("/a.png"), QStringLiteral("/b.png")}};
  const BackgroundConfig reordered{QStringList{QStringLiteral("/b.png"), QStringLiteral("/a.png")}};
  const BackgroundConfig same{QStringList{QStringLiteral("/a.png"), QStringLiteral("/b.png")}};
  EXPECT_FALSE(lhs == reordered);
  EXPECT_TRUE(lhs == same);
}
