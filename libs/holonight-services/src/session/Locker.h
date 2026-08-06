#pragma once

#include "SessionCommandResult.h"

#include <QString>

class ProcessEnvironment;
class CommandRunner;

// Compositor-independent lock strategy shared by every SessionBackend. Layered, and never
// hardcodes a single locker:
//   1. If a known lock-handler daemon (hypridle/swayidle/xss-lock) is running, invoke
//      `loginctl lock-session` (integrated path: sets logind LockedHint, coordinates suspend).
//   2. Otherwise resolve a locker from PATH in priority order (hyprlock, swaylock, gtklock,
//      waylock) and spawn the first found — unless it is already running (idempotency guard).
//   3. If nothing is available, do nothing (no crash, no error).
// Exposed lockerAvailable()/lockerName() are a snapshot taken at construction (CONSTANT to QML);
// lock() itself re-probes live so it reflects daemons/lockers started after construction.
class Locker {
 public:
  Locker(const ProcessEnvironment* env, CommandRunner* runner);

  [[nodiscard]] SessionCommandResult lock();

  [[nodiscard]] bool lockerAvailable() const { return locker_available_; }
  [[nodiscard]] QString lockerName() const { return locker_name_; }

 private:
  enum class Mode : std::uint8_t { None, Daemon, Locker };
  struct Resolution {
    Mode mode{Mode::None};
    QString name;  // daemon or locker basename
    QString path;  // locker full path (empty for daemon / none)
  };

  [[nodiscard]] Resolution resolve() const;

  const ProcessEnvironment* env_;
  CommandRunner* runner_;
  bool locker_available_{false};
  QString locker_name_;
};
