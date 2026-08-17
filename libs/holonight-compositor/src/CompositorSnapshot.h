#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

struct CompositorCapabilities {
  bool workspace_listing{false};
  bool workspace_activation{false};
  bool window_activation{false};
  bool numeric_workspace_creation{false};
  bool special_workspaces{false};
  bool active_window{false};
  bool focused_output{false};
  bool urgency{false};
  bool occupancy{false};

  bool operator==(const CompositorCapabilities&) const = default;
};

struct CompositorWorkspace {
  QString id;
  std::optional<int> numeric_slot;
  QString display_name;
  int stable_order{0};
  QString kind{QStringLiteral("normal")};
  QStringList outputs;
  bool active{false};
  bool focused{false};
  bool urgent{false};
  std::optional<bool> occupied;

  bool operator==(const CompositorWorkspace&) const = default;
};

struct CompositorActiveWindow {
  QString app_id;
  QString title;
  QString category;

  bool operator==(const CompositorActiveWindow&) const = default;
};

struct CompositorSnapshot {
  bool connected{false};
  QString diagnostic;
  QString focused_output;
  CompositorCapabilities capabilities;
  QList<CompositorWorkspace> workspaces;
  QHash<QString, CompositorActiveWindow> active_windows;
};
