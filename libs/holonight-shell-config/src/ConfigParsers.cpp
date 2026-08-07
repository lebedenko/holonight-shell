#include <QDir>
#include <QFile>
#include <QHash>
#include <QLoggingCategory>
#include <QSaveFile>

#include <cmath>
#include <holonight_shell_config/config_parsers.h>
#include <limits>
#include <optional>

namespace HoloNight::ShellConfig {

Q_LOGGING_CATEGORY(lcConfigParsers, "holonight.config")

std::optional<WidgetPosition> widgetPositionFromString(const QString& value) {
  static const QHash<QString, WidgetPosition> kMap{
      {QStringLiteral("left-top"), WidgetPosition::LeftTop},
      {QStringLiteral("center-top"), WidgetPosition::CenterTop},
      {QStringLiteral("right-top"), WidgetPosition::RightTop},
      {QStringLiteral("left-center"), WidgetPosition::LeftCenter},
      {QStringLiteral("center-center"), WidgetPosition::CenterCenter},
      {QStringLiteral("right-center"), WidgetPosition::RightCenter},
      {QStringLiteral("left-bottom"), WidgetPosition::LeftBottom},
      {QStringLiteral("center-bottom"), WidgetPosition::CenterBottom},
      {QStringLiteral("right-bottom"), WidgetPosition::RightBottom},
  };
  const auto found = kMap.constFind(value);
  if (found == kMap.constEnd()) {
    return std::nullopt;
  }
  return *found;
}

QString widgetPositionToString(WidgetPosition position) {
  switch (position) {
    case WidgetPosition::LeftTop:
      return QStringLiteral("left-top");
    case WidgetPosition::CenterTop:
      return QStringLiteral("center-top");
    case WidgetPosition::RightTop:
      return QStringLiteral("right-top");
    case WidgetPosition::LeftCenter:
      return QStringLiteral("left-center");
    case WidgetPosition::CenterCenter:
      return QStringLiteral("center-center");
    case WidgetPosition::RightCenter:
      return QStringLiteral("right-center");
    case WidgetPosition::LeftBottom:
      return QStringLiteral("left-bottom");
    case WidgetPosition::CenterBottom:
      return QStringLiteral("center-bottom");
    case WidgetPosition::RightBottom:
      return QStringLiteral("right-bottom");
  }
  return QStringLiteral("center-center");
}

bool widgetPositionIsTopAnchored(WidgetPosition position) {
  return position == WidgetPosition::LeftTop || position == WidgetPosition::CenterTop ||
         position == WidgetPosition::RightTop;
}

QString BackgroundConfig::imageForMonitor(const QStringList& images, int monitor_index) {
  if (images.isEmpty()) {
    return {};
  }
  if (monitor_index >= images.size()) {
    return images.last();
  }
  return images.at(monitor_index);
}

bool MissingDefaults::any() const {
  return workspace_count || tray_max_items || background_images || weather_api_key || weather_geo_api_key ||
         weather_units || weather_lang || weather_refresh_interval || notif_default_timeout || notif_max_visible ||
         notif_history_enabled || notif_history_max_items || notif_history_max_age_days || notif_history_persist_body;
}

namespace {

QString expandTilde(const QString& path) {
  if (path == QLatin1String("~")) {
    return QDir::homePath();
  }
  if (path.startsWith(QLatin1String("~/"))) {
    return QDir::homePath() + path.mid(1);
  }
  return path;
}

int readInt(toml::node_view<const toml::node> node, int def_val, const char* key_name, bool& missing) {
  if (!node) {
    missing = true;
    return def_val;
  }
  auto opt = node.value<int64_t>();
  if (!opt) {
    qCWarning(lcConfigParsers) << "Config:" << key_name << "integer expected but got different type; using default"
                               << def_val;
    return def_val;
  }
  const int64_t clamped_value = std::clamp(*opt, static_cast<int64_t>((std::numeric_limits<int>::min)()),
                                           static_cast<int64_t>((std::numeric_limits<int>::max)()));
  return static_cast<int>(clamped_value);
}

int readPositiveInt(toml::node_view<const toml::node> node, int def_val, const char* key_name, bool& missing) {
  const int value = readInt(node, def_val, key_name, missing);
  if (!missing && value <= 0) {
    qCWarning(lcConfigParsers) << "Config:" << key_name << value << "is not positive; using default" << def_val;
    return def_val;
  }
  return value;
}

QString readStr(toml::node_view<const toml::node> node, const QString& def_val, const char* key_name, bool& missing) {
  if (!node) {
    missing = true;
    return def_val;
  }
  auto opt = node.value<std::string>();
  if (!opt) {
    qCWarning(lcConfigParsers) << "Config:" << key_name << "string expected but got different type; using default"
                               << def_val;
    return def_val;
  }
  return QString::fromStdString(*opt);
}

bool readBool(toml::node_view<const toml::node> node, bool def_val, const char* key_name, bool& missing) {
  if (!node) {
    missing = true;
    return def_val;
  }
  auto opt = node.value<bool>();
  if (!opt) {
    qCWarning(lcConfigParsers) << "Config:" << key_name << "boolean expected but got different type; using default"
                               << def_val;
    return def_val;
  }
  return *opt;
}

int clampRange(int val, int min_val, int max_val, const char* key_name) {
  if (val < min_val) {
    qCWarning(lcConfigParsers) << "Config:" << key_name << val << "is below minimum" << min_val << "— clamping";
    return min_val;
  }
  if (val > max_val) {
    qCWarning(lcConfigParsers) << "Config:" << key_name << val << "exceeds maximum" << max_val << "— clamping";
    return max_val;
  }
  return val;
}

std::optional<double> readOptionalCoordinate(toml::node_view<const toml::node> node, double min_value, double max_value,
                                             const char* key_name) {
  if (!node) {
    return std::nullopt;
  }
  const auto value = node.value<double>();
  if (!value) {
    qCWarning(lcConfigParsers) << "Config:" << key_name << "must be a floating-point number; ignoring";
    return std::nullopt;
  }
  if (!std::isfinite(*value) || *value < min_value || *value > max_value) {
    qCWarning(lcConfigParsers) << "Config:" << key_name << *value << "is outside the range" << min_value << "to"
                               << max_value << "; ignoring";
    return std::nullopt;
  }
  return *value;
}

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
BarWorkspacesConfig parseBarWorkspaces(const toml::table& tbl, MissingDefaults& missing) {
  BarWorkspacesConfig cfg;
  cfg.count = readInt(tbl["bar"]["workspaces"]["count"], cfg.count, "bar.workspaces.count", missing.workspace_count);
  cfg.count =
      clampRange(cfg.count, BarWorkspacesConfig::kMinCount, BarWorkspacesConfig::kMaxCount, "bar.workspaces.count");
  return cfg;
}

BarSystemTrayConfig parseBarSystemTray(const toml::table& tbl, MissingDefaults& missing) {
  BarSystemTrayConfig cfg;
  cfg.max_items =
      readInt(tbl["bar"]["systemtray"]["max_items"], cfg.max_items, "bar.systemtray.max_items", missing.tray_max_items);
  cfg.max_items = clampRange(cfg.max_items, BarSystemTrayConfig::kMinMaxItems, BarSystemTrayConfig::kMaxMaxItems,
                             "bar.systemtray.max_items");
  return cfg;
}

TrayIconOverridesConfig parseTrayIconOverrides(const toml::table& tbl) {
  TrayIconOverridesConfig cfg;
  const auto* overrides = tbl["tray"]["icon_overrides"].as_table();
  if (overrides == nullptr) {
    return cfg;
  }

  for (const auto& [key, node] : *overrides) {
    const auto* entry = node.as_table();
    if (entry == nullptr) {
      qCWarning(lcConfigParsers) << "Config: tray.icon_overrides." << key.str().data()
                                 << "table expected but got different type; skipping";
      continue;
    }

    TrayIconOverrideConfig override;
    override.name = QString::fromStdString(std::string{key.str()});

    bool missing = false;
    override.id = readStr((*entry)["id"], override.id,
                          qPrintable(QStringLiteral("tray.icon_overrides.%1.id").arg(override.name)), missing);
    missing = false;
    override.service =
        readStr((*entry)["service"], override.service,
                qPrintable(QStringLiteral("tray.icon_overrides.%1.service").arg(override.name)), missing);
    missing = false;
    override.object_path =
        readStr((*entry)["object_path"], override.object_path,
                qPrintable(QStringLiteral("tray.icon_overrides.%1.object_path").arg(override.name)), missing);
    missing = false;
    override.title = readStr((*entry)["title"], override.title,
                             qPrintable(QStringLiteral("tray.icon_overrides.%1.title").arg(override.name)), missing);
    missing = false;
    override.icon = readStr((*entry)["icon"], override.icon,
                            qPrintable(QStringLiteral("tray.icon_overrides.%1.icon").arg(override.name)), missing);
    missing = false;
    override.attention_icon =
        readStr((*entry)["attention_icon"], override.attention_icon,
                qPrintable(QStringLiteral("tray.icon_overrides.%1.attention_icon").arg(override.name)), missing);

    if (override.icon.isEmpty()) {
      qCWarning(lcConfigParsers) << "Config: tray.icon_overrides." << override.name
                                 << "requires a non-empty icon; skipping";
      continue;
    }
    if (override.id.isEmpty() && override.service.isEmpty() && override.object_path.isEmpty() &&
        override.title.isEmpty()) {
      qCWarning(lcConfigParsers) << "Config: tray.icon_overrides." << override.name
                                 << "requires id, service, object_path, or title matcher; skipping";
      continue;
    }
    cfg.items.append(override);
  }
  return cfg;
}

BackgroundConfig parseBackground(const toml::table& tbl, MissingDefaults& missing) {
  BackgroundConfig cfg;
  const auto node = tbl["background"]["images"];
  if (!node) {
    missing.background_images = true;
    return cfg;
  }
  const auto* arr = node.as_array();
  if (arr == nullptr) {
    qCWarning(lcConfigParsers) << "Config: background.images array expected but got different type; ignoring";
    return cfg;
  }
  for (const auto& elem : *arr) {
    const auto opt = elem.value<std::string>();
    if (!opt) {
      qCWarning(lcConfigParsers) << "Config: background.images entry is not a string; skipping";
      continue;
    }
    cfg.images << expandTilde(QString::fromStdString(*opt));
  }
  return cfg;
}

WeatherConfig parseWeather(const toml::table& tbl, MissingDefaults& missing) {
  WeatherConfig cfg;
  const auto sec = tbl["weather"];

  bool unused_missing = false;
  cfg.provider = readStr(sec["provider"], cfg.provider, "weather.provider", unused_missing);
  cfg.location_source = readStr(sec["location_source"], cfg.location_source, "weather.location_source", unused_missing);
  cfg.api_key = readStr(sec["api_key"], cfg.api_key, "weather.api_key", missing.weather_api_key);
  cfg.geo_api_key = readStr(sec["geo_api_key"], cfg.geo_api_key, "weather.geo_api_key", missing.weather_geo_api_key);
  cfg.units = readStr(sec["units"], cfg.units, "weather.units", missing.weather_units);
  cfg.temp_unit = readStr(sec["temp_unit"], cfg.temp_unit, "weather.temp_unit", unused_missing);
  cfg.wind_unit = readStr(sec["wind_unit"], cfg.wind_unit, "weather.wind_unit", unused_missing);
  cfg.pressure_unit = readStr(sec["pressure_unit"], cfg.pressure_unit, "weather.pressure_unit", unused_missing);
  cfg.lang = readStr(sec["lang"], cfg.lang, "weather.lang", missing.weather_lang);
  cfg.show_in_bar = readBool(sec["show_in_bar"], cfg.show_in_bar, "weather.show_in_bar", unused_missing);
  cfg.compact_mode = readBool(sec["compact_mode"], cfg.compact_mode, "weather.compact_mode", unused_missing);
  cfg.show_feels_like =
      readBool(sec["show_feels_like"], cfg.show_feels_like, "weather.show_feels_like", unused_missing);
  cfg.show_location = readBool(sec["show_location"], cfg.show_location, "weather.show_location", unused_missing);
  cfg.refresh_interval = readPositiveInt(sec["refresh_interval"], cfg.refresh_interval, "weather.refresh_interval",
                                         missing.weather_refresh_interval);

  // latitude/longitude are optional with no missing sentinel: their absence is meaningful
  // (it triggers IP-geolocation fallback), so they are never written back as defaults.
  cfg.latitude = readOptionalCoordinate(sec["latitude"], -90.0, 90.0, "weather.latitude");
  cfg.longitude = readOptionalCoordinate(sec["longitude"], -180.0, 180.0, "weather.longitude");

  // Display metadata has no missing-default write-back.
  bool location_missing_ignored = false;
  cfg.city = readStr(sec["city"], cfg.city, "weather.city", location_missing_ignored);
  cfg.country = readStr(sec["country"], cfg.country, "weather.country", location_missing_ignored);

  return cfg;
}

NotificationsConfig parseNotifications(const toml::table& tbl, MissingDefaults& missing) {
  NotificationsConfig cfg;
  const auto sec = tbl["notifications"];
  cfg.default_timeout_ms = readPositiveInt(sec["default_timeout_ms"], cfg.default_timeout_ms,
                                           "notifications.default_timeout_ms", missing.notif_default_timeout);
  cfg.max_visible =
      readInt(sec["max_visible"], cfg.max_visible, "notifications.max_visible", missing.notif_max_visible);
  cfg.max_visible = clampRange(cfg.max_visible, NotificationsConfig::kMinVisible, NotificationsConfig::kMaxVisible,
                               "notifications.max_visible");
  return cfg;
}

NotificationHistoryConfig parseNotificationHistory(const toml::table& tbl, MissingDefaults& missing) {
  NotificationHistoryConfig cfg;
  const auto sec = tbl["notifications"]["history"];

  if (!sec["enabled"]) {
    missing.notif_history_enabled = true;
  } else if (const auto val = sec["enabled"].value<bool>()) {
    cfg.enabled = *val;
  } else {
    qCWarning(lcConfigParsers) << "Config: notifications.history.enabled bool expected; using default" << cfg.enabled;
  }

  cfg.max_items = readPositiveInt(sec["max_items"], cfg.max_items, "notifications.history.max_items",
                                  missing.notif_history_max_items);
  cfg.max_age_days = readPositiveInt(sec["max_age_days"], cfg.max_age_days, "notifications.history.max_age_days",
                                     missing.notif_history_max_age_days);

  if (!sec["persist_body"]) {
    missing.notif_history_persist_body = true;
  } else if (const auto val = sec["persist_body"].value<bool>()) {
    cfg.persist_body = *val;
  } else {
    qCWarning(lcConfigParsers) << "Config: notifications.history.persist_body bool expected; using default"
                               << cfg.persist_body;
  }

  return cfg;
}

bool parseTimeToEventDeadline(const toml::table& entry, bool strict, TimeToEventConfig& tte) {
  const auto deadline_opt = entry["deadline"].value<std::string>();
  if (!deadline_opt) {
    if (strict) {
      qCWarning(lcConfigParsers) << "Config: time-to-event widget" << tte.title << "requires a deadline; skipping";
      return false;
    }
    return true;
  }

  const QString deadline_str = QString::fromStdString(*deadline_opt);
  tte.has_time = deadline_str.contains(QLatin1Char('T'));
  if (tte.has_time) {
    tte.deadline = QDateTime::fromString(deadline_str, Qt::ISODate);
  } else if (const QDate date = QDate::fromString(deadline_str, Qt::ISODate); date.isValid()) {
    tte.deadline = date.startOfDay();
  }
  if (!tte.deadline.isValid() && strict) {
    qCWarning(lcConfigParsers) << "Config: time-to-event widget" << tte.title << "has unparseable deadline"
                               << deadline_str << "— skipping";
    return false;
  }
  return true;
}

// Parses the time-to-event-specific fields (title, deadline, show_seconds). Enabled entries return
// nullopt with one warning logged if the required title or deadline is missing/unparseable; disabled
// entries retain invalid fields for Settings-app round trips.
std::optional<TimeToEventConfig> parseTimeToEventFields(const toml::table& entry, bool strict) {
  TimeToEventConfig tte;

  const auto title_opt = entry["title"].value<std::string>();
  tte.title = title_opt ? QString::fromStdString(*title_opt) : QString{};
  if (tte.title.isEmpty() && strict) {
    qCWarning(lcConfigParsers) << "Config: time-to-event widget requires a non-empty title; skipping";
    return std::nullopt;
  }

  if (!parseTimeToEventDeadline(entry, strict, tte)) {
    return std::nullopt;
  }

  tte.show_seconds = entry["show_seconds"].value<bool>().value_or(false);
  return tte;
}

// Parses the clock-specific fields (show_seconds, date_format, locale). All are optional, so this
// never fails — invalid date_format/locale values are tolerated here and fall back at format time.
ClockConfig parseClockFields(const toml::table& entry) {
  ClockConfig clock;
  clock.show_seconds = entry["show_seconds"].value<bool>().value_or(true);
  if (const auto fmt = entry["date_format"].value<std::string>()) {
    clock.date_format = QString::fromStdString(*fmt);
  }
  if (const auto loc = entry["locale"].value<std::string>()) {
    clock.locale = QString::fromStdString(*loc);
  }
  return clock;
}

MprisWidgetConfig parseMprisFields(const toml::table& entry) {
  MprisWidgetConfig mpris;
  mpris.pause_hide_minutes = entry["pause_hide_minutes"].value<int>().value_or(mpris.pause_hide_minutes);
  mpris.pause_hide_minutes = clampRange(mpris.pause_hide_minutes, MprisWidgetConfig::kMinPauseHideMinutes,
                                        MprisWidgetConfig::kMaxPauseHideMinutes, "widget.mpris.pause_hide_minutes");
  return mpris;
}

// Resolves the shared `position` field. Returns nullopt (with one warning) only when an explicit
// position string is present but invalid; an absent field yields the CenterCenter default.
std::optional<WidgetPosition> parseWidgetPositionField(const toml::table& entry, const QString& label, bool strict) {
  const auto pos_opt = entry["position"].value<std::string>();
  if (!pos_opt) {
    return WidgetPosition::CenterCenter;
  }
  const QString pos_str = QString::fromStdString(*pos_opt);
  const auto position = widgetPositionFromString(pos_str);
  if (!position) {
    qCWarning(lcConfigParsers) << "Config: widget" << label << "has invalid position" << pos_str << "— skipping";
    if (strict) {
      return std::nullopt;
    }
    return WidgetPosition::CenterCenter;
  }
  return position;
}

// Reads the optional `monitors` array of output names; non-string entries are skipped with a warning.
QStringList parseWidgetMonitors(const toml::table& entry, const QString& label) {
  QStringList monitors;
  if (const auto* arr = entry["monitors"].as_array()) {
    for (const auto& elem : *arr) {
      if (const auto name = elem.value<std::string>()) {
        monitors << QString::fromStdString(*name);
      } else {
        qCWarning(lcConfigParsers) << "Config: widget" << label << "monitors entry is not a string; skipping entry";
      }
    }
  }
  return monitors;
}

// Parses one [[widget]] table entry into a fully-populated WidgetDefinition, or nullopt if the entry
// is invalid (unknown type, missing/empty title, missing/unparseable deadline, invalid position) —
// each rejection path logs exactly one warning so the widget is silently skipped by the caller.
std::optional<WidgetDefinition> parseWidgetEntry(const toml::table& entry) {
  const auto type_opt = entry["type"].value<std::string>();
  const QString type = type_opt ? QString::fromStdString(*type_opt) : QString{};
  // `enabled` is generic across every widget type, so read it before the type switch. It also
  // controls parse strictness: a disabled entry's fields are still parsed (so they survive a
  // Settings-app round-trip save) but never reject the whole entry on an individually invalid field.
  const bool enabled = entry["enabled"].value<bool>().value_or(true);

  if (type == QLatin1String("time-to-event")) {
    auto tte = parseTimeToEventFields(entry, /*strict=*/enabled);
    if (!tte) {  // only reachable when enabled == true
      return std::nullopt;
    }
    const auto position = parseWidgetPositionField(entry, tte->title, /*strict=*/enabled);
    if (!position) {  // only reachable when enabled == true
      return std::nullopt;
    }

    WidgetDefinition def;
    def.type = WidgetType::TimeToEvent;
    def.enabled = enabled;
    def.time_to_event = *tte;
    def.position = *position;
    def.monitors = parseWidgetMonitors(entry, tte->title);
    return def;
  }

  if (type == QLatin1String("clock")) {
    const QString label = QStringLiteral("clock");
    const auto position = parseWidgetPositionField(entry, label, /*strict=*/enabled);
    if (!position) {  // only reachable when enabled == true
      return std::nullopt;
    }

    WidgetDefinition def;
    def.type = WidgetType::Clock;
    def.enabled = enabled;
    def.clock = parseClockFields(entry);
    def.position = *position;
    def.monitors = parseWidgetMonitors(entry, label);
    return def;
  }

  if (type == QLatin1String("mpris")) {
    const QString label = QStringLiteral("mpris");
    const auto position = parseWidgetPositionField(entry, label, /*strict=*/enabled);
    if (!position) {  // only reachable when enabled == true
      return std::nullopt;
    }

    WidgetDefinition def;
    def.type = WidgetType::Mpris;
    def.enabled = enabled;
    def.mpris = parseMprisFields(entry);
    def.position = *position;
    def.monitors = parseWidgetMonitors(entry, label);
    return def;
  }

  // Unknown/missing type: preserved verbatim, including its pre-existing enabled/disabled
  // asymmetry — a disabled entry with an unrecognized type is never subject to the
  // "unknown type — skipping" rejection an enabled entry with the same type hits.
  if (!enabled) {
    WidgetDefinition def;
    def.enabled = false;
    return def;
  }

  qCWarning(lcConfigParsers) << "Config: [[widget]] has unknown or missing type" << type << "— skipping";
  return std::nullopt;
}

QStringList parseStringArray(toml::node_view<const toml::node> node, const char* key_name) {
  QStringList result;
  const auto* arr = node.as_array();
  if (arr == nullptr) {
    return result;
  }
  for (const auto& elem : *arr) {
    if (const auto val = elem.value<std::string>()) {
      result << QString::fromStdString(*val);
    } else {
      qCWarning(lcConfigParsers) << "Config:" << key_name << "entry is not a string; skipping";
    }
  }
  return result;
}

QList<CalendarCaldavAccountConfig> parseCalendarCaldavAccounts(toml::node_view<const toml::node> sec) {
  QList<CalendarCaldavAccountConfig> accounts;
  const auto* caldav_tbl = sec["caldav"].as_table();
  if (caldav_tbl == nullptr) {
    return accounts;
  }
  for (const auto& [key, node] : *caldav_tbl) {
    const auto* entry = node.as_table();
    if (entry == nullptr) {
      qCWarning(lcConfigParsers) << "Config: calendar.caldav." << key.str().data()
                                 << "table expected but got different type; skipping";
      continue;
    }
    CalendarCaldavAccountConfig account;
    account.account_name = QString::fromStdString(std::string{key.str()});
    if (const auto val = (*entry)["url"].value<std::string>()) {
      account.url = QString::fromStdString(*val);
    }
    if (const auto val = (*entry)["username"].value<std::string>()) {
      account.username = QString::fromStdString(*val);
    }
    if (const auto val = (*entry)["password_keyring_key"].value<std::string>()) {
      account.password_keyring_key = QString::fromStdString(*val);
    }
    account.include = parseStringArray(
        (*entry)["include"], qPrintable(QStringLiteral("calendar.caldav.%1.include").arg(account.account_name)));
    account.exclude = parseStringArray(
        (*entry)["exclude"], qPrintable(QStringLiteral("calendar.caldav.%1.exclude").arg(account.account_name)));

    if (account.url.isEmpty()) {
      qCWarning(lcConfigParsers) << "Config: calendar.caldav." << account.account_name << "requires url; skipping";
      continue;
    }
    if (account.username.isEmpty()) {
      qCWarning(lcConfigParsers) << "Config: calendar.caldav." << account.account_name << "requires username; skipping";
      continue;
    }
    accounts.append(account);
  }
  return accounts;
}

QList<CalendarIcsAccountConfig> parseCalendarIcsAccounts(toml::node_view<const toml::node> sec) {
  QList<CalendarIcsAccountConfig> accounts;
  const auto* ics_tbl = sec["ics"].as_table();
  if (ics_tbl == nullptr) {
    return accounts;
  }
  for (const auto& [key, node] : *ics_tbl) {
    const auto* entry = node.as_table();
    if (entry == nullptr) {
      qCWarning(lcConfigParsers) << "Config: calendar.ics." << key.str().data()
                                 << "table expected but got different type; skipping";
      continue;
    }
    CalendarIcsAccountConfig account;
    account.account_name = QString::fromStdString(std::string{key.str()});
    if (const auto val = (*entry)["url"].value<std::string>()) {
      account.url = QString::fromStdString(*val);
    }
    if (const auto val = (*entry)["label"].value<std::string>()) {
      account.label = QString::fromStdString(*val);
    }
    if (account.url.isEmpty()) {
      qCWarning(lcConfigParsers) << "Config: calendar.ics." << account.account_name << "requires url; skipping";
      continue;
    }
    accounts.append(account);
  }
  return accounts;
}

CalendarConfig parseCalendar(const toml::table& tbl) {
  CalendarConfig cfg;
  const auto sec = tbl["calendar"];

  const auto day_str = sec["week_start_day"].value_or(std::string("Mon"));
  cfg.week_start_day = (day_str == "Sun") ? WeekStartDay::Sunday : WeekStartDay::Monday;
  cfg.caldav_accounts = parseCalendarCaldavAccounts(sec);
  cfg.ics_accounts = parseCalendarIcsAccounts(sec);
  return cfg;
}

LogoConfig parseLogo(const toml::table& tbl) {
  LogoConfig cfg;
  const auto sec = tbl["logo"];

  bool file_missing_ignored = false;
  cfg.file = readStr(sec["file"], cfg.file, "logo.file", file_missing_ignored);
  if (!cfg.file.isEmpty()) {
    cfg.file = expandTilde(cfg.file);
  }

  cfg.generic = sec["generic"].value<bool>().value_or(false);
  return cfg;
}

OsdConfig parseOsd(const toml::table& tbl) {
  OsdConfig cfg;
  const auto sec = tbl["osd"];

  cfg.enabled = sec["enabled"].value<bool>().value_or(cfg.enabled);

  bool timeout_missing_ignored = false;
  const int timeout = readInt(sec["timeout"], cfg.timeout_ms, "osd.timeout", timeout_missing_ignored);
  cfg.timeout_ms = clampRange(timeout, OsdConfig::kMinTimeoutMs, OsdConfig::kMaxTimeoutMs, "osd.timeout");

  if (const auto pos_opt = sec["position"].value<std::string>()) {
    const QString pos_str = QString::fromStdString(*pos_opt);
    if (const auto position = widgetPositionFromString(pos_str)) {
      cfg.position = *position;
    } else {
      qCWarning(lcConfigParsers) << "Config: osd.position" << pos_str << "is invalid; using default"
                                 << widgetPositionToString(cfg.position);
    }
  }

  cfg.volume.enabled = sec["volume"]["enabled"].value<bool>().value_or(cfg.volume.enabled);
  cfg.brightness.enabled = sec["brightness"]["enabled"].value<bool>().value_or(cfg.brightness.enabled);
  cfg.keyboard_layout.enabled = sec["keyboard_layout"]["enabled"].value<bool>().value_or(cfg.keyboard_layout.enabled);
  return cfg;
}

WidgetsConfig parseWidgets(const toml::table& tbl) {
  WidgetsConfig cfg;

  bool margin_missing = false;
  const int margin = readInt(tbl["widgets"]["margin"], cfg.margin, "widgets.margin", margin_missing);
  if (!margin_missing && margin < 0) {
    qCWarning(lcConfigParsers) << "Config: widgets.margin" << margin << "is negative; using default" << cfg.margin;
  } else {
    cfg.margin = margin;
  }

  if (const auto* arr = tbl["widget"].as_array()) {
    for (const auto& node : *arr) {
      const auto* entry = node.as_table();
      if (entry == nullptr) {
        qCWarning(lcConfigParsers) << "Config: [[widget]] entry is not a table; skipping";
        continue;
      }
      if (auto def = parseWidgetEntry(*entry)) {
        cfg.definitions.append(*def);
      }
    }
  }

  return cfg;
}
// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

QStringList weatherDefaultLines(const MissingDefaults& missing) {
  const WeatherConfig weather;
  QStringList lines;
  if (missing.weather_api_key) {
    lines << QStringLiteral("api_key = \"\" # openweathermap.org key; empty = weather widget disabled");
  }
  if (missing.weather_geo_api_key) {
    lines << QStringLiteral("geo_api_key = \"\" # ipgeolocation.io key; used only when latitude/longitude are unset");
  }
  if (missing.weather_units) {
    lines << QStringLiteral("units = %1 # metric | imperial | standard").arg(tomlQuote(weather.units));
  }
  if (missing.weather_lang) {
    lines << QStringLiteral("lang = %1").arg(tomlQuote(weather.lang));
  }
  if (missing.weather_refresh_interval) {
    lines << QStringLiteral("refresh_interval = %1 # seconds; must be > 0").arg(weather.refresh_interval);
  }
  // latitude/longitude intentionally omitted — their absence enables IP geolocation.
  return lines;
}

QStringList notificationsDefaultLines(const MissingDefaults& missing) {
  const NotificationsConfig notifications;
  QStringList lines;
  if (missing.notif_default_timeout) {
    lines << QStringLiteral("default_timeout_ms = %1 # auto-dismiss delay for normal/low urgency; must be > 0")
                 .arg(notifications.default_timeout_ms);
  }
  if (missing.notif_max_visible) {
    lines << QStringLiteral("max_visible = %1 # concurrent on-screen toasts per monitor; accepted: %2-%3")
                 .arg(notifications.max_visible)
                 .arg(NotificationsConfig::kMinVisible)
                 .arg(NotificationsConfig::kMaxVisible);
  }
  return lines;
}

QStringList notificationHistoryDefaultLines(const MissingDefaults& missing) {
  const NotificationHistoryConfig cfg;
  QStringList lines;
  if (missing.notif_history_enabled) {
    lines << QStringLiteral("enabled = %1").arg(cfg.enabled ? QStringLiteral("true") : QStringLiteral("false"));
  }
  if (missing.notif_history_max_items) {
    lines << QStringLiteral("max_items = %1").arg(cfg.max_items);
  }
  if (missing.notif_history_max_age_days) {
    lines << QStringLiteral("max_age_days = %1").arg(cfg.max_age_days);
  }
  if (missing.notif_history_persist_body) {
    lines << QStringLiteral("persist_body = %1 # set false to skip storing notification body for privacy")
                 .arg(cfg.persist_body ? QStringLiteral("true") : QStringLiteral("false"));
  }
  return lines;
}

QStringList defaultLinesForSection(const QString& section, const MissingDefaults& missing) {
  const BarWorkspacesConfig workspaces;
  const BarSystemTrayConfig tray;
  QStringList lines;

  if (section == QLatin1String("weather")) {
    lines = weatherDefaultLines(missing);
  } else if (section == QLatin1String("notifications")) {
    lines = notificationsDefaultLines(missing);
  } else if (section == QLatin1String("notifications.history")) {
    lines = notificationHistoryDefaultLines(missing);
  } else if (section == QLatin1String("bar.workspaces") && missing.workspace_count) {
    lines << QStringLiteral("count = %1 # accepted: %2-%3")
                 .arg(workspaces.count)
                 .arg(BarWorkspacesConfig::kMinCount)
                 .arg(BarWorkspacesConfig::kMaxCount);
  } else if (section == QLatin1String("bar.systemtray") && missing.tray_max_items) {
    lines << QStringLiteral("max_items = %1 # accepted: %2-%3")
                 .arg(tray.max_items)
                 .arg(BarSystemTrayConfig::kMinMaxItems)
                 .arg(BarSystemTrayConfig::kMaxMaxItems);
  } else if (section == QLatin1String("background") && missing.background_images) {
    lines << QStringLiteral("images = [] # per-monitor wallpapers, in monitor order; empty = solid color");
  }

  return lines;
}

void insertMissingSectionDefaults(QStringList& lines, const QString& section, const QStringList& missing_lines) {
  if (missing_lines.isEmpty()) {
    return;
  }

  const QString header = QStringLiteral("[%1]").arg(section);
  int section_start = -1;
  for (int i = 0; i < lines.size(); ++i) {
    if (lines.at(i).trimmed() == header) {
      section_start = i;
      break;
    }
  }

  if (section_start == -1) {
    if (!lines.isEmpty() && !lines.last().trimmed().isEmpty()) {
      lines << QString{};
    }
    lines << header;
    lines << missing_lines;
    return;
  }

  qsizetype insert_at = lines.size();
  for (qsizetype i = section_start + 1; i < lines.size(); ++i) {
    const QString trimmed = lines.at(i).trimmed();
    if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
      insert_at = i;
      break;
    }
  }

  for (qsizetype i = missing_lines.size() - 1; i >= 0; --i) {
    lines.insert(insert_at, missing_lines.at(i));
  }
}

}  // namespace

QString tomlQuote(const QString& value) {
  QString escaped = value;
  escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
  escaped.replace(QLatin1Char('"'), QLatin1String("\\\""));
  return QLatin1Char('"') + escaped + QLatin1Char('"');
}

ProductConfig parseConfigTable(const toml::table& table, MissingDefaults& missing) {
  ProductConfig parsed;
  parsed.bar_workspaces = parseBarWorkspaces(table, missing);
  parsed.bar_system_tray = parseBarSystemTray(table, missing);
  parsed.tray_icon_overrides = parseTrayIconOverrides(table);
  parsed.background = parseBackground(table, missing);
  parsed.weather = parseWeather(table, missing);
  parsed.notifications = parseNotifications(table, missing);
  parsed.notification_history = parseNotificationHistory(table, missing);
  parsed.widgets = parseWidgets(table);
  parsed.calendar = parseCalendar(table);
  parsed.logo = parseLogo(table);
  parsed.osd = parseOsd(table);
  return parsed;
}

bool writeMissingDefaults(const QString& path, const MissingDefaults& missing) {
  if (!missing.any()) {
    return true;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qCWarning(lcConfigParsers) << "Failed to open config for missing default write-back:" << path;
    return false;
  }

  QString content = QString::fromUtf8(file.readAll());
  QStringList lines = content.split(QLatin1Char('\n'));
  if (!lines.isEmpty() && lines.last().isEmpty()) {
    lines.removeLast();
  }

  insertMissingSectionDefaults(lines, QStringLiteral("bar.workspaces"),
                               defaultLinesForSection(QStringLiteral("bar.workspaces"), missing));
  insertMissingSectionDefaults(lines, QStringLiteral("bar.systemtray"),
                               defaultLinesForSection(QStringLiteral("bar.systemtray"), missing));
  insertMissingSectionDefaults(lines, QStringLiteral("background"),
                               defaultLinesForSection(QStringLiteral("background"), missing));
  insertMissingSectionDefaults(lines, QStringLiteral("weather"),
                               defaultLinesForSection(QStringLiteral("weather"), missing));
  insertMissingSectionDefaults(lines, QStringLiteral("notifications"),
                               defaultLinesForSection(QStringLiteral("notifications"), missing));
  insertMissingSectionDefaults(lines, QStringLiteral("notifications.history"),
                               defaultLinesForSection(QStringLiteral("notifications.history"), missing));

  QSaveFile out(path);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qCWarning(lcConfigParsers) << "Failed to write missing config defaults:" << path;
    return false;
  }
  out.write(lines.join(QLatin1Char('\n')).toUtf8());
  out.write("\n");
  if (!out.commit()) {
    qCWarning(lcConfigParsers) << "Failed to commit missing config defaults:" << path;
    return false;
  }
  return true;
}

}  // namespace HoloNight::ShellConfig
