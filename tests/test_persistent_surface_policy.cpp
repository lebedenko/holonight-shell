#include "BackgroundManager.h"
#include "ConfigService.h"
#include "LayerShellManager.h"
#include "MprisWidgetManager.h"
#include "ShellConstants.h"
#include "WidgetManager.h"
#include "WidgetSurfacePolicy.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>

#include <gtest/gtest.h>

using namespace HoloNight::ShellConfig;
using namespace Holonight::Wayland;

namespace {

template <typename Manager>
class InspectableManager : public Manager {
 public:
  using Manager::Manager;
  using Manager::surfaceSpec;
};

void expectPersistentDefaults(const LayerSurfaceSpec& spec, QScreen* screen) {
  EXPECT_EQ(spec.output, screen);
  EXPECT_EQ(spec.keyboard_interactivity, KeyboardInteractivity::None);
  EXPECT_TRUE(spec.window_flags.testFlag(Qt::FramelessWindowHint));
  EXPECT_TRUE(spec.window_flags.testFlag(Qt::BypassWindowManagerHint));
  EXPECT_EQ(spec.color, QColor(Qt::transparent));
  EXPECT_TRUE(static_cast<bool>(spec.before_load));
}

}  // namespace

class PersistentHostLifecycleManager : public PerMonitorLayerManager {
 public:
  PersistentHostLifecycleManager()
      : PerMonitorLayerManager("PersistentHostLifecycleManager", [this] {
          auto host = std::make_unique<LayerSurfaceHost>();
          hosts.push_back(host.get());
          return host;
        }) {}

  [[nodiscard]] std::size_t surfaceCount() const { return surfaces().size(); }
  QList<LayerSurfaceHost*> hosts;
  QStringList configured_outputs;
  QList<LayerSurfaceSpec> opened_specs;

 protected:
  LayerConfig layerConfig() const override {
    return {.layer = Layer::Bottom, .namespace_name = QStringLiteral("test-persistent"), .extra_flags = {}};
  }
  void configureSurface(LayerSurfaceSpec& spec, QScreen*) override {
    spec.anchors = Anchor::Top | Anchor::Left;
    spec.width = 100;
    spec.height = 50;
  }
  QmlSource qmlSource(QScreen*) override {
    return {.url = QUrl(QStringLiteral("qrc:/HolonightShell/Widgets/WidgetSurface.qml"))};
  }
  bool openHost(LayerSurfaceHost&, const LayerSurfaceSpec& spec) override {
    opened_specs.push_back(spec);
    return true;
  }
  void onHostConfigured(const QString& monitor_name) override { configured_outputs.push_back(monitor_name); }
};

TEST(PersistentSurfacePolicy, DescribesBarAndBackgroundHostsCompletely) {
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);
  InspectableManager<LayerShellManager> bar(nullptr);
  const LayerSurfaceSpec bar_spec = bar.surfaceSpec(screen);
  expectPersistentDefaults(bar_spec, screen);
  EXPECT_EQ(bar_spec.name_space, QStringLiteral("bar"));
  EXPECT_EQ(bar_spec.layer, Layer::Top);
  EXPECT_EQ(bar_spec.anchors, Anchor::Top | Anchor::Left | Anchor::Right);
  EXPECT_EQ(bar_spec.width, 0);
  EXPECT_EQ(bar_spec.height, kBarHeight);
  EXPECT_EQ(bar_spec.exclusive_zone, kBarHeight);
  EXPECT_EQ(bar_spec.input_region_policy, InputRegionPolicy::Default);
  EXPECT_EQ(bar_spec.qml_url, QUrl(QStringLiteral("qrc:/HolonightShell/Topbar/TopBar.qml")));
  EXPECT_EQ(bar_spec.initial_properties.value(QStringLiteral("barMonitorName")).toString(), screen->name());

  ConfigService config;
  InspectableManager<BackgroundManager> background(&config);
  const LayerSurfaceSpec background_spec = background.surfaceSpec(screen);
  expectPersistentDefaults(background_spec, screen);
  EXPECT_EQ(background_spec.name_space, QStringLiteral("background"));
  EXPECT_EQ(background_spec.layer, Layer::Background);
  EXPECT_EQ(background_spec.anchors, Anchor::Top | Anchor::Bottom | Anchor::Left | Anchor::Right);
  EXPECT_EQ(background_spec.width, 0);
  EXPECT_EQ(background_spec.height, 0);
  EXPECT_EQ(background_spec.exclusive_zone, -1);
  EXPECT_EQ(background_spec.input_region_policy, InputRegionPolicy::Empty);
  EXPECT_TRUE(background_spec.window_flags.testFlag(Qt::WindowTransparentForInput));
  EXPECT_EQ(background_spec.qml_url, QUrl(QStringLiteral("qrc:/HolonightShell/Background/Background.qml")));
  EXPECT_TRUE(background_spec.initial_properties.contains(QStringLiteral("imagePath")));
}

TEST(PersistentSurfacePolicy, DescribesClockCountdownAndMprisHostsCompletely) {
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);
  WidgetDefinition clock;
  clock.type = WidgetType::Clock;
  clock.position = WidgetPosition::RightTop;
  InspectableManager<WidgetManager> clock_manager(clock, 32, 0, QList<QStringList>{}, nullptr);
  const LayerSurfaceSpec clock_spec = clock_manager.surfaceSpec(screen);
  expectPersistentDefaults(clock_spec, screen);
  EXPECT_EQ(clock_spec.name_space, QStringLiteral("widget"));
  EXPECT_EQ(clock_spec.layer, Layer::Bottom);
  EXPECT_EQ(clock_spec.anchors, Anchor::Right | Anchor::Top);
  EXPECT_EQ(clock_spec.width, 460);
  EXPECT_EQ(clock_spec.height, 200);
  EXPECT_EQ(clock_spec.margin_top, kBarHeight + 32);
  EXPECT_EQ(clock_spec.margin_right, 32);
  EXPECT_EQ(clock_spec.margin_bottom, 32);
  EXPECT_EQ(clock_spec.margin_left, 32);
  EXPECT_EQ(clock_spec.exclusive_zone, -1);
  EXPECT_EQ(clock_spec.input_region_policy, InputRegionPolicy::Empty);
  EXPECT_EQ(clock_spec.qml_url, QUrl(QStringLiteral("qrc:/HolonightShell/Widgets/WidgetSurface.qml")));
  EXPECT_EQ(clock_spec.initial_properties.value(QStringLiteral("widgetType")).toString(), QStringLiteral("clock"));

  WidgetDefinition countdown;
  countdown.type = WidgetType::TimeToEvent;
  countdown.position = WidgetPosition::LeftBottom;
  countdown.time_to_event.title = QStringLiteral("Launch");
  InspectableManager<WidgetManager> countdown_manager(countdown, 12, 1, QList<QStringList>{}, nullptr);
  const LayerSurfaceSpec countdown_spec = countdown_manager.surfaceSpec(screen);
  EXPECT_EQ(countdown_spec.anchors, Anchor::Left | Anchor::Bottom);
  EXPECT_EQ(countdown_spec.margin_top, 12);
  EXPECT_EQ(countdown_spec.initial_properties.value(QStringLiteral("widgetType")).toString(),
            QStringLiteral("time-to-event"));
  EXPECT_EQ(countdown_spec.initial_properties.value(QStringLiteral("titleText")).toString(), QStringLiteral("Launch"));

  WidgetDefinition mpris;
  mpris.type = WidgetType::Mpris;
  mpris.position = WidgetPosition::CenterCenter;
  InspectableManager<MprisWidgetManager> mpris_manager(mpris, 24, 2, QList<QStringList>{}, nullptr, nullptr, nullptr);
  const LayerSurfaceSpec mpris_spec = mpris_manager.surfaceSpec(screen);
  expectPersistentDefaults(mpris_spec, screen);
  EXPECT_EQ(mpris_spec.name_space, QStringLiteral("widget"));
  EXPECT_EQ(mpris_spec.layer, Layer::Bottom);
  EXPECT_EQ(mpris_spec.anchors, Anchors{});
  EXPECT_EQ(mpris_spec.width, kMprisWidgetWidth);
  EXPECT_EQ(mpris_spec.height, kMprisWidgetHeight);
  EXPECT_EQ(mpris_spec.margin_top, 24);
  EXPECT_EQ(mpris_spec.exclusive_zone, -1);
  EXPECT_EQ(mpris_spec.input_region_policy, InputRegionPolicy::Empty);
  EXPECT_EQ(mpris_spec.qml_url, QUrl(QStringLiteral("qrc:/HolonightShell/Widgets/MprisWidgetSurface.qml")));
}

TEST(PersistentSurfaceLifecycle, KeepsConfiguredHostAndIgnoresDuplicateTerminalCallbacks) {
  PersistentHostLifecycleManager manager;
  manager.start();
  ASSERT_EQ(manager.hosts.size(), 1);
  ASSERT_EQ(manager.surfaceCount(), 1U);
  LayerSurfaceHost* host = manager.hosts.front();

  host->configured();
  QCoreApplication::processEvents();
  ASSERT_EQ(manager.configured_outputs.size(), 1);
  EXPECT_EQ(manager.surfaceCount(), 1U);

  host->failed(QStringLiteral("provider failed"));
  host->closed();
  QCoreApplication::processEvents();
  EXPECT_EQ(manager.surfaceCount(), 0U);
}

TEST(PersistentSurfaceLifecycle, RemovesAndRecreatesHostAcrossOutputHotplug) {
  PersistentHostLifecycleManager manager;
  manager.start();
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);
  ASSERT_EQ(manager.surfaceCount(), 1U);

  qGuiApp->screenRemoved(screen);
  EXPECT_EQ(manager.surfaceCount(), 0U);
  qGuiApp->screenAdded(screen);
  EXPECT_EQ(manager.surfaceCount(), 1U);
  EXPECT_EQ(manager.hosts.size(), 2);
  EXPECT_EQ(manager.opened_specs.back().output, screen);
}
