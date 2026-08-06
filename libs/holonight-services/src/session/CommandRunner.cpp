#include "CommandRunner.h"

#include <QLoggingCategory>
#include <QProcess>

namespace {
Q_LOGGING_CATEGORY(lcSession, "holonight.session")
}  // namespace

bool DetachedCommandRunner::run(const QString& program, const QStringList& args) {
  const bool started = QProcess::startDetached(program, args);
  if (!started) {
    qCWarning(lcSession) << "failed to launch" << program << args;
  }
  return started;
}
