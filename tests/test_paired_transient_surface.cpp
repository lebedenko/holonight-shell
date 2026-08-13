#include "PairedTransientSurfaceHost.h"

#include <QGuiApplication>
#include <QScreen>

#include <gtest/gtest.h>

using namespace Holonight::Wayland;

namespace {
PairedLayerSurfaceSpec pairSpec(QScreen* screen) {
  const auto spec = [screen](const QString& name) {
    return LayerSurfaceSpec{.output = screen,
                            .name_space = name,
                            .anchors = Anchor::Top | Anchor::Left,
                            .width = 100,
                            .height = 50,
                            .qml_url = QUrl(QStringLiteral("qrc:/HolonightShell/Widgets/WidgetSurface.qml"))};
  };
  return {.dismiss = spec(QStringLiteral("test-dismiss")), .content = spec(QStringLiteral("test-content"))};
}

class PairedHostHarness : public PairedTransientSurfaceHost {
 public:
  PairedHostHarness()
      : PairedTransientSurfaceHost("PairedHostHarness", [this] {
          auto host = std::make_unique<LayerSurfaceHost>();
          hosts.push_back(host.get());
          return host;
        }) {}

  bool request(const PairedLayerSurfaceSpec& spec) { return openPair(spec); }
  void explicitClose() {
    clearPendingPair();
    closePair();
  }
  void availabilityChanged() { providerAvailabilityChanged(); }
  void removeOutput(QScreen* screen) { outputRemoved(screen); }
  [[nodiscard]] bool active() const { return hasPair(); }
  [[nodiscard]] bool pending() const { return hasPendingPair(); }

  QList<LayerSurfaceHost*> hosts;
  QList<bool> open_results;
  bool available = true;
  int opened_count = 0;
  int configured_count = 0;
  int terminal_count = 0;

 protected:
  bool openHost(LayerSurfaceHost&, const LayerSurfaceSpec&) override {
    if (open_results.isEmpty()) return true;
    return open_results.takeFirst();
  }
  bool providerAvailable() const override { return available; }
  void onPairOpened() override { ++opened_count; }
  void onPairConfigured() override { ++configured_count; }
  void onPairTerminated() override { ++terminal_count; }
};
}  // namespace

TEST(PairedTransientSurfaceLifecycle, OpensDismissBeforeContentAndRetainsConfigureEvents) {
  QScreen* screen = QGuiApplication::primaryScreen();
  ASSERT_NE(screen, nullptr);
  PairedHostHarness harness;
  ASSERT_TRUE(harness.request(pairSpec(screen)));
  ASSERT_EQ(harness.hosts.size(), 2);
  EXPECT_EQ(harness.opened_count, 1);

  harness.hosts[0]->configured();
  harness.hosts[1]->configured();
  QCoreApplication::processEvents();
  EXPECT_EQ(harness.configured_count, 2);
  EXPECT_TRUE(harness.active());
}

TEST(PairedTransientSurfaceLifecycle, ExplicitCloseTearsDownBothWithoutTerminalCallback) {
  PairedHostHarness harness;
  ASSERT_TRUE(harness.request(pairSpec(QGuiApplication::primaryScreen())));
  harness.explicitClose();
  EXPECT_FALSE(harness.active());
  EXPECT_FALSE(harness.pending());
  EXPECT_EQ(harness.terminal_count, 0);
}

TEST(PairedTransientSurfaceLifecycle, EitherOpenFailureTearsDownThePair) {
  PairedHostHarness first_failure;
  first_failure.open_results = {false};
  EXPECT_FALSE(first_failure.request(pairSpec(QGuiApplication::primaryScreen())));
  EXPECT_FALSE(first_failure.active());
  EXPECT_EQ(first_failure.terminal_count, 1);

  PairedHostHarness second_failure;
  second_failure.open_results = {true, false};
  EXPECT_FALSE(second_failure.request(pairSpec(QGuiApplication::primaryScreen())));
  EXPECT_FALSE(second_failure.active());
  EXPECT_EQ(second_failure.terminal_count, 1);
}

TEST(PairedTransientSurfaceLifecycle, EitherHostCloseAndDuplicateCallbacksTerminateOnce) {
  for (int terminal_index : {0, 1}) {
    PairedHostHarness harness;
    ASSERT_TRUE(harness.request(pairSpec(QGuiApplication::primaryScreen())));
    LayerSurfaceHost* terminal = harness.hosts[terminal_index];
    terminal->closed();
    terminal->failed(QStringLiteral("duplicate"));
    QCoreApplication::processEvents();
    EXPECT_FALSE(harness.active());
    EXPECT_EQ(harness.terminal_count, 1);
  }
}

TEST(PairedTransientSurfaceLifecycle, ReplacementIgnoresStaleCallbacks) {
  PairedHostHarness harness;
  ASSERT_TRUE(harness.request(pairSpec(QGuiApplication::primaryScreen())));
  LayerSurfaceHost* stale = harness.hosts[0];
  ASSERT_TRUE(harness.request(pairSpec(QGuiApplication::primaryScreen())));
  stale->closed();
  QCoreApplication::processEvents();
  EXPECT_TRUE(harness.active());
  EXPECT_EQ(harness.terminal_count, 0);
}

TEST(PairedTransientSurfaceLifecycle, ReplaysUnavailableRequestAndRetainsItAcrossProviderLoss) {
  PairedHostHarness harness;
  harness.available = false;
  EXPECT_FALSE(harness.request(pairSpec(QGuiApplication::primaryScreen())));
  EXPECT_TRUE(harness.pending());
  harness.available = true;
  harness.availabilityChanged();
  EXPECT_TRUE(harness.active());

  harness.available = false;
  harness.availabilityChanged();
  EXPECT_FALSE(harness.active());
  EXPECT_TRUE(harness.pending());
  harness.available = true;
  harness.availabilityChanged();
  EXPECT_TRUE(harness.active());
  EXPECT_EQ(harness.opened_count, 2);
}

TEST(PairedTransientSurfaceLifecycle, OutputRemovalAndGlobalLossCleanUpBothHosts) {
  QScreen* screen = QGuiApplication::primaryScreen();
  PairedHostHarness removed;
  ASSERT_TRUE(removed.request(pairSpec(screen)));
  removed.removeOutput(screen);
  EXPECT_FALSE(removed.active());
  EXPECT_FALSE(removed.pending());
  EXPECT_EQ(removed.terminal_count, 1);

  PairedHostHarness lost;
  ASSERT_TRUE(lost.request(pairSpec(screen)));
  lost.available = false;
  lost.availabilityChanged();
  EXPECT_FALSE(lost.active());
  EXPECT_TRUE(lost.pending());
  EXPECT_EQ(lost.terminal_count, 1);
}
