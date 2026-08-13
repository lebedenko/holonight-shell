#include "ShellApplication.h"

#include "ActivityGateManager.h"
#include "AiChatService.h"
#include "AppearanceService.h"
#include "AudioChannelSource.h"
#include "AudioService.h"
#include "BackgroundManager.h"
#include "BatteryService.h"
#include "BrightnessChannelSource.h"
#include "BrightnessService.h"
#include "CalendarService.h"
#include "CompositorService.h"
#include "ConfigService.h"
#include "ControlServer.h"
#include "IdleService.h"
#include "KeyboardLayoutChannelSource.h"
#include "KeyboardLayoutService.h"
#include "LauncherService.h"
#include "LauncherSurface.h"
#include "LayerShell.h"
#include "LayerShellManager.h"
#include "LidStateMonitor.h"
#include "LowBatteryMonitor.h"
#include "MimeService.h"
#include "MprisArtworkCache.h"
#include "MprisService.h"
#include "MprisWidgetManager.h"
#include "NetworkService.h"
#include "NotificationManager.h"
#include "NotificationRuleModel.h"
#include "NotificationRuleStore.h"
#include "NotificationServer.h"
#include "NotificationService.h"
#include "OsdController.h"
#include "OsdSurface.h"
#include "PortalService.h"

using namespace HoloNight::ShellConfig;
#include "PowerProfilesService.h"
#include "RecentAppsTracker.h"
#include "ScreenSaverAdaptor.h"
#include "SessionIntegrationService.h"
#include "SessionService.h"
#include "SettingsNavigationService.h"
#include "SidebarManager.h"
#include "StatusPopupSurface.h"
#include "SuspendInhibitorService.h"
#include "SystemInfoService.h"
#include "ThemeService.h"
#include "TooltipSurface.h"
#include "TrayMenuSurface.h"
#include "TrayModel.h"
#include "TrayWatcher.h"
#include "WeatherIconBridge.h"
#include "WeatherService.h"
#include "WidgetManager.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QScreen>
#include <QSet>
#include <QTimer>
#include <QtQml/qqml.h>
#include <QtQml/qqmlengine.h>
#include <QtWaylandClient/QWaylandClientExtension>

#include <holonight/wayland/layershellcontext.h>
#include <utility>

Q_LOGGING_CATEGORY(lcWidgetCoord, "holonight.widgets")

namespace {

// Monitor-filters of earlier widgets that share this widget's position. An earlier widget blocks this
// one on a monitor it targets (empty filter = all monitors), so this widget is dropped there.
QList<QStringList> blockersForWidget(const QList<WidgetDefinition>& defs, qsizetype index) {
  QList<QStringList> blockers;
  for (qsizetype j = 0; j < index; ++j) {
    if (defs.at(j).position == defs.at(index).position) {
      blockers.append(defs.at(j).monitors);
    }
  }
  return blockers;
}

// Warn once per configured monitor name that is not currently connected (REQ-F-011). The name is still
// honored: WidgetManager::shouldCreateSurface matches it live, so the widget appears on hotplug.
void warnUnknownMonitors(const WidgetDefinition& def, const QSet<QString>& connected, QSet<QString>& warned) {
  for (const QString& name : def.monitors) {
    if (!connected.contains(name) && !warned.contains(name)) {
      warned.insert(name);
      qCWarning(lcWidgetCoord) << "Configured widget monitor" << name
                               << "is not currently connected; the widget will appear if it is plugged in";
    }
  }
}

}  // namespace

ShellApplication::ShellApplication(QObject* parent)
    : QObject(parent),
      activity_gate_manager_(new ActivityGateManager(this)),
      config_service_(new ConfigService(this)),
      calendar_service_(new CalendarService(this)),
      compositor_(new CompositorService(this)),
      keyboard_layout_(new KeyboardLayoutService(this)),
      ai_chat_service_(new AiChatService(this)),
      settings_navigation_service_(new SettingsNavigationService(this)),
      battery_(new BatteryService(this)),
      audio_(new AudioService(this)),
      network_(new NetworkService(this)),
      power_profiles_(new PowerProfilesService(this)),
      session_(new SessionService(compositor_->backendKind(), this)),
      system_info_(new SystemInfoService(config_service_, this)),
      appearance_(new AppearanceService(this)),
      theme_(new ThemeService(appearance_, this)),
      weather_(new WeatherService(config_service_, this)),
      mpris_(new MprisService(this)),
      // Constructed here (well before rebuildWidgets() ever runs, per DESIGN.md §2.7/T-032) —
      // no async dependency, and rebuildWidgets() only needs a raw pointer into it.
      mpris_artwork_cache_(std::make_unique<MprisArtworkCache>()),
      recent_apps_tracker_(new RecentAppsTracker(this)),
      launcher_(new LauncherService(DesktopEntryScanner(), std::make_unique<ProcessLauncherBackend>(), {},
                                    recent_apps_tracker_, this)),
      status_popup_surface_(new StatusPopupSurface(this)),
      launcher_surface_(new LauncherSurface(this)),
      tooltip_surface_(new TooltipSurface(this)),
      tray_menu_surface_(new TrayMenuSurface(this)),
      tray_model_(new TrayModel(config_service_, this)),
      tray_watcher_(new TrayWatcher(tray_model_, this)),
      notification_rule_model_(new NotificationRuleModel(new NotificationRuleStore(this), DesktopEntryScanner(), this)),
      notification_service_(new NotificationService(config_service_, compositor_, notification_rule_model_, this)),
      notification_server_(new NotificationServer(notification_service_, this)),
      notification_manager_(new NotificationManager(notification_service_, this)),
      brightness_service_(new BrightnessService(this)),
      // The three channel sources are created here rather than kept as members: nothing outside the
      // controller ever addresses one, and parenting them to `this` (not to the controller) matches
      // what OsdController documents about their lifetime -- sources outlive it (REQ-C-001/F-025).
      // The empty NowFn is the controller's documented "use the real clock" argument; it is only
      // spelled out because `parent` follows it.
      osd_controller_(new OsdController(
          {new AudioChannelSource(audio_, this), new BrightnessChannelSource(brightness_service_, this),
           new KeyboardLayoutChannelSource(keyboard_layout_, this)},
          {}, this)),
      osd_surface_(new OsdSurface(this)),
      idle_service_(new IdleService(notification_service_, this)),
      session_integration_service_(new SessionIntegrationService(this)),
      mime_service_(new MimeService(DesktopEntryScanner::defaultApplicationDirs(), this)),
      portal_service_(new PortalService(this)),
      suspend_inhibitor_service_(new SuspendInhibitorService(this)),
      screen_saver_adaptor_(new ScreenSaverAdaptor(idle_service_, this)),
      control_server_(new ControlServer(this)) {
  session_integration_service_->setExpectedCursorTheme(appearance_->cursorTheme());
  connect(appearance_, &AppearanceService::cursorThemeChanged, this,
          [this]() { session_integration_service_->setExpectedCursorTheme(appearance_->cursorTheme()); });
}

ShellApplication::~ShellApplication() = default;

void ShellApplication::registerQmlTypes() {
  if (registered_) {
    return;
  }

  auto reg = [](auto* obj, const char* name) {
    using T = std::remove_pointer_t<decltype(obj)>;
    QQmlEngine::setObjectOwnership(obj, QQmlEngine::CppOwnership);
    qmlRegisterSingletonType<T>("HolonightShell", 1, 0, name,
                                [obj](QQmlEngine*, QJSEngine*) -> QObject* { return obj; });
  };

  reg(brightness_service_, "BrightnessService");
  reg(session_integration_service_, "SessionIntegrationService");
  reg(mime_service_, "MimeService");
  reg(portal_service_, "PortalService");
  reg(calendar_service_, "CalendarService");
  reg(compositor_, "CompositorService");
  reg(keyboard_layout_, "KeyboardLayoutService");
  reg(battery_, "BatteryService");
  reg(audio_, "AudioService");
  reg(settings_navigation_service_, "SettingsNavigationService");
  reg(network_, "NetworkService");
  reg(power_profiles_, "PowerProfilesService");
  reg(session_, "SessionService");
  reg(system_info_, "SystemInfoService");
  reg(appearance_, "AppearanceService");
  reg(weather_, "WeatherService");
  reg(mpris_, "MprisService");
  reg(launcher_, "LauncherService");
  reg(recent_apps_tracker_, "RecentAppsTracker");
  reg(status_popup_surface_, "StatusPopupSurface");
  reg(launcher_surface_, "LauncherSurface");
  reg(tooltip_surface_, "TooltipSurface");
  reg(tray_menu_surface_, "TrayMenuSurface");
  reg(tray_model_, "TrayModel");
  reg(notification_service_, "NotificationService");
  reg(notification_rule_model_, "NotificationRuleModel");
  reg(idle_service_, "IdleService");
  reg(suspend_inhibitor_service_, "SuspendInhibitorService");
  reg(osd_controller_, "OsdController");
  // OsdView.qml calls back into this one when its exit animation ends; the controller is registered
  // alongside it so a QML console session can reach setSuppressed()/setChannelEnabled() too.
  reg(osd_surface_, "OsdSurface");

  qmlRegisterSingletonType<WeatherIconBridge>(
      "HolonightShell", 1, 0, "WeatherIconBridge",
      [](QQmlEngine* engine, QJSEngine*) -> QObject* { return new WeatherIconBridge(engine); });

  registered_ = true;
}

void ShellApplication::startServices() {
  if (services_started_) {
    return;
  }

  tray_model_->setMenuSurface(tray_menu_surface_);
  compositor_->setWorkspaceDisplayCount(config_service_->barWorkspaces().count);
  connect(config_service_, &ConfigService::barWorkspacesChanged, this,
          [this] { compositor_->setWorkspaceDisplayCount(config_service_->barWorkspaces().count); });
  connect(compositor_, &CompositorService::revisionChanged, this, [this] {
    const QString key =
        compositor_->focusedOutput() + QLatin1Char(':') + QString::number(compositor_->focusedWorkspaceRow());
    if (!compositor_navigation_key_.isEmpty() && key != compositor_navigation_key_) {
      closeTransientOverlays();
    }
    compositor_navigation_key_ = key;
  });

  compositor_->start();
  keyboard_layout_->start();
  battery_->start();
  audio_->start();
  network_->start();
  power_profiles_->start();
  weather_->start();
  launcher_->start();
  connect(launcher_, &LauncherService::entriesUpdated, mime_service_, &MimeService::refreshAllRoles);
  session_integration_service_->setPostRebuildRefreshCallbacks([this] { mime_service_->refreshAllRoles(); },
                                                               [this] { launcher_->reload(); });
  tray_watcher_->start();
  notification_server_->start();
  if (notification_server_->conflictDetected()) {
    notification_service_->setDaemonConflict(notification_server_->conflictOwner());
  }
  connectSessionFailureNotifications();
  connectNotificationRuleFailureNotifications();
  screen_saver_adaptor_->registerService();
  connect(idle_service_, &IdleService::idleChanged, screen_saver_adaptor_, &ScreenSaverAdaptor::onIdleChanged);
  connect(idle_service_, &IdleService::idleChanged, weather_, &WeatherService::onIdleChanged);
  connect(idle_service_, &IdleService::idleChanged, calendar_service_, &CalendarService::onIdleChanged);
  connect(control_server_, &ControlServer::toggleLauncherRequested, this,
          [this] { launcher_surface_->toggle(resolveOsdMonitor()); });
  connect(control_server_, &ControlServer::toggleSidebarRequested, this, [this](const QString& monitor_name) {
    if (sidebar_manager_ != nullptr) {
      sidebar_manager_->toggle(monitor_name);
    }
  });
  connect(control_server_, &ControlServer::toggleChatRequested, this, [this](const QString& monitor_name) {
    ai_chat_service_->togglePanel(monitor_name.isEmpty() ? resolveOsdMonitor() : monitor_name);
  });
  control_server_->start();

  // REQ-F-008/REQ-C-012: the controller decides *what* to show, this hop decides *where*. The
  // monitor is resolved at emit time so the surface follows focus; ensureSurface() no-ops when the
  // OSD is already on that output and rebuilds it when focus has moved.
  connect(osd_controller_, &OsdController::displayLevelEvent, this, [this](const OsdLevelEvent& event) {
    osd_surface_->ensureSurface(resolveOsdMonitor());
    osd_surface_->showLevel(event.channel, event.value, event.muted);
  });
  connect(osd_controller_, &OsdController::displaySelectionEvent, this, [this](const OsdSelectionEvent& event) {
    osd_surface_->ensureSurface(resolveOsdMonitor());
    osd_surface_->showSelection(event.channel, event.short_label, event.full_label);
  });
  // hide() only starts the exit animation; OsdView.qml calls back into OsdSurface to destroy the
  // surface once it finishes, so there is nothing to tear down here.
  connect(osd_controller_, &OsdController::hideRequested, osd_surface_, &OsdSurface::hide);

  // REQ-C-007. Both signals matter: `popupVisibleChanged` covers open/close, `activePopupChanged`
  // covers switching straight from one popup to another without an intervening hide.
  connect(status_popup_surface_, &StatusPopupSurface::popupVisibleChanged, this,
          &ShellApplication::updateAudioOsdSuppression);
  connect(status_popup_surface_, &StatusPopupSurface::activePopupChanged, this,
          &ShellApplication::updateAudioOsdSuppression);
  // Brightness suppression is wired in startLayerSurfaces() instead — SidebarManager needs the
  // LayerShell and does not exist yet at this point.

  // REQ-C-005/C-010/C-011. Applied once up front so the OSD never runs on the struct defaults, then
  // re-applied on every reload; the setters are all idempotent, so re-pushing unchanged fields is free.
  applyOsdConfig();
  connect(config_service_, &ConfigService::osdConfigChanged, this, &ShellApplication::applyOsdConfig);

  low_battery_monitor_ = new LowBatteryMonitor(battery_, this);

  suspend_inhibitor_service_->start();

  lid_monitor_ = new LidStateMonitor(this);
  activity_gate_manager_->registerGate(calendar_service_);
  activity_gate_manager_->registerGate(weather_);
  activity_gate_manager_->registerGate(suspend_inhibitor_service_);
  connect(lid_monitor_, &LidStateMonitor::lidStateChanged, activity_gate_manager_,
          &ActivityGateManager::onLidStateChanged);
  lid_monitor_->start();

  services_started_ = true;
}

void ShellApplication::startShell() {
  if (shell_started_) {
    return;
  }

  if (!registered_ || !services_started_) {
    qCritical("ShellApplication: startShell() called before registerQmlTypes()/startServices(); ignoring");
    return;
  }

  // Local wrapper remains for unfinished transient/sidebar surfaces; persistent managers use
  // HolonightQt's process-wide LayerShellContext.
  layer_shell_ = std::make_unique<LayerShell>();
  layer_shell_manager_ = std::make_unique<LayerShellManager>(tray_model_, this);
  background_manager_ = std::make_unique<BackgroundManager>(config_service_, this);
  startLayerSurfacesWhenReady();

  shell_started_ = true;
}

void ShellApplication::connectSessionFailureNotifications() {
  connect(session_, &SessionService::commandFailed, this, [this](const QString& action, const QString& reason) {
    notification_server_->Notify(QStringLiteral("HoloNight Shell"), 0, QString(),
                                 QStringLiteral("Session command failed: %1").arg(action), reason, {}, {}, -1);
  });
}

void ShellApplication::connectNotificationRuleFailureNotifications() {
  connect(notification_rule_model_, &NotificationRuleModel::rulePersistenceFailed, this,
          [this](const QString& action, const QString& reason) {
            notification_server_->Notify(QStringLiteral("HoloNight Shell"), 0, QString(),
                                         QStringLiteral("Notification rule save failed: %1").arg(action), reason, {},
                                         {}, -1);
          });
}

void ShellApplication::startLayerSurfacesWhenReady() {
  auto* context = Holonight::Wayland::LayerShellContext::instance();
  if (context->isAvailable()) {
    startLayerSurfaces();
    return;
  }

  connect(context, &Holonight::Wayland::LayerShellContext::availabilityChanged, this, [this, context]() {
    if (context->isAvailable()) {
      startLayerSurfaces();
    }
  });

  // Fallback if the compositor never announces the global.
  QTimer::singleShot(3000, this, [this, context]() {
    if (managers_started_) {
      return;
    }
    if (!context->isAvailable()) {
      qCritical("ShellApplication: wlr-layer-shell protocol not available: %s", qPrintable(context->diagnostic()));
      QCoreApplication::exit(1);
      return;
    }
    startLayerSurfaces();
  });
}

void ShellApplication::startLayerSurfaces() {
  if (managers_started_) {
    return;
  }
  managers_started_ = true;

  // SidebarManager needs LayerShell, so it is created here (not in the constructor).
  // Register as a QML singleton before any QML loads so ClockSection.qml can call
  // SidebarManager.toggle() once T-014 wires the clock trigger.
  sidebar_manager_ = std::make_unique<SidebarManager>(*layer_shell_, this);
  QQmlEngine::setObjectOwnership(sidebar_manager_.get(), QQmlEngine::CppOwnership);
  qmlRegisterSingletonType<SidebarManager>(
      "HolonightShell", 1, 0, "SidebarManager",
      [this](QQmlEngine*, QJSEngine*) -> QObject* { return sidebar_manager_.get(); });

  connect(sidebar_manager_.get(), &SidebarManager::sidebarOpened, notification_service_,
          &NotificationService::onSidebarOpened);

  layer_shell_manager_->start();
  background_manager_->start();
  sidebar_manager_->start();

  // REQ-C-008. Wired after start() on purpose: SidebarManager installs its own screenRemoved
  // handler inside start(), and same-sender direct connections fire in connection order, so ours
  // must come second to observe the already-cleaned-up state. Without the screenRemoved hop,
  // unplugging a monitor whose sidebar sat on QuickSettings would latch brightness suppression on
  // forever -- SidebarManager drops that monitor's state silently, with no signal of its own.
  connect(sidebar_manager_.get(), &SidebarManager::sidebarOpened, this,
          &ShellApplication::updateBrightnessOsdSuppression);
  connect(sidebar_manager_.get(), &SidebarManager::sidebarClosed, this,
          &ShellApplication::updateBrightnessOsdSuppression);
  connect(sidebar_manager_.get(), &SidebarManager::currentTabChanged, this,
          &ShellApplication::updateBrightnessOsdSuppression);
  connect(qGuiApp, &QGuiApplication::screenRemoved, this, &ShellApplication::updateBrightnessOsdSuppression);

  rebuildWidgets();
  connect(config_service_, &ConfigService::widgetsConfigChanged, this, &ShellApplication::rebuildWidgets);
}

void ShellApplication::closeTransientOverlays() {
  tooltip_surface_->hide();
  status_popup_surface_->hide();
  launcher_surface_->hide();
  tray_menu_surface_->hide();
  if (sidebar_manager_ != nullptr) {
    sidebar_manager_->closeAll();
  }
}

QString ShellApplication::resolveOsdMonitor() const {
  if (QString focused = compositor_->focusedOutput();
      compositor_->connected() && compositor_->hasFocusedOutput() && !focused.isEmpty()) {
    return focused;
  }
  // Resolve the primary screen's *name* rather than handing OsdSurface an empty string: the surface
  // keys its "already on this monitor" check on the name it was given, so an empty one would force a
  // needless rebuild the first time the same monitor arrives under its real name.
  const QScreen* primary = QGuiApplication::primaryScreen();
  return primary != nullptr ? primary->name() : QString();
}

void ShellApplication::applyOsdConfig() {
  const OsdConfig& cfg = config_service_->osd();
  osd_controller_->setEnabled(cfg.enabled);
  // The channel ids are the strings each OsdChannelSource returns from channel(); there is no shared
  // constant, so these three literals are the coupling to AudioChannelSource.h et al.
  osd_controller_->setChannelEnabled(QStringLiteral("audio-volume"), cfg.volume.enabled);
  osd_controller_->setChannelEnabled(QStringLiteral("screen-brightness"), cfg.brightness.enabled);
  osd_controller_->setChannelEnabled(QStringLiteral("keyboard-layout"), cfg.keyboard_layout.enabled);
  osd_controller_->setTimeoutMs(cfg.timeout_ms);
  osd_surface_->setPosition(cfg.position);
}

void ShellApplication::updateAudioOsdSuppression() {
  // Matches AudioWidget.qml's `popupId: "audio"`. Stringly-typed by necessity: the id crosses the
  // QML/C++ boundary as data, so a rename there has to be mirrored here.
  const bool suppress =
      status_popup_surface_->isPopupVisible() && status_popup_surface_->activePopupId() == QStringLiteral("audio");
  osd_controller_->setSuppressed(QStringLiteral("audio-volume"), suppress);
}

void ShellApplication::updateBrightnessOsdSuppression() {
  if (sidebar_manager_ == nullptr) {
    return;
  }
  // Index into SidebarContent.qml's `tabDefinitions`. Reordering those entries silently changes what
  // suppresses the brightness OSD -- there is no compile-time link between the two.
  static constexpr int kQuickSettingsTabIndex = 4;
  // Suppression is one flag per channel, but sidebar state is per monitor, so any monitor showing
  // the QuickSettings tab suppresses the (single) OSD.
  bool suppress = false;
  for (const QScreen* screen : QGuiApplication::screens()) {
    const QString name = screen->name();
    if (sidebar_manager_->isOpen(name) && sidebar_manager_->currentTabForMonitor(name) == kQuickSettingsTabIndex) {
      suppress = true;
      break;
    }
  }
  osd_controller_->setSuppressed(QStringLiteral("screen-brightness"), suppress);
}

void ShellApplication::rebuildWidgets() {
  if (!managers_started_) {
    return;
  }
  // Destroy every existing widget surface before rebuilding (config order / collisions may have
  // changed). The unique_ptr reset tears surfaces down while the LayerShell is still alive.
  widget_managers_.clear();

  const WidgetsConfig& cfg = config_service_->widgets();
  QSet<QString> connected;
  for (const QScreen* screen : QGuiApplication::screens()) {
    connected.insert(screen->name());
  }

  // Disabled definitions create no surfaces and must not block a later widget's position, so drop
  // them before computing collisions/indices.
  QList<WidgetDefinition> defs;
  for (const WidgetDefinition& def : cfg.definitions) {
    if (def.enabled) {
      defs.append(def);
    }
  }

  QSet<QString> warned_unknown_monitors;
  for (qsizetype i = 0; i < defs.size(); ++i) {
    warnUnknownMonitors(defs.at(i), connected, warned_unknown_monitors);
    std::unique_ptr<PerMonitorLayerManager> manager;
    if (defs.at(i).type == WidgetType::Mpris) {
      manager =
          std::make_unique<MprisWidgetManager>(defs.at(i), cfg.margin, static_cast<int>(i), blockersForWidget(defs, i),
                                               compositor_, mpris_, mpris_artwork_cache_.get(), this);
    } else {
      manager = std::make_unique<WidgetManager>(defs.at(i), cfg.margin, static_cast<int>(i), blockersForWidget(defs, i),
                                                compositor_, this);
    }
    manager->start();
    widget_managers_.push_back(std::move(manager));
  }
  emit widgetsChanged();
}
