#include "WidgetCountdown.h"

#include <QDateTime>
#include <QStringList>

#include <array>

namespace {

constexpr qint64 kSecsPerMinute = 60;
constexpr qint64 kSecsPerHour = 60 * 60;
constexpr qint64 kSecsPerDay = 24 * kSecsPerHour;

QString padUnit(qint64 value, QChar suffix) {
  return QStringLiteral("%1%2").arg(value, 2, 10, QLatin1Char('0')).arg(suffix);
}

}  // namespace

QString formatCountdown(const QDateTime& deadline, const QDateTime& now, bool show_seconds) {
  const qint64 remaining = now.secsTo(deadline);
  if (remaining <= 0) {
    return QStringLiteral("Now");
  }

  const qint64 days = remaining / kSecsPerDay;
  const qint64 hours = (remaining % kSecsPerDay) / kSecsPerHour;
  const qint64 minutes = (remaining % kSecsPerHour) / kSecsPerMinute;
  const qint64 seconds = remaining % kSecsPerMinute;

  struct Unit {
    qint64 value{};
    QChar suffix;
  };
  std::array<Unit, 4> units{{{.value = days, .suffix = QLatin1Char('d')},
                             {.value = hours, .suffix = QLatin1Char('h')},
                             {.value = minutes, .suffix = QLatin1Char('m')},
                             {.value = seconds, .suffix = QLatin1Char('s')}}};
  // Drop the seconds column entirely when it must never be shown.
  const size_t count = show_seconds ? units.size() : units.size() - 1;

  // Find the largest non-zero unit; emit from there down to the smallest displayed unit.
  size_t start = count;
  for (size_t i = 0; i < count; ++i) {
    if (units.at(i).value != 0) {
      start = i;
      break;
    }
  }
  if (start == count) {
    // Everything below the displayed resolution: under a minute with show_seconds=false. Show "00m".
    return padUnit(0, QLatin1Char('m'));
  }

  QStringList parts;
  for (size_t i = start; i < count; ++i) {
    parts << padUnit(units.at(i).value, units.at(i).suffix);
  }
  return parts.join(QLatin1Char(' '));
}

QString formatDeadlineLabel(const QDateTime& deadline, bool has_time) {
  return has_time ? deadline.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                  : deadline.toString(QStringLiteral("yyyy-MM-dd"));
}
