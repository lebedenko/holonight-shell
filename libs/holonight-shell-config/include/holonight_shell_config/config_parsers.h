#pragma once

#include <QLoggingCategory>
#include <QString>

#include <holonight_shell_config/config_structs.h>
#include <toml++/toml.h>

// Defined once in ConfigParsers.cpp. QLoggingCategory objects are resolved by their string name
// at the logging backend, so this forward declaration is sufficient for other library targets
// (e.g. holonight-services) to log through the same "holonight.config" category.
namespace HoloNight::ShellConfig {

Q_DECLARE_LOGGING_CATEGORY(lcConfigParsers)

struct MissingDefaults {
  bool workspace_count{false};
  bool tray_max_items{false};
  bool background_images{false};
  bool weather_api_key{false};
  bool weather_geo_api_key{false};
  bool weather_units{false};
  bool weather_lang{false};
  bool weather_refresh_interval{false};
  bool notif_default_timeout{false};
  bool notif_max_visible{false};
  bool notif_history_enabled{false};
  bool notif_history_max_items{false};
  bool notif_history_max_age_days{false};
  bool notif_history_persist_body{false};

  [[nodiscard]] bool any() const;
};

struct ProductConfig {
  BarWorkspacesConfig bar_workspaces;
  BarSystemTrayConfig bar_system_tray;
  TrayIconOverridesConfig tray_icon_overrides;
  BackgroundConfig background;
  WeatherConfig weather;
  NotificationsConfig notifications;
  NotificationHistoryConfig notification_history;
  WidgetsConfig widgets;
  CalendarConfig calendar;
  LogoConfig logo;
  OsdConfig osd;

  bool operator==(const ProductConfig&) const = default;
};

[[nodiscard]] QString tomlQuote(const QString& value);
[[nodiscard]] ProductConfig parseConfigTable(const toml::table& table, MissingDefaults& missing);
bool writeMissingDefaults(const QString& path, const MissingDefaults& missing);

}  // namespace HoloNight::ShellConfig
