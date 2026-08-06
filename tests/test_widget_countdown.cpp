#include "WidgetCountdown.h"

#include <QDateTime>

#include <gtest/gtest.h>

namespace {

// Fixed reference instant; deadlines are expressed as offsets from it.
QDateTime baseNow() { return {QDate(2026, 1, 1), QTime(0, 0, 0)}; }

QDateTime nowPlus(qint64 secs) { return baseNow().addSecs(secs); }

constexpr qint64 kMinute = 60;
constexpr qint64 kHour = 60 * 60;
constexpr qint64 kDay = 24 * kHour;

}  // namespace

// REQ-F-022: adaptive countdown, leading zero units dropped, each unit zero-padded to two digits.
TEST(WidgetCountdown, AdaptiveDaysHoursMinutesNoSeconds) {
  const QDateTime deadline = nowPlus((12 * kDay) + (4 * kHour) + (37 * kMinute));
  EXPECT_EQ(formatCountdown(deadline, baseNow(), false), QStringLiteral("12d 04h 37m"));
}

TEST(WidgetCountdown, AdaptiveHoursMinutesSeconds) {
  const QDateTime deadline = nowPlus((4 * kHour) + (37 * kMinute) + 12);
  EXPECT_EQ(formatCountdown(deadline, baseNow(), true), QStringLiteral("04h 37m 12s"));
}

TEST(WidgetCountdown, AdaptiveMinutesSeconds) {
  const QDateTime deadline = nowPlus((37 * kMinute) + 12);
  EXPECT_EQ(formatCountdown(deadline, baseNow(), true), QStringLiteral("37m 12s"));
}

TEST(WidgetCountdown, AdaptiveSecondsOnly) {
  const QDateTime deadline = nowPlus(12);
  EXPECT_EQ(formatCountdown(deadline, baseNow(), true), QStringLiteral("12s"));
}

// REQ-F-023: under a minute with show_seconds=false shows "00m" (no rounding, no seconds field).
TEST(WidgetCountdown, SubMinuteNoSecondsShowsZeroMinutes) {
  const QDateTime deadline = nowPlus(45);
  EXPECT_EQ(formatCountdown(deadline, baseNow(), false), QStringLiteral("00m"));
}

// REQ-F-023: with show_seconds=false the seconds column never appears even when minutes are present.
TEST(WidgetCountdown, NoSecondsSuppressesSecondsColumn) {
  const QDateTime deadline = nowPlus((90 * kMinute) + 30);
  EXPECT_EQ(formatCountdown(deadline, baseNow(), false), QStringLiteral("01h 30m"));
}

// REQ-F-025: at or past the deadline the countdown reads "Now" and stops.
TEST(WidgetCountdown, ReachedDeadlineShowsNow) {
  EXPECT_EQ(formatCountdown(baseNow(), baseNow(), true), QStringLiteral("Now"));
  EXPECT_EQ(formatCountdown(nowPlus(-30), baseNow(), true), QStringLiteral("Now"));
}

// Trailing zero seconds are still shown when show_seconds=true (down to the smallest displayed unit).
TEST(WidgetCountdown, ShowSecondsKeepsTrailingZeroSeconds) {
  const QDateTime deadline = nowPlus((12 * kDay) + (4 * kHour) + (37 * kMinute));
  EXPECT_EQ(formatCountdown(deadline, baseNow(), true), QStringLiteral("12d 04h 37m 00s"));
}

// REQ-F-021: fixed, locale-independent date label; time appended only when the deadline had a time.
TEST(WidgetCountdown, DateLabelDateOnly) {
  const QDateTime deadline(QDate(2026, 7, 15), QTime(0, 0, 0));
  EXPECT_EQ(formatDeadlineLabel(deadline, false), QStringLiteral("2026-07-15"));
}

TEST(WidgetCountdown, DateLabelWithTime) {
  const QDateTime deadline(QDate(2026, 7, 15), QTime(14, 30, 0));
  EXPECT_EQ(formatDeadlineLabel(deadline, true), QStringLiteral("2026-07-15 14:30"));
}
