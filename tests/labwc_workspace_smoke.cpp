#include "IntegrationLoader.h"
#include "NumberedWorkspaceProvider.h"

#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QRasterWindow>
#include <QTimer>

#include <cstdio>

class SmokeWindow final : public QRasterWindow {
 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.fillRect(QRect(QPoint(), size()), Qt::blue);
  }
};

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
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
  std::unique_ptr<CompositorBackend> enumeration_backend;
  QString destination;
  SmokeWindow first, second;
  first.setTitle("labwc-smoke-first");
  second.setTitle("labwc-smoke-second");
  first.resize(320, 200);
  second.resize(320, 200);
  int window_stage = 0;
  bool workspace_passed = false;
  QObject::connect(backend.get(), &CompositorBackend::snapshotReady, &app, [&](const CompositorSnapshot& snapshot) {
    if (workspace_passed) {
      if (!snapshot.capabilities.active_window) return;
      if (snapshot.workspaces.size() != 2) {
        app.exit(8);
        return;
      }
      QString title;
      for (const auto& window : snapshot.active_windows) title = window.title;
      if (window_stage == 0 && title == "labwc-smoke-first") {
        window_stage = -1;
        enumeration_backend = loader.createCompositor();
        QObject::connect(enumeration_backend.get(), &CompositorBackend::snapshotReady, &app,
                         [&](const CompositorSnapshot& initial) {
                           if (window_stage != -1 || !initial.capabilities.active_window) return;
                           for (const auto& window : initial.active_windows) {
                             if (window.title == "labwc-smoke-first" && !window.app_id.isEmpty()) {
                               window_stage = 1;
                               second.show();
                               break;
                             }
                           }
                         });
        enumeration_backend->start();
      } else if (window_stage == 1 && title == "labwc-smoke-second") {
        window_stage = 2;
        second.setTitle("labwc-smoke-renamed");
      } else if (window_stage == 2 && title == "labwc-smoke-renamed") {
        window_stage = 3;
        second.close();
      } else if (window_stage == 3 && title == "labwc-smoke-first") {
        window_stage = 4;
        first.close();
      } else if (window_stage == 4 && snapshot.active_windows.isEmpty()) {
        std::puts("labwc active windows: focus, title updates and closure passed");
        app.exit(0);
      }
      return;
    }
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
          workspace_passed = true;
          first.show();
        }
      }
    }
  });
  QTimer::singleShot(8000, &app, [&] { app.exit(7); });
  backend->start();
  return app.exec();
}
