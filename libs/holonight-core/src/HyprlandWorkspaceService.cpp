#include "HyprlandWorkspaceService.h"

#include "HyprlandIpc.h"
#include "WorkspaceModel.h"

#include <QLoggingCategory>

#include <algorithm>
#include <memory>

Q_LOGGING_CATEGORY(lcHyprlandWorkspace, "holonight.hyprland.workspace")

HyprlandWorkspaceEventUpdate hyprlandWorkspaceEventUpdate(const QByteArray& line) {
  HyprlandWorkspaceEventUpdate update;

  const std::optional<int> workspace_id = parseHyprlandWorkspaceEvent(line);
  if (workspace_id.has_value()) {
    update.focused_workspace_id = *workspace_id;
  }

  const std::optional<HyprlandFocusedMonitor> focused_monitor = parseHyprlandFocusedMonitorEvent(line);
  if (focused_monitor.has_value()) {
    bool parsed_ok = false;
    const int focused_monitor_workspace_id = focused_monitor->workspace_name.toInt(&parsed_ok);
    if (parsed_ok) {
      update.focused_workspace_id = focused_monitor_workspace_id;
    }
  }

  const std::optional<QString> urgent_window_address = parseHyprlandUrgentWindowEvent(line);
  if (urgent_window_address.has_value()) {
    update.urgent_window_address = *urgent_window_address;
  }

  update.refresh_workspaces = isHyprlandWorkspaceRefreshEvent(line);
  return update;
}

QByteArray hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery query, int workspace_id) {
  switch (query) {
    case HyprlandWorkspacePendingQuery::ActiveWorkspace:
      return QByteArrayLiteral("j/activeworkspace");
    case HyprlandWorkspacePendingQuery::Workspaces:
      return QByteArrayLiteral("j/workspaces");
    case HyprlandWorkspacePendingQuery::DispatchWorkspace:
      return QByteArrayLiteral("dispatch workspace ") + QByteArray::number(workspace_id);
    case HyprlandWorkspacePendingQuery::UrgentClient:
      return QByteArrayLiteral("j/clients");
    case HyprlandWorkspacePendingQuery::None:
      return {};
  }
  return {};
}

QByteArray hyprlandLuaWorkspaceFocusCommand(int workspaceId) {
  return QByteArrayLiteral("dispatch hl.dsp.focus({ workspace = ") + QByteArray::number(workspaceId) +
         QByteArrayLiteral(" })");
}

bool hyprlandIpcResponseIsError(const QByteArray& response) {
  return response.trimmed().toLower().startsWith(QByteArrayLiteral("error:"));
}

void queueHyprlandWorkspaceCommand(HyprlandWorkspaceCommandQueue* queue, HyprlandWorkspacePendingQuery query,
                                   const QByteArray& command) {
  if (queue == nullptr) {
    return;
  }
  if (query == HyprlandWorkspacePendingQuery::ActiveWorkspace) {
    queue->queued_active_query = true;
  } else if (query == HyprlandWorkspacePendingQuery::Workspaces) {
    queue->queued_workspaces_query = true;
  } else if (query == HyprlandWorkspacePendingQuery::DispatchWorkspace) {
    const qsizetype space = command.lastIndexOf(' ');
    queue->queued_dispatch_workspace_id = command.sliced(space + 1).toInt();
  } else if (query == HyprlandWorkspacePendingQuery::UrgentClient) {
    const QString address = QString::fromUtf8(command).trimmed();
    if (!address.isEmpty() && !queue->queued_urgent_window_addresses.contains(address)) {
      queue->queued_urgent_window_addresses.append(address);
    }
  }
}

std::optional<HyprlandWorkspaceQueuedCommand> takeNextHyprlandWorkspaceCommand(HyprlandWorkspaceCommandQueue* queue) {
  if (queue == nullptr) {
    return std::nullopt;
  }
  if (queue->queued_dispatch_workspace_id > 0) {
    const int workspace_id = queue->queued_dispatch_workspace_id;
    queue->queued_dispatch_workspace_id = 0;
    return HyprlandWorkspaceQueuedCommand{
        .query = HyprlandWorkspacePendingQuery::DispatchWorkspace,
        .command = hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery::DispatchWorkspace, workspace_id),
    };
  }
  if (queue->queued_active_query) {
    queue->queued_active_query = false;
    return HyprlandWorkspaceQueuedCommand{
        .query = HyprlandWorkspacePendingQuery::ActiveWorkspace,
        .command = hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery::ActiveWorkspace),
    };
  }
  if (!queue->queued_urgent_window_addresses.isEmpty()) {
    const QString address = queue->queued_urgent_window_addresses.takeFirst();
    return HyprlandWorkspaceQueuedCommand{
        .query = HyprlandWorkspacePendingQuery::UrgentClient,
        .command = address.toUtf8(),
    };
  }
  if (queue->queued_workspaces_query) {
    queue->queued_workspaces_query = false;
    return HyprlandWorkspaceQueuedCommand{
        .query = HyprlandWorkspacePendingQuery::Workspaces,
        .command = hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery::Workspaces),
    };
  }
  return std::nullopt;
}

HyprlandWorkspaceCommandResult hyprlandWorkspaceCommandResult(HyprlandWorkspacePendingQuery query,
                                                              const QByteArray& response, bool parse_buffer,
                                                              const QString& urgent_window_address) {
  HyprlandWorkspaceCommandResult result;
  if (!parse_buffer) {
    return result;
  }

  if (query == HyprlandWorkspacePendingQuery::ActiveWorkspace) {
    result.focused_workspace_id = parseHyprlandActiveWorkspaceJson(response);
  } else if (query == HyprlandWorkspacePendingQuery::Workspaces) {
    const std::optional<HyprlandWorkspaceSnapshot> snapshot = parseHyprlandWorkspacesJson(response);
    if (snapshot.has_value()) {
      result.occupied_workspace_ids = snapshot->occupied_workspace_ids;
    }
  } else if (query == HyprlandWorkspacePendingQuery::DispatchWorkspace) {
    result.refresh_active_workspace = true;
    result.refresh_workspaces = true;
  } else if (query == HyprlandWorkspacePendingQuery::UrgentClient) {
    result.urgent_workspace_id = workspaceIdForHyprlandClientAddressJson(response, urgent_window_address);
  }
  return result;
}

HyprlandWorkspaceService::HyprlandWorkspaceService(WorkspaceModel* model, QObject* parent)
    : HyprlandWorkspaceService(model, std::make_unique<HyprlandIpcClient>(QStringLiteral("HyprlandWorkspaceService:")),
                               parent) {}

HyprlandWorkspaceService::HyprlandWorkspaceService(WorkspaceModel* model, HyprlandIpcTransportPtr ipc_client,
                                                   QObject* parent)
    : QObject(parent), model_(model), ipc_client_(std::move(ipc_client)) {
  connect(model_, &WorkspaceModel::activateWorkspaceRequested, this, &HyprlandWorkspaceService::activateWorkspace);
}

void HyprlandWorkspaceService::start() {
  if (started_) {
    return;
  }
  started_ = true;
  connectSocket();
}

void HyprlandWorkspaceService::connectSocket() {
  connect(ipc_client_.get(), &HyprlandIpcTransport::eventStreamConnected, this,
          &HyprlandWorkspaceService::onEventSocketConnected, Qt::UniqueConnection);
  connect(ipc_client_.get(), &HyprlandIpcTransport::eventLineReceived, this,
          &HyprlandWorkspaceService::processEventLine, Qt::UniqueConnection);
  connect(ipc_client_.get(), &HyprlandIpcTransport::commandFinished, this, &HyprlandWorkspaceService::onCommandFinished,
          Qt::UniqueConnection);
  ipc_client_->connectEventStream();
}

void HyprlandWorkspaceService::processEventLine(const QByteArray& line) {
  const HyprlandWorkspaceEventUpdate update = hyprlandWorkspaceEventUpdate(line);
  if (update.focused_workspace_id.has_value()) {
    model_->setFocusedWorkspaceId(*update.focused_workspace_id);
  }
  if (update.urgent_window_address.has_value()) {
    queryUrgentClientWorkspace(*update.urgent_window_address);
  }
  if (update.refresh_workspaces) {
    queryWorkspaces();
  }
}

void HyprlandWorkspaceService::onEventSocketConnected() {
  queryActiveWorkspace();
  queryWorkspaces();
}

void HyprlandWorkspaceService::queryActiveWorkspace() {
  startCommand(HyprlandWorkspacePendingQuery::ActiveWorkspace,
               hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery::ActiveWorkspace));
}

void HyprlandWorkspaceService::queryWorkspaces() {
  startCommand(HyprlandWorkspacePendingQuery::Workspaces,
               hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery::Workspaces));
}

void HyprlandWorkspaceService::queryUrgentClientWorkspace(const QString& address) {
  startCommand(HyprlandWorkspacePendingQuery::UrgentClient, address.toUtf8());
}

void HyprlandWorkspaceService::activateWorkspace(int wsId) {
  if (wsId <= 0) {
    return;
  }
  qCInfo(lcHyprlandWorkspace) << "Dispatching workspace activation" << wsId;
  startCommand(HyprlandWorkspacePendingQuery::DispatchWorkspace,
               hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery::DispatchWorkspace, wsId), wsId);
}

void HyprlandWorkspaceService::startCommand(HyprlandWorkspacePendingQuery query, const QByteArray& command,
                                            int dispatchWorkspaceId, bool dispatchUsesLua) {
  if (ipc_client_->hasRunningCommand()) {
    queueHyprlandWorkspaceCommand(&command_queue_, query, command);
    return;
  }

  pending_query_ = query;
  active_command_ = command;
  pending_dispatch_workspace_id_ = query == HyprlandWorkspacePendingQuery::DispatchWorkspace ? dispatchWorkspaceId : 0;
  pending_dispatch_uses_lua_ = query == HyprlandWorkspacePendingQuery::DispatchWorkspace && dispatchUsesLua;
  if (query == HyprlandWorkspacePendingQuery::UrgentClient) {
    pending_urgent_window_address_ = QString::fromUtf8(command);
  } else {
    pending_urgent_window_address_.clear();
  }
  const QByteArray wire_command =
      query == HyprlandWorkspacePendingQuery::UrgentClient ? hyprlandWorkspaceCommand(query) : command;
  if (!ipc_client_->runCommand(wire_command)) {
    if (query == HyprlandWorkspacePendingQuery::DispatchWorkspace) {
      qCWarning(lcHyprlandWorkspace) << "Failed to submit workspace activation" << pending_dispatch_workspace_id_;
    }
    active_command_.clear();
    pending_urgent_window_address_.clear();
    pending_query_ = HyprlandWorkspacePendingQuery::None;
    pending_dispatch_workspace_id_ = 0;
    pending_dispatch_uses_lua_ = false;
    return;
  }
}

void HyprlandWorkspaceService::onCommandFinished(const QByteArray& response, bool success) {
  finishCommandResponse(response, success);
}

void HyprlandWorkspaceService::finishCommandResponse(const QByteArray& response, bool parse_buffer) {
  const HyprlandWorkspacePendingQuery finished_query = pending_query_;
  const int finished_dispatch_workspace_id = pending_dispatch_workspace_id_;
  const bool finished_dispatch_uses_lua = pending_dispatch_uses_lua_;
  const bool dispatch_response_error = finished_query == HyprlandWorkspacePendingQuery::DispatchWorkspace &&
                                       parse_buffer && hyprlandIpcResponseIsError(response);

  if (finished_query == HyprlandWorkspacePendingQuery::DispatchWorkspace) {
    if (dispatch_response_error && !finished_dispatch_uses_lua) {
      qCInfo(lcHyprlandWorkspace) << "Legacy workspace activation was rejected; retrying Lua dispatcher"
                                  << finished_dispatch_workspace_id;
    } else if (parse_buffer && !dispatch_response_error) {
      qCInfo(lcHyprlandWorkspace) << "Workspace activation command completed" << finished_dispatch_workspace_id;
    } else {
      qCWarning(lcHyprlandWorkspace) << "Workspace activation command failed" << finished_dispatch_workspace_id
                                     << response;
    }
  }

  if (parse_buffer && !dispatch_response_error) {
    const HyprlandWorkspaceCommandResult result =
        hyprlandWorkspaceCommandResult(finished_query, response, parse_buffer, pending_urgent_window_address_);
    if (result.focused_workspace_id.has_value()) {
      model_->setFocusedWorkspaceId(*result.focused_workspace_id);
    }
    if (result.occupied_workspace_ids.has_value()) {
      model_->setOccupiedWorkspaceIds(*result.occupied_workspace_ids);
    }
    if (result.urgent_workspace_id.has_value()) {
      model_->addUrgentWorkspaceId(*result.urgent_workspace_id);
    }
    if (result.refresh_active_workspace) {
      queryActiveWorkspace();
    }
    if (result.refresh_workspaces) {
      queryWorkspaces();
    }
  }

  active_command_.clear();
  pending_urgent_window_address_.clear();
  pending_query_ = HyprlandWorkspacePendingQuery::None;
  pending_dispatch_workspace_id_ = 0;
  pending_dispatch_uses_lua_ = false;

  if (dispatch_response_error && !finished_dispatch_uses_lua) {
    startCommand(HyprlandWorkspacePendingQuery::DispatchWorkspace,
                 hyprlandLuaWorkspaceFocusCommand(finished_dispatch_workspace_id), finished_dispatch_workspace_id,
                 true);
    return;
  }
  runQueuedCommand();
}

void HyprlandWorkspaceService::runQueuedCommand() {
  const std::optional<HyprlandWorkspaceQueuedCommand> command = takeNextHyprlandWorkspaceCommand(&command_queue_);
  if (command.has_value()) {
    const int workspace_id = command->query == HyprlandWorkspacePendingQuery::DispatchWorkspace
                                 ? command->command.sliced(command->command.lastIndexOf(' ') + 1).toInt()
                                 : 0;
    startCommand(command->query, command->command, workspace_id);
  }
}
