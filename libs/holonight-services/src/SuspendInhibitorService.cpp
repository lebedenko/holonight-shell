#include "SuspendInhibitorService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QProcess>
#include <QTimer>

#include <memory>

Q_LOGGING_CATEGORY(lcSuspendInhibitor, "holonight.suspendinhibitor")

static constexpr int kPollIntervalMs{5000};
static constexpr int kBusctlTimeoutMs{2000};

SuspendInhibitorService::SuspendInhibitorService(QObject* parent)
    : QObject(parent), model_(this), poll_timer_(new QTimer(this)) {
  poll_timer_->setInterval(kPollIntervalMs);
  poll_timer_->setSingleShot(false);
  connect(poll_timer_, &QTimer::timeout, this, [this] { poll(); });
  connect(&model_, &InhibitorModel::countChanged, this, &SuspendInhibitorService::inhibitorCountChanged);
}

SuspendInhibitorService::SuspendInhibitorService(SkipInitTag /*tag*/, QObject* parent)
    : QObject(parent), model_(this), poll_timer_(new QTimer(this)), synchronous_poll_for_tests_(true) {
  connect(&model_, &InhibitorModel::countChanged, this, &SuspendInhibitorService::inhibitorCountChanged);
}

void SuspendInhibitorService::start() {
  poll();
  poll_timer_->start();
}

void SuspendInhibitorService::pauseActivity() {
  if (paused_) {
    return;
  }
  paused_ = true;
  poll_timer_->stop();
}

void SuspendInhibitorService::resumeActivity() {
  if (!paused_) {
    return;
  }
  paused_ = false;
  poll();
  poll_timer_->start();
}

QList<InhibitorEntry> SuspendInhibitorService::listInhibitors() {
  // Qt's reply.arguments() crashes on a(ssssuu) replies (same libdbus assertion bug as
  // ListSessions a(susso)). Use busctl as a reliable out-of-process fallback.
  QProcess busctl;
  busctl.start(QStringLiteral("busctl"),
               {QStringLiteral("call"), QStringLiteral("--system"), QStringLiteral("--no-pager"),
                QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("ListInhibitors"),
                QStringLiteral("--json=short")});
  if (!busctl.waitForFinished(kBusctlTimeoutMs) || busctl.exitCode() != 0) {
    qCWarning(lcSuspendInhibitor) << "ListInhibitors via busctl failed:" << busctl.errorString();
    return {};
  }

  return parseBusctlOutput(busctl.readAllStandardOutput());
}

QList<InhibitorEntry> SuspendInhibitorService::parseBusctlOutput(const QByteArray& output) {
  // Output: {"type":"a(ssssuu)","data":[[entry, ...]]}
  // Each entry: [what, who, why, mode, uid, pid]
  const QJsonDocument doc = QJsonDocument::fromJson(output.trimmed());
  if (doc.isNull() || !doc.isObject()) {
    qCWarning(lcSuspendInhibitor) << "ListInhibitors: invalid JSON from busctl";
    return {};
  }

  const QJsonArray outer = doc.object().value(QStringLiteral("data")).toArray();
  if (outer.isEmpty()) {
    return {};
  }

  QList<InhibitorEntry> entries;
  for (const auto& val : outer.at(0).toArray()) {
    const QJsonArray entry = val.toArray();
    if (entry.size() < 3) {
      continue;
    }
    if (!entry.at(0).toString().contains(QLatin1String("sleep"))) {
      continue;
    }
    entries.append({.who = entry.at(1).toString(), .why = entry.at(2).toString()});
  }
  return entries;
}

void SuspendInhibitorService::poll() {
  if (synchronous_poll_for_tests_) {
    model_.setEntries(listInhibitors());
    return;
  }
  startAsyncPoll();
}

void SuspendInhibitorService::startAsyncPoll() {
  if (poll_in_flight_) {
    return;
  }
  poll_in_flight_ = true;

  auto* process = new QProcess(this);
  process->setProgram(QStringLiteral("busctl"));
  process->setArguments({QStringLiteral("call"), QStringLiteral("--system"), QStringLiteral("--no-pager"),
                         QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                         QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("ListInhibitors"),
                         QStringLiteral("--json=short")});

  auto* timeout = new QTimer(process);
  timeout->setSingleShot(true);
  timeout->setInterval(kBusctlTimeoutMs);
  auto completed = std::make_shared<bool>(false);
  connect(timeout, &QTimer::timeout, process, [process] {
    qCWarning(lcSuspendInhibitor) << "ListInhibitors via busctl timed out";
    process->kill();
  });
  connect(process, &QProcess::finished, this, [this, process, completed](int exit_code, QProcess::ExitStatus) {
    if (*completed) {
      return;
    }
    *completed = true;
    finishAsyncPoll(process, exit_code);
  });
  connect(process, &QProcess::errorOccurred, this, [this, process, completed](QProcess::ProcessError error) {
    if (error == QProcess::Crashed || *completed) {
      return;
    }
    *completed = true;
    poll_in_flight_ = false;
    qCWarning(lcSuspendInhibitor) << "ListInhibitors via busctl failed:" << process->errorString();
    process->deleteLater();
  });

  timeout->start();
  process->start();
}

void SuspendInhibitorService::finishAsyncPoll(QProcess* process, int exit_code) {
  poll_in_flight_ = false;
  if (exit_code != 0) {
    qCWarning(lcSuspendInhibitor) << "ListInhibitors via busctl failed:" << process->errorString();
    process->deleteLater();
    return;
  }

  const QList<InhibitorEntry> entries = parseBusctlOutput(process->readAllStandardOutput());
  if (!paused_) {
    model_.setEntries(entries);
  }
  process->deleteLater();
}
