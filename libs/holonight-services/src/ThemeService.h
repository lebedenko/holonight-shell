#pragma once

#include <QObject>

class AppearanceService;
class SettingsPortalBackend;

// Projects appearance changes to the Settings portal.
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

 private:
  void syncPortalAppearance();

  AppearanceService* appearance_{nullptr};
  SettingsPortalBackend* settings_portal_backend_{nullptr};
};
