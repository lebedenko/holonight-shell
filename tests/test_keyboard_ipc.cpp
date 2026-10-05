#include "ShellHyprlandIpc.h"

#include <QTest>

#include <gtest/gtest.h>

TEST(HyprlandKeyboardIpc, ParsesKeyboardLayoutEvent) {
  const std::optional<ShellHyprlandKeyboardLayout> parsed =
      parseShellHyprlandKeyboardLayoutEvent("activelayout>>at-translated-set-2-keyboard,English (US)");

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->keyboard_name, QStringLiteral("at-translated-set-2-keyboard"));
  EXPECT_EQ(parsed->layout_name, QStringLiteral("English (US)"));
}

TEST(HyprlandKeyboardIpc, IgnoresUnrelatedKeyboardLayoutEvent) {
  EXPECT_FALSE(parseShellHyprlandKeyboardLayoutEvent("activewindow>>kitty,title").has_value());
}

TEST(HyprlandKeyboardIpc, ParsesMainKeyboardLayoutFromDevicesJson) {
  const std::optional<QString> parsed = parseShellHyprlandKeyboardLayoutDevicesJson(R"json({
    "keyboards": [
      {"name": "other-keyboard", "active_keymap": "English (US)", "main": false},
      {"name": "main-keyboard", "active_keymap": "Ukrainian", "main": true}
    ]
})json");

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(*parsed, QStringLiteral("Ukrainian"));
}

TEST(HyprlandKeyboardIpc, FallsBackToFirstKeyboardLayoutFromDevicesJson) {
  const std::optional<QString> parsed =
      parseShellHyprlandKeyboardLayoutDevicesJson(R"json({"keyboards":[{"active_keymap":"English (US)"}]})json");

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(*parsed, QStringLiteral("English (US)"));
}

TEST(HyprlandKeyboardIpc, WarnsOnMalformedKeyboardLayoutDevicesJson) {
  QTest::ignoreMessage(QtWarningMsg, "parseShellHyprlandKeyboardLayoutDevicesJson: expected JSON object");
  EXPECT_FALSE(parseShellHyprlandKeyboardLayoutDevicesJson("not json").has_value());
}

TEST(HyprlandKeyboardIpc, FormatsKeyboardLayoutCode) {
  EXPECT_EQ(shellKeyboardLayoutCode(QStringLiteral("English (US)")), QStringLiteral("EN"));
  EXPECT_EQ(shellKeyboardLayoutCode(QStringLiteral("Ukrainian")), QStringLiteral("UK"));
  EXPECT_EQ(shellKeyboardLayoutCode(QStringLiteral("custom layout")), QStringLiteral("CU"));
}
