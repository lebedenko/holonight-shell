#include "LauncherSurface.h"
#include "NotificationToastSurface.h"
#include "OsdSurface.h"
#include "ShellConstants.h"
#include "TooltipGeometry.h"
#include "TooltipSurface.h"

#include <QGuiApplication>
#include <QScreen>

#include <gtest/gtest.h>

using namespace HoloNight::ShellConfig;
using namespace Holonight::Wayland;

namespace {
void expectTransientDefaults(const LayerSurfaceSpec& spec, QScreen* screen) {
  EXPECT_EQ(spec.output, screen);
  EXPECT_EQ(spec.exclusive_zone, 0);
  EXPECT_TRUE(spec.window_flags.testFlag(Qt::FramelessWindowHint));
  EXPECT_TRUE(spec.window_flags.testFlag(Qt::BypassWindowManagerHint));
  EXPECT_EQ(spec.color, QColor(Qt::transparent));
}

class TransientHostLifecycleHarness : public TransientSurfaceHost {
 public:
  TransientHostLifecycleHarness()
      : TransientSurfaceHost("TransientHostLifecycleHarness", [this] {
          auto result = std::make_unique<LayerSurfaceHost>();
          hosts.push_back(result.get());
          return result;
        }) {}

  bool request(const LayerSurfaceSpec& spec) { return openSurface(spec); }
  void close() { closeSurface(); }
  [[nodiscard]] bool active() const { return hasSurface(); }
  QList<LayerSurfaceHost*> hosts;
  int configured_count = 0;
  int terminal_count = 0;

 protected:
  bool openHost(LayerSurfaceHost& /*host*/, const LayerSurfaceSpec& /*spec*/) override { return true; }
  [[nodiscard]] bool providerAvailable() const override { return true; }
  void onSurfaceConfigured() override { ++configured_count; }
  void onSurfaceTerminated() override { ++terminal_count; }
};
}  // namespace

TEST(TransientSurfacePolicy, DescribesLauncherAndToastCompletely) {
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);

  const LayerSurfaceSpec launcher_spec = LauncherSurface::surfaceSpec(screen);
  expectTransientDefaults(launcher_spec, screen);
  EXPECT_EQ(launcher_spec.name_space, QStringLiteral("launcher"));
  EXPECT_EQ(launcher_spec.layer, Layer::Top);
  EXPECT_EQ(launcher_spec.anchors, Anchor::Top | Anchor::Bottom | Anchor::Left | Anchor::Right);
  EXPECT_EQ(launcher_spec.width, 0);
  EXPECT_EQ(launcher_spec.height, 0);
  EXPECT_EQ(launcher_spec.keyboard_interactivity, KeyboardInteractivity::Exclusive);
  EXPECT_EQ(launcher_spec.qml_url, QUrl(QStringLiteral("qrc:/HolonightShell/Launcher/Launcher.qml")));
  EXPECT_TRUE(static_cast<bool>(launcher_spec.before_load));

  const LayerSurfaceSpec toast_spec = NotificationToastSurface::surfaceSpec(screen, QStringLiteral("DP-1"));
  expectTransientDefaults(toast_spec, screen);
  EXPECT_EQ(toast_spec.name_space, QStringLiteral("notifications"));
  EXPECT_EQ(toast_spec.layer, Layer::Overlay);
  EXPECT_EQ(toast_spec.anchors, Anchor::Top | Anchor::Right);
  EXPECT_EQ(toast_spec.width, 462);
  EXPECT_EQ(toast_spec.height, 120);
  EXPECT_EQ(toast_spec.margin_top, 8);
  EXPECT_EQ(toast_spec.margin_right, 12);
  EXPECT_EQ(toast_spec.initial_properties.value(QStringLiteral("monitorName")).toString(), QStringLiteral("DP-1"));
  EXPECT_TRUE(static_cast<bool>(toast_spec.before_load));
}

TEST(TransientSurfacePolicy, DescribesOsdPlacementAndInputPolicy) {
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);
  OsdSurface osd;

  osd.setPosition(WidgetPosition::CenterBottom);
  LayerSurfaceSpec spec = osd.surfaceSpec(screen, QStringLiteral("HDMI-A-1"));
  expectTransientDefaults(spec, screen);
  EXPECT_EQ(spec.name_space, QStringLiteral("osd"));
  EXPECT_EQ(spec.layer, Layer::Overlay);
  EXPECT_EQ(spec.anchors, Anchor::Bottom);
  EXPECT_EQ(spec.width, 220);
  EXPECT_EQ(spec.height, 96);
  EXPECT_EQ(spec.margin_bottom, 24);
  EXPECT_EQ(spec.keyboard_interactivity, KeyboardInteractivity::None);
  EXPECT_EQ(spec.input_region_policy, InputRegionPolicy::Empty);
  EXPECT_EQ(spec.qml_url, QUrl(QStringLiteral("qrc:/HolonightShell/Osd/OsdView.qml")));

  osd.setPosition(WidgetPosition::RightTop);
  spec = osd.surfaceSpec(screen, QStringLiteral("HDMI-A-1"));
  EXPECT_EQ(spec.anchors, Anchor::Top | Anchor::Right);
  EXPECT_EQ(spec.margin_top, kBarHeight + 24);
}

TEST(TransientSurfacePolicy, ComputesTooltipGeometryInSpec) {
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);
  const int anchor_x = screen->geometry().x() + 300;
  const LayerSurfaceSpec spec = TooltipSurface::surfaceSpec(screen, anchor_x, 40);
  expectTransientDefaults(spec, screen);
  EXPECT_EQ(spec.name_space, QStringLiteral("tooltip"));
  EXPECT_EQ(spec.layer, Layer::Top);
  EXPECT_EQ(spec.anchors, Anchor::Top | Anchor::Left);
  EXPECT_EQ(spec.width, TooltipGeometry::kWidth);
  EXPECT_EQ(spec.height, 106);
  EXPECT_EQ(spec.margin_top, 4);
  EXPECT_EQ(spec.margin_left,
            TooltipGeometry::leftMargin(screen->geometry().width(), screen->geometry().x(), anchor_x, 40));
  EXPECT_EQ(spec.qml_url, QUrl(QStringLiteral("qrc:/HolonightShell/Popups/Tooltip/TooltipPopup.qml")));
}

TEST(TransientSurfaceLifecycle, RetainsConfigureAndGuardsDuplicateTerminalCallbacks) {
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);
  TransientHostLifecycleHarness harness;
  LayerSurfaceSpec spec{.output = screen,
                        .name_space = QStringLiteral("test-transient"),
                        .anchors = Anchor::Top | Anchor::Left,
                        .width = 100,
                        .height = 50,
                        .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Widgets/WidgetSurface.qml"))};
  ASSERT_TRUE(harness.request(spec));
  ASSERT_EQ(harness.hosts.size(), 1);
  LayerSurfaceHost* first = harness.hosts.front();
  first->configured();
  QCoreApplication::processEvents();
  EXPECT_EQ(harness.configured_count, 1);
  EXPECT_TRUE(harness.active());

  first->failed(QStringLiteral("provider failure"));
  first->closed();
  QCoreApplication::processEvents();
  EXPECT_FALSE(harness.active());
  EXPECT_EQ(harness.terminal_count, 1);
}

TEST(TransientSurfaceLifecycle, ReplacementIgnoresStaleTerminalCallback) {
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);
  TransientHostLifecycleHarness harness;
  LayerSurfaceSpec spec{.output = screen,
                        .name_space = QStringLiteral("test-transient"),
                        .anchors = Anchor::Top | Anchor::Left,
                        .width = 100,
                        .height = 50,
                        .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Widgets/WidgetSurface.qml"))};
  ASSERT_TRUE(harness.request(spec));
  LayerSurfaceHost* stale = harness.hosts.front();
  ASSERT_TRUE(harness.request(spec));
  ASSERT_EQ(harness.hosts.size(), 2);
  stale->closed();
  QCoreApplication::processEvents();
  EXPECT_TRUE(harness.active());
  EXPECT_EQ(harness.terminal_count, 0);
}
