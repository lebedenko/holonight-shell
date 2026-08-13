#pragma once

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

class QGuiApplication;
class CompositorService;
class AiChatService;
class AppearanceService;
class CalendarService;
class BackgroundManager;
class BrightnessService;
class IdleService;
class MimeService;
class PortalService;
class ScreenSaverAdaptor;
class SessionIntegrationService;
class SettingsNavigationService;
class ConfigService;
class ControlServer;
class AudioService;
class BatteryService;
class KeyboardLayoutService;
class LayerShell;
class LayerShellManager;
class PerMonitorLayerManager;
class LauncherService;
class MprisArtworkCache;
class MprisService;
class LauncherSurface;
class RecentAppsTracker;
class NetworkService;
class NotificationManager;
class NotificationRuleModel;
class NotificationServer;
class NotificationService;
class OsdController;
class OsdSurface;
class PowerProfilesService;
class SidebarManager;
class StatusPopupSurface;
class SessionService;
class SystemInfoService;
class ThemeService;
class TooltipSurface;
class TrayMenuSurface;
class TrayModel;
class TrayWatcher;
class ActivityGateManager;
class LidStateMonitor;
class LowBatteryMonitor;
class SuspendInhibitorService;
class WeatherService;

class ShellApplication : public QObject {
  Q_OBJECT

 public:
  explicit ShellApplication(QObject* parent = nullptr);
  ~ShellApplication() override;
  ShellApplication(const ShellApplication&) = delete;
  ShellApplication& operator=(const ShellApplication&) = delete;
  ShellApplication(ShellApplication&&) = delete;
  ShellApplication& operator=(ShellApplication&&) = delete;

  void registerQmlTypes();
  void startServices();
  void startShell();

#ifdef HOLONIGHT_TESTS
  void markReadyForShellStartForTest() {
    registered_ = true;
    services_started_ = true;
  }
  void connectSessionFailureNotificationsForTest() { connectSessionFailureNotifications(); }
  [[nodiscard]] bool shellSurfaceManagersConstructedForTest() const {
    return layer_shell_ != nullptr && layer_shell_manager_ != nullptr && background_manager_ != nullptr;
  }
  [[nodiscard]] NotificationService* notificationServiceForTest() const { return notification_service_; }
  [[nodiscard]] SessionService* sessionServiceForTest() const { return session_; }
#endif

 Q_SIGNALS:
  // Emitted after desktop widget managers have been rebuilt from the latest config and started.
  void widgetsChanged();

 private:
  // Single readiness gate for the per-monitor layer-surface managers: waits for the shared layer-shell
  // global to bind (with a 3s fallback that aborts if it never appears), then starts both managers.
  void startLayerSurfacesWhenReady();
  void startLayerSurfaces();
  void connectSessionFailureNotifications();
  void connectNotificationRuleFailureNotifications();
  // (Re)build the desktop-widget managers from the current config: resolve per-widget collision
  // blockers, warn once about configured-but-absent monitors, then create and start one
  // WidgetManager per definition. Runs on initial layer-surface start and on every widgetsConfigChanged.
  void rebuildWidgets();
  void closeTransientOverlays();
  // Which monitor the next OSD event belongs on: the focused one, falling back to the primary
  // screen's name when nothing is focused yet (REQ-F-008). Resolved per event rather than cached, so
  // moving focus between monitors routes the *next* event without any explicit teardown.
  [[nodiscard]] QString resolveOsdMonitor() const;
  // REQ-F-006: an OSD is redundant while the surface that already shows the same value is on screen.
  // Both recompute from scratch on every relevant change rather than tracking a counter, so a missed
  // or duplicated signal cannot leave the flag stuck.
  void updateAudioOsdSuppression();
  void updateBrightnessOsdSuppression();
  // REQ-C-005: ShellApplication is the only thing that reads OSD config; the controller and surface
  // never touch ConfigService. Pushes the whole struct every time -- same relationship rebuildWidgets()
  // has to widgetsConfigChanged().
  void applyOsdConfig();

  // Constructed first (destroyed first) so it never outlives an IActivityGate implementer it
  // holds a raw, non-owning pointer to (CalendarService, WeatherService, SuspendInhibitorService).
  ActivityGateManager* activity_gate_manager_ = nullptr;
  ConfigService* config_service_ = nullptr;
  CalendarService* calendar_service_ = nullptr;
  CompositorService* compositor_ = nullptr;
  KeyboardLayoutService* keyboard_layout_ = nullptr;
  AiChatService* ai_chat_service_ = nullptr;
  SettingsNavigationService* settings_navigation_service_ = nullptr;
  BatteryService* battery_ = nullptr;
  AudioService* audio_ = nullptr;
  NetworkService* network_ = nullptr;
  PowerProfilesService* power_profiles_ = nullptr;
  SessionService* session_ = nullptr;
  SystemInfoService* system_info_ = nullptr;
  AppearanceService* appearance_ = nullptr;
  ThemeService* theme_ = nullptr;
  WeatherService* weather_ = nullptr;
  MprisService* mpris_ = nullptr;
  // Plain (non-QObject) class, shared by reference across every MprisWidgetManager instance
  // (docs/sdd/mpris-desktop-widget/DESIGN.md §2.4) — not a QML singleton, so the two-file
  // registerQmlTypes() gotcha that bit the MPRIS topbar pill does not apply here (see
  // MprisArtworkCache.h's own header comment).
  std::unique_ptr<MprisArtworkCache> mpris_artwork_cache_;
  RecentAppsTracker* recent_apps_tracker_ = nullptr;
  LauncherService* launcher_ = nullptr;
  StatusPopupSurface* status_popup_surface_ = nullptr;
  LauncherSurface* launcher_surface_ = nullptr;
  TooltipSurface* tooltip_surface_ = nullptr;
  TrayMenuSurface* tray_menu_surface_ = nullptr;
  TrayModel* tray_model_ = nullptr;
  TrayWatcher* tray_watcher_ = nullptr;
  NotificationRuleModel* notification_rule_model_ = nullptr;
  NotificationService* notification_service_ = nullptr;
  NotificationServer* notification_server_ = nullptr;
  NotificationManager* notification_manager_ = nullptr;
  BrightnessService* brightness_service_ = nullptr;
  // Declared here, after every service an OSD channel source reads from (audio_, keyboard_layout_,
  // brightness_service_), because members initialize in declaration order and the sources are
  // constructed inline in the controller's initializer (REQ-C-001).
  OsdController* osd_controller_ = nullptr;
  OsdSurface* osd_surface_ = nullptr;
  IdleService* idle_service_ = nullptr;
  LidStateMonitor* lid_monitor_ = nullptr;
  LowBatteryMonitor* low_battery_monitor_ = nullptr;
  SuspendInhibitorService* suspend_inhibitor_service_ = nullptr;
  SessionIntegrationService* session_integration_service_ = nullptr;
  MimeService* mime_service_ = nullptr;
  PortalService* portal_service_ = nullptr;
  ScreenSaverAdaptor* screen_saver_adaptor_ = nullptr;
  ControlServer* control_server_ = nullptr;
  // Declared before the managers so it is destroyed *after* them: the managers hold a LayerShell& and
  // their surfaces must tear down while the shell is still alive (members destruct in reverse order).
  std::unique_ptr<LayerShell> layer_shell_;
  std::unique_ptr<LayerShellManager> layer_shell_manager_;
  std::unique_ptr<BackgroundManager> background_manager_;
  std::unique_ptr<SidebarManager> sidebar_manager_;
  // Declared after the shell so the widget surfaces tear down while the LayerShell is still alive.
  // Element type is the common base, not WidgetManager, since WidgetType::Mpris definitions
  // construct a MprisWidgetManager instead (rebuildWidgets()'s switch on def.type).
  std::vector<std::unique_ptr<PerMonitorLayerManager>> widget_managers_;

  bool registered_ = false;
  bool services_started_ = false;
  bool shell_started_ = false;
  bool managers_started_ = false;
  QString compositor_navigation_key_;
};
