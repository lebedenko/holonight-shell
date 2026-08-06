#pragma once

#include "IdleBackend.h"

// Fallback backend when ext-idle-notify-v1 is not available on the compositor.
// GetSessionIdleTime always returns 0; idleThresholdExceeded is never emitted.
class NullIdleBackend final : public IdleBackend {
  Q_OBJECT
 public:
  explicit NullIdleBackend(QObject* parent = nullptr) : IdleBackend(parent) {}

  [[nodiscard]] uint getSessionIdleTimeSeconds() const override { return 0; }
};
