#pragma once

#include <QString>

class QDateTime;

// Pure formatting helpers for the time-to-event widget, factored out of WidgetManager so they can be
// unit-tested without a live LayerShell. `now` is passed explicitly to keep the functions deterministic.

// Adaptive countdown from `now` to `deadline`. Shows from the largest non-zero unit down to the
// smallest displayed unit (seconds when show_seconds, otherwise minutes), each zero-padded to two
// digits, e.g. "12d 04h 37m", "04h 37m 12s", "37m 12s", "12s". Returns "Now" once the deadline is
// reached or passed. With show_seconds=false and under a minute remaining, returns "00m" (no rounding).
[[nodiscard]] QString formatCountdown(const QDateTime& deadline, const QDateTime& now, bool show_seconds);

// Fixed, locale-independent event-date label: "yyyy-MM-dd" for a date-only deadline, or
// "yyyy-MM-dd HH:mm" when the deadline carried a time component.
[[nodiscard]] QString formatDeadlineLabel(const QDateTime& deadline, bool has_time);
