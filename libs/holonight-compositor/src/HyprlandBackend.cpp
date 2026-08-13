#include "HyprlandBackend.h"

#include "HyprlandIpc.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>
#include <limits>

namespace {
bool responseIsError(const QByteArray& response) {
  return response.trimmed().toLower().startsWith(QByteArrayLiteral("error:"));
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
    if (refresh_dirty_) {
      scheduleRefresh();
    }
  } else if (phase_ == Phase::Activation) {
    if (responseIsError(response)) {
      phase_ = Phase::LuaActivation;
      const QByteArray command = QByteArrayLiteral("dispatch hl.dsp.focus({ workspace = ") + activation_id_.toUtf8() +
                                 QByteArrayLiteral(" })");
      if (!transport_->runCommand(command)) {
        fail(QStringLiteral("Hyprland Lua workspace activation failed"));
      }
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
  for (const QJsonValue& value : monitors.array()) {
    const QJsonObject monitor = value.toObject();
    const QString output = monitor.value(QStringLiteral("name")).toString();
    const int active = monitor.value(QStringLiteral("activeWorkspace")).toObject().value(QStringLiteral("id")).toInt();
    if (!output.isEmpty() && active != 0) {
      workspace_outputs.insert(active, output);
      active_workspaces.insert(active);
    }
    if (monitor.value(QStringLiteral("focused")).toBool(false)) {
      snapshot.focused_output = output;
    }
  }
  QHash<int, int> window_counts;
  QHash<int, HyprlandClientInfo> focused_clients;
  for (const HyprlandClientInfo& client : *clients) {
    ++window_counts[client.workspace_id];
    if (!focused_clients.contains(client.workspace_id) ||
        client.focus_history_id < focused_clients.value(client.workspace_id).focus_history_id) {
      focused_clients.insert(client.workspace_id, client);
    }
    if (urgent_addresses_.contains(client.address) ||
        std::ranges::any_of(urgent_addresses_, [&client](const QString& address) {
          return client.address.endsWith(address, Qt::CaseInsensitive);
        })) {
      // Retained until the workspace becomes focused below.
    }
  }
  int order = 0;
  for (const QJsonValue& value : workspaces.array()) {
    const QJsonObject workspace = value.toObject();
    const int id = workspace.value(QStringLiteral("id")).toInt();
    const QString name = workspace.value(QStringLiteral("name")).toString();
    if (id == 0 || name.isEmpty()) {
      continue;
    }
    const bool special = id < 0 || name.startsWith(QStringLiteral("special:"));
    const bool active = active_workspaces.contains(id);
    bool urgent = false;
    for (const HyprlandClientInfo& client : *clients) {
      if (client.workspace_id == id && std::ranges::any_of(urgent_addresses_, [&client](const QString& address) {
            return client.address.endsWith(address, Qt::CaseInsensitive);
          })) {
        urgent = true;
      }
    }
    if (active) {
      for (const HyprlandClientInfo& client : *clients) {
        urgent_addresses_.remove(client.address);
      }
    }
    snapshot.workspaces.append(
        {.id = QString::number(id),
         .numeric_slot = special ? std::nullopt : std::optional<int>{id},
         .display_name = name,
         .stable_order = order++,
         .kind = special ? QStringLiteral("special") : QStringLiteral("normal"),
         .outputs = workspace_outputs.contains(id) ? QStringList{workspace_outputs.value(id)} : QStringList{},
         .active = active,
         .focused = active && workspace_outputs.value(id) == snapshot.focused_output,
         .urgent = urgent,
         .occupied = window_counts.value(id) > 0});
    if (active && focused_clients.contains(id)) {
      const HyprlandClientInfo& client = focused_clients[id];
      snapshot.active_windows.insert(workspace_outputs.value(id), {.app_id = client.app_class, .title = client.title});
    }
  }
  emit snapshotReady(std::move(snapshot));
}

void HyprlandBackend::activateWorkspace(const QString& workspace_id) {
  bool valid = false;
  workspace_id.toInt(&valid);
  if (!valid || phase_ != Phase::Idle || transport_->hasRunningCommand()) {
    return;
  }
  activation_id_ = workspace_id;
  phase_ = Phase::Activation;
  if (!transport_->runCommand(QByteArrayLiteral("dispatch workspace ") + workspace_id.toUtf8())) {
    phase_ = Phase::Idle;
    fail(QStringLiteral("Hyprland workspace activation failed"));
  }
}

void HyprlandBackend::fail(const QString& diagnostic) {
  phase_ = Phase::Idle;
  emit snapshotReady({.diagnostic = diagnostic});
}
