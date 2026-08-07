#include "ConfigService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

#include <holonight_shell_config/config_path.h>
#include <holonight_shell_config/config_writer.h>

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
  ensureDirectoryExists();
  loadOrCreateConfig();
  startWatcher();
}

ConfigService::~ConfigService() {
  if (s_instance_ == this) {
    s_instance_ = nullptr;
  }
}

ConfigService* ConfigService::instance() { return s_instance_; }

void ConfigService::resolveConfigPath() {
  config_path_ = HoloNight::ShellConfig::resolveProductConfigPath();
  config_dir_path_ = QFileInfo(config_path_).dir().path();
}

void ConfigService::ensureDirectoryExists() {
  QDir dir = QFileInfo(config_path_).dir();
  if (!dir.exists() && !dir.mkpath(QLatin1String("."))) {
    qCWarning(lcConfig) << "Failed to create config directory:" << dir.absolutePath();
  }
}

void ConfigService::loadOrCreateConfig() {
  if (!QFile::exists(config_path_)) {
    writeConfig();
    qCInfo(lcConfig) << "Created default config:" << config_path_;
  }
  parseFile();
}

void ConfigService::writeConfig() {
  if (!HoloNight::ShellConfig::ProductConfigWriter::write(HoloNight::ShellConfig::ProductConfig{}, config_path_)) {
    qCWarning(lcConfig) << "Failed to write default config:" << config_path_;
  }
}

void ConfigService::parseFile() {
  if (watcher_active_) {
    if (QFileInfo::exists(config_path_)) {
      watcher_.addPath(config_path_);
    }
  }
  QFile file(config_path_);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qCWarning(lcConfig) << "Failed to open config file:" << config_path_;
    return;
  }
  const std::string content = file.readAll().toStdString();
  file.close();

  toml::table table;
  try {
    table = toml::parse(content);
  } catch (const toml::parse_error& err) {
    qCWarning(lcConfig) << "Config parse error in" << config_path_ << ":" << err.description().data();
    return;
  }

  HoloNight::ShellConfig::MissingDefaults missing;
  const HoloNight::ShellConfig::ProductConfig parsed = HoloNight::ShellConfig::parseConfigTable(table, missing);

  if (missing.any()) {
    if (HoloNight::ShellConfig::writeMissingDefaults(config_path_, missing)) {
      qCInfo(lcConfig) << "Wrote missing config keys back to:" << config_path_;
    }
  }

  applyParsedConfig(parsed);
  qCInfo(lcConfig) << "Config loaded from" << config_path_;
}

void ConfigService::applyParsedConfig(const HoloNight::ShellConfig::ProductConfig& parsed) {
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

void ConfigService::startWatcher() {
  if (QFileInfo::exists(config_dir_path_)) {
    watcher_.addPath(config_dir_path_);
  }
  if (QFile::exists(config_path_)) {
    watcher_.addPath(config_path_);
  }
  connect(&watcher_, &QFileSystemWatcher::fileChanged, this, &ConfigService::onFileChanged);
  watcher_active_ = true;
}

void ConfigService::onFileChanged(const QString& path) {
  if (path != config_path_ && path != config_dir_path_) {
    return;
  }
  // Re-add immediately for in-place writes; parseFile() re-arms again after
  // the debounce, by which point the new inode from an atomic rename is stable.
  if (QFileInfo::exists(config_path_)) {
    watcher_.addPath(config_path_);
  }
  debounce_timer_.start(kDebounceMs);
}
