#pragma once

#include "CompositorBackend.h"
#include "HyprlandIpcClient.h"
#include "NumberedWorkspaceProvider.h"

#include <QSet>
#include <QVariantList>

#include <cstdint>

class HyprlandBackend final : public CompositorBackend, public NumberedWorkspaceProvider {
  Q_OBJECT
  Q_PROPERTY(QVariantList specialWorkspaces READ specialWorkspaces NOTIFY specialWorkspacesChanged)

 public:
  explicit HyprlandBackend(HyprlandIpcTransportPtr transport = {}, QObject* parent = nullptr);
  void start() override;
  NumberedWorkspaceState numberedWorkspaces() const override { return numbered_; }
  void activateNumberedSlot(int slot) override;
  QVariantList specialWorkspaces() const { return special_workspaces_; }
  Q_INVOKABLE void activateSpecialWorkspace(const QString& id);
 Q_SIGNALS:
  void specialWorkspacesChanged();

 public:
  void activateWorkspace(const QString& workspace_id) override;
  [[nodiscard]] WindowActivationResult requestWindowActivation(const WindowActivationRequest& request) override;

 private:
  enum class Phase : std::uint8_t {
    Idle,
    Monitors,
    Workspaces,
    Clients,
    WorkspaceActivation,
    LuaActivation,
    WindowActivation,
    LuaWindowActivation
  };
  void scheduleRefresh();
  void beginRefresh();
  void handleCommand(const QByteArray& response, bool success);
  void handleCommandFailure(Phase failed_phase);
  void handleEvent(const QByteArray& line);
  void runLuaActivation();
  void runLuaWindowActivation();
  void drainWork();
  bool beginWindowActivation(const QString& address);
  void publishClients(const QByteArray& clients_json);
  void fail(const QString& diagnostic);

  void dispatchWorkspace(const QString& id);
  NumberedWorkspaceState numbered_;
  QVariantList special_workspaces_;
  HyprlandIpcTransportPtr transport_;
  Phase phase_{Phase::Idle};
  QByteArray monitors_;
  QByteArray workspaces_;
  QString activation_id_;
  QString window_activation_address_;
  bool activation_is_special_{false};
  QString pending_activation_;
  QList<WindowActivationCandidate> activation_candidates_;
  QList<QString> activation_addresses_;
  std::optional<QString> pending_window_address_;
  QSet<QString> urgent_addresses_;
  bool refresh_dirty_{false};
};
