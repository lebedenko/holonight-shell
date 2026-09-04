#include "WindowActivationServer.h"

#include "CompositorService.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QLoggingCategory>

namespace {

Q_LOGGING_CATEGORY(lcWindowActivationServer, "holonight.windowactivation.server")

QStringView resultCategory(WindowActivationResult result) {
  switch (result) {
    case WindowActivationResult::Accepted:
      return u"accepted";
    case WindowActivationResult::InvalidRequest:
      return u"invalid";
    case WindowActivationResult::Unsupported:
      return u"unsupported";
    case WindowActivationResult::Disconnected:
      return u"disconnected";
    case WindowActivationResult::Missing:
      return u"missing";
    case WindowActivationResult::Ambiguous:
      return u"ambiguous";
    case WindowActivationResult::Busy:
      return u"busy";
    case WindowActivationResult::Failed:
      return u"failed";
  }
  return u"failed";
}

class WindowActivationAdaptor final : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.holonight.Shell.WindowActivation1")

 public:
  explicit WindowActivationAdaptor(WindowActivationServer* server) : QDBusAbstractAdaptor(server), server_(server) {}

 public Q_SLOTS:
  // Protocol spelling is fixed by the public D-Bus contract.
  // NOLINTNEXTLINE(readability-identifier-naming)
  bool RequestWindowActivation(const QList<quint32>& processLineage, const QString& titleHint) {
    return server_->requestWindowActivation(processLineage, titleHint);
  }

 private:
  WindowActivationServer* server_;
};

}  // namespace

WindowActivationServer::WindowActivationServer(CompositorService* compositor, QObject* parent)
    : QObject(parent), compositor_(compositor) {
  new WindowActivationAdaptor(this);
}

WindowActivationServer::~WindowActivationServer() {
  QDBusConnection bus = QDBusConnection::sessionBus();
  if (object_registered_) {
    bus.unregisterObject(QLatin1String(kObjectPath));
  }
  if (service_registered_) {
    bus.unregisterService(QLatin1String(kServiceName));
  }
}

bool WindowActivationServer::start() {
  if (start_attempted_) {
    return object_registered_;
  }
  start_attempted_ = true;

  QDBusConnection bus = QDBusConnection::sessionBus();
  if (!bus.isConnected() || !bus.registerService(QLatin1String(kServiceName))) {
    qCWarning(lcWindowActivationServer) << "registration failed: service unavailable";
    return false;
  }
  service_registered_ = true;

  if (!bus.registerObject(QLatin1String(kObjectPath), this, QDBusConnection::ExportAdaptors)) {
    qCWarning(lcWindowActivationServer) << "registration failed: object unavailable";
    bus.unregisterService(QLatin1String(kServiceName));
    service_registered_ = false;
    return false;
  }
  object_registered_ = true;
  qCInfo(lcWindowActivationServer) << "registered";
  return true;
}

bool WindowActivationServer::requestWindowActivation(const QList<quint32>& process_lineage, const QString& title_hint) {
  const WindowActivationResult result =
      compositor_ == nullptr
          ? WindowActivationResult::Unsupported
          : compositor_->requestWindowActivation({.process_lineage = process_lineage, .title_hint = title_hint});
  qCInfo(lcWindowActivationServer) << "request result:" << resultCategory(result);
  return result == WindowActivationResult::Accepted;
}

#include "WindowActivationServer.moc"
