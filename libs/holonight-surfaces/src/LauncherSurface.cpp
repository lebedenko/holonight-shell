#include "LauncherSurface.h"

#include "IconImageProvider.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QQuickItem>
#include <QScreen>

#include <holonight/wayland/layershellcontext.h>

Q_LOGGING_CATEGORY(lcLauncher, "holonight.launcher")

using namespace Holonight::Wayland;

LauncherSurface::LauncherSurface(QObject* parent) : TransientSurfaceHost("LauncherSurface", parent) {
  connect(LayerShellContext::instance(), &LayerShellContext::availabilityChanged, this, [this]() {
    executeCommand(lifecycle_.providerAvailabilityChanged(LayerShellContext::instance()->isAvailable()));
  });
}

LauncherSurface::~LauncherSurface() { destroySurface(); }

void LauncherSurface::toggle(const QString& screen_name) {
  qCInfo(lcLauncher) << "toggle" << screen_name << "visible" << visible_ << "closing" << lifecycle_.closing();
  executeCommand(lifecycle_.toggle(screen_name, LayerShellContext::instance()->isAvailable(), hasSurface()));
}

void LauncherSurface::show(const QString& screen_name) {
  qCInfo(lcLauncher) << "show requested" << screen_name << "provider available"
                     << LayerShellContext::instance()->isAvailable() << "visible" << visible_ << "closing"
                     << lifecycle_.closing() << "view" << view();
  executeCommand(lifecycle_.show(screen_name, LayerShellContext::instance()->isAvailable()));
}

void LauncherSurface::hide() {
  qCInfo(lcLauncher) << "hide requested"
                     << "view" << view() << "visible" << visible_ << "closing" << lifecycle_.closing();
  executeCommand(lifecycle_.hide(hasSurface()));
}

void LauncherSurface::notifyHideReady() {
  qCInfo(lcLauncher) << "notifyHideReady"
                     << "view" << view() << "visible" << visible_ << "closing" << lifecycle_.closing();
  executeCommand(lifecycle_.notifyHideReady(hasSurface()));
}

bool LauncherSurface::ensureSurface(const QString& screen_name) {
  qCInfo(lcLauncher) << "ensureSurface" << screen_name;
  QScreen* screen = QGuiApplication::primaryScreen();
  if (!screen_name.isEmpty()) {
    for (QScreen* candidate : QGuiApplication::screens()) {
      if (candidate->name() == screen_name) {
        screen = candidate;
        break;
      }
    }
  }
  if (screen == nullptr) {
    qCritical("LauncherSurface: no screen available");
    return false;
  }

  return openSurface(surfaceSpec(screen));
}

void LauncherSurface::destroySurface() {
  qCInfo(lcLauncher) << "destroySurface" << "view" << view();
  clearPendingSurface();
  closeSurface();
}

LayerSurfaceSpec LauncherSurface::surfaceSpec(QScreen* screen) {
  return {.output = screen,
          .name_space = QStringLiteral("launcher"),
          .layer = Layer::Top,
          .anchors = Anchor::Top | Anchor::Bottom | Anchor::Left | Anchor::Right,
          .width = 0,
          .height = 0,
          .exclusive_zone = 0,
          .keyboard_interactivity = KeyboardInteractivity::Exclusive,
          .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Launcher/Launcher.qml")),
          .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
          .color = Qt::transparent,
          .before_load = [](QQmlEngine* engine) {
            engine->addImageProvider(QStringLiteral("icon"), new IconImageProvider());
          }};
}

void LauncherSurface::onSurfaceTerminated() { executeCommand(lifecycle_.surfaceClosed()); }

void LauncherSurface::executeCommand(const LauncherSurfaceCommand& command) {
  switch (command.type) {
    case LauncherSurfaceCommandType::None:
      break;
    case LauncherSurfaceCommandType::CreateSurface: {
      destroySurface();
      executeCommand(lifecycle_.surfaceCreated(ensureSurface(command.screen_name)));
      break;
    }
    case LauncherSurfaceCommandType::DestroySurface:
      destroySurface();
      break;
    case LauncherSurfaceCommandType::StartCloseAnimation:
      if (auto* root = qobject_cast<QQuickItem*>(rootObject())) {
        QMetaObject::invokeMethod(root, "startClose");
      }
      break;
  }

  if (command.visible.has_value()) {
    setVisible(*command.visible);
  }
}

void LauncherSurface::setVisible(bool visible) {
  if (visible_ == visible) {
    return;
  }
  visible_ = visible;
  emit visibleChanged();
}
