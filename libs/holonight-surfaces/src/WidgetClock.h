#pragma once

#include <QString>

class QDateTime;
class QLocale;

// Pure formatting helpers for the clock widget, factored out of WidgetManager so they can be
// unit-tested without a live LayerShell. `now` is passed explicitly to keep the functions
// deterministic; fallback conditions are reported via out-params so the caller owns warn-once logging.

// 24-hour hours:minutes, e.g. "14:23". Rendered at the large size by the QML body.
[[nodiscard]] QString formatClockTime(const QDateTime& now);

// Zero-padded two-digit seconds, e.g. "07". Returns an empty string when `show_seconds` is false, so
// the QML side can hide the smaller seconds element via a `visible` binding.
[[nodiscard]] QString formatClockSeconds(const QDateTime& now, bool show_seconds);

// Resolves a BCP-47 language tag (e.g. "de_DE") to a QLocale. An empty string yields the system
// locale; an unrecognized tag yields the system locale with `fell_back` set true.
[[nodiscard]] QLocale resolveClockLocale(const QString& locale_string, bool& fell_back);

// Formats `now` as a date string using `locale` and the Qt date `date_format` pattern. An empty
// pattern uses the default "dddd, d MMMM yyyy". If a non-empty custom pattern yields an empty result,
// falls back to the default pattern and sets `fell_back` true.
[[nodiscard]] QString formatClockDate(const QDateTime& now, const QLocale& locale, const QString& date_format,
                                      bool& fell_back);

// Milliseconds until the next tick, aligned to a wall-clock boundary: the next second when
// `show_seconds` is true, otherwise the next full minute. Always at least 1. A single-shot timer
// re-deriving this each tick stays drift-free across freeze/resume.
[[nodiscard]] int clockTickIntervalMs(const QDateTime& now, bool show_seconds);
