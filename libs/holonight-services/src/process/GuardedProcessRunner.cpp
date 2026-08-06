#include "GuardedProcessRunner.h"

#include <QPointer>
#include <QTimer>

#include <memory>
#include <utility>

void runGuardedProcess(const QString& program, const QStringList& arguments, int timeout_ms,
                       std::function<void(GuardedProcessResult)> callback, QObject* callback_context) {
  if (timeout_ms <= 0) {
    GuardedProcessResult result;
    result.had_error = true;
    result.stderr_text = QStringLiteral("process timeout must be positive");
    callback(std::move(result));
    return;
  }

  auto* proc = new QProcess();
  proc->setProgram(program);
  proc->setArguments(arguments);

  auto* timer = new QTimer(proc);
  timer->setSingleShot(true);
  timer->setInterval(timeout_ms);

  auto callback_ptr = std::make_shared<std::function<void(GuardedProcessResult)>>(std::move(callback));
  auto completed = std::make_shared<bool>(false);
  const bool has_callback_context = callback_context != nullptr;
  QPointer<QObject> guarded_context(callback_context);

  const auto deliver = [callback_ptr, has_callback_context, guarded_context](GuardedProcessResult result) {
    if (!has_callback_context || !guarded_context.isNull()) {
      (*callback_ptr)(std::move(result));
    }
  };

  QObject::connect(timer, &QTimer::timeout, proc, [proc, completed, deliver]() mutable {
    if (*completed) {
      return;
    }
    *completed = true;
    GuardedProcessResult result;
    result.timed_out = true;
    result.stderr_text = QStringLiteral("process timed out");
    proc->kill();
    deliver(std::move(result));
    proc->deleteLater();
  });

  QObject::connect(proc, &QProcess::finished, proc,
                   [proc, completed, deliver](int exit_code, QProcess::ExitStatus) mutable {
                     if (*completed) {
                       return;
                     }
                     *completed = true;
                     GuardedProcessResult result;
                     result.exit_code = exit_code;
                     result.stdout_text = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
                     result.stderr_text = QString::fromUtf8(proc->readAllStandardError()).trimmed();
                     deliver(std::move(result));
                     proc->deleteLater();
                   });

  QObject::connect(proc, &QProcess::errorOccurred, proc,
                   [proc, completed, deliver](QProcess::ProcessError error) mutable {
                     if (error == QProcess::Crashed) {
                       return;
                     }
                     if (*completed) {
                       return;
                     }
                     *completed = true;
                     GuardedProcessResult result;
                     result.error = error;
                     result.had_error = true;
                     result.stderr_text = proc->errorString();
                     deliver(std::move(result));
                     proc->deleteLater();
                   });

  timer->start();
  proc->start();
}
