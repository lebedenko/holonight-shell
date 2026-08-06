#include "ActiveWindowService.h"

#include "HyprlandIpc.h"

#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

Q_LOGGING_CATEGORY(lcAws, "holonight.activewindow")

static void addMonitorWindow(ActiveWindowState& state, ActiveWindowStateChange& change, const QString& monitor_name,
                             const QString& app_class, const QString& title) {
  const HyprlandActiveWindow& old_entry = state.monitor_windows.value(monitor_name);
  if (old_entry.app_class == app_class && old_entry.title == title) {
    return;
  }

  state.monitor_windows.insert(
      monitor_name,
      HyprlandActiveWindow{.app_class = app_class, .title = title, .category = state.category_cache.value(app_class)});
  if (!change.changed_monitors.contains(monitor_name)) {
    change.changed_monitors.append(monitor_name);
  }
  if (!app_class.isEmpty()) {
    change.classes_to_resolve.insert(app_class);
  }
}

static void setMonitorWorkspace(ActiveWindowState& state, ActiveWindowStateChange& change, const QString& monitor_name,
                                int workspace_id) {
  if (monitor_name.isEmpty() || workspace_id <= 0 || state.monitor_workspaces.value(monitor_name, -1) == workspace_id) {
    return;
  }

  state.monitor_workspaces.insert(monitor_name, workspace_id);
  if (!change.workspace_changed_monitors.contains(monitor_name)) {
    change.workspace_changed_monitors.append(monitor_name);
  }
  if (!change.changed_monitors.contains(monitor_name)) {
    change.changed_monitors.append(monitor_name);
  }
}

ActiveWindowStateChange applyActiveWindowEvent(ActiveWindowState& state, const QByteArray& line) {
  ActiveWindowStateChange change;

  const std::optional<HyprlandActiveWindow> active_window = parseHyprlandActiveWindowEvent(line);
  if (active_window.has_value()) {
    if (!state.focused_monitor_name.isEmpty()) {
      addMonitorWindow(state, change, state.focused_monitor_name, active_window->app_class, active_window->title);
    }
    return change;
  }

  const std::optional<HyprlandFocusedMonitor> focused_monitor = parseHyprlandFocusedMonitorEvent(line);
  if (focused_monitor.has_value()) {
    state.focused_monitor_name = focused_monitor->monitor_name;
    bool parsed_ok = false;
    const int workspace_id = focused_monitor->workspace_name.toInt(&parsed_ok);
    if (parsed_ok) {
      setMonitorWorkspace(state, change, focused_monitor->monitor_name, workspace_id);
    }
    return change;
  }

  const std::optional<HyprlandOpenWindow> open_window = parseHyprlandOpenWindowEvent(line);
  if (open_window.has_value()) {
    bool parsed_ok = false;
    const int ws_id = open_window->workspace_name.toInt(&parsed_ok);
    if (parsed_ok) {
      for (auto it = state.monitor_workspaces.constBegin(); it != state.monitor_workspaces.constEnd(); ++it) {
        if (it.value() == ws_id) {
          addMonitorWindow(state, change, it.key(), open_window->app_class, open_window->title);
          break;
        }
      }
    } else {
      qCInfo(lcAws) << "ActiveWindowService: openwindow workspace name not numeric:" << open_window->workspace_name;
    }
    return change;
  }

  change.requery_requested = line.startsWith("closewindow>>") || line.startsWith("movewindow>>") ||
                             line.startsWith("workspace>>") || line.startsWith("destroyworkspace>>");
  return change;
}

ActiveWindowStateChange applyActiveWindowSnapshot(ActiveWindowState& state,
                                                  const QHash<QString, int>& monitor_workspaces,
                                                  const QList<HyprlandClientInfo>& clients) {
  ActiveWindowStateChange change;

  for (auto it = state.monitor_workspaces.begin(); it != state.monitor_workspaces.end();) {
    if (monitor_workspaces.contains(it.key())) {
      ++it;
      continue;
    }
    const QString monitor_name = it.key();
    it = state.monitor_workspaces.erase(it);
    change.workspace_changed_monitors.append(monitor_name);
    if (!change.changed_monitors.contains(monitor_name)) {
      change.changed_monitors.append(monitor_name);
    }
  }
  for (auto it = state.monitor_windows.begin(); it != state.monitor_windows.end();) {
    if (monitor_workspaces.contains(it.key())) {
      ++it;
      continue;
    }
    const QString monitor_name = it.key();
    it = state.monitor_windows.erase(it);
    if (!change.changed_monitors.contains(monitor_name)) {
      change.changed_monitors.append(monitor_name);
    }
  }
  if (!state.focused_monitor_name.isEmpty() && !monitor_workspaces.contains(state.focused_monitor_name)) {
    state.focused_monitor_name.clear();
  }

  for (auto it = monitor_workspaces.constBegin(); it != monitor_workspaces.constEnd(); ++it) {
    const QString& mon = it.key();
    const int ws_id = it.value();

    setMonitorWorkspace(state, change, mon, ws_id);

    HyprlandActiveWindow new_entry;
    // focusHistoryID: 0 = most recently focused (empirically verified on Hyprland 0.55.2)
    int best_focus_id = std::numeric_limits<int>::max();
    for (const HyprlandClientInfo& client : clients) {
      if (client.workspace_id == ws_id && client.focus_history_id < best_focus_id) {
        best_focus_id = client.focus_history_id;
        new_entry.app_class = client.app_class;
        new_entry.title = client.title;
      }
    }

    addMonitorWindow(state, change, mon, new_entry.app_class, new_entry.title);
  }
  return change;
}

ActiveWindowService::ActiveWindowService(QObject* parent)
    : ActiveWindowService(std::make_unique<HyprlandIpcClient>(QStringLiteral("ActiveWindowService:")), parent) {}

ActiveWindowService::ActiveWindowService(HyprlandIpcTransportPtr ipc_client, QObject* parent)
    : QObject(parent), ipc_client_(std::move(ipc_client)) {}

void ActiveWindowService::start() {
  if (started_) {
    return;
  }
  started_ = true;
  connectSocket();
}

QString ActiveWindowService::titleForMonitor(const QString& monitor_name) const {
  return active_window_state_.monitor_windows.value(monitor_name).title;
}

QString ActiveWindowService::appClassForMonitor(const QString& monitor_name) const {
  return active_window_state_.monitor_windows.value(monitor_name).app_class;
}

QString ActiveWindowService::categoryForMonitor(const QString& monitor_name) const {
  return active_window_state_.monitor_windows.value(monitor_name).category;
}

QString ActiveWindowService::focusedMonitorName() const { return active_window_state_.focused_monitor_name; }

int ActiveWindowService::visibleWorkspaceIdForMonitor(const QString& monitor_name) const {
  return active_window_state_.monitor_workspaces.value(monitor_name, -1);
}

void ActiveWindowService::connectSocket() {
  connect(ipc_client_.get(), &HyprlandIpcTransport::eventStreamConnected, this,
          &ActiveWindowService::onEventSocketConnected, Qt::UniqueConnection);
  connect(ipc_client_.get(), &HyprlandIpcTransport::eventLineReceived, this, &ActiveWindowService::processEventLine,
          Qt::UniqueConnection);
  connect(ipc_client_.get(), &HyprlandIpcTransport::commandFinished, this, &ActiveWindowService::onCommandFinished,
          Qt::UniqueConnection);
  ipc_client_->connectEventStream();
}

void ActiveWindowService::queryAllMonitorWindows() {
  if (command_phase_ != CommandPhase::Idle) {
    requery_pending_ = true;
    return;
  }

  command_phase_ = CommandPhase::Monitors;
  const bool started = ipc_client_->runCommand(QByteArrayLiteral("j/monitors"), [](const QByteArray& response) {
    return parseHyprlandMonitorsJson(response).has_value();
  });
  if (!started) {
    command_phase_ = CommandPhase::Idle;
  }
}

void ActiveWindowService::processEventLine(const QByteArray& line) {
  const QString prev_focused = active_window_state_.focused_monitor_name;
  const ActiveWindowStateChange change = applyActiveWindowEvent(active_window_state_, line);
  if (active_window_state_.focused_monitor_name != prev_focused) {
    emit focusedMonitorChanged(active_window_state_.focused_monitor_name);
  }
  for (const QString& monitor : change.workspace_changed_monitors) {
    emit visibleWorkspaceChanged(monitor);
  }
  for (const QString& monitor : change.changed_monitors) {
    emit monitorWindowChanged(monitor);
  }
  for (const QString& app_class : change.classes_to_resolve) {
    scheduleResolveCategory(app_class, QString());
  }

  if (change.requery_requested) {
    queryAllMonitorWindows();
  }
}

void ActiveWindowService::onEventSocketConnected() { queryAllMonitorWindows(); }

void ActiveWindowService::onCommandFinished(const QByteArray& response, bool success) {
  finishCommandResponse(response, success);
}

void ActiveWindowService::advanceToClientsPhase() {
  command_phase_ = CommandPhase::Clients;
  const bool started = ipc_client_->runCommand(QByteArrayLiteral("j/clients"), [](const QByteArray& response) {
    return parseHyprlandClientsJson(response).has_value();
  });
  if (!started) {
    command_phase_ = CommandPhase::Idle;
  }
}

void ActiveWindowService::finishCommandResponse(const QByteArray& response, bool parse_buffer) {
  const CommandPhase finished_phase = command_phase_;

  if (parse_buffer && finished_phase == CommandPhase::Monitors) {
    const auto monitors = parseHyprlandMonitorsJson(response);
    if (monitors.has_value()) {
      pending_monitor_workspaces_ = *monitors;
      pending_monitor_snapshot_ = true;
      const auto focused_monitor = parseHyprlandFocusedMonitorNameJson(response);
      if (focused_monitor.has_value() && !focused_monitor->isEmpty() &&
          active_window_state_.focused_monitor_name != *focused_monitor) {
        active_window_state_.focused_monitor_name = *focused_monitor;
        emit focusedMonitorChanged(active_window_state_.focused_monitor_name);
      }
    } else {
      qCWarning(lcAws) << "ActiveWindowService: failed to parse j/monitors response";
    }
  } else if (parse_buffer && finished_phase == CommandPhase::Clients) {
    const auto clients = parseHyprlandClientsJson(response);
    if (clients.has_value()) {
      pending_clients_ = *clients;
      pending_clients_snapshot_ = true;
    } else {
      qCWarning(lcAws) << "ActiveWindowService: failed to parse j/clients response";
    }
  }

  if (finished_phase == CommandPhase::Monitors && pending_monitor_snapshot_) {
    advanceToClientsPhase();
    if (command_phase_ == CommandPhase::Clients) {
      return;
    }
  }

  if (finished_phase == CommandPhase::Clients && pending_monitor_snapshot_ && pending_clients_snapshot_) {
    applyMonitorWindowsFromPending();
  }

  command_phase_ = CommandPhase::Idle;
  pending_monitor_snapshot_ = false;
  pending_clients_snapshot_ = false;
  pending_monitor_workspaces_.clear();
  pending_clients_.clear();

  if (requery_pending_) {
    requery_pending_ = false;
    queryAllMonitorWindows();
  }
}

void ActiveWindowService::applyMonitorWindowsFromPending() {
  const QString previous_focused_monitor = active_window_state_.focused_monitor_name;
  const ActiveWindowStateChange change =
      applyActiveWindowSnapshot(active_window_state_, pending_monitor_workspaces_, pending_clients_);
  if (active_window_state_.focused_monitor_name != previous_focused_monitor) {
    emit focusedMonitorChanged(active_window_state_.focused_monitor_name);
  }
  for (const QString& monitor : change.workspace_changed_monitors) {
    emit visibleWorkspaceChanged(monitor);
  }
  for (const QString& monitor : change.changed_monitors) {
    emit monitorWindowChanged(monitor);
  }
  for (const QString& app_class : change.classes_to_resolve) {
    scheduleResolveCategory(app_class, QString());
  }
}

void ActiveWindowService::scheduleResolveCategory(const QString& app_class, const QString& /*monitor_name*/) {
  if (app_class.isEmpty()) {
    return;
  }
  if (resolved_classes_.contains(app_class)) {
    const QString cached = active_window_state_.category_cache.value(app_class);
    for (auto it = active_window_state_.monitor_windows.begin(); it != active_window_state_.monitor_windows.end();
         ++it) {
      if (it->app_class == app_class && it->category != cached) {
        it->category = cached;
        emit monitorWindowChanged(it.key());
      }
    }
    return;
  }

  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
  auto* watcher = new QFutureWatcher<QString>(this);
  connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, app_class] {
    const QString resolved = watcher->result();
    resolved_classes_.insert(app_class);
    active_window_state_.category_cache.insert(app_class, resolved);
    for (auto it = active_window_state_.monitor_windows.begin(); it != active_window_state_.monitor_windows.end();
         ++it) {
      if (it->app_class == app_class && it->category != resolved) {
        it->category = resolved;
        emit monitorWindowChanged(it.key());
      }
    }
    watcher->deleteLater();
  });
  watcher->setFuture(QtConcurrent::run(&ActiveWindowService::scanDesktopFiles, app_class));
}

QString activeWindowCategoryForDesktopCategories(const QString& categories_field) {
  static const std::array<std::pair<QLatin1StringView, QLatin1StringView>, 12> kPriority{{
      {QLatin1String("WebBrowser"), QLatin1String("browser")},
      {QLatin1String("TextEditor"), QLatin1String("editor")},
      {QLatin1String("Development"), QLatin1String("editor")},
      {QLatin1String("TerminalEmulator"), QLatin1String("terminal")},
      {QLatin1String("FileManager"), QLatin1String("files")},
      {QLatin1String("InstantMessaging"), QLatin1String("chat")},
      {QLatin1String("Chat"), QLatin1String("chat")},
      {QLatin1String("Audio"), QLatin1String("music")},
      {QLatin1String("Music"), QLatin1String("music")},
      {QLatin1String("Video"), QLatin1String("video")},
      {QLatin1String("Settings"), QLatin1String("settings")},
      {QLatin1String("System"), QLatin1String("settings")},
  }};

  const QStringList tokens = categories_field.split(QLatin1Char(';'), Qt::SkipEmptyParts);
  for (const auto& [token, icon] : kPriority) {
    if (tokens.contains(QString(token))) {
      return {icon};
    }
  }
  return {};
}

QString activeWindowCategoriesFromDesktopEntry(const QString& contents, const QString& app_class_lower) {
  const QStringList lines = contents.split(QLatin1Char('\n'));
  QString first_categories;
  QString last_categories;
  bool in_desktop_entry = false;
  bool matched = app_class_lower.isEmpty();
  bool found_categories = false;
  for (const QString& line : lines) {
    if (line.startsWith(QLatin1Char('['))) {
      if (line == QLatin1String("[Desktop Entry]")) {
        in_desktop_entry = true;
      } else if (in_desktop_entry) {
        break;
      }
      continue;
    }
    if (!in_desktop_entry) {
      continue;
    }
    const qsizetype sep_pos = line.indexOf(QLatin1Char('='));
    if (sep_pos < 0) {
      continue;
    }
    const QString key = line.left(sep_pos);
    const QString value = line.mid(static_cast<int>(sep_pos) + 1);
    if (key == QLatin1String("Categories")) {
      if (!found_categories) {
        first_categories = value;
        found_categories = true;
      }
      last_categories = value;
    } else if (key == QLatin1String("Name") || key == QLatin1String("Exec")) {
      matched = matched || value.toLower().contains(app_class_lower);
    }
  }
  if (app_class_lower.isEmpty()) {
    return first_categories;
  }
  return matched ? last_categories : QString();
}

static QString readDesktopEntryCategories(const QString& path, const QString& app_class_lower) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  return activeWindowCategoriesFromDesktopEntry(QString::fromUtf8(file.readAll()), app_class_lower);
}

static QString scanSingleDesktopFile(const QString& path, const QString& app_class_lower) {
  return readDesktopEntryCategories(path, app_class_lower);
}

QString ActiveWindowService::scanDesktopFiles(const QString& app_class) {
  const QStringList search_dirs = {
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/applications/"),
      QLatin1String("/usr/share/applications/"),
  };

  // Pass 1: exact filename match
  for (const QString& dir : search_dirs) {
    for (const QString& name : {app_class, app_class.toLower()}) {
      const QString path = dir + name + QLatin1String(".desktop");
      if (QFile::exists(path)) {
        const QString categories = readDesktopEntryCategories(path, QString());
        if (!categories.isEmpty()) {
          return activeWindowCategoryForDesktopCategories(categories);
        }
      }
    }
  }

  // Pass 2: case-insensitive Name= / Exec= scan
  const QString app_class_lower = app_class.toLower();
  for (const QString& dir : search_dirs) {
    const QFileInfoList entries = QDir(dir).entryInfoList({QLatin1String("*.desktop")}, QDir::Files);
    for (const QFileInfo& entry : entries) {
      const QString categories = scanSingleDesktopFile(entry.absoluteFilePath(), app_class_lower);
      if (!categories.isEmpty()) {
        return activeWindowCategoryForDesktopCategories(categories);
      }
    }
  }

  return {};
}
