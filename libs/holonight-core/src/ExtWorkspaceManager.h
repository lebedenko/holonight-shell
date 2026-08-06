#pragma once

#include "WorkspaceModel.h"

class ConfigService;
#include "qwayland-ext-workspace-v1.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>

class ExtWorkspaceManager;

class ExtWorkspaceHandle : public QtWayland::ext_workspace_handle_v1 {
 public:
  explicit ExtWorkspaceHandle(struct ::ext_workspace_handle_v1* proto, ExtWorkspaceManager* mgr);
  ~ExtWorkspaceHandle() override;

  ExtWorkspaceHandle(const ExtWorkspaceHandle&) = delete;
  ExtWorkspaceHandle& operator=(const ExtWorkspaceHandle&) = delete;
  ExtWorkspaceHandle(ExtWorkspaceHandle&&) = delete;
  ExtWorkspaceHandle& operator=(ExtWorkspaceHandle&&) = delete;

 protected:
  void ext_workspace_handle_v1_name(const QString& name) override;
  void ext_workspace_handle_v1_state(uint32_t state) override;
  void ext_workspace_handle_v1_removed() override;

 private:
  friend class ExtWorkspaceManager;

  WorkspaceModel::WorkspaceEntry entry_;
  struct ::ext_workspace_handle_v1* raw_;
  ExtWorkspaceManager* manager_;
};

class ExtWorkspaceGroup : public QtWayland::ext_workspace_group_handle_v1 {
 public:
  explicit ExtWorkspaceGroup(struct ::ext_workspace_group_handle_v1* proto, ExtWorkspaceManager* mgr);
  ~ExtWorkspaceGroup() override;

  ExtWorkspaceGroup(const ExtWorkspaceGroup&) = delete;
  ExtWorkspaceGroup& operator=(const ExtWorkspaceGroup&) = delete;
  ExtWorkspaceGroup(ExtWorkspaceGroup&&) = delete;
  ExtWorkspaceGroup& operator=(ExtWorkspaceGroup&&) = delete;

  [[nodiscard]] bool hasOutput() const { return !outputs_.isEmpty(); }
  [[nodiscard]] bool hasWorkspace(struct ::ext_workspace_handle_v1* proto) const { return workspaces_.contains(proto); }
  // Resolved lazily (not cached) on every call — see ext_workspace_manager_v1_done() for why.
  [[nodiscard]] QStringList monitorNames() const;

 protected:
  void ext_workspace_group_handle_v1_output_enter(struct ::wl_output* output) override;
  void ext_workspace_group_handle_v1_output_leave(struct ::wl_output* output) override;
  void ext_workspace_group_handle_v1_workspace_enter(struct ::ext_workspace_handle_v1* workspace) override;
  void ext_workspace_group_handle_v1_workspace_leave(struct ::ext_workspace_handle_v1* workspace) override;
  void ext_workspace_group_handle_v1_removed() override;

 private:
  ExtWorkspaceManager* manager_;
  QList<struct ::wl_output*> outputs_;
  QSet<struct ::ext_workspace_handle_v1*> workspaces_;
};

class ExtWorkspaceManager : public QWaylandClientExtensionTemplate<ExtWorkspaceManager>,
                            public QtWayland::ext_workspace_manager_v1 {
  Q_OBJECT

 public:
  explicit ExtWorkspaceManager(WorkspaceModel* model, ConfigService* config, QObject* parent = nullptr);
  ~ExtWorkspaceManager() override;

  ExtWorkspaceManager(const ExtWorkspaceManager&) = delete;
  ExtWorkspaceManager& operator=(const ExtWorkspaceManager&) = delete;
  ExtWorkspaceManager(ExtWorkspaceManager&&) = delete;
  ExtWorkspaceManager& operator=(ExtWorkspaceManager&&) = delete;

 protected:
  void ext_workspace_manager_v1_workspace_group(struct ::ext_workspace_group_handle_v1* workspace_group) override;
  void ext_workspace_manager_v1_workspace(struct ::ext_workspace_handle_v1* workspace) override;
  void ext_workspace_manager_v1_done() override;

 private:
  friend class ExtWorkspaceHandle;
  friend class ExtWorkspaceGroup;

  void activateWorkspace(int wsId);
  void activateSpecialWorkspace(const QString& name);

  WorkspaceModel* model_;
  QList<ExtWorkspaceGroup*> groups_;
  QHash<struct ::ext_workspace_handle_v1*, ExtWorkspaceHandle*> handle_map_;
};
