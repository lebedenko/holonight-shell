#include "IdleService.h"

#include "ExtIdleNotifyBackend.h"
#include "IdleBackend.h"
#include "IdleInhibitor.h"
#include "LogindSessionResolver.h"
#include "NotificationService.h"
#include "NotificationTypes.h"
#include "ProcessEnvironment.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QLoggingCategory>
#include <QTimer>
#include <QVariantMap>

Q_LOGGING_CATEGORY(lcIdleService, "holonight.idle.service")

namespace {
constexpr auto kLogindService = "org.freedesktop.login1";
constexpr auto kLogindSessionIface = "org.freedesktop.login1.Session";
constexpr auto kPropertiesIface = "org.freedesktop.DBus.Properties";
constexpr auto kPropertiesChanged = "PropertiesChanged";
constexpr int kMissingDaemonNotificationDelayMs = 5000;

bool hasIdleDaemon(const ProcessEnvironment& env) {
  return env.isRunning(QStringLiteral("hypridle")) || env.isRunning(QStringLiteral("swayidle")) ||
         env.isUserServiceActive(QStringLiteral("hypridle.service")) ||
         env.isUserServiceActive(QStringLiteral("swayidle.service"));
}
}  // namespace

IdleService::IdleService(NotificationService* notif, QObject* parent)
    : QObject(parent),
      backend_(std::make_unique<ExtIdleNotifyBackend>(idle_threshold_ms_, this)),
      user_inhibitor_(std::make_unique<IdleInhibitor>(this)) {
  init(notif);
}

IdleService::IdleService(std::unique_ptr<IdleBackend> backend, bool daemon_detected, QObject* parent)
    : QObject(parent),
      backend_(std::move(backend)),
      user_inhibitor_(std::make_unique<IdleInhibitor>(this)),
      daemon_detected_(daemon_detected) {
  connect(backend_.get(), &IdleBackend::idleThresholdExceeded, this, &IdleService::onThresholdExceeded);
}

IdleService::IdleService(std::unique_ptr<IdleBackend> backend, std::unique_ptr<IdleInhibitor> inhibitor,
                         bool daemon_detected, QObject* parent)
    : QObject(parent),
      backend_(std::move(backend)),
      user_inhibitor_(std::move(inhibitor)),
      daemon_detected_(daemon_detected) {
  user_inhibitor_->setParent(this);
  connect(backend_.get(), &IdleBackend::idleThresholdExceeded, this, &IdleService::onThresholdExceeded);
}

IdleService::~IdleService() = default;

bool IdleService::idleBackendAvailable() const { return idle_backend_available_; }

uint IdleService::getIdleTimeSeconds() const { return backend_->getSessionIdleTimeSeconds(); }

void IdleService::setIdleThresholdMs(int threshold_ms) {
  if (threshold_ms == idle_threshold_ms_) {
    return;
  }
  idle_threshold_ms_ = threshold_ms;
  emit idleThresholdMsChanged();

  auto* ext_backend = qobject_cast<ExtIdleNotifyBackend*>(backend_.get());
  if (ext_backend != nullptr) {
    ext_backend->updateThreshold(threshold_ms);
  }
}

void IdleService::setIdleInhibited(bool inhibited) {
  if (inhibited == user_inhibited_) {
    return;
  }
  if (inhibited) {
    if (!user_inhibitor_->acquire(QStringLiteral("User-requested keep-awake"))) {
      return;
    }
    user_inhibited_ = true;
  } else {
    user_inhibitor_->release();
    user_inhibited_ = false;
  }
  emit idleInhibitedChanged();
}

void IdleService::onThresholdExceeded(bool idle) {
  if (idle == is_idle_) {
    return;
  }
  is_idle_ = idle;
  emit idleChanged(idle);
}

void IdleService::onLockedHintChanged(const QString& interface_name, const QVariantMap& changed_properties,
                                      const QStringList& /*invalidated_properties*/) {
  if (interface_name != QLatin1String(kLogindSessionIface)) {
    return;
  }
  const auto pos = changed_properties.find(QStringLiteral("LockedHint"));
  if (pos == changed_properties.end()) {
    return;
  }
  const bool locked = pos->toBool();
  if (locked == session_locked_) {
    return;
  }
  session_locked_ = locked;
  emit sessionLockedChanged();
}

void IdleService::init(NotificationService* notif) {
  connect(backend_.get(), &IdleBackend::idleThresholdExceeded, this, &IdleService::onThresholdExceeded);
  if (auto* ext_backend = qobject_cast<ExtIdleNotifyBackend*>(backend_.get())) {
    setIdleBackendAvailable(ext_backend->isBackendActive());
    connect(ext_backend, &ExtIdleNotifyBackend::backendActiveChanged, this, &IdleService::setIdleBackendAvailable);
  }
  detectDaemon(notif);
  subscribeLockedHint();
}

void IdleService::setIdleBackendAvailable(bool available) {
  if (available == idle_backend_available_) {
    return;
  }
  idle_backend_available_ = available;
  emit idleBackendAvailableChanged();
}

void IdleService::detectDaemon(NotificationService* notif) {
  SystemProcessEnvironment env;
  daemon_detected_ = hasIdleDaemon(env);

  if (!daemon_detected_ && notif != nullptr) {
    QTimer::singleShot(
        kMissingDaemonNotificationDelayMs, this, [this, notif_ptr = QPointer<NotificationService>(notif)]() {
          if (notif_ptr == nullptr) {
            return;
          }
          SystemProcessEnvironment delayed_env;
          const bool delayed_daemon_detected = hasIdleDaemon(delayed_env);
          if (delayed_daemon_detected) {
            daemon_detected_ = true;
            emit idleDaemonDetectedChanged();
            qCInfo(lcIdleService) << "Idle daemon appeared after startup; suppressing missing-daemon notification";
            return;
          }
          NotificationData data;
          data.app_name = QStringLiteral("HoloNight Shell");
          data.summary = QStringLiteral("No idle daemon detected");
          data.body = QStringLiteral(
              "Automatic screen lock and screen dimming won't work. "
              "Install and start hypridle or swayidle.");
          data.expire_timeout_ms = -1;
          notif_ptr->addOrReplace(std::move(data));
        });
  }
}

void IdleService::subscribeLockedHint() {
  session_path_ = resolveActiveLogindSessionPath();
  if (session_path_.isEmpty()) {
    qCInfo(lcIdleService) << "Could not determine logind session path; LockedHint unavailable";
    return;
  }

  QDBusConnection::systemBus().connect(QLatin1String(kLogindService), session_path_, QLatin1String(kPropertiesIface),
                                       QLatin1String(kPropertiesChanged), this,
                                       SLOT(onLockedHintChanged(QString, QVariantMap, QStringList)));

  readInitialLockedHint(session_path_);
}

void IdleService::readInitialLockedHint(const QString& session_path) {
  QDBusInterface props(QLatin1String(kLogindService), session_path, QLatin1String(kPropertiesIface),
                       QDBusConnection::systemBus());
  const QDBusMessage reply =
      props.call(QStringLiteral("Get"), QLatin1String(kLogindSessionIface), QStringLiteral("LockedHint"));
  if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
    qCInfo(lcIdleService) << "Could not read initial LockedHint:" << reply.errorMessage();
    return;
  }
  const QVariant inner = reply.arguments().at(0).value<QDBusVariant>().variant();
  session_locked_ = inner.toBool();
}
