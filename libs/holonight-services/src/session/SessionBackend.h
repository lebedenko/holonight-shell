#pragma once

#include "SessionCommandResult.h"

#include <QString>
#include <QStringList>

#include <memory>

class ProcessEnvironment;
class CommandRunner;
class Locker;

// Abstract session backend. Compositor-specific behaviour (logout, lock dispatch, capability
// reporting) is virtual; the power actions (sleep/reboot/shutdown) are identical systemctl calls
// on every compositor and are implemented once here. The shared, compositor-independent lock
// strategy lives in an owned Locker, reached by subclasses through runLocker().
class SessionBackend {
 public:
  virtual ~SessionBackend();

  SessionBackend(const SessionBackend&) = delete;
  SessionBackend& operator=(const SessionBackend&) = delete;
  SessionBackend(SessionBackend&&) = delete;
  SessionBackend& operator=(SessionBackend&&) = delete;

  // Compositor-specific.
  [[nodiscard]] virtual SessionCommandResult logout() = 0;
  [[nodiscard]] virtual SessionCommandResult lockScreen() = 0;
  [[nodiscard]] virtual bool logoutSupported() const = 0;
  [[nodiscard]] virtual QString backendName() const = 0;

  // Common across all backends (systemctl / logind).
  [[nodiscard]] SessionCommandResult sleep();
  [[nodiscard]] SessionCommandResult reboot();
  [[nodiscard]] SessionCommandResult shutdown();

  // Lock-handler availability, forwarded from the shared Locker (identical for every backend).
  [[nodiscard]] bool lockerAvailable() const;
  [[nodiscard]] QString lockerName() const;

 protected:
  SessionBackend(const ProcessEnvironment* env, CommandRunner* runner);

  // Runs the shared layered lock strategy. Subclasses call this from lockScreen().
  [[nodiscard]] SessionCommandResult runLocker();

  // Launches a command through the injected runner. Subclasses use this for compositor-specific
  // actions (e.g. Hyprland logout) instead of touching the runner directly.
  [[nodiscard]] SessionCommandResult run(const QString& program, const QStringList& args);

 private:
  CommandRunner* runner_;
  std::unique_ptr<Locker> locker_;
};
