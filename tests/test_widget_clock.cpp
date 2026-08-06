#include "WidgetClock.h"

#include <QDateTime>
#include <QLocale>

#include <gtest/gtest.h>

namespace {

// Fixed reference instant: 2026-06-08 14:23:07.250 (a Monday).
QDateTime fixedNow() { return {QDate(2026, 6, 8), QTime(14, 23, 7, 250)}; }

}  // namespace

// REQ-F-001 / REQ-F-006: 24-hour HH:mm time string.
TEST(WidgetClock, FormatsTime24Hour) {
  EXPECT_EQ(formatClockTime(fixedNow()), QStringLiteral("14:23"));
  EXPECT_EQ(formatClockTime(QDateTime(QDate(2026, 1, 1), QTime(2, 5, 0))), QStringLiteral("02:05"));
}

// REQ-F-001 / REQ-F-009: zero-padded seconds when shown, empty string when hidden.
TEST(WidgetClock, SecondsShownAreZeroPadded) { EXPECT_EQ(formatClockSeconds(fixedNow(), true), QStringLiteral("07")); }

TEST(WidgetClock, SecondsHiddenWhenDisabled) { EXPECT_EQ(formatClockSeconds(fixedNow(), false), QString()); }

// REQ-F-002: default pattern is "dddd, d MMMM yyyy" when no date_format is configured.
TEST(WidgetClock, DateUsesDefaultPatternWhenEmpty) {
  const QLocale locale(QLocale::English, QLocale::UnitedKingdom);
  bool fell_back = true;
  const QString out = formatClockDate(fixedNow(), locale, QString(), fell_back);
  EXPECT_EQ(out, QStringLiteral("Monday, 8 June 2026"));
  EXPECT_FALSE(fell_back);  // empty input uses the default silently, not a fallback
}

// REQ-F-007: a custom date_format pattern overrides the default.
TEST(WidgetClock, DateHonoursCustomPattern) {
  const QLocale locale(QLocale::English, QLocale::UnitedKingdom);
  bool fell_back = true;
  EXPECT_EQ(formatClockDate(fixedNow(), locale, QStringLiteral("yyyy-MM-dd"), fell_back), QStringLiteral("2026-06-08"));
  EXPECT_FALSE(fell_back);
}

TEST(WidgetClock, DateFallsBackForInvalidPatternText) {
  const QLocale locale(QLocale::English, QLocale::UnitedKingdom);
  bool fell_back = false;
  EXPECT_EQ(formatClockDate(fixedNow(), locale, QStringLiteral("invalid%syntax"), fell_back),
            QStringLiteral("Monday, 8 June 2026"));
  EXPECT_TRUE(fell_back);
}

TEST(WidgetClock, DateAllowsQuotedLiteralText) {
  const QLocale locale(QLocale::English, QLocale::UnitedKingdom);
  bool fell_back = true;
  EXPECT_EQ(formatClockDate(fixedNow(), locale, QStringLiteral("yyyy 'year'"), fell_back), QStringLiteral("2026 year"));
  EXPECT_FALSE(fell_back);
}

// REQ-F-008: weekday/month names follow the supplied locale.
TEST(WidgetClock, DateUsesLocaleNames) {
  const QLocale german(QLocale::German, QLocale::Germany);
  bool fell_back = false;
  EXPECT_EQ(formatClockDate(fixedNow(), german, QStringLiteral("MMMM"), fell_back), QStringLiteral("Juni"));
}

// REQ-F-008: empty locale string resolves to the system locale without flagging a fallback.
TEST(WidgetClock, LocaleEmptyResolvesToSystem) {
  bool fell_back = true;
  const QLocale resolved = resolveClockLocale(QString(), fell_back);
  EXPECT_FALSE(fell_back);
  EXPECT_EQ(resolved, QLocale::system());
}

// REQ-F-008: a valid tag resolves to that locale.
TEST(WidgetClock, LocaleValidTagResolves) {
  bool fell_back = true;
  const QLocale resolved = resolveClockLocale(QStringLiteral("de_DE"), fell_back);
  EXPECT_FALSE(fell_back);
  EXPECT_EQ(resolved.language(), QLocale::German);
}

// REQ-F-012: an unrecognized locale tag falls back to the system locale and flags it.
TEST(WidgetClock, LocaleInvalidTagFallsBackToSystem) {
  bool fell_back = false;
  const QLocale resolved = resolveClockLocale(QStringLiteral("xyz_ABC"), fell_back);
  EXPECT_TRUE(fell_back);
  EXPECT_EQ(resolved, QLocale::system());
}

// REQ-F-013: with seconds shown, the tick aligns to the next second boundary.
TEST(WidgetClock, TickIntervalSecondsAligned) {
  EXPECT_EQ(clockTickIntervalMs(fixedNow(), true), 750);  // 1000 - 250ms
  EXPECT_EQ(clockTickIntervalMs(QDateTime(QDate(2026, 1, 1), QTime(0, 0, 0, 0)), true), 1000);
}

// REQ-F-014: without seconds, the tick aligns to the next full minute.
TEST(WidgetClock, TickIntervalMinuteAligned) {
  // 14:23:07.250 → 52s 750ms to the next minute = 52750ms.
  EXPECT_EQ(clockTickIntervalMs(fixedNow(), false), 52750);
  EXPECT_EQ(clockTickIntervalMs(QDateTime(QDate(2026, 1, 1), QTime(0, 0, 0, 0)), false), 60000);
}
