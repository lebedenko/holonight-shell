#pragma once

#include <QMetaType>
#include <QObject>
#include <QString>

#include <variant>

// REQ-F-001. Normalized OSD payloads. Free-standing structs rather than nested types so
// OsdChannelSource can name them without depending on OsdController.
//
// Same value-type idiom as WeatherData.h: Q_GADGET so QML can read fields off a signal
// argument without a wrapper QObject, Q_PROPERTY names stay camelCase (the QML-facing
// contract) while backing members are lower_case for readability-identifier-naming.

// A continuous 0-100 value with an optional mute state (volume, brightness).
struct OsdLevelEvent {
  Q_GADGET
  Q_PROPERTY(QString channel MEMBER channel)
  Q_PROPERTY(int value MEMBER value)
  Q_PROPERTY(bool muted MEMBER muted)

 public:
  QString channel;    // "audio-volume" | "screen-brightness"
  int value{0};       // percent, 0-100
  bool muted{false};  // true keeps the real value in `value`; the renderer dims the bar

  bool operator==(const OsdLevelEvent&) const = default;
};

// A discrete choice out of a set, shown as a label pair (keyboard layout).
struct OsdSelectionEvent {
  Q_GADGET
  Q_PROPERTY(QString channel MEMBER channel)
  Q_PROPERTY(QString shortLabel MEMBER short_label)
  Q_PROPERTY(QString fullLabel MEMBER full_label)

 public:
  QString channel;      // "keyboard-layout"
  QString short_label;  // "EN" — the identity for diffing; see OsdController::diffEquivalent
  QString full_label;   // "English (US)" — presentation only, deliberately not diffed

  bool operator==(const OsdSelectionEvent&) const = default;
};

// A single token, so Q_DECLARE_METATYPE below does not break on the variant's comma.
using OsdEvent = std::variant<OsdLevelEvent, OsdSelectionEvent>;

Q_DECLARE_METATYPE(OsdLevelEvent)
Q_DECLARE_METATYPE(OsdSelectionEvent)
Q_DECLARE_METATYPE(OsdEvent)
