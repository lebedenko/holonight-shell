#include "IntegrationLoader.h"
#include "NumberedWorkspaceProvider.h"

#include <QFile>
#include <QGuiApplication>
#include <QTimer>

#include <cstdio>

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  IntegrationLoader loader;
  if (loader.backendName() != "labwc") return 1;
  auto backend = loader.createCompositor();
  if (!backend || dynamic_cast<NumberedWorkspaceProvider*>(backend.get())) return 2;
  QFile maps("/proc/self/maps");
  if (!maps.open(QIODevice::ReadOnly)) return 3;
  const auto mappings = maps.readAll();
  for (const auto* name : {"hyprland", "sway", "wayland"}) {
    if (mappings.contains(QByteArray("libholonight_backend_") + name)) return 4;
  }
  QString destination;
  QObject::connect(backend.get(), &CompositorBackend::snapshotReady, &app, [&](const CompositorSnapshot& snapshot) {
    if (!snapshot.connected || !snapshot.capabilities.workspace_activation) return;
    if (snapshot.workspaces.size() != 2) {
      app.exit(5);
      return;
    }
    if (destination.isEmpty()) {
      QStringList names;
      for (const auto& workspace : snapshot.workspaces) {
        names.append(workspace.display_name);
        if (!workspace.active && workspace.can_activate) destination = workspace.id;
      }
      names.sort();
      if (names != QStringList{"code", "web"} || destination.isEmpty()) {
        app.exit(6);
        return;
      }
      QTimer::singleShot(0, &app, [&] { backend->activateWorkspace(destination); });
    } else {
      for (const auto& workspace : snapshot.workspaces) {
        if (workspace.id == destination && workspace.active) {
          std::puts("labwc plugin: real workspace listing, activation and isolation passed");
          app.exit(0);
        }
      }
    }
  });
  QTimer::singleShot(8000, &app, [&] { app.exit(7); });
  backend->start();
  return app.exec();
}
