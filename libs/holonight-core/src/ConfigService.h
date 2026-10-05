#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <holonight/document_watcher.h>
#include <holonight_shell_config/config_schema.h>
#include <holonight_shell_config/config_structs.h>

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

  [[nodiscard]] const HoloNight::ShellConfig::TaskbarConfig& taskbar() const { return taskbar_; }
  [[nodiscard]] const HoloNight::ShellConfig::BarWorkspacesConfig& barWorkspaces() const { return bar_workspaces_; }
  [[nodiscard]] const HoloNight::ShellConfig::BarSystemTrayConfig& barSystemTray() const { return bar_system_tray_; }
  [[nodiscard]] const HoloNight::ShellConfig::TrayIconOverridesConfig& trayIconOverrides() const {
    return tray_icon_overrides_;
  }
  [[nodiscard]] const HoloNight::ShellConfig::BackgroundConfig& background() const { return background_; }
  [[nodiscard]] const HoloNight::ShellConfig::WeatherConfig& weather() const { return weather_; }
  [[nodiscard]] const HoloNight::ShellConfig::NotificationsConfig& notifications() const { return notifications_; }
  [[nodiscard]] const HoloNight::ShellConfig::NotificationHistoryConfig& notificationHistory() const {
    return notification_history_;
  }
  [[nodiscard]] const HoloNight::ShellConfig::WidgetsConfig& widgets() const { return widgets_; }
  [[nodiscard]] HoloNight::ShellConfig::CalendarConfig calendarConfig() const { return calendar_config_; }
  [[nodiscard]] const HoloNight::ShellConfig::LogoConfig& logo() const { return logo_; }
  [[nodiscard]] const HoloNight::ShellConfig::OsdConfig& osd() const { return osd_; }

  [[nodiscard]] const std::vector<HoloNight::Config::Diagnostic>& diagnostics() const { return diagnostics_; }

  [[nodiscard]] QString configFilePath() const { return config_path_; }

 Q_SIGNALS:
  void diagnosticsChanged();
  void taskbarChanged();
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

 private:
  void resolveConfigPath();

  void applyParsedConfig(const HoloNight::ShellConfig::ProductConfig& parsed);

  static ConfigService* s_instance_;

  QString config_path_;
  HoloNight::ShellConfig::TaskbarConfig taskbar_;
  HoloNight::ShellConfig::BarWorkspacesConfig bar_workspaces_;
  HoloNight::ShellConfig::BarSystemTrayConfig bar_system_tray_;
  HoloNight::ShellConfig::TrayIconOverridesConfig tray_icon_overrides_;
  HoloNight::ShellConfig::BackgroundConfig background_;
  HoloNight::ShellConfig::WeatherConfig weather_;
  HoloNight::ShellConfig::NotificationsConfig notifications_;
  HoloNight::ShellConfig::NotificationHistoryConfig notification_history_;
  HoloNight::ShellConfig::WidgetsConfig widgets_;
  HoloNight::ShellConfig::CalendarConfig calendar_config_;
  HoloNight::ShellConfig::LogoConfig logo_;
  HoloNight::ShellConfig::OsdConfig osd_;
  std::unique_ptr<Holonight::DocumentWatcher> watcher_;
  std::vector<HoloNight::Config::Diagnostic> diagnostics_;
  QTimer debounce_timer_;

  static constexpr int kDebounceMs{200};
};
