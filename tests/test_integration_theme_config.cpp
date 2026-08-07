#include "AppearanceService.h"
#include "SettingsPortalBackend.h"
#include "ThemeService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

namespace {

constexpr auto kCustomAppearance = R"(version = 1

[theme]
scheme = "holonight-dark"
accent = "blue"

[typography]
ui_family = "Fira Code"
ui_size = 14
monospace_family = "Inconsolata"
monospace_size = 13
title_family = "Exo"
title_size = 9
display_family = "Orbitron"
display_size = 20

[icons]
theme = "HoloNight"
fallback = "Papirus"
cursor = "default"

[layout]
scale = 1.25

[shape]
style = "rounded"
scale = 1.5
base_radius = 8.0
base_chamfer = 3.0
)";

void writeAppearance(const QString& path, const QByteArray& content) {
  ASSERT_TRUE(QDir().mkpath(QFileInfo(path).absolutePath()));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
  ASSERT_EQ(file.write(content), content.size());
  file.close();
}

class AppearanceIntegrationTest : public ::testing::Test {
 protected:
  QTemporaryDir directory;  // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)
  QString path;             // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes)

  void SetUp() override {
    path = directory.filePath(QStringLiteral("appearance.toml"));
    qputenv("HOLONIGHT_APPEARANCE_FILE", path.toUtf8());
  }

  void TearDown() override { qunsetenv("HOLONIGHT_APPEARANCE_FILE"); }
};

TEST_F(AppearanceIntegrationTest, StartupProjectsCompleteCanonicalAppearance) {
  writeAppearance(path, kCustomAppearance);

  AppearanceService appearance;

  EXPECT_EQ(appearance.scheme(), QStringLiteral("holonight-dark"));
  EXPECT_EQ(appearance.accent(), QStringLiteral("blue"));
  EXPECT_EQ(appearance.colorMode(), QStringLiteral("dark"));
  EXPECT_EQ(appearance.uiFont(), QStringLiteral("Fira Code"));
  EXPECT_EQ(appearance.uiFontSize(), 14);
  EXPECT_EQ(appearance.monospaceFont(), QStringLiteral("Inconsolata"));
  EXPECT_EQ(appearance.monospaceFontSize(), 13);
  EXPECT_EQ(appearance.displayFont(), QStringLiteral("Orbitron"));
  EXPECT_EQ(appearance.displayFontSize(), 20);
  EXPECT_EQ(appearance.fixedFont(), QStringLiteral("Inconsolata"));
  EXPECT_EQ(appearance.clockFont(), QStringLiteral("Orbitron"));
  EXPECT_EQ(appearance.clockFontSize(), 20);
  EXPECT_EQ(appearance.titleFont(), QStringLiteral("Exo"));
  EXPECT_EQ(appearance.titleFontSize(), 9);
  EXPECT_EQ(appearance.iconTheme(), QStringLiteral("HoloNight"));
  EXPECT_EQ(appearance.fallbackIconTheme(), QStringLiteral("Papirus"));
  EXPECT_EQ(appearance.cursorTheme(), QStringLiteral("default"));
  EXPECT_DOUBLE_EQ(appearance.layoutScale(), 1.25);
  EXPECT_EQ(appearance.shapeStyle(), QStringLiteral("rounded"));
  EXPECT_DOUBLE_EQ(appearance.shapeScale(), 1.5);
  EXPECT_TRUE(appearance.hasBaseRadius());
  EXPECT_DOUBLE_EQ(appearance.baseRadius(), 8.0);
  EXPECT_TRUE(appearance.hasBaseChamfer());
  EXPECT_DOUBLE_EQ(appearance.baseChamfer(), 3.0);
  EXPECT_EQ(appearance.revision(), 0);
}

TEST_F(AppearanceIntegrationTest, LiveReplacementEmitsPreciseSignalsBeforeRevision) {
  writeAppearance(path, kCustomAppearance);
  AppearanceService appearance;
  QSignalSpy ui_font_spy(&appearance, &AppearanceService::uiFontChanged);
  QSignalSpy title_font_spy(&appearance, &AppearanceService::titleFontChanged);
  QSignalSpy icon_theme_spy(&appearance, &AppearanceService::iconThemeChanged);
  QSignalSpy revision_spy(&appearance, &AppearanceService::revisionChanged);
  QStringList signal_order;
  QObject::connect(&appearance, &AppearanceService::uiFontChanged, [&signal_order]() { signal_order << "uiFont"; });
  QObject::connect(&appearance, &AppearanceService::revisionChanged, [&signal_order]() { signal_order << "revision"; });

  QByteArray edited(kCustomAppearance);
  edited.replace("ui_family = \"Fira Code\"", "ui_family = \"Cascadia Code\"");
  writeAppearance(path, edited);

  QTRY_COMPARE_WITH_TIMEOUT(appearance.uiFont(), QStringLiteral("Cascadia Code"), 2000);
  EXPECT_EQ(ui_font_spy.count(), 1);
  EXPECT_EQ(title_font_spy.count(), 0);
  EXPECT_EQ(icon_theme_spy.count(), 0);
  EXPECT_EQ(revision_spy.count(), 1);
  EXPECT_EQ(appearance.revision(), 1);
  EXPECT_EQ(signal_order, (QStringList{QStringLiteral("uiFont"), QStringLiteral("revision")}));
}

TEST_F(AppearanceIntegrationTest, PortalProjectionPrecedesRevisionDrivenPaletteReload) {
  writeAppearance(path, kCustomAppearance);
  AppearanceService appearance;
  QStringList signal_order;
  QObject::connect(&appearance, &AppearanceService::revisionChanged, [&signal_order]() { signal_order << "revision"; });
  ThemeService theme(&appearance);
  auto* portal = theme.findChild<SettingsPortalBackend*>();
  ASSERT_NE(portal, nullptr);
  QSignalSpy portal_spy(portal, &SettingsPortalBackend::SettingChanged);
  QSignalSpy palette_spy(&theme, &ThemeService::paletteReloadRequested);
  QObject::connect(portal, &SettingsPortalBackend::SettingChanged, [&signal_order]() { signal_order << "portal"; });
  QObject::connect(&theme, &ThemeService::paletteReloadRequested, [&signal_order]() { signal_order << "palette"; });

  QByteArray edited(kCustomAppearance);
  edited.replace("scheme = \"holonight-dark\"", "scheme = \"holonight-day\"");
  edited.replace("accent = \"blue\"", "accent = \"violet\"");
  writeAppearance(path, edited);

  QTRY_COMPARE_WITH_TIMEOUT(appearance.scheme(), QStringLiteral("holonight-day"), 2000);
  ASSERT_GE(portal_spy.count(), 1);
  EXPECT_EQ(palette_spy.count(), 1);
  EXPECT_EQ(signal_order.first(), QStringLiteral("portal"));
  EXPECT_EQ(signal_order.at(signal_order.size() - 2), QStringLiteral("revision"));
  EXPECT_EQ(signal_order.last(), QStringLiteral("palette"));
}

TEST_F(AppearanceIntegrationTest, SemanticallyUnchangedReplacementDoesNotAdvanceRevision) {
  writeAppearance(path, kCustomAppearance);
  AppearanceService appearance;
  QSignalSpy revision_spy(&appearance, &AppearanceService::revisionChanged);

  writeAppearance(path, kCustomAppearance);
  QTest::qWait(300);

  EXPECT_EQ(appearance.revision(), 0);
  EXPECT_EQ(revision_spy.count(), 0);
}

TEST_F(AppearanceIntegrationTest, InvalidLiveReplacementPreservesLastKnownGoodAppearance) {
  writeAppearance(path, kCustomAppearance);
  AppearanceService appearance;
  QSignalSpy ui_font_spy(&appearance, &AppearanceService::uiFontChanged);
  QSignalSpy revision_spy(&appearance, &AppearanceService::revisionChanged);

  writeAppearance(path, QByteArrayLiteral("not valid = ["));
  QTest::qWait(300);

  EXPECT_EQ(appearance.uiFont(), QStringLiteral("Fira Code"));
  EXPECT_EQ(appearance.shapeStyle(), QStringLiteral("rounded"));
  EXPECT_EQ(appearance.revision(), 0);
  EXPECT_EQ(ui_font_spy.count(), 0);
  EXPECT_EQ(revision_spy.count(), 0);
}

}  // namespace
