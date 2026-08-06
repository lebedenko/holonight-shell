#pragma once

#include "HyprlandIpc.h"
#include "HyprlandIpcClient.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QString>
#include <QStringList>

#include <memory>

struct ActiveWindowState {
  QHash<QString, HyprlandActiveWindow> monitor_windows;
  QHash<QString, int> monitor_workspaces;
  QString focused_monitor_name;
  QHash<QString, QString> category_cache;
};

struct ActiveWindowStateChange {
  QStringList changed_monitors;
  QStringList workspace_changed_monitors;
  QSet<QString> classes_to_resolve;
  bool requery_requested{false};
};

[[nodiscard]] ActiveWindowStateChange applyActiveWindowEvent(ActiveWindowState& state, const QByteArray& line);
[[nodiscard]] ActiveWindowStateChange applyActiveWindowSnapshot(ActiveWindowState& state,
                                                                const QHash<QString, int>& monitor_workspaces,
                                                                const QList<HyprlandClientInfo>& clients);
[[nodiscard]] QString activeWindowCategoryForDesktopCategories(const QString& categories_field);
[[nodiscard]] QString activeWindowCategoriesFromDesktopEntry(const QString& contents, const QString& app_class_lower);

class ActiveWindowService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(QString focusedMonitor READ focusedMonitorName NOTIFY focusedMonitorChanged)

 public:
  explicit ActiveWindowService(QObject* parent = nullptr);
  explicit ActiveWindowService(HyprlandIpcTransportPtr ipc_client, QObject* parent = nullptr);
  ~ActiveWindowService() override = default;

  ActiveWindowService(const ActiveWindowService&) = delete;
  ActiveWindowService& operator=(const ActiveWindowService&) = delete;
  ActiveWindowService(ActiveWindowService&&) = delete;
  ActiveWindowService& operator=(ActiveWindowService&&) = delete;

  void start();

  Q_INVOKABLE [[nodiscard]] QString titleForMonitor(const QString& monitor_name) const;
  Q_INVOKABLE [[nodiscard]] QString appClassForMonitor(const QString& monitor_name) const;
  Q_INVOKABLE [[nodiscard]] QString categoryForMonitor(const QString& monitor_name) const;
  Q_INVOKABLE [[nodiscard]] QString focusedMonitorName() const;

  // Workspace ID currently displayed on the named monitor, or -1 if the monitor is unknown. Used by
  // MonitorOccupancyService to decide which workspace's occupancy governs a monitor's widgets.
  [[nodiscard]] int visibleWorkspaceIdForMonitor(const QString& monitor_name) const;

 Q_SIGNALS:
  void monitorWindowChanged(const QString& monitor_name);
  void visibleWorkspaceChanged(const QString& monitor_name);
  void focusedMonitorChanged(const QString& monitor_name);

 private:
  enum class CommandPhase : uint8_t { Idle, Monitors, Clients };

  void connectSocket();
  void queryAllMonitorWindows();
  void onEventSocketConnected();
  void onCommandFinished(const QByteArray& response, bool success);
  void finishCommandResponse(const QByteArray& response, bool parse_buffer);
  void advanceToClientsPhase();
  void applyMonitorWindowsFromPending();
  void processEventLine(const QByteArray& line);
  void scheduleResolveCategory(const QString& app_class, const QString& monitor_name);
  [[nodiscard]] static QString scanDesktopFiles(const QString& app_class);

  HyprlandIpcTransportPtr ipc_client_;

  ActiveWindowState active_window_state_;

  CommandPhase command_phase_{CommandPhase::Idle};
  bool requery_pending_{false};
  bool pending_monitor_snapshot_{false};
  bool pending_clients_snapshot_{false};
  QHash<QString, int> pending_monitor_workspaces_;
  QList<HyprlandClientInfo> pending_clients_;

  QSet<QString> resolved_classes_;
  bool started_{false};
};
