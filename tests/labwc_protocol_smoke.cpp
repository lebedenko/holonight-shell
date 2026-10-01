#include "LabwcBackend.h"

#include <QGuiApplication>
#include <QRasterWindow>
#include <QTimer>

#include <cstdio>

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  const int version = app.arguments().value(1).toInt();
  if (version < 1 || version > 3) return 1;
  LabwcBackend backend(nullptr, version);
  QRasterWindow window;
  window.setTitle("labwc-version-smoke");
  window.resize(320, 200);
  QString id;
  int stage = 0;
  const auto request = [&](WindowCommand command) {
    if (backend.requestWindowCommand(id, command) != WindowCommandResult::Accepted) app.exit(2);
  };
  QObject::connect(&backend, &CompositorBackend::snapshotReady, &app, [&](const CompositorSnapshot& snapshot) {
    const CompositorWindow* target = nullptr;
    for (const auto& candidate : snapshot.windows) {
      if (candidate.title == window.title()) target = &candidate;
    }
    if (stage == 0 && target) {
      id = target->id;
      if (target->operations.contains(WindowCommand::Fullscreen) != (version >= 2)) {
        app.exit(3);
        return;
      }
      if (version == 1 &&
          backend.requestWindowCommand(id, WindowCommand::Fullscreen) != WindowCommandResult::Unsupported) {
        app.exit(4);
        return;
      }
      stage = 1;
      request(WindowCommand::Minimize);
    } else if (stage == 1 && target && target->minimized) {
      stage = 2;
      request(WindowCommand::Restore);
      request(WindowCommand::Activate);
    } else if (stage == 2 && target && !target->minimized && target->activated) {
      stage = 3;
      request(WindowCommand::Maximize);
    } else if (stage == 3 && target && target->maximized) {
      stage = 4;
      request(WindowCommand::Unmaximize);
    } else if (stage == 4 && target && !target->maximized) {
      if (version >= 2) {
        stage = 5;
        request(WindowCommand::Fullscreen);
      } else {
        stage = 7;
        request(WindowCommand::Close);
      }
    } else if (stage == 5 && target && target->fullscreen) {
      stage = 6;
      request(WindowCommand::Unfullscreen);
    } else if (stage == 6 && target && !target->fullscreen) {
      stage = 7;
      request(WindowCommand::Close);
    } else if (stage == 7 && !target) {
      if (backend.requestWindowCommand(id, WindowCommand::Activate) != WindowCommandResult::InvalidWindow) {
        app.exit(5);
        return;
      }
      std::printf("labwc negotiated protocol v%d: inventory, commands and stale IDs passed\n", version);
      app.exit(0);
    }
  });
  QTimer::singleShot(12000, &app, [&] { app.exit(6); });
  backend.start();
  window.show();
  return app.exec();
}
