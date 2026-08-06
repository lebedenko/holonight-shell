#pragma once

#include <QObject>

// Abstract idle-tracking backend. The concrete implementation subscribes to the Wayland
// ext-idle-notify-v1 protocol; NullIdleBackend is used when the protocol is unavailable.
class IdleBackend : public QObject {
  Q_OBJECT
 public:
  ~IdleBackend() override = default;

  IdleBackend(const IdleBackend&) = delete;
  IdleBackend& operator=(const IdleBackend&) = delete;
  IdleBackend(IdleBackend&&) = delete;
  IdleBackend& operator=(IdleBackend&&) = delete;

  // Returns elapsed idle time in whole seconds: (now − last user activity).
  // NullIdleBackend always returns 0.
  [[nodiscard]] virtual uint getSessionIdleTimeSeconds() const = 0;

 Q_SIGNALS:
  // Emitted once when the seat crosses the configured idle threshold (idle=true) and once when
  // user activity resumes (idle=false). Not emitted on every 1-second Wayland poll event.
  void idleThresholdExceeded(bool idle);

 protected:
  explicit IdleBackend(QObject* parent = nullptr);
};
