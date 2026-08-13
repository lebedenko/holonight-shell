#include "SidebarSurfacePolicy.h"

#include "IconImageProvider.h"
#include "ShellConstants.h"

#include <QQmlEngine>
#include <QScreen>

#include <algorithm>

using Holonight::Wayland::Anchor;
using Holonight::Wayland::KeyboardInteractivity;
using Holonight::Wayland::Layer;
using Holonight::Wayland::LayerSurfaceSpec;

int sidebarDefaultHeight() { return kSidebarDefaultHeight; }

int boundedSidebarHeight(int requested_height, int screen_height) {
  const int max_height = screen_height <= 0
                             ? kSidebarDefaultHeight
                             : std::max(kSidebarMinHeight, screen_height - kSidebarTopMargin - kSidebarBottomMargin);
  return std::clamp(requested_height, kSidebarMinHeight, max_height);
}

LayerSurfaceSpec sidebarSurfaceSpec(QScreen* screen, const QString& monitor_name, int panel_height, int current_tab) {
  return {
      .output = screen,
      .name_space = QStringLiteral("sidebar"),
      .layer = Layer::Top,
      .anchors = Anchor::Top | Anchor::Bottom | Anchor::Left | Anchor::Right,
      .width = 0,
      .height = 0,
      .exclusive_zone = 0,
      .keyboard_interactivity = KeyboardInteractivity::Exclusive,
      .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/RightSidebar/RightSidebar.qml")),
      .initial_properties =
          {
              {QStringLiteral("barMonitorName"), monitor_name},
              {QStringLiteral("active"), false},
              {QStringLiteral("currentTab"), current_tab},
              {QStringLiteral("panelHeight"), panel_height},
          },
      .window_flags = Qt::FramelessWindowHint | Qt::BypassWindowManagerHint,
      .color = Qt::transparent,
      .before_load =
          [](QQmlEngine* engine) {
            if (engine != nullptr) {
              engine->addImageProvider(QStringLiteral("icon"), new IconImageProvider());
            }
          },
  };
}
