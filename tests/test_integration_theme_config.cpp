#include "AppearanceService.h"

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
scale = 1.0

[shape]
style = "inherit"
scale = 1.0
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

TEST_F(AppearanceIntegrationTest, StartupReadsCanonicalTypography) {
  writeAppearance(path, kCustomAppearance);

  AppearanceService appearance;

  EXPECT_EQ(appearance.uiFont(), QStringLiteral("Fira Code"));
  EXPECT_EQ(appearance.uiFontSize(), 14);
  EXPECT_EQ(appearance.fixedFont(), QStringLiteral("Inconsolata"));
  EXPECT_EQ(appearance.clockFont(), QStringLiteral("Orbitron"));
  EXPECT_EQ(appearance.clockFontSize(), 20);
  EXPECT_EQ(appearance.titleFont(), QStringLiteral("Exo"));
  EXPECT_EQ(appearance.titleFontSize(), 9);
}

TEST_F(AppearanceIntegrationTest, LiveReplacementUpdatesTypographyAndEmitsNarrowSignals) {
  writeAppearance(path, kCustomAppearance);
  AppearanceService appearance;
  QSignalSpy ui_font_spy(&appearance, &AppearanceService::uiFontChanged);
  QSignalSpy title_font_spy(&appearance, &AppearanceService::titleFontChanged);

  QByteArray edited(kCustomAppearance);
  edited.replace("ui_family = \"Fira Code\"", "ui_family = \"Cascadia Code\"");
  writeAppearance(path, edited);

  QTRY_COMPARE_WITH_TIMEOUT(appearance.uiFont(), QStringLiteral("Cascadia Code"), 2000);
  EXPECT_EQ(ui_font_spy.count(), 1);
  EXPECT_EQ(title_font_spy.count(), 0);
}

TEST_F(AppearanceIntegrationTest, InvalidLiveReplacementPreservesLastKnownGoodTypography) {
  writeAppearance(path, kCustomAppearance);
  AppearanceService appearance;
  QSignalSpy ui_font_spy(&appearance, &AppearanceService::uiFontChanged);

  writeAppearance(path, QByteArrayLiteral("not valid = ["));
  QTest::qWait(300);

  EXPECT_EQ(appearance.uiFont(), QStringLiteral("Fira Code"));
  EXPECT_EQ(ui_font_spy.count(), 0);
}

}  // namespace
