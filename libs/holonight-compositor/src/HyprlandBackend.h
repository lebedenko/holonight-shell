#pragma once

#include "CompositorBackend.h"
#include "HyprlandIpcClient.h"

#include <QSet>

class HyprlandBackend final : public CompositorBackend {
  Q_OBJECT

 public:
  explicit HyprlandBackend(HyprlandIpcTransportPtr transport = {}, QObject* parent = nullptr);
  void start() override;
  void activateWorkspace(const QString& workspace_id) override;

 private:
  enum class Phase { Idle, Monitors, Workspaces, Clients, Activation, LuaActivation };
  void scheduleRefresh();
  void beginRefresh();
  void handleCommand(const QByteArray& response, bool success);
  void handleEvent(const QByteArray& line);
  void publishClients(const QByteArray& clients_json);
  void fail(const QString& diagnostic);

  HyprlandIpcTransportPtr transport_;
  Phase phase_{Phase::Idle};
  QByteArray monitors_;
  QByteArray workspaces_;
  QString activation_id_;
  QSet<QString> urgent_addresses_;
  bool refresh_dirty_{false};
};
