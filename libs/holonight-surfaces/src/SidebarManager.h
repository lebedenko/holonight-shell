#pragma once

#include "LayerShell.h"

#include <QHash>
#include <QObject>
#include <QString>

class LayerSurface;
class QQmlEngine;
class QQuickView;
class QScreen;
struct wl_surface;

// Manages create-on-open fullscreen wlr-layer-shell sidebar host surfaces.
// Exposes toggle/close/closeAll as QML invokables so the clock widget can open the sidebar.
// Mutual exclusion: opening on monitor B closes any open sidebar on monitor A.
// The QML root handles launcher-style dismissal: background clicks close, panel clicks are absorbed.
class SidebarManager : public QObject {
  Q_OBJECT

 public:
  SidebarManager(LayerShell& shell, QObject* parent = nullptr);
  ~SidebarManager() override;

  SidebarManager(const SidebarManager&) = delete;
  SidebarManager& operator=(const SidebarManager&) = delete;
  SidebarManager(SidebarManager&&) = delete;
  SidebarManager& operator=(SidebarManager&&) = delete;

  void start();

  [[nodiscard]] Q_INVOKABLE bool isOpen(const QString& monitor_name) const;
  // True if monitor_name matches a currently connected QScreen. Used to reject control-socket
  // commands for unknown monitors before any sidebar state is touched (REQ-F-5.1).
  [[nodiscard]] Q_INVOKABLE static bool isKnownMonitor(const QString& monitor_name);
  Q_INVOKABLE void toggle(const QString& monitor_name);
  Q_INVOKABLE void close(const QString& monitor_name);
  Q_INVOKABLE void closeAll();
  // Called by RightSidebar.qml via ScriptAction at the end of the close animation.
  // Destroys the layer surface unless the sidebar was reopened during the animation.
  Q_INVOKABLE void onClosingAnimationFinished(const QString& monitor_name);
  // Called by RightSidebar.qml when content height changes. Cached for the current and next view.
  Q_INVOKABLE void onContentHeightChanged(const QString& monitor_name, int height);
  // Called by RightSidebar.qml when the active tab changes so close/re-open can recreate the view.
  Q_INVOKABLE void onCurrentTabChanged(const QString& monitor_name, int tab_index);
  // The tab last reported for a monitor, or 0 if that monitor has never reported one -- matching
  // what createSurface() seeds the QML root's currentTab with. Exists because currentTabChanged
  // only announces "something moved on this monitor"; a consumer aggregating across every monitor
  // still needs to read back the ones that did not move (REQ-F-006).
  [[nodiscard]] Q_INVOKABLE int currentTabForMonitor(const QString& monitor_name) const;

 Q_SIGNALS:
  void sidebarOpened(const QString& monitor_name);
  void sidebarClosed(const QString& monitor_name);
  void currentTabChanged(const QString& monitor_name, int tab_index);

 private:
  struct SidebarSurface {
    QQuickView* view = nullptr;
    LayerSurface* surface = nullptr;
    wl_surface* wl_surface_ptr = nullptr;
  };

  void openOnMonitor(const QString& monitor_name);
  void closeOnMonitor(const QString& monitor_name);
  [[nodiscard]] bool createSurface(const QString& monitor_name);
  void destroySurface(const QString& monitor_name);
  static void configureEngine(QQmlEngine& engine);
  [[nodiscard]] static int boundedHeight(const QString& monitor_name, int requested_height);
  [[nodiscard]] static QScreen* findScreen(const QString& monitor_name);
  [[nodiscard]] QQuickView* findView(const QString& monitor_name) const;

  LayerShell& shell_;
  bool started_{false};
  QHash<QString, bool> open_state_;
  QHash<QString, int> stored_heights_;
  QHash<QString, int> current_tabs_;
  QHash<QString, SidebarSurface> surfaces_;
};
