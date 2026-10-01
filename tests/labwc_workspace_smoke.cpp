#include "IntegrationLoader.h"
#include "NumberedWorkspaceProvider.h"

#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QRasterWindow>
#include <QScreen>
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
  if (app.arguments().contains(QStringLiteral("--hold"))) {
    SmokeWindow first, second;
    first.setTitle("labwc-taskbar-first");
    second.setTitle("labwc-taskbar-second");
    first.resize(320, 200);
    second.resize(320, 200);
    IntegrationLoader observer_loader;
    auto observer = observer_loader.createCompositor();
    QObject::connect(observer.get(), &CompositorBackend::snapshotReady, &app, [](const CompositorSnapshot& snapshot) {
      for (const auto& window : snapshot.windows) {
        if (!window.activated) continue;
        QFile marker(qEnvironmentVariable("XDG_RUNTIME_DIR") + QStringLiteral("/labwc-smoke-activated"));
        if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) marker.write(window.title.toUtf8());
      }
    });
    const auto record_screens = [] {
      const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
      QFile count(runtime + QStringLiteral("/labwc-smoke-screen-count"));
      if (count.open(QIODevice::WriteOnly | QIODevice::Truncate))
        count.write(QByteArray::number(QGuiApplication::screens().size()));
      QFile primary(runtime + QStringLiteral("/labwc-smoke-primary-output"));
      if (primary.open(QIODevice::WriteOnly | QIODevice::Truncate) && QGuiApplication::primaryScreen())
        primary.write(QGuiApplication::primaryScreen()->name().toUtf8());
    };
    record_screens();
    QObject::connect(&app, &QGuiApplication::screenRemoved, &app,
                     [&](QScreen*) { QTimer::singleShot(0, &app, record_screens); });
    observer->start();
    first.show();
    if (QGuiApplication::screens().size() > 1) {
      second.setScreen(QGuiApplication::screens().last());
      second.showFullScreen();
    } else
      second.show();
    return app.exec();
  }
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
  QString command_id;
  const auto request = [&](WindowCommand operation) {
    if (backend->requestWindowCommand(command_id, operation) != WindowCommandResult::Accepted) app.exit(9);
  };
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
      } else if (window_stage >= 2) {
        const CompositorWindow* target = nullptr;
        for (const auto& window : snapshot.windows) {
          if (window.title == "labwc-smoke-renamed") target = &window;
        }
        if (window_stage == 2 && target) {
          if (snapshot.windows.size() != 2 || !snapshot.capabilities.window_listing) {
            app.exit(10);
            return;
          }
          command_id = target->id;
          window_stage = 3;
          request(WindowCommand::Minimize);
        } else if (window_stage == 3 && target && target->minimized) {
          window_stage = 4;
          request(WindowCommand::Restore);
          request(WindowCommand::Activate);
        } else if (window_stage == 4 && target && !target->minimized && target->activated) {
          window_stage = 5;
          request(WindowCommand::Maximize);
        } else if (window_stage == 5 && target && target->maximized) {
          window_stage = 6;
          request(WindowCommand::Unmaximize);
        } else if (window_stage == 6 && target && !target->maximized) {
          window_stage = 7;
          request(WindowCommand::Fullscreen);
        } else if (window_stage == 7 && target && target->fullscreen) {
          window_stage = 8;
          request(WindowCommand::Unfullscreen);
        } else if (window_stage == 8 && target && !target->fullscreen) {
          window_stage = 9;
          request(WindowCommand::Close);
        } else if (window_stage == 9 && !target && snapshot.windows.size() == 1) {
          if (backend->requestWindowCommand(command_id, WindowCommand::Close) != WindowCommandResult::InvalidWindow) {
            app.exit(11);
            return;
          }
          window_stage = 10;
          first.close();
        } else if (window_stage == 10 && snapshot.windows.isEmpty()) {
          std::puts("labwc windows: inventory, minimize, maximize, fullscreen, activation, close and stale IDs passed");
          app.exit(0);
        }
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
  QTimer::singleShot(15000, &app, [&] { app.exit(7); });
  backend->start();
  return app.exec();
}
