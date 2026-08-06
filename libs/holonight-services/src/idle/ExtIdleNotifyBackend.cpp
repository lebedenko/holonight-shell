#include "ExtIdleNotifyBackend.h"

#include "qwayland-ext-idle-notify-v1.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QtGui/qguiapplication_platform.h>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>

Q_LOGGING_CATEGORY(lcIdleBackend, "holonight.idle.backend")

static constexpr uint kTrackerThresholdMs = 1000;

// Minimal Wayland global binding — not a QObject so it avoids diamond inheritance with IdleBackend.
class IdleNotifierGlobal : public QWaylandClientExtensionTemplate<IdleNotifierGlobal>,
                           public QtWayland::ext_idle_notifier_v1 {
 public:
  IdleNotifierGlobal() : QWaylandClientExtensionTemplate(1) {}
  ~IdleNotifierGlobal() override {
    if (isInitialized()) {
      destroy();
    }
  }
  IdleNotifierGlobal(const IdleNotifierGlobal&) = delete;
  IdleNotifierGlobal& operator=(const IdleNotifierGlobal&) = delete;
  IdleNotifierGlobal(IdleNotifierGlobal&&) = delete;
  IdleNotifierGlobal& operator=(IdleNotifierGlobal&&) = delete;
};

// Wraps one ext_idle_notification_v1. Calls callback(true) on idled, callback(false) on resumed.
class IdleNotification : public QtWayland::ext_idle_notification_v1 {
 public:
  using Callback = std::function<void(bool)>;
  explicit IdleNotification(::ext_idle_notification_v1* handle, Callback callback)
      : QtWayland::ext_idle_notification_v1(handle), callback_(std::move(callback)) {}
  ~IdleNotification() override {
    if (isInitialized()) {
      destroy();
    }
  }
  IdleNotification(const IdleNotification&) = delete;
  IdleNotification& operator=(const IdleNotification&) = delete;
  IdleNotification(IdleNotification&&) = delete;
  IdleNotification& operator=(IdleNotification&&) = delete;

 protected:
  void ext_idle_notification_v1_idled() override { callback_(true); }
  void ext_idle_notification_v1_resumed() override { callback_(false); }

 private:
  Callback callback_;
};

static ::wl_seat* defaultSeat() {
  if (qGuiApp == nullptr) {
    return nullptr;
  }
  const auto* wayland_app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
  return wayland_app != nullptr ? wayland_app->seat() : nullptr;
}

ExtIdleNotifyBackend::ExtIdleNotifyBackend(int threshold_ms, QObject* parent)
    : IdleBackend(parent), global_(std::make_unique<IdleNotifierGlobal>()), threshold_ms_(threshold_ms) {
  connect(global_.get(), &QWaylandClientExtension::activeChanged, this, &ExtIdleNotifyBackend::onGlobalActiveChanged);
}

ExtIdleNotifyBackend::~ExtIdleNotifyBackend() { destroyNotifications(); }

uint ExtIdleNotifyBackend::getSessionIdleTimeSeconds() const {
  if (!backend_active_ || !tracker_idle_) {
    return 0;
  }
  const auto elapsed = std::chrono::steady_clock::now() - last_activity_;
  const auto secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
  return static_cast<uint>(std::max<long long>(0LL, secs));
}

bool ExtIdleNotifyBackend::isBackendActive() const { return backend_active_; }

void ExtIdleNotifyBackend::updateThreshold(int threshold_ms) {
  threshold_ms_ = threshold_ms;
  destroyThresholder();
  if (!global_->isActive()) {
    return;
  }
  ::wl_seat* seat = defaultSeat();
  if (seat == nullptr) {
    return;
  }
  thresholder_ =
      std::make_unique<IdleNotification>(global_->get_idle_notification(static_cast<uint>(threshold_ms_), seat),
                                         [this](bool idle) { onThresholderEvent(idle); });
}

void ExtIdleNotifyBackend::onGlobalActiveChanged() {
  const bool active = global_->isActive();
  if (active == backend_active_) {
    return;
  }
  backend_active_ = active;
  emit backendActiveChanged(active);

  if (!active) {
    qCInfo(lcIdleBackend) << "ext-idle-notify-v1 protocol lost";
    destroyNotifications();
    tracker_idle_ = false;
    return;
  }
  qCInfo(lcIdleBackend) << "ext-idle-notify-v1 protocol active";
  createNotifications();
}

void ExtIdleNotifyBackend::createNotifications() {
  ::wl_seat* seat = defaultSeat();
  if (seat == nullptr) {
    qCWarning(lcIdleBackend) << "wl_seat not available; idle tracking disabled";
    return;
  }

  tracker_ = std::make_unique<IdleNotification>(global_->get_idle_notification(kTrackerThresholdMs, seat),
                                                [this](bool idle) { onTrackerEvent(idle); });

  thresholder_ =
      std::make_unique<IdleNotification>(global_->get_idle_notification(static_cast<uint>(threshold_ms_), seat),
                                         [this](bool idle) { onThresholderEvent(idle); });
}

void ExtIdleNotifyBackend::destroyNotifications() {
  thresholder_.reset();
  tracker_.reset();
}

void ExtIdleNotifyBackend::destroyThresholder() { thresholder_.reset(); }

void ExtIdleNotifyBackend::onTrackerEvent(bool idle) {
  tracker_idle_ = idle;
  if (idle) {
    last_activity_ = std::chrono::steady_clock::now() - std::chrono::milliseconds(kTrackerThresholdMs);
  } else {
    last_activity_ = std::chrono::steady_clock::now();
  }
}

void ExtIdleNotifyBackend::onThresholderEvent(bool idle) { emit idleThresholdExceeded(idle); }
