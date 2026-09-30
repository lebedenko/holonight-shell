#include "ProcessEnvironment.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

bool SystemProcessEnvironment::isRunning(const QString& process_name) const {
  // Linux truncates /proc/<pid>/comm to 15 characters; compare against the same truncation so
  // long daemon names still match.
  const QString wanted = process_name.left(15);

  const QDir proc(QStringLiteral("/proc"));
  const QStringList entries = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QString& entry : entries) {
    bool numeric = false;
    entry.toLongLong(&numeric);
    if (!numeric) {
      continue;
    }

    QFile comm(proc.filePath(entry + QStringLiteral("/comm")));
    if (!comm.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }
    const QString name = QString::fromUtf8(comm.readAll()).trimmed();
    if (name == wanted) {
      return true;
    }
  }
  return false;
}

QString SystemProcessEnvironment::findExecutable(const QString& name) const {
  return QStandardPaths::findExecutable(name);
}

bool SystemProcessEnvironment::isUserServiceActive(const QString& service_name) const {
  QProcess systemctl;
  systemctl.start(QStringLiteral("systemctl"),
                  {QStringLiteral("--user"), QStringLiteral("is-active"), QStringLiteral("--quiet"), service_name});
  if (!systemctl.waitForFinished(1000)) {
    systemctl.kill();
    systemctl.waitForFinished();
    return false;
  }
  return systemctl.exitStatus() == QProcess::NormalExit && systemctl.exitCode() == 0;
}
