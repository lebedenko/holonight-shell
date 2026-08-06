#include "WidgetClock.h"

#include <QDateTime>
#include <QLocale>
#include <QTime>

#include <algorithm>

namespace {

constexpr int kSecsPerMinute = 60;
constexpr int kMsPerSecond = 1000;

// Single source of truth for the default date layout, e.g. "Sunday, 8 June 2026".
const QString kDefaultDateFormat = QStringLiteral("dddd, d MMMM yyyy");

bool isAllowedPatternLetter(QChar chr) {
  static const QString allowed = QStringLiteral("dMyHhmsztAaPp");
  return allowed.contains(chr);
}

bool isUsableDateFormat(const QString& date_format) {
  if (date_format.isEmpty()) {
    return true;
  }

  bool in_quote = false;
  bool has_pattern_token = false;
  for (qsizetype i = 0; i < date_format.size(); ++i) {
    const QChar chr = date_format.at(i);
    if (chr == QLatin1Char('\'')) {
      if (i + 1 < date_format.size() && date_format.at(i + 1) == QLatin1Char('\'')) {
        ++i;
      } else {
        in_quote = !in_quote;
      }
      continue;
    }
    if (in_quote || !chr.isLetter()) {
      continue;
    }
    if (!isAllowedPatternLetter(chr)) {
      return false;
    }
    has_pattern_token = true;
  }
  return !in_quote && has_pattern_token;
}

}  // namespace

QString formatClockTime(const QDateTime& now) { return now.toString(QStringLiteral("HH:mm")); }

QString formatClockSeconds(const QDateTime& now, bool show_seconds) {
  if (!show_seconds) {
    return {};
  }
  return now.toString(QStringLiteral("ss"));
}

QLocale resolveClockLocale(const QString& locale_string, bool& fell_back) {
  fell_back = false;
  if (locale_string.isEmpty()) {
    return QLocale::system();
  }
  const QLocale locale(locale_string);
  // QLocale falls back to the C locale for an unrecognized language tag; treat that as invalid input
  // and prefer the system locale so weekday/month names remain meaningful.
  if (locale.language() == QLocale::C) {
    fell_back = true;
    return QLocale::system();
  }
  return locale;
}

QString formatClockDate(const QDateTime& now, const QLocale& locale, const QString& date_format, bool& fell_back) {
  fell_back = false;
  if (!isUsableDateFormat(date_format)) {
    fell_back = true;
    return locale.toString(now, kDefaultDateFormat);
  }
  const QString pattern = date_format.isEmpty() ? kDefaultDateFormat : date_format;
  QString result = locale.toString(now, pattern);
  if (result.isEmpty() && !date_format.isEmpty()) {
    fell_back = true;
    result = locale.toString(now, kDefaultDateFormat);
  }
  return result;
}

int clockTickIntervalMs(const QDateTime& now, bool show_seconds) {
  const QTime time = now.time();
  if (show_seconds) {
    return std::max(kMsPerSecond - time.msec(), 1);
  }
  return std::max(((kSecsPerMinute - time.second()) * kMsPerSecond) - time.msec(), 1);
}
