#pragma once

#include "IdleBackend.h"

#include <chrono>
#include <memory>

class IdleNotifierGlobal;
class IdleNotification;

// Implements idle tracking via the ext-idle-notify-v1 Wayland protocol.
// Registers two notifications: one at 1 second to maintain an accurate last-activity timestamp
// (used by GetSessionIdleTime), and one at threshold_ms to emit idleThresholdExceeded.
// When the compositor does not advertise the protocol, stays silent (isBackendActive = false).
class ExtIdleNotifyBackend final : public IdleBackend {
  Q_OBJECT

 public:
  explicit ExtIdleNotifyBackend(int threshold_ms, QObject* parent = nullptr);
  ~ExtIdleNotifyBackend() override;

  ExtIdleNotifyBackend(const ExtIdleNotifyBackend&) = delete;
  ExtIdleNotifyBackend& operator=(const ExtIdleNotifyBackend&) = delete;
  ExtIdleNotifyBackend(ExtIdleNotifyBackend&&) = delete;
  ExtIdleNotifyBackend& operator=(ExtIdleNotifyBackend&&) = delete;

  [[nodiscard]] uint getSessionIdleTimeSeconds() const override;

  // Recreates the threshold notification with the new timeout. The 1-second tracker is unaffected.
  void updateThreshold(int threshold_ms);

  [[nodiscard]] bool isBackendActive() const;

 Q_SIGNALS:
  void backendActiveChanged(bool active);

 private Q_SLOTS:
  void onGlobalActiveChanged();

 private:
  void createNotifications();
  void destroyNotifications();
  void destroyThresholder();

  void onTrackerEvent(bool idle);
  void onThresholderEvent(bool idle);

  std::unique_ptr<IdleNotifierGlobal> global_;
  std::unique_ptr<IdleNotification> tracker_;
  std::unique_ptr<IdleNotification> thresholder_;

  int threshold_ms_;
  std::chrono::steady_clock::time_point last_activity_{std::chrono::steady_clock::now()};
  bool tracker_idle_{false};
  bool backend_active_{false};
};
