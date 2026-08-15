#pragma once

#include "CompositorBackend.h"
#include "HyprlandIpcClient.h"

#include <QSet>

#include <cstdint>

class HyprlandBackend final : public CompositorBackend {
  Q_OBJECT

 public:
  explicit HyprlandBackend(HyprlandIpcTransportPtr transport = {}, QObject* parent = nullptr);
  void start() override;
  void activateWorkspace(const QString& workspace_id) override;

 private:
  enum class Phase : std::uint8_t { Idle, Monitors, Workspaces, Clients, Activation, LuaActivation };
  void scheduleRefresh();
  void beginRefresh();
  void handleCommand(const QByteArray& response, bool success);
  void handleEvent(const QByteArray& line);
  void runLuaActivation();
  void publishClients(const QByteArray& clients_json);
  void fail(const QString& diagnostic);

  HyprlandIpcTransportPtr transport_;
  Phase phase_{Phase::Idle};
  QByteArray monitors_;
  QByteArray workspaces_;
  QString activation_id_;
  bool activation_is_special_{false};
  QString pending_activation_;
  QSet<QString> urgent_addresses_;
  bool refresh_dirty_{false};
};
