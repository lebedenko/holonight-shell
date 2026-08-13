#include "SidebarManager.h"

#include "SidebarSurfacePolicy.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QScreen>

#include <algorithm>
#include <holonight/wayland/layershellcontext.h>
#include <utility>

using Holonight::Wayland::LayerShellContext;
using Holonight::Wayland::LayerSurfaceHost;
using Holonight::Wayland::LayerSurfaceSpec;

Q_LOGGING_CATEGORY(lcSidebar, "holonight.sidebar")

SidebarManager::SidebarManager(QObject* parent)
    : SidebarManager([] { return std::make_unique<LayerSurfaceHost>(); }, parent) {}

SidebarManager::SidebarManager(HostFactory host_factory, QObject* parent)
    : QObject(parent), host_factory_(std::move(host_factory)) {
  qputenv("QT_WAYLAND_USE_BYPASSWINDOWMANAGERHINT", "1");
}

SidebarManager::~SidebarManager() {
  const QList<QString> keys = surfaces_.keys();
  for (const QString& key : keys) {
    destroySurface(key, surfaces_.value(key).generation);
  }
}

void SidebarManager::start() {
  if (started_) {
    return;
  }
  started_ = true;
  connect(LayerShellContext::instance(), &LayerShellContext::availabilityChanged, this,
          &SidebarManager::handleProviderAvailabilityChanged);
  connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen* screen) {
    if (screen != nullptr) {
      handleOutputRemoved(screen->name());
    }
  });
}

bool SidebarManager::isOpen(const QString& monitor_name) const { return open_state_.value(monitor_name, false); }

bool SidebarManager::isKnownMonitor(const QString& monitor_name) {
  if (monitor_name.isEmpty()) {
    return false;
  }
  return std::ranges::any_of(QGuiApplication::screens(),
                             [&monitor_name](const QScreen* screen) { return screen->name() == monitor_name; });
}

void SidebarManager::toggle(const QString& monitor_name) {
  if (screenForName(monitor_name) == nullptr) {
    qCWarning(lcSidebar) << "toggle: rejecting unknown monitor" << monitor_name;
    return;
  }
  if (isOpen(monitor_name)) {
    closeOnMonitor(monitor_name);
    return;
  }
  closeAll();
  openOnMonitor(monitor_name);
}

void SidebarManager::close(const QString& monitor_name) {
  if (isOpen(monitor_name)) {
    closeOnMonitor(monitor_name);
  }
}

void SidebarManager::closeAll() {
  const QList<QString> keys = open_state_.keys();
  for (const QString& key : keys) {
    if (isOpen(key)) {
      closeOnMonitor(key);
    }
  }
}

void SidebarManager::onClosingAnimationFinished(const QString& monitor_name) {
  const auto iter = surfaces_.constFind(monitor_name);
  if (iter == surfaces_.constEnd() || !iter->closing || isOpen(monitor_name)) {
    return;
  }
  destroySurface(monitor_name, iter->generation);
}

void SidebarManager::onContentHeightChanged(const QString& monitor_name, int height) {
  const int clamped = boundedHeight(monitor_name, height);
  stored_heights_.insert(monitor_name, clamped);
  if (QObject* root = rootObject(monitor_name)) {
    root->setProperty("panelHeight", clamped);
  }
}

void SidebarManager::onCurrentTabChanged(const QString& monitor_name, int tab_index) {
  current_tabs_.insert(monitor_name, tab_index);
  Q_EMIT currentTabChanged(monitor_name, tab_index);
}

int SidebarManager::currentTabForMonitor(const QString& monitor_name) const {
  return current_tabs_.value(monitor_name, 0);
}

bool SidebarManager::openHost(LayerSurfaceHost& host, const LayerSurfaceSpec& spec) { return host.open(spec); }
bool SidebarManager::providerAvailable() const { return LayerShellContext::instance()->isAvailable(); }

QScreen* SidebarManager::screenForName(const QString& monitor_name) const {
  if (monitor_name.isEmpty()) {
    return nullptr;
  }
  for (QScreen* screen : QGuiApplication::screens()) {
    if (screen->name() == monitor_name) {
      return screen;
    }
  }
  return nullptr;
}

void SidebarManager::openOnMonitor(const QString& monitor_name) {
  if (!providerAvailable()) {
    return;
  }
  if (auto iter = surfaces_.find(monitor_name); iter != surfaces_.end()) {
    const auto state = iter->host->state();
    if (state != LayerSurfaceHost::State::Closed && state != LayerSurfaceHost::State::Failed) {
      iter->closing = false;
      open_state_.insert(monitor_name, true);
      if (QObject* root = iter->host->rootObject()) {
        root->setProperty("active", true);
        if (auto* item = qobject_cast<QQuickItem*>(root)) {
          item->forceActiveFocus();
        }
      }
      Q_EMIT sidebarOpened(monitor_name);
      return;
    }
    destroySurface(monitor_name, iter->generation);
  }

  QScreen* screen = screenForName(monitor_name);
  if (screen == nullptr) {
    return;
  }
  const int stored_height = stored_heights_.value(monitor_name, sidebarDefaultHeight());
  const LayerSurfaceSpec spec = sidebarSurfaceSpec(screen, monitor_name, boundedHeight(monitor_name, stored_height),
                                                   currentTabForMonitor(monitor_name));
  std::unique_ptr<LayerSurfaceHost> host = host_factory_();
  if (host == nullptr) {
    return;
  }
  const quint64 generation = ++next_generation_;
  LayerSurfaceHost* expected_host = host.get();
  connect(
      host.get(), &LayerSurfaceHost::failed, this,
      [this, monitor_name, generation, expected_host](const QString&) {
        handleHostTerminated(monitor_name, generation, expected_host);
      },
      Qt::QueuedConnection);
  connect(
      host.get(), &LayerSurfaceHost::closed, this,
      [this, monitor_name, generation, expected_host]() {
        handleHostTerminated(monitor_name, generation, expected_host);
      },
      Qt::QueuedConnection);
  if (!openHost(*host, spec)) {
    host->close();
    host.release()->deleteLater();
    return;
  }
  std::shared_ptr<LayerSurfaceHost> owned_host(host.release(), [](LayerSurfaceHost* value) { value->deleteLater(); });
  surfaces_.insert(monitor_name, SidebarSurface{.host = std::move(owned_host), .generation = generation});
  open_state_.insert(monitor_name, true);
  if (QObject* root = expected_host->rootObject()) {
    root->setProperty("active", true);
    if (auto* item = qobject_cast<QQuickItem*>(root)) {
      item->forceActiveFocus();
    }
  }
  Q_EMIT sidebarOpened(monitor_name);
}

void SidebarManager::closeOnMonitor(const QString& monitor_name) {
  auto iter = surfaces_.find(monitor_name);
  open_state_.insert(monitor_name, false);
  if (iter == surfaces_.end()) {
    return;
  }
  iter->closing = true;
  if (QObject* root = iter->host->rootObject()) {
    root->setProperty("active", false);
  }
  Q_EMIT sidebarClosed(monitor_name);
}

void SidebarManager::destroySurface(const QString& monitor_name, quint64 generation) {
  auto iter = surfaces_.find(monitor_name);
  if (iter == surfaces_.end() || iter->generation != generation) {
    return;
  }
  std::shared_ptr<LayerSurfaceHost> host = std::move(iter->host);
  surfaces_.erase(iter);
  if (host != nullptr) {
    host->close();
    host.reset();
  }
}

void SidebarManager::handleHostTerminated(const QString& monitor_name, quint64 generation,
                                          LayerSurfaceHost* expected_host) {
  auto iter = surfaces_.find(monitor_name);
  if (iter == surfaces_.end() || iter->generation != generation || iter->host.get() != expected_host) {
    return;
  }
  const bool was_open = open_state_.value(monitor_name, false);
  open_state_.insert(monitor_name, false);
  destroySurface(monitor_name, generation);
  if (was_open) {
    Q_EMIT sidebarClosed(monitor_name);
  }
}

void SidebarManager::handleProviderAvailabilityChanged() {
  if (providerAvailable()) {
    return;
  }
  const QList<QString> keys = surfaces_.keys();
  for (const QString& key : keys) {
    const bool was_open = isOpen(key);
    open_state_.insert(key, false);
    destroySurface(key, surfaces_.value(key).generation);
    if (was_open) {
      Q_EMIT sidebarClosed(key);
    }
  }
}

void SidebarManager::handleOutputRemoved(const QString& monitor_name) {
  const bool was_open = isOpen(monitor_name);
  open_state_.remove(monitor_name);
  if (surfaces_.contains(monitor_name)) {
    destroySurface(monitor_name, surfaces_.value(monitor_name).generation);
  }
  stored_heights_.remove(monitor_name);
  current_tabs_.remove(monitor_name);
  if (was_open) {
    Q_EMIT sidebarClosed(monitor_name);
  }
}

int SidebarManager::boundedHeight(const QString& monitor_name, int requested_height) const {
  const QScreen* screen = screenForName(monitor_name);
  return boundedSidebarHeight(requested_height, screen == nullptr ? 0 : screen->geometry().height());
}

QObject* SidebarManager::rootObject(const QString& monitor_name) const {
  const auto iter = surfaces_.constFind(monitor_name);
  return iter == surfaces_.constEnd() || iter->host == nullptr ? nullptr : iter->host->rootObject();
}
