#pragma once

#include "AudioTypes.h"

#include <QObject>
#include <QTimer>

#include <cstdint>
#include <memory>
#include <vector>

class PulseAudioSystem;

class PulseAudioBackend : public QObject {
  Q_OBJECT

 public:
  explicit PulseAudioBackend(QObject* parent = nullptr);
  ~PulseAudioBackend() override;

  PulseAudioBackend(const PulseAudioBackend&) = delete;
  PulseAudioBackend& operator=(const PulseAudioBackend&) = delete;
  PulseAudioBackend(PulseAudioBackend&&) = delete;
  PulseAudioBackend& operator=(PulseAudioBackend&&) = delete;

  static void setPulseAudioSystem(PulseAudioSystem* sys);
  static void resetPulseAudioSystem();

  // Test seam only — overrides the production backoff schedule (default: 1s,2s,4s,8s,16s,30s-cap)
  // so reconnect-timing tests don't block on real wall-clock delays.
  static void setReconnectBackoffScheduleForTests(std::vector<int> delays_ms);
  static void resetReconnectBackoffSchedule();

  void start();
  void stop();

  void setDeviceVolume(uint32_t idx, int percent);
  void setDeviceMuted(uint32_t idx, bool muted);
  void setSourceVolume(uint32_t idx, int percent);
  void setSourceMuted(uint32_t idx, bool muted);
  void setDefaultOutput(uint32_t idx);
  void setDefaultInput(uint32_t idx);
  void setDefaultOutputByName(const QString& name);
  void setDefaultInputByName(const QString& name);
  void setStreamVolume(uint32_t idx, int percent);
  void setStreamMuted(uint32_t idx, bool muted);
  void moveStreamToDevice(uint32_t stream_idx, uint32_t device_idx);

 Q_SIGNALS:
  void deviceAdded(AudioDevice device);
  void sinkRemoved(uint32_t idx);
  void sourceRemoved(uint32_t idx);
  void deviceChanged(AudioDevice device);
  void streamAdded(AudioStream stream);
  void streamRemoved(uint32_t idx);
  void streamChanged(AudioStream stream);
  void availableChanged(bool available);
  void healthStateChanged(AudioHealthState state);

 private Q_SLOTS:
  void onContextLost();
  void onReconnectSucceeded();
  void attemptReconnect();

 private:
  void scheduleReconnect();
  void setHealthState(AudioHealthState state);

  static constexpr int kMaxReconnectAttempts = 8;

  int reconnect_attempt_{0};
  QTimer* reconnect_timer_{nullptr};
  AudioHealthState health_state_{AudioHealthState::Connected};

  struct Impl;
  std::unique_ptr<Impl> impl_;
};
