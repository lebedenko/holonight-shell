#pragma once

#include <QString>

// Probe seam for session/lock logic: answers "is this process running?" and "where is this
// executable on PATH?". Abstract so unit tests can inject a fake and exercise every lock branch
// without a live Wayland session or real processes. The production implementation
// (SystemProcessEnvironment) reads /proc and consults QStandardPaths.
class ProcessEnvironment {
 public:
  virtual ~ProcessEnvironment() = default;

  ProcessEnvironment(const ProcessEnvironment&) = delete;
  ProcessEnvironment& operator=(const ProcessEnvironment&) = delete;
  ProcessEnvironment(ProcessEnvironment&&) = delete;
  ProcessEnvironment& operator=(ProcessEnvironment&&) = delete;

  // True if a process whose name (comm) equals process_name is currently running.
  [[nodiscard]] virtual bool isRunning(const QString& process_name) const = 0;

  // Full path to name if found on PATH, otherwise an empty string.
  [[nodiscard]] virtual QString findExecutable(const QString& name) const = 0;

  // True if the named systemd user service is currently active.
  [[nodiscard]] virtual bool isUserServiceActive(const QString& service_name) const = 0;

 protected:
  ProcessEnvironment() = default;
};

// Production probe: isRunning scans /proc/<pid>/comm, user service state is read from systemd,
// and findExecutable delegates to QStandardPaths::findExecutable so no executable paths are ever hardcoded.
class SystemProcessEnvironment final : public ProcessEnvironment {
 public:
  [[nodiscard]] bool isRunning(const QString& process_name) const override;
  [[nodiscard]] QString findExecutable(const QString& name) const override;
  [[nodiscard]] bool isUserServiceActive(const QString& service_name) const override;
};
