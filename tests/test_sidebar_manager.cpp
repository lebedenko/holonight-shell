#include "SidebarManager.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPointer>
#include <QScreen>
#include <QSignalSpy>

#include <gtest/gtest.h>

using Holonight::Wayland::LayerSurfaceHost;
using Holonight::Wayland::LayerSurfaceSpec;

class SidebarManagerHarness final : public SidebarManager {
 public:
  SidebarManagerHarness()
      : SidebarManager([this] {
          auto host = std::make_unique<LayerSurfaceHost>();
          hosts.append(host.get());
          return host;
        }) {}

  QList<LayerSurfaceHost*> hosts;
  QList<LayerSurfaceSpec> specs;
  bool available = true;
  bool open_succeeds = true;
  QScreen* screen = QGuiApplication::primaryScreen();

  void providerChanged() { handleProviderAvailabilityChanged(); }
  void outputRemoved(const QString& name) { handleOutputRemoved(name); }

 protected:
  bool openHost(LayerSurfaceHost& /*host*/, const LayerSurfaceSpec& spec) override {
    specs.append(spec);
    return open_succeeds;
  }
  [[nodiscard]] bool providerAvailable() const override { return available; }
  [[nodiscard]] QScreen* screenForName(const QString& name) const override {
    return !name.isEmpty() && screen != nullptr ? screen : nullptr;
  }
};

static void deliverQueuedSignals() { QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall); }

TEST(SidebarManagerMonitorValidation, RejectsUnknownAndEmptyMonitorNames) {
  EXPECT_FALSE(SidebarManager::isKnownMonitor(QStringLiteral("definitely-not-a-real-monitor-xyz123")));
  EXPECT_FALSE(SidebarManager::isKnownMonitor(QString{}));
}

TEST(SidebarManagerLifecycle, OpensWithCachedHeightAndTabAndEmitsOnce) {
  SidebarManagerHarness manager;
  QSignalSpy opened(&manager, &SidebarManager::sidebarOpened);
  manager.onContentHeightChanged(QStringLiteral("DP-3"), 700);
  manager.onCurrentTabChanged(QStringLiteral("DP-3"), 4);
  manager.toggle(QStringLiteral("DP-3"));

  ASSERT_TRUE(manager.isOpen(QStringLiteral("DP-3")));
  ASSERT_EQ(opened.count(), 1);
  ASSERT_EQ(manager.specs.size(), 1);
  EXPECT_EQ(manager.specs[0].initial_properties.value(QStringLiteral("panelHeight")).toInt(), 700);
  EXPECT_EQ(manager.specs[0].initial_properties.value(QStringLiteral("currentTab")).toInt(), 4);
}

TEST(SidebarManagerLifecycle, ConfigureNotificationDoesNotDuplicateOpenSignal) {
  SidebarManagerHarness manager;
  QSignalSpy opened(&manager, &SidebarManager::sidebarOpened);
  manager.toggle(QStringLiteral("DP-3"));
  QMetaObject::invokeMethod(manager.hosts.front(), "configured", Qt::DirectConnection);
  deliverQueuedSignals();
  EXPECT_TRUE(manager.isOpen(QStringLiteral("DP-3")));
  EXPECT_EQ(opened.count(), 1);
}

TEST(SidebarManagerLifecycle, AnimatedCloseRetainsHostUntilCallback) {
  SidebarManagerHarness manager;
  manager.toggle(QStringLiteral("DP-3"));
  QPointer<LayerSurfaceHost> host = manager.hosts.front();
  manager.close(QStringLiteral("DP-3"));
  EXPECT_FALSE(manager.isOpen(QStringLiteral("DP-3")));
  manager.onClosingAnimationFinished(QStringLiteral("DP-3"));
  deliverQueuedSignals();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  EXPECT_TRUE(host.isNull());
}

TEST(SidebarManagerLifecycle, CrossMonitorOpenRetainsClosingHost) {
  SidebarManagerHarness manager;
  manager.toggle(QStringLiteral("DP-3"));
  manager.toggle(QStringLiteral("HDMI-A-1"));
  EXPECT_FALSE(manager.isOpen(QStringLiteral("DP-3")));
  EXPECT_TRUE(manager.isOpen(QStringLiteral("HDMI-A-1")));
  EXPECT_EQ(manager.hosts.size(), 2);
}

TEST(SidebarManagerLifecycle, SameMonitorReopenMakesOldAnimationCallbackNoOp) {
  SidebarManagerHarness manager;
  manager.toggle(QStringLiteral("DP-3"));
  manager.close(QStringLiteral("DP-3"));
  manager.toggle(QStringLiteral("DP-3"));
  manager.onClosingAnimationFinished(QStringLiteral("DP-3"));
  EXPECT_TRUE(manager.isOpen(QStringLiteral("DP-3")));
  EXPECT_EQ(manager.hosts.size(), 1);
}

TEST(SidebarManagerLifecycle, OpenFailureLeavesLogicalStateClosed) {
  SidebarManagerHarness manager;
  QSignalSpy opened(&manager, &SidebarManager::sidebarOpened);
  manager.open_succeeds = false;
  manager.toggle(QStringLiteral("DP-3"));
  EXPECT_FALSE(manager.isOpen(QStringLiteral("DP-3")));
  EXPECT_EQ(opened.count(), 0);
}

TEST(SidebarManagerLifecycle, HostFailureAndDuplicateTerminalCallbacksCloseOnce) {
  SidebarManagerHarness manager;
  QSignalSpy closed(&manager, &SidebarManager::sidebarClosed);
  manager.toggle(QStringLiteral("DP-3"));
  LayerSurfaceHost* host = manager.hosts.front();
  QMetaObject::invokeMethod(host, "failed", Qt::DirectConnection, Q_ARG(QString, QStringLiteral("failure")));
  QMetaObject::invokeMethod(host, "closed", Qt::DirectConnection);
  deliverQueuedSignals();
  EXPECT_FALSE(manager.isOpen(QStringLiteral("DP-3")));
  EXPECT_EQ(closed.count(), 1);
}

TEST(SidebarManagerLifecycle, StaleTerminalCallbackCannotCloseReplacement) {
  SidebarManagerHarness manager;
  manager.toggle(QStringLiteral("DP-3"));
  LayerSurfaceHost* stale = manager.hosts.front();
  manager.close(QStringLiteral("DP-3"));
  manager.onClosingAnimationFinished(QStringLiteral("DP-3"));
  manager.toggle(QStringLiteral("DP-3"));
  QMetaObject::invokeMethod(stale, "closed", Qt::DirectConnection);
  deliverQueuedSignals();
  EXPECT_TRUE(manager.isOpen(QStringLiteral("DP-3")));
}

TEST(SidebarManagerLifecycle, OutputRemovalClearsStateAndCache) {
  SidebarManagerHarness manager;
  QSignalSpy closed(&manager, &SidebarManager::sidebarClosed);
  manager.onCurrentTabChanged(QStringLiteral("DP-3"), 4);
  manager.toggle(QStringLiteral("DP-3"));
  manager.outputRemoved(QStringLiteral("DP-3"));
  EXPECT_FALSE(manager.isOpen(QStringLiteral("DP-3")));
  EXPECT_EQ(manager.currentTabForMonitor(QStringLiteral("DP-3")), 0);
  EXPECT_EQ(closed.count(), 1);
}

TEST(SidebarManagerLifecycle, ProviderLossClosesAllImmediatelyWithoutReplay) {
  SidebarManagerHarness manager;
  QSignalSpy closed(&manager, &SidebarManager::sidebarClosed);
  manager.toggle(QStringLiteral("DP-3"));
  manager.available = false;
  manager.providerChanged();
  EXPECT_FALSE(manager.isOpen(QStringLiteral("DP-3")));
  EXPECT_EQ(closed.count(), 1);
  manager.available = true;
  manager.providerChanged();
  EXPECT_FALSE(manager.isOpen(QStringLiteral("DP-3")));
  EXPECT_EQ(manager.specs.size(), 1);
}

TEST(SidebarManagerCurrentTab, EmitsRepeatedReportsAndKeepsPerMonitorState) {
  SidebarManagerHarness manager;
  QSignalSpy spy(&manager, &SidebarManager::currentTabChanged);
  manager.onCurrentTabChanged(QStringLiteral("DP-3"), 4);
  manager.onCurrentTabChanged(QStringLiteral("DP-3"), 4);
  manager.onCurrentTabChanged(QStringLiteral("HDMI-A-1"), 2);
  EXPECT_EQ(manager.currentTabForMonitor(QStringLiteral("DP-3")), 4);
  EXPECT_EQ(manager.currentTabForMonitor(QStringLiteral("HDMI-A-1")), 2);
  EXPECT_EQ(spy.count(), 3);
}
