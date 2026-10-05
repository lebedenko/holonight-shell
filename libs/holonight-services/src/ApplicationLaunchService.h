#pragma once
#include "launcher/DesktopEntryScanner.h"
#include "launcher/LauncherCommand.h"

#include <QDBusObjectPath>
#include <QHash>
#include <QObject>
#include <QProcessEnvironment>

#include <functional>

class ApplicationLaunchService : public QObject {
 public:
  enum class Backend { Uwsm, Systemd, Detached, Unavailable };
  struct Capabilities {
    bool managerAvailable{false};
    bool uwsmActive{false};
    bool runningAsService{false};
    QString error;
  };
  using Probe = std::function<Capabilities()>;
  static Backend backendFor(const Capabilities& capabilities);
  static QString nativeUnitName(const QString& executable);
  static QProcessEnvironment nativeEnvironment(QProcessEnvironment environment);
  using Completion = std::function<void(const QString&, const QString&)>;
  using Transport = std::function<QString(Backend, const LauncherCommand&, const DesktopEntry*, const QString&)>;
  explicit ApplicationLaunchService(QObject* parent = nullptr, Transport transport = {}, Probe probe = {});
  QString launch(LauncherCommand command, QObject* context, Completion completion);
  QString launchDesktop(DesktopEntry entry, QString action, QObject* context, Completion completion);

 private:
  QString submit(LauncherCommand command, DesktopEntry entry, QString action, QObject* context, Completion completion);
  Transport transport_;
  Probe probe_;
};

// Receives startup job results on the launch worker's event loop.
class ApplicationLaunchJobObserver : public QObject {
  Q_OBJECT
 public:
  QString jobPath;
  QString result;
  QHash<QString, QString> results;
 signals:
  void finished();
 public slots:
  void jobRemoved(uint, const QDBusObjectPath& path, const QString&, const QString& status) {
    results.insert(path.path(), status);
    if (path.path() == jobPath) {
      result = status;
      emit finished();
    }
  }
};
