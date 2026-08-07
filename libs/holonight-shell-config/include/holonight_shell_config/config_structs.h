#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace HoloNight::ShellConfig {

struct BarWorkspacesConfig {
  int count{5};
  static constexpr int kMinCount{3};
  static constexpr int kMaxCount{10};

  bool operator==(const BarWorkspacesConfig&) const = default;
};

struct BarSystemTrayConfig {
  int max_items{3};
  static constexpr int kMinMaxItems{2};
  static constexpr int kMaxMaxItems{5};

  bool operator==(const BarSystemTrayConfig&) const = default;
};

struct TrayIconOverrideConfig {
  QString name;
  QString id;
  QString service;
  QString object_path;
  QString title;
  QString icon;
  QString attention_icon;

  bool operator==(const TrayIconOverrideConfig&) const = default;
};

struct TrayIconOverridesConfig {
  QList<TrayIconOverrideConfig> items;

  bool operator==(const TrayIconOverridesConfig&) const = default;
};

struct BackgroundConfig {
  // Tilde-expanded absolute image paths, in config order. Empty = solid-color fallback.
  QStringList images;

  // QStringList comparison is element-wise and order-sensitive, which is required:
  // reordering images reassigns wallpapers to monitors and must register as a change.
  bool operator==(const BackgroundConfig&) const = default;

  // Positional mapping for the monitor at monitor_index (index into QGuiApplication::screens()):
  // returns images[monitor_index] when in range, the last image when there are fewer images than
  // monitors (underflow repeats last), and "" when the list is empty (caller renders solid color).
  [[nodiscard]] static QString imageForMonitor(const QStringList& images, int monitor_index);
};

struct NotificationsConfig {
  int default_timeout_ms{5000};
  int max_visible{3};
  static constexpr int kMinVisible{1};
  static constexpr int kMaxVisible{10};

  bool operator==(const NotificationsConfig&) const = default;
};

struct NotificationHistoryConfig {
  bool enabled{true};
  int max_items{100};
  int max_age_days{14};
  bool persist_body{true};

  bool operator==(const NotificationHistoryConfig&) const = default;
};

enum class WeekStartDay : uint8_t { Monday, Sunday };

struct CalendarCaldavAccountConfig {
  QString account_name;  // TOML section key, e.g. "work"
  QString url;           // CalDAV server URL (principal or calendar URL)
  QString username;
  QString password_keyring_key;  // libsecret lookup key, e.g. "holonight-shell/caldav/work"
  QStringList include;           // empty = include all calendars
  QStringList exclude;           // applied after include filter

  bool operator==(const CalendarCaldavAccountConfig&) const = default;
};

struct CalendarIcsAccountConfig {
  QString account_name;  // TOML section key, e.g. "holidays"
  QString url;           // ICS feed URL (may contain embedded auth token)
  QString label;         // optional display name override

  bool operator==(const CalendarIcsAccountConfig&) const = default;
};

struct CalendarConfig {
  WeekStartDay week_start_day{WeekStartDay::Monday};
  QList<CalendarCaldavAccountConfig> caldav_accounts;  // empty = CalDAV disabled
  QList<CalendarIcsAccountConfig> ics_accounts;        // empty = ICS disabled

  bool operator==(const CalendarConfig&) const = default;
};

struct WeatherConfig {
  QString provider{"open-meteo"};
  QString location_source{"manual"};
  QString api_key;      // empty = weather disabled (no key is baked into the binary)
  QString geo_api_key;  // empty = no IP-geolocation fallback available
  // std::optional because 0.0 is a valid coordinate (null island): a missing-key sentinel
  // cannot distinguish "absent" from "present and 0.0". Absence triggers geolocation.
  std::optional<double> latitude;
  std::optional<double> longitude;
  QString city;  // informational display label; may be filled from geolocation
  QString country;
  QString units{"metric"};
  QString temp_unit{"celsius"};
  QString wind_unit{"kmh"};
  QString pressure_unit{"hpa"};
  QString lang{"en"};
  bool show_in_bar{true};
  bool compact_mode{false};
  bool show_feels_like{true};
  bool show_location{true};
  int refresh_interval{1800};

  bool operator==(const WeatherConfig&) const = default;
};

struct LogoConfig {
  QString file;         // tilde-expanded absolute path; empty = no file override
  bool generic{false};  // true = force the bundled generic tux logo (assets/linux-logo/linux.svg)

  bool operator==(const LogoConfig&) const = default;
};

// One of the nine fixed anchor positions a desktop widget can occupy on a monitor.
enum class WidgetPosition : std::uint8_t {
  LeftTop,
  CenterTop,
  RightTop,
  LeftCenter,
  CenterCenter,
  RightCenter,
  LeftBottom,
  CenterBottom,
  RightBottom,
};

// Maps a config position string (e.g. "center-top") to the enum. Returns nullopt for unknown strings.
[[nodiscard]] std::optional<WidgetPosition> widgetPositionFromString(const QString& value);
// Inverse of widgetPositionFromString, for logging.
[[nodiscard]] QString widgetPositionToString(WidgetPosition position);
// True for the three top-anchored positions, which must clear the top bar.
[[nodiscard]] bool widgetPositionIsTopAnchored(WidgetPosition position);

// Discriminator for WidgetDefinition. The type-specific struct is embedded in WidgetDefinition
// alongside this tag (parallel structs, not a variant — revisit if a third diverging type lands).
enum class WidgetType : std::uint8_t {
  TimeToEvent,
  Clock,
  Mpris,
};

struct TimeToEventConfig {
  QString title;
  QDateTime deadline;
  bool has_time{false};  // true iff the deadline config string carried a time component (a 'T' + time)
  bool show_seconds{false};

  bool operator==(const TimeToEventConfig&) const = default;
};

struct ClockConfig {
  bool show_seconds{true};
  QString date_format;  // empty = default "dddd, d MMMM yyyy" applied at format time
  QString locale;       // empty = QLocale::system() at format time

  bool operator==(const ClockConfig&) const = default;
};

struct MprisWidgetConfig {
  // Minutes the widget stays visible in the Paused state before auto-hiding.
  int pause_hide_minutes{10};
  static constexpr int kMinPauseHideMinutes{1};
  static constexpr int kMaxPauseHideMinutes{60};

  bool operator==(const MprisWidgetConfig&) const = default;
};

struct WidgetDefinition {
  WidgetType type{WidgetType::TimeToEvent};
  QStringList monitors;  // empty = all monitors
  WidgetPosition position{WidgetPosition::CenterCenter};
  bool enabled{true};               // false = no WidgetManager/surfaces created (any widget type)
  TimeToEventConfig time_to_event;  // valid when type == WidgetType::TimeToEvent
  ClockConfig clock;                // valid when type == WidgetType::Clock
  MprisWidgetConfig mpris;          // valid when type == WidgetType::Mpris

  bool operator==(const WidgetDefinition&) const = default;
};

struct WidgetsConfig {
  int margin{32};  // logical pixels, applied uniformly to all edges of every widget
  QList<WidgetDefinition> definitions;

  bool operator==(const WidgetsConfig&) const = default;
};

// Per-channel OSD opt-out. A struct rather than a bare bool so channel-specific keys
// (timeout override, icon override) can be added later without changing the config shape.
struct OsdChannelConfig {
  bool enabled{true};

  bool operator==(const OsdChannelConfig&) const = default;
};

struct OsdConfig {
  bool enabled{true};
  int timeout_ms{1500};
  static constexpr int kMinTimeoutMs{300};
  static constexpr int kMaxTimeoutMs{10000};
  // Explicit: WidgetPosition's first enumerator is LeftTop, so value-initialization is not the default we want.
  WidgetPosition position{WidgetPosition::CenterBottom};
  OsdChannelConfig volume;
  OsdChannelConfig brightness;
  OsdChannelConfig keyboard_layout;

  bool operator==(const OsdConfig&) const = default;
};

}  // namespace HoloNight::ShellConfig
