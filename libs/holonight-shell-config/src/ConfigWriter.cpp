#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QTextStream>

#include <holonight_shell_config/config_writer.h>

namespace HoloNight::ShellConfig {

Q_LOGGING_CATEGORY(lcProductConfigWriter, "holonight.config.writer")

namespace {

void writeStringArray(QTextStream& out, const QStringList& items) {
  out << "[";
  for (int idx = 0; idx < items.size(); ++idx) {
    if (idx > 0) {
      out << ", ";
    }
    out << tomlQuote(items.at(idx));
  }
  out << "]";
}

void writeTrayOverrides(QTextStream& out, const TrayIconOverridesConfig& cfg) {
  out << "[tray.icon_overrides]\n";
  out << "# Example: override a tray item that publishes only D-Bus pixmaps.\n";
  out << "# [tray.icon_overrides.slack]\n";
  out << "# id = \"Slack_status_icon_1\"\n";
  out << "# icon = \"slack-indicator\"\n";
  out << "# attention_icon = \"slack-indicator\"\n";
  for (const auto& item : cfg.items) {
    out << "\n[tray.icon_overrides." << item.name << "]\n";
    if (!item.id.isEmpty()) {
      out << "id = " << tomlQuote(item.id) << "\n";
    }
    if (!item.service.isEmpty()) {
      out << "service = " << tomlQuote(item.service) << "\n";
    }
    if (!item.object_path.isEmpty()) {
      out << "object_path = " << tomlQuote(item.object_path) << "\n";
    }
    if (!item.title.isEmpty()) {
      out << "title = " << tomlQuote(item.title) << "\n";
    }
    out << "icon = " << tomlQuote(item.icon) << "\n";
    if (!item.attention_icon.isEmpty()) {
      out << "attention_icon = " << tomlQuote(item.attention_icon) << "\n";
    }
  }
  out << "\n";
}

void writeWidgetDefinition(QTextStream& out, const WidgetDefinition& def) {
  out << "[[widget]]\n";
  switch (def.type) {
    case WidgetType::TimeToEvent:
      out << "type = \"time-to-event\"\n";
      break;
    case WidgetType::Clock:
      out << "type = \"clock\"\n";
      break;
    case WidgetType::Mpris:
      out << "type = \"mpris\"\n";
      break;
  }
  out << "enabled = " << (def.enabled ? "true" : "false") << "\n";
  out << "position = " << tomlQuote(widgetPositionToString(def.position)) << "\n";
  if (!def.monitors.isEmpty()) {
    out << "monitors = ";
    writeStringArray(out, def.monitors);
    out << "\n";
  }
  if (def.type == WidgetType::TimeToEvent) {
    out << "title = " << tomlQuote(def.time_to_event.title) << "\n";
    const QString deadline_str = def.time_to_event.has_time ? def.time_to_event.deadline.toString(Qt::ISODate)
                                                            : def.time_to_event.deadline.date().toString(Qt::ISODate);
    out << "deadline = " << tomlQuote(deadline_str) << "\n";
    out << "show_seconds = " << (def.time_to_event.show_seconds ? "true" : "false") << "\n";
  } else if (def.type == WidgetType::Clock) {
    out << "show_seconds = " << (def.clock.show_seconds ? "true" : "false") << "\n";
    if (!def.clock.date_format.isEmpty()) {
      out << "date_format = " << tomlQuote(def.clock.date_format) << "\n";
    }
    if (!def.clock.locale.isEmpty()) {
      out << "locale = " << tomlQuote(def.clock.locale) << "\n";
    }
  } else if (def.type == WidgetType::Mpris) {
    out << "pause_hide_minutes = " << def.mpris.pause_hide_minutes << "\n";
  }
  out << "\n";
}

void writeWidgets(QTextStream& out, const WidgetsConfig& cfg) {
  out << "[widgets]\n";
  out << "margin = " << cfg.margin << " # logical pixels, applied to all widget edges\n\n";
  for (const auto& def : cfg.definitions) {
    writeWidgetDefinition(out, def);
  }
}

void writeCalendar(QTextStream& out, const CalendarConfig& cfg) {
  out << "[calendar]\n";
  out << "week_start_day = " << tomlQuote(cfg.week_start_day == WeekStartDay::Sunday ? "Sun" : "Mon") << "\n\n";
  for (const auto& account : cfg.caldav_accounts) {
    out << "[calendar.caldav." << account.account_name << "]\n";
    out << "url = " << tomlQuote(account.url) << "\n";
    out << "username = " << tomlQuote(account.username) << "\n";
    out << "password_keyring_key = " << tomlQuote(account.password_keyring_key) << "\n";
    if (!account.include.isEmpty()) {
      out << "include = ";
      writeStringArray(out, account.include);
      out << "\n";
    }
    if (!account.exclude.isEmpty()) {
      out << "exclude = ";
      writeStringArray(out, account.exclude);
      out << "\n";
    }
    out << "\n";
  }
  for (const auto& account : cfg.ics_accounts) {
    out << "[calendar.ics." << account.account_name << "]\n";
    out << "url = " << tomlQuote(account.url) << "\n";
    if (!account.label.isEmpty()) {
      out << "label = " << tomlQuote(account.label) << "\n";
    }
    out << "\n";
  }
}

void writeOsd(QTextStream& out, const OsdConfig& cfg) {
  out << "[osd]\n";
  out << "enabled = " << (cfg.enabled ? "true" : "false") << " # master switch for the on-screen display\n";
  out << "timeout = " << cfg.timeout_ms
      << " # ms the OSD stays visible after the last update; accepted: " << OsdConfig::kMinTimeoutMs << "-"
      << OsdConfig::kMaxTimeoutMs << "\n";
  out << "position = " << tomlQuote(widgetPositionToString(cfg.position))
      << " # same nine anchors as desktop widgets\n\n";
  out << "[osd.volume]\n";
  out << "enabled = " << (cfg.volume.enabled ? "true" : "false") << "\n\n";
  out << "[osd.brightness]\n";
  out << "enabled = " << (cfg.brightness.enabled ? "true" : "false") << "\n\n";
  out << "[osd.keyboard_layout]\n";
  out << "enabled = " << (cfg.keyboard_layout.enabled ? "true" : "false") << "\n\n";
}

}  // namespace

bool ProductConfigWriter::write(const ProductConfig& config, const QString& path) {
  QDir dir = QFileInfo(path).absoluteDir();
  if (!dir.mkpath(QStringLiteral("."))) {
    qCWarning(lcProductConfigWriter) << "Failed to create config directory:" << dir.absolutePath();
    return false;
  }

  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    qCWarning(lcProductConfigWriter) << "Failed to open config for writing:" << path;
    return false;
  }

  QTextStream out(&file);
  out.setRealNumberPrecision(
      17);  // round-trip-safe for double (weather lat/lon); wide enough no int/string field cares

  out << "[bar.workspaces]\n";
  out << "count = " << config.bar_workspaces.count << " # accepted: " << BarWorkspacesConfig::kMinCount << "-"
      << BarWorkspacesConfig::kMaxCount << "\n\n";

  out << "[bar.systemtray]\n";
  out << "max_items = " << config.bar_system_tray.max_items << " # accepted: " << BarSystemTrayConfig::kMinMaxItems
      << "-" << BarSystemTrayConfig::kMaxMaxItems << "\n\n";

  writeTrayOverrides(out, config.tray_icon_overrides);

  out << "[background]\n";
  out << "images = ";
  writeStringArray(out, config.background.images);
  out << " # per-monitor wallpapers, in monitor order; empty = solid color\n\n";

  out << "[weather]\n";
  out << "provider = " << tomlQuote(config.weather.provider) << "\n";
  out << "location_source = " << tomlQuote(config.weather.location_source) << "\n";
  out << "api_key = " << tomlQuote(config.weather.api_key)
      << " # openweathermap.org key; empty = weather widget disabled\n";
  out << "geo_api_key = " << tomlQuote(config.weather.geo_api_key)
      << " # ipgeolocation.io key; used only when latitude/longitude are unset\n";
  if (config.weather.latitude) {
    out << "latitude = " << *config.weather.latitude << "\n";
  } else {
    out << "# latitude = 49.83968  # uncomment to pin location and skip IP geolocation\n";
  }
  if (config.weather.longitude) {
    out << "longitude = " << *config.weather.longitude << "\n";
  } else {
    out << "# longitude = 24.02972\n";
  }
  if (!config.weather.city.isEmpty()) {
    out << "city = " << tomlQuote(config.weather.city) << "\n";
  }
  if (!config.weather.country.isEmpty()) {
    out << "country = " << tomlQuote(config.weather.country) << "\n";
  }
  out << "units = " << tomlQuote(config.weather.units) << " # metric | imperial | standard\n";
  out << "temp_unit = " << tomlQuote(config.weather.temp_unit) << "\n";
  out << "wind_unit = " << tomlQuote(config.weather.wind_unit) << "\n";
  out << "pressure_unit = " << tomlQuote(config.weather.pressure_unit) << "\n";
  out << "lang = " << tomlQuote(config.weather.lang) << "\n";
  out << "show_in_bar = " << (config.weather.show_in_bar ? "true" : "false") << "\n";
  out << "compact_mode = " << (config.weather.compact_mode ? "true" : "false") << "\n";
  out << "show_feels_like = " << (config.weather.show_feels_like ? "true" : "false") << "\n";
  out << "show_location = " << (config.weather.show_location ? "true" : "false") << "\n";
  out << "refresh_interval = " << config.weather.refresh_interval << " # seconds; must be > 0\n\n";

  out << "[notifications]\n";
  out << "default_timeout_ms = " << config.notifications.default_timeout_ms
      << " # auto-dismiss delay for normal/low urgency; must be > 0\n";
  out << "max_visible = " << config.notifications.max_visible
      << " # concurrent on-screen toasts per monitor; accepted: " << NotificationsConfig::kMinVisible << "-"
      << NotificationsConfig::kMaxVisible << "\n\n";

  out << "[notifications.history]\n";
  out << "enabled = " << (config.notification_history.enabled ? "true" : "false") << "\n";
  out << "max_items = " << config.notification_history.max_items << "\n";
  out << "max_age_days = " << config.notification_history.max_age_days << "\n";
  out << "persist_body = " << (config.notification_history.persist_body ? "true" : "false")
      << " # set false to skip storing notification body for privacy\n\n";

  writeWidgets(out, config.widgets);
  writeCalendar(out, config.calendar);
  writeOsd(out, config.osd);

  if (!file.commit()) {
    qCWarning(lcProductConfigWriter) << "Failed to commit config:" << path;
    return false;
  }
  return true;
}

}  // namespace HoloNight::ShellConfig
