#include "ThemeService.h"

#include "SettingsPortalBackend.h"
#include "ThemeConfigPath.h"

#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcTheme, "holonight.theme")

ThemeService::ThemeService(QObject* parent)
    : QObject(parent), settings_portal_backend_(new SettingsPortalBackend(this)) {
  resolveThemeConfigPath();
  startThemeConfigWatcher();
}

void ThemeService::resolveThemeConfigPath() {
  const ThemeConfigPaths paths = resolveHolonightThemeConfigPaths();
  theme_config_dir_path_ = paths.dir_path;
  theme_config_path_ = paths.file_path;
}

void ThemeService::startThemeConfigWatcher() {
  armThemeConfigWatch();
  connect(&theme_config_watcher_, &QFileSystemWatcher::fileChanged, this, &ThemeService::onThemeConfigPathChanged);
  connect(&theme_config_watcher_, &QFileSystemWatcher::directoryChanged, this, &ThemeService::onThemeConfigPathChanged);
}

void ThemeService::armThemeConfigWatch() {
  if (QFileInfo::exists(theme_config_dir_path_) &&
      !theme_config_watcher_.directories().contains(theme_config_dir_path_)) {
    theme_config_watcher_.addPath(theme_config_dir_path_);
  }
  if (QFileInfo::exists(theme_config_path_) && !theme_config_watcher_.files().contains(theme_config_path_)) {
    theme_config_watcher_.addPath(theme_config_path_);
  }
}

void ThemeService::onThemeConfigPathChanged(const QString& path) {
  if (path != theme_config_path_ && path != theme_config_dir_path_) {
    return;
  }
  armThemeConfigWatch();
  settings_portal_backend_->reloadFromThemeConfig();
  qCInfo(lcTheme) << "Theme config changed, requesting palette reload:" << theme_config_path_;
  emit paletteReloadRequested();
}
