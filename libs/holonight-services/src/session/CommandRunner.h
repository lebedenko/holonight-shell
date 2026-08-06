#pragma once

#include <QString>
#include <QStringList>

// Launch seam for session/lock actions. Abstract so tests can spy on what *would* be launched
// (program + args) without actually starting a process. The production implementation
// (DetachedCommandRunner) uses QProcess::startDetached, which reparents the child to init(1) and
// therefore never leaves a zombie for us to reap.
class CommandRunner {
 public:
  virtual ~CommandRunner() = default;

  CommandRunner(const CommandRunner&) = delete;
  CommandRunner& operator=(const CommandRunner&) = delete;
  CommandRunner(CommandRunner&&) = delete;
  CommandRunner& operator=(CommandRunner&&) = delete;

  // Launches program with args (detached). Returns false if the launch could not be started.
  // Implementations must never throw and never block.
  virtual bool run(const QString& program, const QStringList& args) = 0;

 protected:
  CommandRunner() = default;
};

class DetachedCommandRunner final : public CommandRunner {
 public:
  bool run(const QString& program, const QStringList& args) override;
};
