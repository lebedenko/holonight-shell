#include "ConfigService.h"

#include <QLoggingCategory>

#include <holonight_shell_config/config_path.h>

Q_LOGGING_CATEGORY(lcConfig, "holonight.config")

ConfigService* ConfigService::s_instance_ = nullptr;

// ---------------------------------------------------------------------------
// ConfigService
// ---------------------------------------------------------------------------

ConfigService::ConfigService(QObject* parent) : QObject(parent) {
  s_instance_ = this;
  debounce_timer_.setSingleShot(true);
  connect(&debounce_timer_, &QTimer::timeout, this, &ConfigService::parseFile);
  resolveConfigPath();
  watcher_ = std::make_unique<Holonight::DocumentWatcher>(config_path_);
  connect(watcher_.get(), &Holonight::DocumentWatcher::documentChanged, this,
          [this] { debounce_timer_.start(kDebounceMs); });
  parseFile();
}

ConfigService::~ConfigService() {
  if (s_instance_ == this) {
    s_instance_ = nullptr;
  }
}

ConfigService* ConfigService::instance() { return s_instance_; }

void ConfigService::resolveConfigPath() { config_path_ = HoloNight::ShellConfig::resolveProductConfigPath(); }

void ConfigService::parseFile() {
  const auto snapshot = watcher_->read();
  const auto parsed =
      snapshot ? HoloNight::ShellConfig::decodeDocument(*snapshot.value)
               : HoloNight::Config::Result<HoloNight::ShellConfig::ProductConfig>::failure(snapshot.diagnostics);
  watcher_->refresh();
  if (diagnostics_ != parsed.diagnostics) {
    diagnostics_ = parsed.diagnostics;
    emit diagnosticsChanged();
    for (const auto& diagnostic : diagnostics_) {
      qCWarning(lcConfig) << QString::fromStdString(diagnostic.message);
    }
  }
  if (parsed) {
    applyParsedConfig(*parsed.value);
  }
}

void ConfigService::applyParsedConfig(const HoloNight::ShellConfig::ProductConfig& parsed) {
  if (parsed.taskbar != taskbar_) {
    taskbar_ = parsed.taskbar;
    emit taskbarChanged();
  }
  if (parsed.bar_workspaces != bar_workspaces_) {
    bar_workspaces_ = parsed.bar_workspaces;
    emit barWorkspacesChanged();
  }
  if (parsed.bar_system_tray != bar_system_tray_) {
    bar_system_tray_ = parsed.bar_system_tray;
    emit barSystemTrayChanged();
  }
  if (parsed.tray_icon_overrides != tray_icon_overrides_) {
    tray_icon_overrides_ = parsed.tray_icon_overrides;
    emit trayIconOverridesChanged();
  }
  if (parsed.background != background_) {
    background_ = parsed.background;
    emit backgroundChanged();
  }
  if (parsed.weather != weather_) {
    weather_ = parsed.weather;
    emit weatherChanged();
  }
  if (parsed.notifications != notifications_) {
    notifications_ = parsed.notifications;
    emit notificationsChanged();
  }
  if (parsed.notification_history != notification_history_) {
    notification_history_ = parsed.notification_history;
    emit notificationHistoryChanged();
  }
  if (parsed.widgets != widgets_) {
    widgets_ = parsed.widgets;
    emit widgetsConfigChanged();
  }
  if (parsed.calendar != calendar_config_) {
    calendar_config_ = parsed.calendar;
    emit calendarConfigChanged();
  }
  if (parsed.osd != osd_) {
    osd_ = parsed.osd;
    emit osdConfigChanged();
  }
  logo_ = parsed.logo;
}
