#include "IdleInhibitor.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QLoggingCategory>

#include <unistd.h>

Q_LOGGING_CATEGORY(lcIdleInhibitor, "holonight.idle.inhibitor")

namespace {
constexpr auto kLogindService = "org.freedesktop.login1";
constexpr auto kLogindPath = "/org/freedesktop/login1";
constexpr auto kLogindManagerIface = "org.freedesktop.login1.Manager";
}  // namespace

IdleInhibitor::IdleInhibitor(QObject* parent) : QObject(parent) {}

IdleInhibitor::~IdleInhibitor() { releaseFd(); }

bool IdleInhibitor::acquire(const QString& reason) {
  if (inhibit_fd_ >= 0) {
    return true;
  }
  QDBusInterface manager(QLatin1String(kLogindService), QLatin1String(kLogindPath), QLatin1String(kLogindManagerIface),
                         QDBusConnection::systemBus());
  const QDBusMessage reply = manager.call(QStringLiteral("Inhibit"), QStringLiteral("idle:sleep"),
                                          QStringLiteral("HoloNight Shell"), reason, QStringLiteral("block"));
  if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
    qCWarning(lcIdleInhibitor) << "Inhibit call failed:" << reply.errorMessage();
    return false;
  }
  const QVariant& arg = reply.arguments().at(0);
  if (arg.userType() != qMetaTypeId<QDBusUnixFileDescriptor>()) {
    qCWarning(lcIdleInhibitor) << "Inhibit reply has unexpected type:" << arg.typeName();
    return false;
  }
  inhibit_fd_ = arg.value<QDBusUnixFileDescriptor>().takeFileDescriptor();
  if (inhibit_fd_ < 0) {
    qCWarning(lcIdleInhibitor) << "Inhibit reply did not contain a valid file descriptor";
    return false;
  }
  qCInfo(lcIdleInhibitor) << "Inhibitor acquired, fd:" << inhibit_fd_ << "reason:" << reason;
  return true;
}

void IdleInhibitor::release() { releaseFd(); }

void IdleInhibitor::releaseFd() {
  if (inhibit_fd_ < 0) {
    return;
  }
  ::close(inhibit_fd_);
  qCInfo(lcIdleInhibitor) << "Inhibitor released, fd:" << inhibit_fd_;
  inhibit_fd_ = -1;
}
