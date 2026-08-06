#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

#include <holonight_config/config_parsers.h>
#include <holonight_config/config_structs.h>
#include <holonight_config/config_writer.h>

class ConfigService : public QObject {
  Q_OBJECT

 public:
  explicit ConfigService(QObject* parent = nullptr);
  ~ConfigService() override;

  ConfigService(const ConfigService&) = delete;
  ConfigService& operator=(const ConfigService&) = delete;
  ConfigService(ConfigService&&) = delete;
  ConfigService& operator=(ConfigService&&) = delete;

  // Returns nullptr if ConfigService was never constructed (e.g. in unit tests).
  static ConfigService* instance();

  [[nodiscard]] const AppearanceConfig& appearance() const { return appearance_; }
  [[nodiscard]] const BarWorkspacesConfig& barWorkspaces() const { return bar_workspaces_; }
  [[nodiscard]] const BarSystemTrayConfig& barSystemTray() const { return bar_system_tray_; }
  [[nodiscard]] const TrayIconOverridesConfig& trayIconOverrides() const { return tray_icon_overrides_; }
  [[nodiscard]] const BackgroundConfig& background() const { return background_; }
  [[nodiscard]] const WeatherConfig& weather() const { return weather_; }
  [[nodiscard]] const NotificationsConfig& notifications() const { return notifications_; }
  [[nodiscard]] const NotificationHistoryConfig& notificationHistory() const { return notification_history_; }
  [[nodiscard]] const WidgetsConfig& widgets() const { return widgets_; }
  [[nodiscard]] CalendarConfig calendarConfig() const { return calendar_config_; }
  [[nodiscard]] const LogoConfig& logo() const { return logo_; }
  [[nodiscard]] const OsdConfig& osd() const { return osd_; }

  [[nodiscard]] QString configFilePath() const { return config_path_; }

 Q_SIGNALS:
  void appearanceChanged();
  void barWorkspacesChanged();
  void barSystemTrayChanged();
  void trayIconOverridesChanged();
  void backgroundChanged();
  void weatherChanged();
  void notificationsChanged();
  void notificationHistoryChanged();
  void calendarConfigChanged();
  // Raw TOML widget definitions changed. ShellApplication rebuilds the live widget surfaces from
  // this and emits its own widgetsChanged() after the new surfaces are ready.
  void widgetsConfigChanged();
  // ShellApplication re-pushes the whole OsdConfig into OsdController/OsdSurface on this.
  void osdConfigChanged();

 private Q_SLOTS:
  void parseFile();
  void onFileChanged(const QString& path);

 private:
  void resolveConfigPath();
  void ensureDirectoryExists();
  void loadOrCreateConfig();
  void writeConfig();
  void startWatcher();

  void applyParsedConfig(const ParsedConfig& parsed);

  static ConfigService* s_instance_;

  QString config_path_;
  QString config_dir_path_;
  AppearanceConfig appearance_;
  BarWorkspacesConfig bar_workspaces_;
  BarSystemTrayConfig bar_system_tray_;
  TrayIconOverridesConfig tray_icon_overrides_;
  BackgroundConfig background_;
  WeatherConfig weather_;
  NotificationsConfig notifications_;
  NotificationHistoryConfig notification_history_;
  WidgetsConfig widgets_;
  CalendarConfig calendar_config_;
  LogoConfig logo_;
  OsdConfig osd_;
  QFileSystemWatcher watcher_;
  QTimer debounce_timer_;
  bool watcher_active_{false};

  static constexpr int kDebounceMs{200};
};
