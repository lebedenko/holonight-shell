#include "HyprlandBackend.h"

#include "HyprlandIpc.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>
#include <limits>
#include <utility>

namespace {
bool responseIsError(const QByteArray& response) {
  return response.trimmed().toLower().startsWith(QByteArrayLiteral("error:"));
}

QByteArray escapeLuaString(QString value) {
  value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
  value.replace(QStringLiteral("\""), QStringLiteral("\\\""));
  return value.toUtf8();
}

bool addressMatches(const QSet<QString>& urgent_addresses, const QString& address) {
  return std::ranges::any_of(
      urgent_addresses, [&address](const QString& urgent) { return address.endsWith(urgent, Qt::CaseInsensitive); });
}

void collectMonitors(const QJsonArray& monitors, CompositorSnapshot* snapshot, QHash<int, QString>* outputs,
                     QSet<int>* active_workspaces) {
  for (const auto value : monitors) {
    const QJsonObject monitor = value.toObject();
    const QString output = monitor.value(QStringLiteral("name")).toString();
    const auto collect_active = [&output, outputs, active_workspaces](const QJsonObject& workspace) {
      const int workspace_id = workspace.value(QStringLiteral("id")).toInt();
      if (!output.isEmpty() && workspace_id != 0) {
        outputs->insert(workspace_id, output);
        active_workspaces->insert(workspace_id);
      }
    };
    collect_active(monitor.value(QStringLiteral("activeWorkspace")).toObject());
    collect_active(monitor.value(QStringLiteral("specialWorkspace")).toObject());
    if (monitor.value(QStringLiteral("focused")).toBool(false)) {
      snapshot->focused_output = output;
    }
  }
}

void collectClients(const QList<HyprlandClientInfo>& clients, QHash<int, int>* window_counts,
                    QHash<int, HyprlandClientInfo>* focused_clients) {
  for (const HyprlandClientInfo& client : clients) {
    ++(*window_counts)[client.workspace_id];
    if (!focused_clients->contains(client.workspace_id) ||
        client.focus_history_id < focused_clients->value(client.workspace_id).focus_history_id) {
      focused_clients->insert(client.workspace_id, client);
    }
  }
}

void appendWorkspaces(const QJsonArray& workspaces, const QList<HyprlandClientInfo>& clients,
                      const QHash<int, QString>& workspace_outputs, const QSet<int>& active_workspaces,
                      const QHash<int, int>& window_counts, const QHash<int, HyprlandClientInfo>& focused_clients,
                      QSet<QString>* urgent_addresses, CompositorSnapshot* snapshot) {
  int order = 0;
  for (const auto value : workspaces) {
    const QJsonObject workspace = value.toObject();
    const int workspace_id = workspace.value(QStringLiteral("id")).toInt();
    const QString name = workspace.value(QStringLiteral("name")).toString();
    if (workspace_id == 0 || name.isEmpty()) {
      continue;
    }
    const bool special = workspace_id < 0 || name.startsWith(QStringLiteral("special:"));
    const bool active = active_workspaces.contains(workspace_id);
    const bool urgent =
        !special && std::ranges::any_of(clients, [urgent_addresses, workspace_id](const HyprlandClientInfo& client) {
          return client.workspace_id == workspace_id && addressMatches(*urgent_addresses, client.address);
        });
    if (active) {
      urgent_addresses->removeIf([&clients, workspace_id](const QString& address) {
        return std::ranges::any_of(clients, [&address, workspace_id](const HyprlandClientInfo& client) {
          return client.workspace_id == workspace_id && client.address.endsWith(address, Qt::CaseInsensitive);
        });
      });
    }
    snapshot->workspaces.append({.id = special ? name : QString::number(workspace_id),
                                 .numeric_slot = special ? std::nullopt : std::optional<int>{workspace_id},
                                 .display_name = name,
                                 .stable_order = order++,
                                 .kind = special ? QStringLiteral("special") : QStringLiteral("normal"),
                                 .outputs = workspace_outputs.contains(workspace_id)
                                                ? QStringList{workspace_outputs.value(workspace_id)}
                                                : QStringList{},
                                 .active = active,
                                 .focused = active && workspace_outputs.value(workspace_id) == snapshot->focused_output,
                                 .urgent = urgent,
                                 .occupied = window_counts.value(workspace_id) > 0});
    if (active && focused_clients.contains(workspace_id)) {
      const HyprlandClientInfo& client = focused_clients[workspace_id];
      snapshot->active_windows.insert(workspace_outputs.value(workspace_id),
                                      {.app_id = client.app_class, .title = client.title});
    }
  }
}
}  // namespace

HyprlandBackend::HyprlandBackend(HyprlandIpcTransportPtr transport, QObject* parent)
    : CompositorBackend(parent),
      transport_(transport ? std::move(transport)
                           : std::make_unique<HyprlandIpcClient>(QStringLiteral("CompositorService:"))) {
  connect(transport_.get(), &HyprlandIpcTransport::eventStreamConnected, this, &HyprlandBackend::scheduleRefresh);
  connect(transport_.get(), &HyprlandIpcTransport::eventStreamDisconnected, this,
          [this] { fail(QStringLiteral("Hyprland IPC disconnected")); });
  connect(transport_.get(), &HyprlandIpcTransport::eventLineReceived, this, &HyprlandBackend::handleEvent);
  connect(transport_.get(), &HyprlandIpcTransport::commandFinished, this, &HyprlandBackend::handleCommand);
}

void HyprlandBackend::start() { transport_->connectEventStream(); }

void HyprlandBackend::scheduleRefresh() {
  if (phase_ != Phase::Idle || transport_->hasRunningCommand()) {
    refresh_dirty_ = true;
    return;
  }
  QTimer::singleShot(0, this, &HyprlandBackend::beginRefresh);
}

void HyprlandBackend::beginRefresh() {
  if (phase_ != Phase::Idle || transport_->hasRunningCommand()) {
    refresh_dirty_ = true;
    return;
  }
  refresh_dirty_ = false;
  phase_ = Phase::Monitors;
  if (!transport_->runCommand(QByteArrayLiteral("j/monitors"))) {
    fail(QStringLiteral("Hyprland monitor query failed"));
  }
}

void HyprlandBackend::handleEvent(const QByteArray& line) {
  if (const auto urgent = parseHyprlandUrgentWindowEvent(line)) {
    urgent_addresses_.insert(*urgent);
  }
  scheduleRefresh();
}

void HyprlandBackend::handleCommand(const QByteArray& response, bool success) {
  if (!success) {
    phase_ = Phase::Idle;
    fail(QStringLiteral("Hyprland snapshot refresh failed"));
    return;
  }
  if (phase_ == Phase::Monitors) {
    monitors_ = response;
    phase_ = Phase::Workspaces;
    if (!transport_->runCommand(QByteArrayLiteral("j/workspaces"))) {
      fail(QStringLiteral("Hyprland workspace query failed"));
    }
  } else if (phase_ == Phase::Workspaces) {
    workspaces_ = response;
    phase_ = Phase::Clients;
    if (!transport_->runCommand(QByteArrayLiteral("j/clients"))) {
      fail(QStringLiteral("Hyprland client query failed"));
    }
  } else if (phase_ == Phase::Clients) {
    phase_ = Phase::Idle;
    publishClients(response);
    if (!pending_activation_.isEmpty()) {
      const QString activation = std::exchange(pending_activation_, {});
      activateWorkspace(activation);
    } else if (refresh_dirty_) {
      scheduleRefresh();
    }
  } else if (phase_ == Phase::Activation) {
    if (responseIsError(response)) {
      runLuaActivation();
      return;
    }
    phase_ = Phase::Idle;
    scheduleRefresh();
  } else if (phase_ == Phase::LuaActivation) {
    phase_ = Phase::Idle;
    if (responseIsError(response)) {
      fail(QStringLiteral("Hyprland workspace activation failed"));
    }
    scheduleRefresh();
  }
}

void HyprlandBackend::runLuaActivation() {
  phase_ = Phase::LuaActivation;
  QByteArray command;
  QString diagnostic;
  if (activation_is_special_) {
    const QString name = activation_id_.sliced(QStringLiteral("special:").size());
    command = QByteArrayLiteral("dispatch hl.dsp.workspace.toggle_special(\"") + escapeLuaString(name) +
              QByteArrayLiteral("\")");
    diagnostic = QStringLiteral("Hyprland Lua special workspace activation failed");
  } else {
    command =
        QByteArrayLiteral("dispatch hl.dsp.focus({ workspace = ") + activation_id_.toUtf8() + QByteArrayLiteral(" })");
    diagnostic = QStringLiteral("Hyprland Lua workspace activation failed");
  }
  if (!transport_->runCommand(command)) {
    fail(diagnostic);
  }
}

void HyprlandBackend::publishClients(const QByteArray& clients_json) {
  const QJsonDocument monitors = QJsonDocument::fromJson(monitors_);
  const QJsonDocument workspaces = QJsonDocument::fromJson(workspaces_);
  const auto clients = parseHyprlandClientsJson(clients_json);
  if (!monitors.isArray() || !workspaces.isArray() || !clients) {
    fail(QStringLiteral("invalid Hyprland snapshot response"));
    return;
  }
  CompositorSnapshot snapshot{
      .connected = true,
      .capabilities = {.workspace_listing = true,
                       .workspace_activation = true,
                       .numeric_workspace_creation = true,
                       .special_workspaces = true,
                       .active_window = true,
                       .focused_output = true,
                       .urgency = true,
                       .occupancy = true},
  };
  QHash<int, QString> workspace_outputs;
  QSet<int> active_workspaces;
  collectMonitors(monitors.array(), &snapshot, &workspace_outputs, &active_workspaces);
  QHash<int, int> window_counts;
  QHash<int, HyprlandClientInfo> focused_clients;
  collectClients(*clients, &window_counts, &focused_clients);
  appendWorkspaces(workspaces.array(), *clients, workspace_outputs, active_workspaces, window_counts, focused_clients,
                   &urgent_addresses_, &snapshot);
  emit snapshotReady(std::move(snapshot));
}

void HyprlandBackend::activateWorkspace(const QString& workspace_id) {
  const bool special = workspace_id.startsWith(QStringLiteral("special:"));
  bool valid = false;
  workspace_id.toInt(&valid);
  if (!valid && !special) {
    return;
  }
  if (phase_ != Phase::Idle || transport_->hasRunningCommand()) {
    pending_activation_ = workspace_id;
    return;
  }
  activation_id_ = workspace_id;
  activation_is_special_ = special;
  phase_ = Phase::Activation;
  const QByteArray command = special ? QByteArrayLiteral("dispatch togglespecialworkspace ") +
                                           workspace_id.sliced(QStringLiteral("special:").size()).toUtf8()
                                     : QByteArrayLiteral("dispatch workspace ") + workspace_id.toUtf8();
  if (!transport_->runCommand(command)) {
    phase_ = Phase::Idle;
    fail(QStringLiteral("Hyprland workspace activation failed"));
  }
}

void HyprlandBackend::fail(const QString& diagnostic) {
  phase_ = Phase::Idle;
  emit snapshotReady({.diagnostic = diagnostic});
}
