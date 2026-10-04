#include "IntegrationLoader.h"
#include "NumberedWorkspaceProvider.h"

#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QRasterWindow>
#include <QScreen>
#include <QTimer>

#include <print>

class SmokeWindow final : public QRasterWindow {
 protected:
  void paintEvent([[maybe_unused]] QPaintEvent* event) override {
    QPainter painter(this);
    painter.fillRect(QRect(QPoint(), size()), Qt::blue);
  }
};

const CompositorWindow* findWindow(const CompositorSnapshot& snapshot) {
  const CompositorWindow* target = nullptr;
  for (const auto& window : snapshot.windows) {
    if (window.title == "labwc-smoke-renamed") {
      target = &window;
    }
  }
  return target;
}

class WorkspaceSmoke {
 public:
  WorkspaceSmoke(QGuiApplication& application, IntegrationLoader& integration, CompositorBackend* compositor)
      : app_(&application), loader_(&integration), backend_(compositor) {
    first_.setTitle("labwc-smoke-first");
    second_.setTitle("labwc-smoke-second");
    first_.resize(320, 200);
    second_.resize(320, 200);
  }
  void advance(const CompositorSnapshot& snapshot) {
    if (workspace_passed_) {
      advanceWindows(snapshot);
      return;
    }
    advanceWorkspaces(snapshot);
  }

 private:
  void request(WindowCommand operation) {
    if (backend_->requestWindowCommand(command_id_, operation) != WindowCommandResult::Accepted) {
      QCoreApplication::exit(9);
    }
  }
  void advanceWindows(const CompositorSnapshot& snapshot) {
    if (!snapshot.capabilities.active_window) {
      return;
    }
    if (snapshot.workspaces.size() != 2) {
      QCoreApplication::exit(8);
      return;
    }
    QString title;
    for (const auto& window : snapshot.active_windows) {
      title = window.title;
    }
    if (window_stage_ == 0 && title == "labwc-smoke-first") {
      window_stage_ = -1;
      enumeration_backend_ = loader_->createCompositor();
      QObject::connect(enumeration_backend_.get(), &CompositorBackend::snapshotReady, app_,
                       [&](const CompositorSnapshot& initial) {
                         if (window_stage_ != -1 || !initial.capabilities.active_window) {
                           return;
                         }
                         for (const auto& window : initial.active_windows) {
                           if (window.title == "labwc-smoke-first" && !window.app_id.isEmpty()) {
                             window_stage_ = 1;
                             second_.show();
                             break;
                           }
                         }
                       });
      enumeration_backend_->start();
    } else if (window_stage_ == 1 && title == "labwc-smoke-second") {
      window_stage_ = 2;
      second_.setTitle("labwc-smoke-renamed");
    } else if (window_stage_ >= 2) {
      advanceCommands(snapshot);
    }
  }
  void advanceCommands(const CompositorSnapshot& snapshot) {
    const auto* target = findWindow(snapshot);
    if (window_stage_ == 2 && (target != nullptr)) {
      if (snapshot.windows.size() != 2 || !snapshot.capabilities.window_listing) {
        QCoreApplication::exit(10);
        return;
      }
      command_id_ = target->id;
      window_stage_ = 3;
      request(WindowCommand::Minimize);
    } else if (window_stage_ == 3 && (target != nullptr) && target->minimized) {
      window_stage_ = 4;
      request(WindowCommand::Restore);
      request(WindowCommand::Activate);
    } else if (window_stage_ == 4 && (target != nullptr) && !target->minimized && target->activated) {
      window_stage_ = 5;
      request(WindowCommand::Maximize);
    } else if (window_stage_ == 5 && (target != nullptr) && target->maximized) {
      window_stage_ = 6;
      request(WindowCommand::Unmaximize);
    } else if (window_stage_ == 6 && (target != nullptr) && !target->maximized) {
      window_stage_ = 7;
      request(WindowCommand::Fullscreen);
    } else if (window_stage_ == 7 && (target != nullptr) && target->fullscreen) {
      window_stage_ = 8;
      request(WindowCommand::Unfullscreen);
    } else if (window_stage_ == 8 && (target != nullptr) && !target->fullscreen) {
      window_stage_ = 9;
      request(WindowCommand::Close);
    } else if (window_stage_ == 9 && (target == nullptr) && snapshot.windows.size() == 1) {
      if (backend_->requestWindowCommand(command_id_, WindowCommand::Close) != WindowCommandResult::InvalidWindow) {
        QCoreApplication::exit(11);
        return;
      }
      window_stage_ = 10;
      first_.close();
    } else if (window_stage_ == 10 && snapshot.windows.isEmpty()) {
      std::println("labwc windows: inventory, minimize, maximize, fullscreen, activation, close and stale IDs passed");
      QCoreApplication::exit(0);
    }
  }
  void advanceWorkspaces(const CompositorSnapshot& snapshot) {
    if (!snapshot.connected || !snapshot.capabilities.workspace_activation) {
      return;
    }
    if (snapshot.workspaces.size() != 2) {
      QCoreApplication::exit(5);
      return;
    }
    if (destination_.isEmpty()) {
      QStringList names;
      for (const auto& workspace : snapshot.workspaces) {
        names.append(workspace.display_name);
        if (!workspace.active && workspace.can_activate) {
          destination_ = workspace.id;
        }
      }
      names.sort();
      if (names != QStringList{"code", "web"} || destination_.isEmpty()) {
        QCoreApplication::exit(6);
        return;
      }
      QTimer::singleShot(0, app_, [&] { backend_->activateWorkspace(destination_); });
    } else {
      for (const auto& workspace : snapshot.workspaces) {
        if (workspace.id == destination_ && workspace.active) {
          std::println("labwc plugin: real workspace listing, activation and isolation passed");
          workspace_passed_ = true;
          first_.show();
        }
      }
    }
  }
  QGuiApplication* app_;
  IntegrationLoader* loader_;
  CompositorBackend* backend_;
  std::unique_ptr<CompositorBackend> enumeration_backend_;
  QString destination_;
  SmokeWindow first_;
  SmokeWindow second_;
  int window_stage_ = 0;
  bool workspace_passed_ = false;
  QString command_id_;
};

int runHold(QGuiApplication& app) {
  SmokeWindow first;
  SmokeWindow second;
  first.setTitle("labwc-taskbar-first");
  second.setTitle("labwc-taskbar-second");
  first.resize(320, 200);
  second.resize(320, 200);
  IntegrationLoader observer_loader;
  auto observer = observer_loader.createCompositor();
  QObject::connect(observer.get(), &CompositorBackend::snapshotReady, &app, [](const CompositorSnapshot& snapshot) {
    for (const auto& window : snapshot.windows) {
      if (!window.activated) {
        continue;
      }
      QFile marker(qEnvironmentVariable("XDG_RUNTIME_DIR") + QStringLiteral("/labwc-smoke-activated"));
      if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        marker.write(window.title.toUtf8());
      }
    }
  });
  const auto record_screens = [] {
    const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    QFile count(runtime + QStringLiteral("/labwc-smoke-screen-count"));
    if (count.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      count.write(QByteArray::number(QGuiApplication::screens().size()));
    }
    QFile primary(runtime + QStringLiteral("/labwc-smoke-primary-output"));
    if (primary.open(QIODevice::WriteOnly | QIODevice::Truncate) && QGuiApplication::primaryScreen()) {
      primary.write(QGuiApplication::primaryScreen()->name().toUtf8());
    }
  };
  record_screens();
  QObject::connect(&app, &QGuiApplication::screenRemoved, &app,
                   [&](QScreen*) { QTimer::singleShot(0, &app, record_screens); });
  observer->start();
  first.show();
  if (QGuiApplication::screens().size() > 1) {
    second.setScreen(QGuiApplication::screens().last());
    second.showFullScreen();
  } else {
    {
      second.show();
    }
  }
  return QGuiApplication::exec();
}

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setQuitOnLastWindowClosed(false);
  if (QCoreApplication::arguments().contains(QStringLiteral("--hold"))) {
    return runHold(app);
  }
  IntegrationLoader loader;
  if (loader.backendName() != "labwc") {
    return 1;
  }
  auto backend = loader.createCompositor();
  if (!backend || (dynamic_cast<NumberedWorkspaceProvider*>(backend.get()) != nullptr)) {
    return 2;
  }
  QFile maps("/proc/self/maps");
  if (!maps.open(QIODevice::ReadOnly)) {
    return 3;
  }
  const auto mappings = maps.readAll();
  for (const auto* name : {"hyprland", "sway", "wayland"}) {
    if (mappings.contains(QByteArray("libholonight_backend_") + name)) {
      return 4;
    }
  }
  WorkspaceSmoke smoke(app, loader, backend.get());
  QObject::connect(backend.get(), &CompositorBackend::snapshotReady, &app,
                   [&smoke](const CompositorSnapshot& snapshot) { smoke.advance(snapshot); });
  QTimer::singleShot(15000, &app, [&] { QCoreApplication::exit(7); });
  backend->start();
  return QGuiApplication::exec();
}
