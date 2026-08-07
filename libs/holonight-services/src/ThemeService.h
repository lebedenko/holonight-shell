#pragma once

#include <QObject>

class AppearanceService;
class SettingsPortalBackend;

// Compatibility coordinator for QML palette reloads and the Settings portal projection.
// AppearanceService remains the only canonical appearance state and filesystem watcher.
class ThemeService : public QObject {
  Q_OBJECT
 public:
  explicit ThemeService(AppearanceService* appearance, QObject* parent = nullptr);
  ~ThemeService() override = default;

  ThemeService(const ThemeService&) = delete;
  ThemeService& operator=(const ThemeService&) = delete;
  ThemeService(ThemeService&&) = delete;
  ThemeService& operator=(ThemeService&&) = delete;

 Q_SIGNALS:
  void paletteReloadRequested();

 private:
  void syncPortalAppearance();

  AppearanceService* appearance_{nullptr};
  SettingsPortalBackend* settings_portal_backend_{nullptr};
};
