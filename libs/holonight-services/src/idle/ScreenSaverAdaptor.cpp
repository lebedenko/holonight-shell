#include "ScreenSaverAdaptor.h"

#include "IdleInhibitor.h"
#include "IdleService.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QFile>
#include <QLoggingCategory>

#include <utility>

Q_LOGGING_CATEGORY(lcScreenSaver, "holonight.idle.screensaver")

namespace {
constexpr auto kScreenSaverService = "org.freedesktop.ScreenSaver";
constexpr auto kScreenSaverPath = "/org/freedesktop/ScreenSaver";
}  // namespace

ScreenSaverAdaptor::ScreenSaverAdaptor(IdleService* idle_service, QObject* parent)
    : ScreenSaverAdaptor(idle_service, std::make_unique<IdleInhibitor>(), parent) {}

ScreenSaverAdaptor::ScreenSaverAdaptor(IdleService* idle_service, std::unique_ptr<IdleInhibitor> inhibitor,
                                       QObject* parent)
    : QObject(parent), idle_service_(idle_service), inhibitor_(std::move(inhibitor)) {
  inhibitor_->setParent(this);
}

ScreenSaverAdaptor::~ScreenSaverAdaptor() = default;

bool ScreenSaverAdaptor::registerService() {
  auto bus = QDBusConnection::sessionBus();
  if (!bus.registerObject(QLatin1String(kScreenSaverPath), this, QDBusConnection::ExportScriptableContents)) {
    qCWarning(lcScreenSaver) << "Failed to register object at" << kScreenSaverPath << bus.lastError().message();
    return false;
  }
  if (!bus.registerService(QLatin1String(kScreenSaverService))) {
    // Name already taken — find out who owns it to give an actionable message.
    const QDBusReply<QString> owner = bus.interface()->serviceOwner(QLatin1String(kScreenSaverService));
    const QString owner_name = owner.isValid() ? owner.value() : QStringLiteral("unknown");

    // Identify the process behind the unique name so we can advise the user.
    const QDBusReply<uint> pid_reply = bus.interface()->servicePid(QLatin1String(kScreenSaverService));
    QString process_name;
    if (pid_reply.isValid()) {
      QFile comm(QStringLiteral("/proc/%1/comm").arg(pid_reply.value()));
      if (comm.open(QIODevice::ReadOnly)) {
        process_name = QString::fromUtf8(comm.readAll()).trimmed();
      }
    }

    if (process_name == QLatin1String("hypridle")) {
      // hypridle 0.1.x owns the name but only exposes Inhibit/UnInhibit.
      // GetSessionIdleTime() and ActiveChanged are missing, so Teams/Zoom "Away" detection
      // won't work. Fixed in hypridle 0.2.0+.
      qCInfo(lcScreenSaver) << "org.freedesktop.ScreenSaver is owned by hypridle which does not provide"
                               " GetSessionIdleTime() or ActiveChanged. Teams/Zoom idle detection requires"
                               " hypridle ≥ 0.2.0. Skipping our registration.";
    } else {
      qCWarning(lcScreenSaver) << "Failed to claim org.freedesktop.ScreenSaver — already owned by"
                               << (process_name.isEmpty() ? owner_name : process_name)
                               << "(GetSessionIdleTime/ActiveChanged will not be available)";
    }
    bus.unregisterObject(QLatin1String(kScreenSaverPath));
    return false;
  }
  qCInfo(lcScreenSaver) << "org.freedesktop.ScreenSaver registered";
  return true;
}

// NOLINTBEGIN(readability-identifier-naming)
uint ScreenSaverAdaptor::GetSessionIdleTime() { return idle_service_->getIdleTimeSeconds(); }

uint ScreenSaverAdaptor::Inhibit(const QString& app_name, const QString& reason) {
  if (inhibit_cookies_.isEmpty() && !inhibitor_->acquire(reason)) {
    qCWarning(lcScreenSaver) << "Inhibit from" << app_name << "failed to acquire logind inhibitor";
    return 0;
  }
  const uint cookie = next_cookie_++;
  inhibit_cookies_.insert(cookie, app_name);
  qCInfo(lcScreenSaver) << "Inhibit from" << app_name << "reason:" << reason << "cookie:" << cookie;
  return cookie;
}

void ScreenSaverAdaptor::UnInhibit(uint cookie) {
  if (!inhibit_cookies_.remove(cookie)) {
    qCWarning(lcScreenSaver) << "UnInhibit: unknown cookie" << cookie;
    return;
  }
  if (inhibit_cookies_.isEmpty()) {
    inhibitor_->release();
  }
  qCInfo(lcScreenSaver) << "UnInhibit cookie:" << cookie;
}
// NOLINTEND(readability-identifier-naming)

void ScreenSaverAdaptor::onIdleChanged(bool idle) {
  emit ActiveChanged(idle);  // NOLINT(readability-identifier-naming)
}
