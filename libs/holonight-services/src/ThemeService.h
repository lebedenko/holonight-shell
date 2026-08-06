#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QString>

class SettingsPortalBackend;

// QML singleton owning live theme state, not a plain accessor wrapper: it runs a self-rearming
// QFileSystemWatcher on the theme config file/directory, and owns a heap-allocated
// SettingsPortalBackend that registers a real org.freedesktop.impl.portal.Settings D-Bus service
// on the session bus for the lifetime of the process. A filesystem change re-arms the watcher,
// pushes the new values into the portal backend (which emits SettingChanged over D-Bus), and only
// then emits paletteReloadRequested() for in-process QML consumers.
class ThemeService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  explicit ThemeService(QObject* parent = nullptr);
  ~ThemeService() override = default;

  ThemeService(const ThemeService&) = delete;
  ThemeService& operator=(const ThemeService&) = delete;
  ThemeService(ThemeService&&) = delete;
  ThemeService& operator=(ThemeService&&) = delete;

 Q_SIGNALS:
  void paletteReloadRequested();

 private Q_SLOTS:
  void onThemeConfigPathChanged(const QString& path);

 private:
  void resolveThemeConfigPath();
  void startThemeConfigWatcher();
  void armThemeConfigWatch();

  QString theme_config_dir_path_;
  QString theme_config_path_;
  QFileSystemWatcher theme_config_watcher_;
  SettingsPortalBackend* settings_portal_backend_{nullptr};
};
