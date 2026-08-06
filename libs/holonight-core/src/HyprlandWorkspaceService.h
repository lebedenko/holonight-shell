#pragma once

#include "HyprlandIpcClient.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include <memory>
#include <optional>

class WorkspaceModel;

enum class HyprlandWorkspacePendingQuery : uint8_t {
  None,
  ActiveWorkspace,
  Workspaces,
  DispatchWorkspace,
  UrgentClient
};

struct HyprlandWorkspaceEventUpdate {
  std::optional<int> focused_workspace_id;
  std::optional<QString> urgent_window_address;
  bool refresh_workspaces{false};
};

struct HyprlandWorkspaceCommandResult {
  std::optional<int> focused_workspace_id;
  std::optional<QSet<int>> occupied_workspace_ids;
  std::optional<int> urgent_workspace_id;
  bool refresh_active_workspace{false};
  bool refresh_workspaces{false};
};

struct HyprlandWorkspaceQueuedCommand {
  HyprlandWorkspacePendingQuery query{HyprlandWorkspacePendingQuery::None};
  QByteArray command;
};

struct HyprlandWorkspaceCommandQueue {
  bool queued_active_query{false};
  bool queued_workspaces_query{false};
  int queued_dispatch_workspace_id{0};
  QList<QString> queued_urgent_window_addresses;
};

[[nodiscard]] HyprlandWorkspaceEventUpdate hyprlandWorkspaceEventUpdate(const QByteArray& line);
[[nodiscard]] QByteArray hyprlandWorkspaceCommand(HyprlandWorkspacePendingQuery query, int workspace_id = 0);
[[nodiscard]] QByteArray hyprlandLuaWorkspaceFocusCommand(int workspaceId);
[[nodiscard]] bool hyprlandIpcResponseIsError(const QByteArray& response);
void queueHyprlandWorkspaceCommand(HyprlandWorkspaceCommandQueue* queue, HyprlandWorkspacePendingQuery query,
                                   const QByteArray& command);
[[nodiscard]] std::optional<HyprlandWorkspaceQueuedCommand> takeNextHyprlandWorkspaceCommand(
    HyprlandWorkspaceCommandQueue* queue);
[[nodiscard]] HyprlandWorkspaceCommandResult hyprlandWorkspaceCommandResult(HyprlandWorkspacePendingQuery query,
                                                                            const QByteArray& response,
                                                                            bool parse_buffer,
                                                                            const QString& urgent_window_address = {});

class HyprlandWorkspaceService : public QObject {
  Q_OBJECT

 public:
  explicit HyprlandWorkspaceService(WorkspaceModel* model, QObject* parent = nullptr);
  HyprlandWorkspaceService(WorkspaceModel* model, HyprlandIpcTransportPtr ipc_client, QObject* parent = nullptr);
  ~HyprlandWorkspaceService() override = default;

  HyprlandWorkspaceService(const HyprlandWorkspaceService&) = delete;
  HyprlandWorkspaceService& operator=(const HyprlandWorkspaceService&) = delete;
  HyprlandWorkspaceService(HyprlandWorkspaceService&&) = delete;
  HyprlandWorkspaceService& operator=(HyprlandWorkspaceService&&) = delete;

  void start();

 private:
  void connectSocket();
  void processEventLine(const QByteArray& line);
  void onEventSocketConnected();
  void queryActiveWorkspace();
  void queryWorkspaces();
  void queryUrgentClientWorkspace(const QString& address);
  void activateWorkspace(int wsId);
  void startCommand(HyprlandWorkspacePendingQuery query, const QByteArray& command, int dispatchWorkspaceId = 0,
                    bool dispatchUsesLua = false);
  void onCommandFinished(const QByteArray& response, bool success);
  void finishCommandResponse(const QByteArray& response, bool parse_buffer);
  void runQueuedCommand();

  WorkspaceModel* model_;
  HyprlandIpcTransportPtr ipc_client_;
  QByteArray active_command_;
  QString pending_urgent_window_address_;
  HyprlandWorkspacePendingQuery pending_query_{HyprlandWorkspacePendingQuery::None};
  int pending_dispatch_workspace_id_{0};
  bool pending_dispatch_uses_lua_{false};
  HyprlandWorkspaceCommandQueue command_queue_;
  bool started_{false};
};
